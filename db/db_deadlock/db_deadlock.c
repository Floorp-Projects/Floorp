/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1996, 1997, 1998\n\
	Sleepycat Software Inc.  All rights reserved.\n";
static const char sccsid[] = "@(#)db_deadlock.c	10.19 (Sleepycat) 4/10/98";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "clib_ext.h"
#include "common_ext.h"

#define	BAD_KILLID	0xffffffff

DB_ENV	*db_init __P((char *, int));
int	 logpid __P((char *, int));
int	 main __P((int, char *[]));
void	 onint __P((int));
void	 siginit __P((void));
void	 usage __P((void));

int	 interrupted;
const char
	*progname = "db_deadlock";			/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DB_ENV *dbenv;
	u_int32_t atype;
	time_t now;
	long usecs;
	u_int32_t flags;
	int ch, verbose;
	char *home, *logfile;

	atype = DB_LOCK_DEFAULT;
	home = logfile = NULL;
	usecs = 0;
	flags = 0;
	verbose = 0;
	while ((ch = getopt(argc, argv, "a:h:L:t:vw")) != EOF)
		switch (ch) {
		case 'a':
			switch (optarg[0]) {
			case 'o':
				atype = DB_LOCK_OLDEST;
				break;
			case 'y':
				atype = DB_LOCK_YOUNGEST;
				break;
			default:
				usage();
				/* NOTREACHED */
			}
			if (optarg[1] != '\0')
				usage();
			break;
		case 'h':
			home = optarg;
			break;
		case 'L':
			logfile = optarg;
			break;
		case 't':
			get_long(optarg, 1, LONG_MAX, &usecs);
			usecs *= 1000000;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			LF_SET(DB_LOCK_CONFLICT);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if (usecs == 0 && !LF_ISSET(DB_LOCK_CONFLICT)) {
		warnx("at least one of -t and -w must be specified");
		usage();
	}

	/*
	 * We detect every 100ms (100000 us) when we're running in
	 * DB_LOCK_CONFLICT mode.
	 */
	if (usecs == 0)
		usecs = 100000;

	/* Initialize the deadlock detector by opening the lock manager. */
	dbenv = db_init(home, verbose);

	if (logfile != NULL && logpid(logfile, 1)) {
		(void)db_appexit(dbenv);
		return (1);
	}

	while (!interrupted) {
		if (dbenv->db_verbose != 0) {
			time(&now);
			__db_err(dbenv, "Running at %.24s", ctime(&now));
		}

		if ((errno = lock_detect(dbenv->lk_info, flags, atype)) != 0)
			break;

		/* Make a pass every "usecs" usecs. */
		(void)__db_sleep(0, usecs);
	}

	if (logfile != NULL)
		(void)logpid(logfile, 0);

	if (interrupted) {
		(void)signal(interrupted, SIG_DFL);
		(void)raise(interrupted);
		/* NOTREACHED */
	}

	return (db_appexit(dbenv));
}

DB_ENV *
db_init(home, verbose)
	char *home;
	int verbose;
{
	DB_ENV *dbenv;

	if ((dbenv = (DB_ENV *)calloc(sizeof(DB_ENV), 1)) == NULL) {
		errno = ENOMEM;
		err(1, NULL);
	}
	dbenv->db_errfile = stderr;
	dbenv->db_errpfx = progname;
	dbenv->db_verbose = verbose;

	if ((errno = db_appinit(home,
	    NULL, dbenv, DB_INIT_LOCK | DB_USE_ENVIRON)) != 0)
		err(1, "db_appinit");

	siginit();

	return (dbenv);
}

/*
 * logpid --
 *	Log that we're running.
 */
int
logpid(fname, is_open)
	char *fname;
	int is_open;
{
	FILE *fp;
	time_t now;

	if (is_open) {
		if ((fp = fopen(fname, "w")) == NULL) {
			warn("%s", fname);
			return (1);
		}
		(void)time(&now);
		fprintf(fp,
		    "%.24s: %lu %s", progname, (u_long)getpid(), ctime(&now));
		fclose(fp);
	} else
		(void)remove(fname);
	return (0);
}

/*
 * siginit --
 *	Initialize the set of signals for which we want to clean up.
 *	Generally, we try not to leave the shared regions locked if
 *	we can.
 */
void
siginit()
{
#ifdef SIGHUP
	(void)signal(SIGHUP, onint);
#endif
	(void)signal(SIGINT, onint);
#ifdef SIGKILL
	(void)signal(SIGKILL, onint);
#endif
	(void)signal(SIGTERM, onint);
}

/*
 * oninit --
 *	Interrupt signal handler.
 */
void
onint(signo)
	int signo;
{
	if ((interrupted = signo) == 0)
		interrupted = SIGINT;
}

void
usage()
{
	(void)fprintf(stderr,
    "usage: db_deadlock [-vw] [-a m | o | y] [-h home] [-L file] [-t sec]\n");
	exit(1);
}
