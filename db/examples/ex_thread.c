/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)ex_thread.c	10.6 (Sleepycat) 5/31/98
 */

#include "config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#endif

#include <db.h>

/*
 * NB: This application is written using POSIX 1003.1b-1993 pthreads
 * interfaces, which may not be portable to your system.
 */
extern int sched_yield(void);			/* Pthread yield function. */

DB_ENV *db_init(char *);
void   *deadlock(void *);
void	fatal(char *, int);
int	main __P((int, char *[]));
int	reader(int);
void	stats(void);
void   *trickle(void *);
void   *tstart(void *);
void	usage(void);
void	word(void);
int	writer(int);

struct _statistics {
	int aborted;				/* Write. */
	int aborts;				/* Read/write. */
	int adds;				/* Write. */
	int deletes;				/* Write. */
	int txns;				/* Write. */
	int found;				/* Read. */
	int notfound;				/* Read. */
} *perf;

const char
	*progname = "ex_thread";		/* Program name. */

#define	DATABASE	"access.db"		/* Database name. */

#define	WORDLIST	"../test/wordlist"	/* Dictionary. */

/*
 * We can seriously increase the number of collisions and transaction
 * aborts by yielding the scheduler after every DB call.  Specify the
 * -p option to do this.
 */
int	punish;					/* -p */
int	nlist;					/* -n */
int	nreaders;				/* -r */
int	verbose;				/* -v */
int	nwriters;				/* -w */

DB     *dbp;					/* Database handle. */
DB_ENV *dbenv;					/* Database environment. */
int	nthreads;				/* Total threads. */
char  **list;					/* Word list. */

/*
 * ex_thread --
 *	Run a simple threaded application of some numbers of readers and
 *	writers competing for a set of words.
 *
 * Example UNIX shell script to run this program:
 *	% rm -rf TESTDIR
 *	% mkdir TESTDIR
 *	% ex_thread -h TESTDIR
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int errno, optind;
	DB_INFO dbinfo;
	pthread_t *tids;
	int ch, i;
	char *home;
	void *retp;

	home = "TESTDIR";
	nlist = 1000;
	nreaders = nwriters = 4;
	while ((ch = getopt(argc, argv, "h:pn:r:vw:")) != EOF)
		switch (ch) {
		case 'h':
			home = optarg;
			break;
		case 'p':
			punish = 1;
			break;
		case 'n':
			nlist = atoi(optarg);
			break;
		case 'r':
			nreaders = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			nwriters = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* Initialize the random number generator. */
	srand(getpid() | time(NULL));

	/* Build the key list. */
	word();

	/* Remove the previous database. */
	(void)unlink(DATABASE);

	/* Initialize the database environment. */
	dbenv = db_init(home);

	/* Initialize the database. */
	memset(&dbinfo, 0, sizeof(dbinfo));
	dbinfo.db_pagesize = 1024;		/* Page size: 1K. */

	if ((errno = db_open(DATABASE, DB_BTREE,
	    DB_CREATE | DB_THREAD, 0664, dbenv, &dbinfo, &dbp)) != 0) {
		fprintf(stderr,
		    "%s: %s: %s\n", progname, DATABASE, strerror(errno));
		exit (1);
	}

	nthreads = nreaders + nwriters + 2;
	printf("Running: readers %d, writers %d\n", nreaders, nwriters);
	fflush(stdout);

	/* Create statistics structures, offset by 1. */
	if ((perf = calloc(nreaders + nwriters + 1, sizeof(*perf))) == NULL)
		fatal(NULL, 1);

	/* Create thread ID structures. */
	if ((tids = malloc(nthreads * sizeof(pthread_t))) == NULL)
		fatal(NULL, 1);

	/* Create reader/writer threads. */
	for (i = 0; i < nreaders + nwriters; ++i)
		if (pthread_create(&tids[i], NULL, tstart, &i))
			fatal("pthread_create", 1);

	/* Create buffer pool trickle thread. */
	if (pthread_create(&tids[i], NULL, trickle, &i))
		fatal("pthread_create", 1);

	/* Create deadlock detector thread. */
	if (pthread_create(&tids[i], NULL, deadlock, &i))
		fatal("pthread_create", 1);

	/* Wait for the threads. */
	for (i = 0; i < nthreads; ++i)
		(void)pthread_join(tids[i], &retp);

	(void)dbp->close(dbp, 0);

	return (db_appexit(dbenv));
}

