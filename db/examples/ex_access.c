/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)ex_access.c	10.14 (Sleepycat) 4/10/98
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

#define	DATABASE	"access.db"

DB_ENV *db_init(char *);
int	main __P((int, char *[]));
void	usage(void);

const char
	*progname = "ex_access";			/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int errno, optind;
	DB *dbp;
	DBC *dbcp;
	DBT key, data;
	DB_ENV *dbenv;
	DB_INFO dbinfo;
	u_int32_t len;
	int ch;
	char *home, *p, *t, buf[1024], rbuf[1024];

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

	/* Remove the previous database. */
	(void)unlink(DATABASE);

	/* Initialize the database environment. */
	dbenv = db_init(home);

	/* Initialize the database. */
	memset(&dbinfo, 0, sizeof(dbinfo));
	dbinfo.db_pagesize = 1024;		/* Page size: 1K. */

	/* Create the database. */
	if ((errno = db_open(DATABASE,
	    DB_BTREE, DB_CREATE, 0664, dbenv, &dbinfo, &dbp)) != 0) {
		fprintf(stderr,
		    "%s: %s: %s\n", progname, DATABASE, strerror(errno));
		exit (1);
	}

	/*
	 * Insert records into the database, where the key is the user
	 * input and the data is the user input in reverse order.
	 */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	for (;;) {
		printf("input> ");
		fflush(stdout);
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		if ((len = strlen(buf)) <= 1)
			continue;
		for (t = rbuf, p = buf + (len - 2); p >= buf;)
			*t++ = *p--;
		*t++ = '\0';

		key.data = buf;
		data.data = rbuf;
		data.size = key.size = len - 1;

		switch (errno =
		    dbp->put(dbp, NULL, &key, &data, DB_NOOVERWRITE)) {
		case 0:
			break;
		default:
			fprintf(stderr,
			    "%s: put: %s\n", progname, strerror(errno));
			exit (1);
		case DB_KEYEXIST:
			printf("%s: key already exists\n", buf);
			break;
		}
	}
	printf("\n");

	/* Acquire a cursor for the database. */
	if ((errno = dbp->cursor(dbp, NULL, &dbcp)) != 0) {
		fprintf(stderr, "%s: cursor: %s\n", progname, strerror(errno));
		exit (1);
	}

	/* Initialize the key/data pair so the flags aren't set. */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/* Walk through the database and print out the key/data pairs. */
	while ((errno = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0)
		printf("%.*s : %.*s\n",
		    (int)key.size, (char *)key.data,
		    (int)data.size, (char *)data.data);
	if (errno != DB_NOTFOUND)
		fprintf(stderr, "%s: get: %s\n", progname, strerror(errno));

	(void)dbcp->c_close(dbcp);
	(void)dbp->close(dbp, 0);

	return (db_appexit(dbenv));
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

	/* Rely on calloc to initialize the structure. */
	if ((dbenv = (DB_ENV *)calloc(sizeof(DB_ENV), 1)) == NULL) {
		fprintf(stderr, "%s: %s\n", progname, strerror(ENOMEM));
		exit (1);
	}
	dbenv->db_errfile = stderr;
	dbenv->db_errpfx = progname;
	dbenv->mp_size = 32 * 1024;		/* Cachesize: 32K. */

	if ((errno = db_appinit(home, NULL, dbenv, DB_CREATE)) != 0) {
		fprintf(stderr,
		    "%s: db_appinit: %s\n", progname, strerror(errno));
		exit (1);
	}
	return (dbenv);
}

void
usage()
{
	(void)fprintf(stderr, "usage: %s [-h home]\n", progname);
	exit(1);
}
