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
static const char sccsid[] = "@(#)db_checkpoint.c	10.17 (Sleepycat) 5/3/98";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_page.h"
#include "log.h"
#include "btree.h"
#include "hash.h"
#include "clib_ext.h"
#include "common_ext.h"

char	*check __P((DB_ENV *, long, long));
DB_ENV	*db_init __P((char *));
int	 logpid __P((char *, int));
int	 main __P((int, char *[]));
void	 onint __P((int));
void	 siginit __P((void));
void	 usage __P((void));

int	 interrupted;
const char
	*progname = "db_checkpoint";		/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DB_ENV *dbenv;
	time_t now;
	long argval;
	u_int32_t kbytes, minutes, seconds;
	int ch, eval, once, verbose;
	char *home, *logfile;

	/*
	 * XXX
	 * Don't allow a fully unsigned 32-bit number, some compilers get
	 * upset and require it to be specified in hexadecimal and so on.
	 */
#define	MAX_UINT32_T	2147483647

	kbytes = minutes = 0;
	once = verbose = 0;
	home = logfile = NULL;
	while ((ch = getopt(argc, argv, "1h:k:L:p:v")) != EOF)
		switch (ch) {
		case '1':
			once = 1;
			break;
		case 'h':
			home = optarg;
			break;
		case 'k':
			get_long(optarg, 1, (long)MAX_UINT32_T, &argval);
			kbytes = argval;
			break;
		case 'L':
			logfile = optarg;
			break;
		case 'p':
			get_long(optarg, 1, (long)MAX_UINT32_T, &argval);
			minutes = argval;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if (once == 0 && kbytes == 0 && minutes == 0) {
		warnx("at least one of -1, -k and -p must be specified");
		usage();
	}

	/* Initialize the environment. */
	dbenv = db_init(home);

	if (logfile != NULL && logpid(logfile, 1)) {
		(void)db_appexit(dbenv);
		return (1);
	}

	/*
	 * If we have only a time delay, then we'll sleep the right amount
	 * to wake up when a checkpoint is necessary.  If we have a "kbytes"
	 * field set, then we'll check every 30 seconds.
	 */
	eval = 0;
	seconds = kbytes != 0 ? 30 : minutes * 60;
	while (!interrupted) {
		if (verbose) {
			(void)time(&now);
			printf("checkpoint: %s", ctime(&now));
		}
		errno = txn_checkpoint(dbenv->tx_info, kbytes, minutes);

		while (errno == DB_INCOMPLETE) {
			if (verbose)
				__db_err(dbenv,
				    "checkpoint did not finish, retrying");
			(void)__db_sleep(2, 0);
			errno = txn_checkpoint(dbenv->tx_info, 0, 0);
		}

		if (errno != 0) {
			eval = 1;
			__db_err(dbenv, "checkpoint: %s", strerror(errno));
			break;
		}

		if (once)
			break;

		(void)__db_sleep(seconds, 0);
	}

	if (logfile != NULL && logpid(logfile, 0))
		eval = 1;

	if (interrupted) {
		(void)signal(interrupted, SIG_DFL);
		(void)raise(interrupted);
		/* NOTREACHED */
	}

	return (db_appexit(dbenv) || eval ? 1 : 0);
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

	if ((dbenv = (DB_ENV *)calloc(sizeof(DB_ENV), 1)) == NULL) {
		errno = ENOMEM;
		err(1, NULL);
	}
	dbenv->db_errfile = stderr;
	dbenv->db_errpfx = progname;

	if ((errno = db_appinit(home, NULL, dbenv,
	   DB_INIT_LOG | DB_INIT_TXN | DB_INIT_MPOOL | DB_USE_ENVIRON)) != 0)
		err(1, "db_appinit");

	if (memp_register(dbenv->mp_info,
	    DB_FTYPE_BTREE, __bam_pgin, __bam_pgout) ||
	    memp_register(dbenv->mp_info,
	    DB_FTYPE_HASH, __ham_pgin, __ham_pgout)) {
		(void)db_appexit(dbenv);
		errx(1,
		    "db_appinit: failed to register access method functions");
	}

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
    "usage: db_checkpoint [-1v] [-h home] [-k kbytes] [-L file] [-p min]\n");
	exit(1);
}
