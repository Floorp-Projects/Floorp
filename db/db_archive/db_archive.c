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
static const char sccsid[] = "@(#)db_archive.c	10.17 (Sleepycat) 4/10/98";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "log.h"
#include "db_dispatch.h"
#include "clib_ext.h"
#include "common_ext.h"

DB_ENV	*db_init __P((char *, int));
void	 onint __P((int));
int	 main __P((int, char *[]));
void	 siginit __P((void));
void	 usage __P((void));

int	 interrupted;
const char
	*progname = "db_archive";			/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DB_ENV *dbenv;
	u_int32_t flags;
	int ch, verbose;
	char *home, **list;

	flags = verbose = 0;
	home = NULL;
	while ((ch = getopt(argc, argv, "ah:lsv")) != EOF)
		switch (ch) {
		case 'a':
			flags |= DB_ARCH_ABS;
			break;
		case 'h':
			home = optarg;
			break;
		case 'l':
			flags |= DB_ARCH_LOG;
			break;
		case 's':
			flags |= DB_ARCH_DATA;
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

	/* Initialize the environment. */
	dbenv = db_init(home, verbose);

	/* Get the list of names. */
	if ((errno = log_archive(dbenv->lg_info, &list, flags, NULL)) != 0) {
		(void)db_appexit(dbenv);
		err(1, "log_archive");
	}

	/* Print the names. */
	if (list != NULL)
		for (; *list != NULL; ++list)
			printf("%s\n", *list);

	return (db_appexit(dbenv) ? 1 : 0);
}

/*
 * db_init --
 *	Initialize the environment.
 */
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

	if ((errno = db_appinit(home, NULL, dbenv,
	    DB_CREATE | DB_INIT_LOG | DB_INIT_TXN | DB_USE_ENVIRON)) != 0)
		err(1, "db_appinit");

	siginit();

	return (dbenv);
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
	(void)fprintf(stderr, "usage: db_archive [-alsv] [-h home]\n");
	exit(1);
}
