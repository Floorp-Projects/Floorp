/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)ex_mpool.c	10.19 (Sleepycat) 5/1/98
 */

#include "config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#endif

#include <db.h>

#define	MPOOL	"mpool"

void	init(char *, int, int);
int	main __P((int, char *[]));
void	run(DB_ENV *, int, int, int);
void	usage(void);

const char
	*progname = "ex_mpool";				/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DB_ENV dbenv;
	int cachesize, ch, hits, npages, pagesize;

	cachesize = 20 * 1024;
	hits = 1000;
	npages = 50;
	pagesize = 1024;
	while ((ch = getopt(argc, argv, "c:h:n:p:")) != EOF)
		switch (ch) {
		case 'c':
			if ((cachesize = atoi(optarg)) < 20 * 1024)
				usage();
			break;
		case 'h':
			if ((hits = atoi(optarg)) <= 0)
				usage();
		case 'n':
			if ((npages = atoi(optarg)) <= 0)
				usage();
			break;
		case 'p':
			if ((pagesize = atoi(optarg)) <= 0)
				usage();
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* Initialize the file. */
	init(MPOOL, pagesize, npages);

	/*
	 * Initialize the environment.
	 *
	 * Output error messages to stderr.
	 * Specify the cachesize.
	 * The memory pool is private.
	 */
	memset(&dbenv, 0, sizeof(dbenv));
	dbenv.db_errfile = stderr;
	dbenv.db_errpfx = progname;
	dbenv.mp_size = (size_t)cachesize;	/* XXX: Possible overflow. */

	/* Get the pages. */
	run(&dbenv, hits, pagesize, npages);

	return (0);
}

/*
 * init --
 *	Create a backing file.
 */
void
init(file, pagesize, npages)
	char *file;
	int pagesize, npages;
{
	int cnt, flags, fd;
	char *p;

	/*
	 * Create a file with the right number of pages, and store a page
	 * number on each page.
	 */
        flags = O_CREAT | O_RDWR | O_TRUNC;
#ifdef WIN32
        flags |= O_BINARY;
#endif
	if ((fd = open(file, flags, 0666)) < 0) {
		fprintf(stderr,
		    "%s: %s: %s\n", progname, file, strerror(errno));
		exit (1);
	}
	if ((p = (char *)malloc(pagesize)) == NULL) {
		fprintf(stderr, "%s: %s\n", progname, strerror(ENOMEM));
		exit (1);
	}

	/* The pages are numbered from 0. */
	for (cnt = 0; cnt <= npages; ++cnt) {
		*(int *)p = cnt;
		if (write(fd, p, pagesize) != pagesize) {
			fprintf(stderr,
			    "%s: %s: %s\n", progname, file, strerror(errno));
			exit (1);
		}
	}
	free(p);
}

/*
 * run --
 *	Get a set of pages.
 */
void
run(dbenv, hits, pagesize, npages)
	DB_ENV *dbenv;
	int hits, pagesize, npages;
{
	DB_MPOOL *dbmp;
	DB_MPOOLFILE *dbmfp;
	db_pgno_t pageno;
	int cnt;
	void *p;

	/* Open a memory pool. */
	if ((errno = memp_open(NULL,
	    DB_CREATE | DB_MPOOL_PRIVATE, 0666, dbenv, &dbmp)) != 0) {
		fprintf(stderr, "%s: %s\n", progname, strerror(errno));
		exit (1);
	}
	/* Open the file in the pool. */
	if ((errno =
	    memp_fopen(dbmp, MPOOL, 0, 0, pagesize, NULL, &dbmfp)) != 0) {
		fprintf(stderr,
		    "%s: %s: %s\n", progname, MPOOL, strerror(errno));
		exit (1);
	}

	srand((u_int)time(NULL));
	for (cnt = 0; cnt < hits; ++cnt) {
		pageno = (rand() % npages) + 1;
		if ((errno = memp_fget(dbmfp, &pageno, 0, &p)) != 0) {
			fprintf(stderr,
			    "%s: unable to retrieve page %lu: %s\n",
			    progname, (u_long)pageno, strerror(errno));
			exit (1);
		}
		if (*(db_pgno_t *)p != pageno) {
			fprintf(stderr,
			    "%s: wrong page retrieved (%lu != %d)\n",
			    progname, (u_long)pageno, *(int *)p);
			exit (1);
		}
		if ((errno = memp_fput(dbmfp, p, 0)) != 0) {
			fprintf(stderr,
			    "%s: unable to return page %lu: %s\n",
			    progname, (u_long)pageno, strerror(errno));
			exit (1);
		}
	}

	/* Close the pool. */
	if ((errno = memp_close(dbmp)) != 0) {
		fprintf(stderr, "%s: %s\n", progname, strerror(errno));
		exit (1);
	}
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: %s [-c cachesize] [-h hits] [-n npages] [-p pagesize]\n",
	    progname);
	exit(1);
}
