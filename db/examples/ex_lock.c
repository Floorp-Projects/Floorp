/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)ex_lock.c	10.14 (Sleepycat) 4/10/98
 */

#include "config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include <db.h>

#ifdef _WIN32
#define	LOCK_HOME	"lock"
#else
#define	LOCK_HOME	"/var/tmp/lock"
#endif

DB_ENV *db_init(char *, u_int32_t, int);
int	main __P((int, char *[]));
void	usage(void);

const char
	*progname = "ex_lock";				/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int errno, optind;
	DBT lock_dbt;
	DB_ENV *dbenv;
	DB_LOCK lock;
	db_lockmode_t lock_type;
	long held;
	u_int32_t len, locker, maxlocks;
	int ch, do_unlink, did_get, i, ret;
	char *home, opbuf[16], objbuf[1024], lockbuf[16];

	home = LOCK_HOME;
	maxlocks = 0;
	do_unlink = 0;
	while ((ch = getopt(argc, argv, "h:m:u")) != EOF)
		switch (ch) {
		case 'h':
			home = optarg;
			break;
		case 'm':
			if ((i = atoi(optarg)) <= 0)
				usage();
			maxlocks = (u_int32_t)i;  /* XXX: possible overflow. */
			break;
		case 'u':
			do_unlink = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	/* Initialize the database environment. */
	dbenv = db_init(home, maxlocks, do_unlink);

	/*
	 * Accept lock requests.
	 */
	ret = lock_id(dbenv->lk_info, &locker);
	if (ret != 0) {
		fprintf(stderr, "Unable to get locker id, %s\n",
		    strerror(ret));
		(void)db_appexit(dbenv);
		exit (1);
	}
	for (held = 0;;) {
		printf("Operation get/release [get]> ");
		fflush(stdout);
		if (fgets(opbuf, sizeof(opbuf), stdin) == NULL)
			break;
		if ((len = strlen(opbuf)) <= 1 || strcmp(opbuf, "get\n") == 0) {
			/* Acquire a lock. */
			printf("input object (text string) to lock> ");
			fflush(stdout);
			if (fgets(objbuf, sizeof(objbuf), stdin) == NULL)
				break;
			if ((len = strlen(objbuf)) <= 1)
				continue;

			do {
				printf("lock type read/write [read]> ");
				fflush(stdout);
				if (fgets(lockbuf,
				    sizeof(lockbuf), stdin) == NULL)
					break;
				len = strlen(lockbuf);
			} while (len >= 1 &&
			    strcmp(lockbuf, "read\n") != 0 &&
			    strcmp(lockbuf, "write\n") != 0);
			if (len == 1 || strcmp(lockbuf, "read\n") == 0)
				lock_type = DB_LOCK_READ;
			else
				lock_type = DB_LOCK_WRITE;

			lock_dbt.data = objbuf;
			lock_dbt.size = strlen(objbuf);
			ret = lock_get(dbenv->lk_info, locker,
			    DB_LOCK_NOWAIT, &lock_dbt, lock_type, &lock);
			did_get = 1;
		} else {
			/* Release a lock. */
			do {
				printf("input lock to release> ");
				fflush(stdout);
				if (fgets(objbuf,
				    sizeof(objbuf), stdin) == NULL)
					break;
			} while ((len = strlen(objbuf)) <= 1);
			lock = (DB_LOCK)strtoul(objbuf, NULL, 16);
			ret = lock_put(dbenv->lk_info, lock);
			did_get = 0;
		}
		switch (ret) {
			case 0:
				printf("Lock 0x%lx %s\n", (unsigned long)lock,
				    did_get ? "granted" : "released");
				held += did_get ? 1 : -1;
				break;
			case DB_LOCK_NOTHELD:
				printf("You do not hold the lock %lu\n",
				    (unsigned long)lock);
				break;
			case DB_LOCK_NOTGRANTED:
				printf("Lock not granted\n");
				break;
			case DB_LOCK_DEADLOCK:
				fprintf(stderr, "%s: lock_%s: %s",
				    progname, did_get ? "get" : "put",
				    "returned DEADLOCK");
				break;
			default:
				fprintf(stderr, "%s: lock_get: %s",
				    progname, strerror(errno));
		}
	}
	printf("\n");
	printf("Closing lock region %ld locks held\n", held);

	return (db_appexit(dbenv));
}

/*
 * db_init --
 *	Initialize the environment.
 */
DB_ENV *
db_init(home, maxlocks, do_unlink)
	char *home;
	u_int32_t maxlocks;
	int do_unlink;
{
	DB_ENV *dbenv;

	/* Rely on calloc to initialize the structure. */
	if ((dbenv = (DB_ENV *)calloc(sizeof(DB_ENV), 1)) == NULL) {
		fprintf(stderr, "%s: %s", progname, strerror(ENOMEM));
		exit (1);
	}
	dbenv->db_errfile = stderr;
	dbenv->db_errpfx = progname;
	dbenv->lk_max = maxlocks;

	/* Open for naming only if we need to remove the old lock region. */
	if (do_unlink) {
		if ((errno = db_appinit(home, NULL, dbenv, 0)) != 0)
			fprintf(stderr,
			    "%s: db_appinit: %s", progname, strerror(errno));
		if (lock_unlink(NULL, 1, dbenv))
			fprintf(stderr,
			    "%s: lock_unlink: %s", progname, strerror(errno));
		if ((errno = db_appexit(dbenv)) != 0)
			fprintf(stderr,
			    "%s: db_appexit: %s", progname, strerror(errno));

		memset(dbenv, 0, sizeof(DB_ENV));
	}

	if ((errno =
	    db_appinit(home, NULL, dbenv, DB_CREATE | DB_INIT_LOCK)) != 0) {
		fprintf(stderr,
		    "%s: db_appinit: %s", progname, strerror(errno));
		exit (1);
	}
	return (dbenv);
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: %s [-u] [-h home] [-m maxlocks]\n", progname);
	exit(1);
}
