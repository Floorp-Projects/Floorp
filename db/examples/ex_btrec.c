/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)ex_btrec.c	10.8 (Sleepycat) 4/11/98
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
#define	WORDLIST	"../test/wordlist"

DB_ENV *db_init __P((char *));
int	main __P((int, char *[]));
void	show __P((char *, DBT *, DBT *));
void	usage __P((void));

const char *progname = "ex_btrec";			/* Program name. */

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
	FILE *fp;
	db_recno_t recno;
	u_int32_t len;
	int ch, cnt;
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

	/* Open the word database. */
	if ((fp = fopen(WORDLIST, "r")) == NULL) {
		fprintf(stderr,
		    "%s: %s: %s\n", progname, WORDLIST, strerror(errno));
		exit (1);
	}

	/* Remove the previous database. */
	(void)unlink(DATABASE);

	/* Initialize the database environment. */
	dbenv = db_init(home);

	/* Initialize the database. */
	memset(&dbinfo, 0, sizeof(dbinfo));
	dbinfo.db_cachesize = 32 * 1024;	/* Cachesize: 32K. */
	dbinfo.db_pagesize = 1024;		/* Page size: 1K. */
	dbinfo.flags |= DB_RECNUM;		/* Record numbers! */

	/* Create the database. */
	if ((errno = db_open(DATABASE,
	    DB_BTREE, DB_CREATE, 0664, dbenv, &dbinfo, &dbp)) != 0) {
		fprintf(stderr,
		    "%s: %s: %s\n", progname, DATABASE, strerror(errno));
		exit (1);
	}

	/*
	 * Insert records into the database, where the key is the word
	 * preceded by its record number, and the data is the same, but
	 * in reverse order.
	 */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	for (cnt = 1; cnt <= 1000; ++cnt) {
		(void)sprintf(buf, "%04d_", cnt);
		if (fgets(buf + 4, sizeof(buf) - 4, fp) == NULL)
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

	/* Close the word database. */
	(void)fclose(fp);

	/* Acquire a cursor for the database. */
	if ((errno = dbp->cursor(dbp, NULL, &dbcp)) != 0) {
		fprintf(stderr, "%s: cursor: %s\n", progname, strerror(errno));
		exit (1);
	}

	/*
	 * Prompt the user for a record number, then retrieve and display
	 * that record.
	 */
	for (;;) {
		/* Get a record number. */
		printf("recno #> ");
		fflush(stdout);
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		recno = atoi(buf);

		/*
		 * Reset the key each time, the dbp->c_get() routine returns
		 * the key and data pair, not just the key!
		 */
		key.data = &recno;
		key.size = sizeof(recno);
		if ((errno = dbcp->c_get(dbcp, &key, &data, DB_SET_RECNO)) != 0)
			goto err;

		/* Display the key and data. */
		show("k/d\t", &key, &data);

		/* Move the cursor a record forward. */
		if ((errno = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) != 0)
			goto err;

		/* Display the key and data. */
		show("next\t", &key, &data);

		/*
		 * Retrieve the record number for the following record into
		 * local memory.
		 */
		data.data = &recno;
		data.size = sizeof(recno);
		data.ulen = sizeof(recno);
		data.flags |= DB_DBT_USERMEM;
		if ((errno = dbcp->c_get(dbcp, &key, &data, DB_GET_RECNO)) != 0)
err:			switch (errno) {
			case DB_NOTFOUND:
				fprintf(stderr,
				    "%s: get: record not found\n", progname);
				continue;
			case DB_KEYEMPTY:
				fprintf(stderr,
				    "%s: get: key empty/deleted\n", progname);
				continue;
			default:
				fprintf(stderr,
				    "%s: get: %s\n", progname, strerror(errno));
				exit (1);
			}
		printf("retrieved recno: %lu\n", (u_long)recno);

		/* Reset the data DBT. */
		memset(&data, 0, sizeof(data));
	}

	(void)dbcp->c_close(dbcp);
	(void)dbp->close(dbp, 0);

	return (db_appexit(dbenv));
}

/*
 * show --
 *	Display a key/data pair.
 */
void
show(msg, key, data)
	DBT *key, *data;
	char *msg;
{
	printf("%s%.*s : %.*s\n", msg,
	    (int)key->size, (char *)key->data,
	    (int)data->size, (char *)data->data);
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
