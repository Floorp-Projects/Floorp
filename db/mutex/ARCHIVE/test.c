/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)test.c	10.4 (Sleepycat) 9/9/97";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"

void run_one();
void usage();

#define	MMAPFILE	"mmap.file"

#define	MAXLOCKS	 20			/* Number of locks. */
#define	NLOCKS		100			/* Number of locks per proc. */
#define	NPROCS		  5			/* Number of processes. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	extern char *optarg;
	db_mutex_t *marp, *mp;
	pid_t pid;
	int ch, cnt, fd, maxlock, nlock, nproc, status;

	maxlock = MAXLOCKS;
	nlock = NLOCKS;
	nproc = NPROCS;
	while ((ch = getopt(argc, argv, "l:n:p:")) != EOF)
		switch(ch) {
		case 'l':
			maxlock = atoi(optarg);
			if (maxlock < 1)
				usage();
			break;
		case 'n':
			nlock = atoi(optarg);
			if (nlock < 1)
				usage();
			break;
		case 'p':
			nproc = atoi(optarg);
			if (nproc < 1)
				usage();
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* Cleanup for debugging. */
	unlink(MMAPFILE);

	/* Initialize the mutex file. */
	(void)printf("Initialize the mutex file...\n");
	fd = open(MMAPFILE, O_CREAT | O_RDWR | O_TRUNC,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd == -1) {
		(void)printf("parent: %s: %s\n", MMAPFILE, strerror(errno));
		exit(1);
	}
	if ((marp =
	    (db_mutex_t *)calloc(maxlock, sizeof(db_mutex_t))) == NULL) {
		(void)printf("parent: %s\n", strerror(errno));
		exit(1);
	}
	if (write(fd, marp,
	    maxlock * sizeof(db_mutex_t)) != maxlock * sizeof(db_mutex_t)) {
		(void)printf("parent: write: %s\n", strerror(errno));
		exit(1);
	}

#ifndef MAP_FAILED
#define	MAP_FAILED	-1
#endif
	(void)printf("Mmap the mutex file...\n");
	marp = (db_mutex_t *)mmap(NULL, MAXLOCKS * sizeof(db_mutex_t),
	    PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)0);
	if (marp == (db_mutex_t *)MAP_FAILED) {
		(void)printf("parent: mmap: %s\n", strerror(errno));
		exit(1);
	}

	/* Initialize the mutexes. */
	(void)printf("Initialize the mutexes...\n");
	for (cnt = 0, mp = marp; cnt < MAXLOCKS; ++cnt, ++mp)
		__db_mutex_init(mp, cnt);

	/* Run processes to compete for the locks. */
	(void)printf("%s: %d processes: %d requests from %d locks\n",
#ifdef HAVE_SPINLOCKS
	    "Using spinlocks",
#else
	    "Using fcntl",
#endif
	    nproc, nlock, maxlock);
	for (cnt = 0; cnt < nproc; ++cnt)
		run_one(maxlock, nlock, nproc, fd, marp);

	/* Wait for processes to finish. */
	(void)printf("Wait for processes...\n");
	while ((pid = waitpid(-1, &status, 0)) > 0) {
		(void)printf("%d", (int)pid);
		if (WIFEXITED(status))
			(void)printf(": exited %d", WEXITSTATUS(status));
		else if (WIFSIGNALED(status)) {
			(void)printf(": signaled %d", WTERMSIG(status));
			if (WCOREDUMP(status))
				(void)printf(": core dumped");
		}
		printf("\n");
	}

	/* Print mutex statistics. */
	for (cnt = 0, mp = marp; cnt < MAXLOCKS; ++cnt, ++mp) {
		if (mp->mutex_set_wait == 0 && mp->mutex_set_nowait == 0)
			continue;
		(void)printf("mutex %2d: wait: %2lu; no wait %2lu\n",
		    cnt, mp->mutex_set_wait, mp->mutex_set_nowait);
	}

#ifdef MUTEX_DESTROY
	/* Discard mutexes. */
	(void)printf("Discard mutexes...\n");
	for (cnt = 0, mp = marp; cnt < MAXLOCKS; ++cnt, ++mp)
		if (__db_mutex_destroy(mp))
			perror("db_mutex_destroy");
#endif

	/* Close the file descriptor. */
	(void)close(fd);

	/* Lose the mmap'd file. */
	(void)munmap((caddr_t)mp, sizeof(db_mutex_t));
	(void)unlink(MMAPFILE);

	exit(0);
}

void
run_one(maxlock, nlock, nproc, fd, mp)
	int maxlock, nlock, nproc, fd;
	db_mutex_t *mp;
{
	pid_t mypid;
	int lock;
	char buf[128];

	/* Create a new process. */
	if (nproc > 1)
		switch (fork()) {
		case -1:
			perror("fork");
			exit (1);
		case 0:
			break;
		default:
			return;
		}

	mypid = getpid();
	srand(time(NULL) / mypid);
	while (nlock--) {
		lock = rand() % maxlock;
#define	VERBOSE
#ifdef	VERBOSE
		(void)sprintf(buf, "%d %d\n", (int)mypid, lock);
		write(1, buf, strlen(buf));
#endif
		if (__db_mutex_lock(mp + lock, fd, NULL)) {
			(void)printf("child %d: never got lock\n", (int)mypid);
			goto err;
		}
		mp[lock].pid = mypid;
		sleep(rand() & 2);
		if (mypid != mp[lock].pid) {
			(void)printf("child %d: RACE!\n", (int)mypid);
			goto err;
		}
		if (__db_mutex_unlock(mp + lock, fd)) {
			(void)printf("child %d: wakeup failed\n", (int)mypid);
			goto err;

		}
	}
	exit(0);

err:	close(fd);
	exit(1);
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: run [-l max_locks] [-n locks] [-p procs]\n");
	exit (1);
}
