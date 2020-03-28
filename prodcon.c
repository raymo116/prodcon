/*
OBJECTIVE:  The objective of this assignment is to use semaphores to protect the
            critical section between two competing threads.

ASSIGNMENT: The idea is to write a C/C++ program that creates two threads. The
            first thread is the consumer thread that consumes the data written
            to a shared memory buffer. The second thread is the producer thread
            that “produces” the data for the shared memory buffer. In order to
            prevent a race condition (e.g. the consumer reading before the
            producer writing) use a mutex semaphore and counting semaphores to
            coordinate when each thread can safely write or read to/from a
            common shared memory region.
*/


#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

void *producerThread(void* param);
void *consumerThread(void* param);

// The size of a memory region being used
int memsize = 32;
// The number of memory blocks being used
int blocks = 1;
// The number of times the producer writes to and the consumer reads from the
// shared memory region
int n_times = 2;
// Te shared memory
unsigned char* shared_memory;
const int BLOCK_SIZE = 32;
const int MAX_MEM_SIZE = 64000;

// Threading
pthread_mutex_t mutex;
sem_t sem[2];

/*
The producer/consumer program (prodcon.c) that takes three arguments from the
command line (no prompting the user from within the program).
*/
int main(int argc, char *argv[]) {

    // Checks to make sure that the correct number of parameters were added
    if (argc != 3) {
        fprintf(stderr,"usage: ./prodcon <memsize (divisible by 32)> <ntimes>\n");
        return -1;
    }

    // Checks to see if the input is positive
    if (atoi(argv[1]) < 1) {
        fprintf(stderr,"Argument %d must be positive\n",atoi(argv[1]));
        return -1;
    } else if (atoi(argv[2]) < 1) {
        fprintf(stderr,"Argument %d must be positive\n",atoi(argv[2]));
        return -1;
    // Checks to make sure that the input is divisible by 32
    } else if (atoi(argv[1])%32) {
        fprintf(stderr,"Argument %d must be divisible by 32\n",atoi(argv[1]));
        return -1;
    // Limit memsize to 64K
    } else if (atoi(argv[1]) > MAX_MEM_SIZE) {
        fprintf(stderr,"Argument %d must be less than 64,000\n",atoi(argv[1]));
        return -1;
    }

    // Initializes based on the user input
    memsize = atoi(argv[1]);
    blocks = memsize/BLOCK_SIZE;
    n_times = atoi(argv[2]);
    shared_memory = malloc(memsize);

    // Initializes the semaphores
    if ((sem_init(&sem[0],0,1) == -1) || (sem_init(&sem[1],0,0) == -1))
        printf("%s\n",strerror(errno));

    // Initializes the consumer
    pthread_mutex_init(&mutex, NULL);

    // Create both the producer and consumer threads
    pthread_t producer;
    pthread_t consumer;

    // Creates threads
    pthread_create(&producer,NULL,producerThread, NULL);
    pthread_create(&consumer,NULL,consumerThread, NULL);

    // Wait for both threads to finish.
    pthread_join(producer,NULL);
    pthread_join(consumer,NULL);

    // Destroy semaphores
	if ((sem_destroy(&sem[0]) != 0) || (sem_destroy(&sem[1]) != 0))
	    printf("%s\n",strerror(errno));

    // Free up memory
    free(shared_memory);
    pthread_mutex_destroy(&mutex);

    return 0;
}

/*
DESC:   The producer thread is to create 30 bytes of random data (0-255) then
        store the checksum (use the Internet checksum) in the last 2 bytes of
        the shared memory block. The producer is to do this ntimes synchronizing
        with the consumer.
*/
void *producerThread(void* param) {

    // Does it n times
    for(int n = 0; n < n_times; ++n) {
        // current index of memory region
        int index;

        // Blocks until consumer is ready to start over
        if (sem_wait(&sem[0]) != 0)
            printf("%s\n",strerror(errno));

        // Cycles through all of the memory blocks
        for (int block = 0; block < blocks; block++) {
            // Puts a lock on the memory
            pthread_mutex_lock(&mutex);

            unsigned short int checksum = 0;

            // Creates random data and saves to checksum
            for(index = (block*BLOCK_SIZE); index < (((block+1)*BLOCK_SIZE)-2); ++index) {
                shared_memory[index] = rand()%255;
                checksum += shared_memory[index];
            }

            // Store the checksum (use the Internet checksum) in the last 2 bytes
            // of the shared memory block
            ((unsigned short int *)shared_memory)[(index+1)/2] = checksum;
            // Release memory
            pthread_mutex_unlock(&mutex);

            // Let consumer loose
            if (sem_post(&sem[1]) != 0)
                printf("%s\n",strerror(errno));
            }

    }

    pthread_exit(0);
}

/*
DESC:   The consumer thread is to read the shared memory buffer of 30 bytes,
        calculate the checksum based upon the 30 bytes and compare that with
        the value stored in the shared memory buffer to ensure that the data did
        not get corrupted. The consumer is to do this ntimes synchronizing each
        read with the producer.
*/
void *consumerThread(void* param) {

    // Does it n times
    for(int n = 0; n < n_times; ++n) {
        int index = 0;

        // Cycles through all of the memory blocks
        for (int block = 0; block < blocks; block++) {

            // Blocks if there's no data to read
    	    if (sem_wait(&sem[1]) != 0)
    	        printf("%s\n",strerror(errno));

            // Locks memory
            pthread_mutex_lock(&mutex);

            unsigned short int calculated_checksum = 0;

            // Reads the memory and calculates the checksum
            for(index = (block*BLOCK_SIZE); index < (((block+1)*BLOCK_SIZE)-2); ++index) {
                calculated_checksum += shared_memory[index];
            }

            // Grabs the checksum from the end of the shared memory region
            int given_checksum = ((unsigned short int *)shared_memory)[(index+1)/2];

            // Compare that with the value stored in the shared memory buffer to
            // ensure that the data did not get corrupted
            // If the consumer detects a mismatched checksum it is to report the
            // error along with the expected checksum and the calculated checksum
            // and exit the program.
            if(given_checksum != calculated_checksum) {
                printf("The checksums at block %d, iteration %d did not match\n", block, n);
                printf("Recieved Checksum: %d\nCalculated Checksum: %d\n", given_checksum, calculated_checksum);
                exit(1);
            }

            // Release memory region
            pthread_mutex_unlock(&mutex);
        }

        // Allow the producer to continue on to the next cycle
        if (sem_post(&sem[0]) != 0)
            printf("%s\n",strerror(errno));
    }

    pthread_exit(0);
}
