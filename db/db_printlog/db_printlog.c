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
static const char sccsid[] = "@(#)db_printlog.c	10.12 (Sleepycat) 4/10/98";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_page.h"
#include "btree.h"
#include "hash.h"
#include "log.h"
#include "txn.h"
#include "db_am.h"
#include "clib_ext.h"

DB_ENV *db_init __P((char *));
int	main __P((int, char *[]));
void	onint __P((int));
void	usage __P((void));

int	 interrupted;
const char
	*progname = "db_printlog";			/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DB_ENV *dbenv;
	DBT data;
	DB_LSN key;
	int ch, eval;
	char *home;

	home = NULL;
	while ((ch = getopt(argc, argv, "h:")) != EOF)
		switch (ch) {
		case 'h':
			home = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if ((home != NULL && argc > 0) || argc > 1)
		usage();

	/* XXX: backward compatibility, first argument is home. */
	if (argc == 1)
		home = argv[0];

	dbenv = db_init(home);

	eval = 0;
	if ((errno = __bam_init_print(dbenv)) != 0 ||
	    (errno = __db_init_print(dbenv)) != 0 ||
	    (errno = __ham_init_print(dbenv)) != 0 ||
	    (errno = __log_init_print(dbenv)) != 0 ||
	    (errno = __txn_init_print(dbenv)) != 0) {
		warn("initialization");
		eval = 1;
		(void)db_appexit(dbenv);
	}

	(void)signal(SIGINT, onint);

	memset(&data, 0, sizeof(data));
	while (!interrupted) {
		if ((errno =
		    log_get(dbenv->lg_info, &key, &data, DB_NEXT)) != 0) {
			if (errno == DB_NOTFOUND)
				break;
			eval = 1;
			warn("log_get");
			break;
		}
		if ((errno =
		    __db_dispatch(dbenv->lg_info, &data, &key, 0, NULL)) != 0) {
			eval = 1;
			warn("dispatch");
			break;
		}
	}

	(void)db_appexit(dbenv);

	if (interrupted) {
		(void)signal(SIGINT, SIG_DFL);
		(void)raise(SIGINT);
		/* NOTREACHED */
	}
	return (eval);
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

	if ((errno =
	    db_appinit(home, NULL, dbenv, DB_CREATE | DB_INIT_LOG)) != 0)
		err(1, "db_appinit");
	return (dbenv);
}

/*
 * oninit --
 *	Interrupt signal handler.
 */
void
onint(signo)
	int signo;
{
	COMPQUIET(signo, 0);

	interrupted = 1;
}

void
usage()
{
	fprintf(stderr, "usage: db_printlog [-h home]\n");
	exit (1);
}