int
reader(id)
	int id;
{
	DBT key, data;
	int n;
	char buf[64];

	/*
	 * DBT's must use local memory or malloc'd memory if the DB handle
	 * is accessed in a threaded fashion.
	 */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	data.flags = DB_DBT_MALLOC;

	/*
	 * Read-only threads do not require transaction protection, unless
	 * there's a need for repeatable reads.
	 */
	for (;;) {
		/*
		 * Randomly yield the scheduler 20% of the time.  Not
		 * normal for an application, but useful here to stir
		 * things up.
		 */
		if (rand() % 5 == 0)
			sched_yield();

		/* Pick a key at random, and look it up. */
		n = rand() % nlist;
		key.data = list[n];
		key.size = strlen(key.data);

		if (verbose) {
			sprintf(buf, "reader: %d: list entry %d\n", id, n);
			write(STDOUT_FILENO, buf, strlen(buf));
		}

		switch (errno = dbp->get(dbp, NULL, &key, &data, 0)) {
		case EAGAIN:			/* Deadlock. */
			++perf[id].aborts;
			break;
		case 0:				/* Success. */
			++perf[id].found;
			free(data.data);
			break;
		case DB_NOTFOUND:		/* Not found. */
			++perf[id].notfound;
			break;
		default:
			sprintf(buf,
			    "reader %d: dbp->get: %s", id, (char *)key.data);
			fatal(buf, 0);
		}
	}
	return (0);
}

