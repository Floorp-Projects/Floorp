#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* for PTHREAD_MUTEX_INITIALIZER under linux */
#endif
#include <pthread.h>

pthread_mutex_t test_mutex_initializer = PTHREAD_MUTEX_INITIALIZER;
int test_mutex (void)
{
	int x = 0;
	pthread_mutex_t mutex;
	x |= pthread_mutex_init (&mutex, NULL);
	x |= pthread_mutex_lock (&mutex);
	x |= pthread_mutex_unlock (&mutex);
	x |= pthread_mutex_destroy (&mutex);
	return 0;
}

int test_mutex_attr (void)
{
	int x = 0;
	pthread_mutexattr_t attr;
	pthread_mutex_t mutex;
	x |= pthread_mutexattr_init (&attr);
	x |= pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
	x |= pthread_mutex_init (&mutex, &attr);
	x |= pthread_mutex_lock (&mutex);
	x |= pthread_mutex_unlock (&mutex);
	x |= pthread_mutex_destroy (&mutex);
	x |= pthread_mutexattr_destroy (&attr);
	return x;
}

int main(void) {
  return 0;
}