int
writer(id)
	int id;
{
	DBT key, data;
	DB_TXNMGR *txnp;
	DB_TXN *tid;
	time_t now, then;
	int n;
	char buf[256], dbuf[10000];

	time(&now);
	then = now;
	txnp = dbenv->tx_info;

	/*
	 * DBT's must use local memory or malloc'd memory if the DB handle
	 * is accessed in a threaded fashion.
	 */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	data.data = dbuf;
	data.ulen = sizeof(dbuf);
	data.flags = DB_DBT_USERMEM;

	for (;;) {
		/*
		 * Randomly yield the scheduler 20% of the time.  Not
		 * normal for an application, but useful here to stir
		 * things up.
		 */
		if (rand() % 5 == 0)
			sched_yield();

		/* Pick a random key. */
		n = rand() % nlist;
		key.data = list[n];
		key.size = strlen(key.data);

		if (verbose) {
			sprintf(buf, "writer: %d: list entry %d\n", id, n);
			write(STDOUT_FILENO, buf, strlen(buf));
		}

		/* Abort and retry. */
		if (0) {
retry:			if (txn_abort(tid))
				fatal("txn_abort", 1);
			++perf[id].aborts;
			++perf[id].aborted;
		}

		/* Thread #1 prints out the stats every 20 seconds. */
		if (id == 1) {
			time(&now);
			if (now - then >= 20) {
				stats();
				then = now;
			}
		}

		/* Begin the transaction. */
		if ((errno = txn_begin(txnp, NULL, &tid)) != 0)
			fatal("txn_begin", 1);

		if (punish)			/* See -p option comment. */
			sched_yield();

		/*
		 * Get the key.  If it doesn't exist, add it.  If it does
		 * exist, delete it.
		 */
		switch (errno = dbp->get(dbp, tid, &key, &data, 0)) {
		case EAGAIN:
			goto retry;
		case 0:
			goto delete;
		case DB_NOTFOUND:
			goto add;
		}

		sprintf(buf, "writer: %d: dbp->get", id);
		fatal(buf, 1);
		/* NOTREACHED */

delete:		if (punish)			/* See -p option comment. */
			sched_yield();

		/* Delete the key. */
		switch (errno = dbp->del(dbp, tid, &key, 0)) {
		case EAGAIN:
			goto retry;
		case 0:
			++perf[id].deletes;
			goto commit;
		}

		sprintf(buf, "writer: %d: dbp->del", id);
		fatal(buf, 1);
		/* NOTREACHED */

add:		if (punish)			/* See -p option comment. */
			sched_yield();

		/* Add the key.  1 data item in 30 is an overflow item. */
		data.size = 20 + rand() % 128;
		if (rand() % 30 == 0)
			data.size += 8192;

		switch (errno = dbp->put(dbp, tid, &key, &data, 0)) {
		case EAGAIN:
			goto retry;
		case 0:
			++perf[id].adds;
			goto commit;
		default:
			sprintf(buf, "writer: %d: dbp->put", id);
			fatal(buf, 1);
		}

commit:		if (punish)			/* See -p option comment. */
			sched_yield();

		/* The transaction finished, commit it. */
		if ((errno = txn_commit(tid)) != 0)
			fatal("txn_commit", 1);

		/*
		 * Every time the thread completes 20 transactions, show
		 * our progress.
		 */
		if (++perf[id].txns % 20 == 0) {
			sprintf(buf,
"writer: %2d: adds: %4d: deletes: %4d: aborts: %4d: txns: %4d\n",
			    id, perf[id].adds, perf[id].deletes,
			    perf[id].aborts, perf[id].txns);
			write(STDOUT_FILENO, buf, strlen(buf));
		}

		/*
		 * If this thread was aborted more than 5 times before
		 * the transaction finished, complain.
		 */
		if (perf[id].aborted > 5) {
			sprintf(buf,
"writer: %2d: adds: %4d: deletes: %4d: aborts: %4d: txns: %4d: ABORTED: %2d\n",
			    id, perf[id].adds, perf[id].deletes,
			    perf[id].aborts, perf[id].txns, perf[id].aborted);
			write(STDOUT_FILENO, buf, strlen(buf));
		}
		perf[id].aborted = 0;
	}
	return (0);
}

/*
 * stats --
 *	Display reader/writer thread statistics.  To display the statistics
 *	for the mpool trickle or deadlock threads, use db_stat(1).
 */
void
stats()
{
	int id;
	char *p, buf[8192];

	p = buf + sprintf(buf, "-------------\n");
	for (id = 0; id < nreaders + nwriters;)
		if (id++ < nwriters)
			p += sprintf(p,
	"writer: %2d: adds: %4d: deletes: %4d: aborts: %4d: txns: %4d\n",
			    id, perf[id].adds,
			    perf[id].deletes, perf[id].aborts, perf[id].txns);
		else
			p += sprintf(p,
	"reader: %2d: found: %5d: notfound: %5d: aborts: %4d\n",
			    id, perf[id].found,
			    perf[id].notfound, perf[id].aborts);
	p += sprintf(p, "-------------\n");

	write(STDOUT_FILENO, buf, p - buf);
}

/*
 * db_init --
 *	Initialize the environment.
 */
DB_ENV *
db_init(home)
	char *home;
{
	DB_ENV *dbenv;

	/* Define our own yield function. */
	db_jump_set(sched_yield, DB_FUNC_YIELD);

	/* Rely on calloc to initialize the structure. */
	if ((dbenv = (DB_ENV *)calloc(sizeof(DB_ENV), 1)) == NULL) {
		fprintf(stderr, "%s: %s\n", progname, strerror(ENOMEM));
		exit (1);
	}
	dbenv->lg_max = 200000;
	dbenv->db_errfile = stderr;
	dbenv->db_errpfx = progname;
	dbenv->mp_size = 100 * 1024;		/* Cachesize: 100K. */

	if ((errno = db_appinit(home, NULL, dbenv,
	    DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG |
	    DB_INIT_MPOOL | DB_INIT_TXN | DB_THREAD)) != 0) {
		fprintf(stderr,
		    "%s: db_appinit: %s\n", progname, strerror(errno));
		exit (1);
	}
	return (dbenv);
}

/*
 * tstart --
 *	Thread start function for readers and writers.
 */
void *
tstart(arg)
	void *arg;
{
	pthread_t tid;
	int id;

	id = *(int *)arg + 1;

	tid = pthread_self();

	if (id <= nwriters) {
		printf("write thread %d starting: tid: %lu\n", id, (u_long)tid);
		fflush(stdout);
		writer(id);
	} else {
		printf("read thread %d starting: tid: %lu\n", id, (u_long)tid);
		fflush(stdout);
		reader(id);
	}

	/* NOTREACHED */
	return (NULL);
}

/*
 * deadlock --
 *	Thread start function for lock_detect().
 */
void *
deadlock(arg)
	void *arg;
{
	struct timeval t;
	pthread_t tid;

	arg = arg;				/* XXX: shut the compiler up. */
	tid = pthread_self();

	printf("deadlock thread starting: tid: %lu\n", (u_long)tid);
	fflush(stdout);

	t.tv_sec = 0;
	t.tv_usec = 100000;
	for (;;) {
		(void)lock_detect(dbenv->lk_info,
		    DB_LOCK_CONFLICT, DB_LOCK_DEFAULT);

		/* Check every 100ms. */
		(void)select(0, NULL, NULL, NULL, &t);
	}

	/* NOTREACHED */
	return (NULL);
}

/*
 * trickle --
 *	Thread start function for memp_trickle().
 */
void *
trickle(arg)
	void *arg;
{
	pthread_t tid;
	int wrote;
	char buf[64];

	arg = arg;				/* XXX: shut the compiler up. */
	tid = pthread_self();

	printf("trickle thread starting: tid: %lu\n", (u_long)tid);
	fflush(stdout);

	for (;;) {
		(void)memp_trickle(dbenv->mp_info, 10, &wrote);
		if (verbose) {
			sprintf(buf, "trickle: wrote %d\n", wrote);
			write(STDOUT_FILENO, buf, strlen(buf));
		}
		if (wrote == 0) {
			sleep(1);
			sched_yield();
		}
	}

	/* NOTREACHED */
	return (NULL);
}

/*
 * word --
 *	Build the dictionary word list.
 */
void
word()
{
	FILE *fp;
	int cnt;
	char buf[256];

	if ((fp = fopen(WORDLIST, "r")) == NULL)
		fatal(WORDLIST, 1);

	if ((list = malloc(nlist * sizeof(char *))) == NULL)
		fatal(NULL, 1);

	for (cnt = 0; cnt < nlist; ++cnt) {
		if (fgets(buf, sizeof(buf), fp) == NULL)
			break;
		if ((list[cnt] = strdup(buf)) == NULL)
			fatal(NULL, 1);
	}
	nlist = cnt;		/* In case nlist was larger than possible. */
}

/*
 * fatal --
 *	Report a fatal error and quit.
 */
void
fatal(msg, syserr)
	char *msg;
	int syserr;
{
	fprintf(stderr, "%s: ", progname);
	if (msg != NULL) {
		fprintf(stderr, "%s", msg);
		if (syserr)
			fprintf(stderr, ": ");
	}
	if (syserr)
		fprintf(stderr, "%s", strerror(errno));
	fprintf(stderr, "\n");
	exit (1);

	/* NOTREACHED */
}

/*
 * usage --
 *	Usage message.
 */
void
usage()
{
	(void)fprintf(stderr,
    "usage: %s [-pv] [-h home] [-n words] [-r readers] [-w writers]\n",
	    progname);
	exit(1);
}
