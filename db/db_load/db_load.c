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
static const char sccsid[] = "@(#)db_load.c	10.20 (Sleepycat) 6/2/98";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_am.h"
#include "clib_ext.h"

void	badnum __P((void));
void	configure __P((DB_INFO *, char **));
DB_ENV *db_init __P((char *));
int	dbt_rdump __P((DBT *));
int	dbt_rprint __P((DBT *));
int	digitize __P((int));
int	main __P((int, char *[]));
void	rheader __P((DBTYPE *, int *, DB_INFO *));
void	usage __P((void));

const char
	*progname = "db_load";				/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DB *dbp;
	DBT key, data;
	DBTYPE argtype, dbtype;
	DB_ENV *dbenv;
	DB_INFO dbinfo;
	db_recno_t recno;
	u_int32_t db_nooverwrite;
	int ch, checkprint, existed, no_header;
	char **clist, **clp, *home;

	/* Allocate enough room for configuration arguments. */
	if ((clp = clist = (char **)calloc(argc + 1, sizeof(char *))) == NULL)
		err(1, NULL);

	home = NULL;
	db_nooverwrite = 0;
	existed = checkprint = no_header = 0;
	argtype = dbtype = DB_UNKNOWN;
	while ((ch = getopt(argc, argv, "c:f:h:nTt:")) != EOF)
		switch (ch) {
		case 'c':
			*clp++ = optarg;
			break;
		case 'f':
			if (freopen(optarg, "r", stdin) == NULL)
				err(1, "%s", optarg);
			break;
		case 'h':
			home = optarg;
			break;
		case 'n':
			db_nooverwrite = DB_NOOVERWRITE;
			break;
		case 'T':
			no_header = checkprint = 1;
			break;
		case 't':
			if (strcmp(optarg, "btree") == 0) {
				argtype = DB_BTREE;
				break;
			}
			if (strcmp(optarg, "hash") == 0) {
				argtype = DB_HASH;
				break;
			}
			if (strcmp(optarg, "recno") == 0) {
				argtype = DB_RECNO;
				break;
			}
			usage();
			/* NOTREACHED */
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	/* Initialize the environment if the user specified one. */
	dbenv = home == NULL ? NULL : db_init(home);

	/*
	 * Read the header.  If there isn't any header, we're expecting flat
	 * text, set the checkprint flag appropriately.
	 */
	memset(&dbinfo, 0, sizeof(DB_INFO));
	if (no_header)
		dbtype = argtype;
	else {
		rheader(&dbtype, &checkprint, &dbinfo);
		if (argtype != DB_UNKNOWN) {
			/* Conversion to/from recno is prohibited. */
			if ((dbtype == DB_RECNO && argtype != DB_RECNO) ||
			    (argtype == DB_RECNO && dbtype != DB_RECNO))
				errx(1,
			    "databases of type recno may not be converted");
			dbtype = argtype;
		}
	}
	if (dbtype == DB_UNKNOWN)
		errx(1, "no database type specified");

	/* Apply command-line configuration changes. */
	configure(&dbinfo, clist);

	/* Open the DB file. */
	if ((errno = db_open(argv[0], dbtype, DB_CREATE,
	    __db_omode("rwrwrw"), dbenv, &dbinfo, &dbp)) != 0)
		err(1, "%s", argv[0]);

	/* Initialize the key/data pair. */
	memset(&key, 0, sizeof(DBT));
	if (dbtype == DB_RECNO) {
		key.data = &recno;
		key.size = sizeof(recno);
	} else
		if ((key.data = (void *)malloc(key.ulen = 1024)) == NULL) {
			errno = ENOMEM;
			err(1, NULL);
		}
	memset(&data, 0, sizeof(DBT));
	if ((data.data = (void *)malloc(data.ulen = 1024)) == NULL) {
		errno = ENOMEM;
		err(1, NULL);
	}

	/* Get each key/data pair and add them to the database. */
	for (recno = 1;; ++recno) {
		if (dbtype == DB_RECNO)
			if (checkprint) {
				if (dbt_rprint(&data))
					break;
			} else {
				if (dbt_rdump(&data))
					break;
			}
		else
			if (checkprint) {
				if (dbt_rprint(&key))
					break;
				if (dbt_rprint(&data))
					goto fmt;
			} else {
				if (dbt_rdump(&key))
					break;
				if (dbt_rdump(&data))
fmt:					err(1, "odd number of key/data pairs");
			}
		switch (errno =
		    dbp->put(dbp, NULL, &key, &data, db_nooverwrite)) {
		case 0:
			break;
		case DB_KEYEXIST:
			existed = 1;
			warnx("%s: line %d: key already exists, not loaded:",
			    argv[0],
			    dbtype == DB_RECNO ? recno : recno * 2 - 1);
			(void)__db_prdbt(&key, checkprint, stderr);
			break;
		default:
			err(1, "%s", argv[0]);
			/* NOTREACHED */
		}
	}

	if ((errno = dbp->close(dbp, 0)) != 0)
		err(1, "%s", argv[0]);
	return (existed ? 1 : 0);
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

	/*
	 * The database may be live, try and use the shared regions.
	 *
	 * If it works, we're done.  Set the error output options so that
	 * future errors are correctly reported.
	 */
	if ((errno = db_appinit(home, NULL, dbenv, DB_INIT_LOCK |
	    DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_USE_ENVIRON)) == 0) {
		dbenv->db_errfile = stderr;
		dbenv->db_errpfx = progname;
		return (dbenv);
	}

	/*
	 * If the db_appinit fails, assume the database isn't live, and don't
	 * bother with an environment.
	 */
	free(dbenv);
	return (NULL);
}

#define	FLAG(name, value, keyword, flag)				\
	if (strcmp(name, keyword) == 0) {				\
		switch (*value) {					\
		case '1':						\
			dbinfop->flags |= (flag);			\
			break;						\
		case '0':						\
			dbinfop->flags &= ~(flag);			\
			break;						\
		default:						\
			badnum();					\
			/* NOTREACHED */				\
		}							\
		continue;						\
	}
#define	NUMBER(name, value, keyword, field, flag)			\
	if (strcmp(name, keyword) == 0) {				\
		get_long(value, 1, LONG_MAX, &val);			\
		dbinfop->field = val;					\
		if (flag != 0)						\
			dbinfop->flags |= (flag);			\
		continue;						\
	}
#define	STRING(name, value, keyword, field, flag)			\
	if (strcmp(name, keyword) == 0) {				\
		dbinfop->field = value[0];				\
		if (flag != 0)						\
			dbinfop->flags |= (flag);			\
		continue;						\
	}

/*
 * configure --
 *	Handle command-line configuration options.
 */
void
configure(dbinfop, clp)
	DB_INFO *dbinfop;
	char **clp;
{
	long val;
	char *name, *value;

	for (; (name = *clp) != NULL; ++clp) {
		if ((value = strchr(name, '=')) == NULL)
			errx(1,
		    "command-line configuration uses name=value format");
		*value++ = '\0';

		NUMBER(name, value, "bt_maxkey", bt_maxkey, 0);
		NUMBER(name, value, "bt_minkey", bt_minkey, 0);
		NUMBER(name, value, "db_lorder", db_lorder, 0);
		NUMBER(name, value, "db_pagesize", db_pagesize, 0);
		FLAG(name, value, "duplicates", DB_DUP);
		NUMBER(name, value, "h_ffactor", h_ffactor, 0);
		NUMBER(name, value, "h_nelem", h_nelem, 0);
		NUMBER(name, value, "re_len", re_len, DB_FIXEDLEN);
		STRING(name, value, "re_pad", re_pad, DB_PAD);
		FLAG(name, value, "recnum", DB_RECNUM);
		FLAG(name, value, "renumber", DB_RENUMBER);

		errx(1, "unknown command-line configuration keyword");
	}
}

/*
 * rheader --
 *	Read the header message.
 */
void
rheader(dbtypep, checkprintp, dbinfop)
	DBTYPE *dbtypep;
	int *checkprintp;
	DB_INFO *dbinfop;
{
	long lineno, val;
	char name[256], value[256];

	*dbtypep = DB_UNKNOWN;
	*checkprintp = 0;

	for (lineno = 1;; ++lineno) {
		/* If we don't see the expected information, it's an error. */
		if (fscanf(stdin, "%[^=]=%s\n", name, value) != 2)
			errx(1, "line %lu: unexpected format", lineno);

		/* Check for the end of the header lines. */
		if (strcmp(name, "HEADER") == 0)
			break;

		if (strcmp(name, "format") == 0) {
			if (strcmp(value, "bytevalue") == 0) {
				*checkprintp = 0;
				continue;
			}
			if (strcmp(value, "print") == 0) {
				*checkprintp = 1;
				continue;
			}
			errx(1, "line %d: unknown format", lineno);
		}
		if (strcmp(name, "type") == 0) {
			if (strcmp(value, "btree") == 0) {
				*dbtypep = DB_BTREE;
				continue;
			}
			if (strcmp(value, "hash") == 0) {
				*dbtypep = DB_HASH;
				continue;
			}
			if (strcmp(value, "recno") == 0) {
				*dbtypep = DB_RECNO;
				continue;
			}
			errx(1, "line %d: unknown type", lineno);
		}
		NUMBER(name, value, "bt_maxkey", bt_maxkey, 0);
		NUMBER(name, value, "bt_minkey", bt_minkey, 0);
		NUMBER(name, value, "db_lorder", db_lorder, 0);
		NUMBER(name, value, "db_pagesize", db_pagesize, 0);
		FLAG(name, value, "duplicates", DB_DUP);
		NUMBER(name, value, "h_ffactor", h_ffactor, 0);
		NUMBER(name, value, "h_nelem", h_nelem, 0);
		NUMBER(name, value, "re_len", re_len, DB_FIXEDLEN);
		STRING(name, value, "re_pad", re_pad, DB_PAD);
		FLAG(name, value, "recnum", DB_RECNUM);
		FLAG(name, value, "renumber", DB_RENUMBER);

		errx(1, "unknown input-file header configuration keyword");
	}
}

/*
 * dbt_rprint --
 *	Read a printable line into a DBT structure.
 */
int
dbt_rprint(dbtp)
	DBT *dbtp;
{
	u_int32_t len;
	u_int8_t *p;
	int c1, c2, escape;

	escape = 0;
	for (p = dbtp->data, len = 0; (c1 = getchar()) != '\n';) {
		if (c1 == EOF) {
			if (len == 0)
				return (1);
			err(1, "unexpected end of key/data pair");
		}
		if (escape) {
			if (c1 != '\\') {
				if ((c2 = getchar()) == EOF)
					err(1,
					    "unexpected end of key/data pair");
				c1 = digitize(c1) << 4 | digitize(c2);
			}
			escape = 0;
		} else
			if (c1 == '\\') {
				escape = 1;
				continue;
			}
		if (len >= dbtp->ulen - 10) {
			dbtp->ulen *= 2;
			if ((dbtp->data =
			    (void *)realloc(dbtp->data, dbtp->ulen)) == NULL) {
				errno = ENOMEM;
				err(1, NULL);
			}
			p = (u_int8_t *)dbtp->data + len;
		}
		++len;
		*p++ = c1;
	}
	dbtp->size = len;
	return (0);
}

/*
 * dbt_rdump --
 *	Read a byte dump line into a DBT structure.
 */
int
dbt_rdump(dbtp)
	DBT *dbtp;
{
	u_int32_t len;
	u_int8_t *p;
	int c1, c2;

	for (p = dbtp->data, len = 0; (c1 = getchar()) != '\n';) {
		if (c1 == EOF) {
			if (len == 0)
				return (1);
			err(1, "unexpected end of key/data pair");
		}
		if ((c2 = getchar()) == EOF)
			err(1, "unexpected end of key/data pair");
		if (len >= dbtp->ulen - 10) {
			dbtp->ulen *= 2;
			if ((dbtp->data =
			    (void *)realloc(dbtp->data, dbtp->ulen)) == NULL) {
				errno = ENOMEM;
				err(1, NULL);
			}
			p = (u_int8_t *)dbtp->data + len;
		}
		++len;
		*p++ = digitize(c1) << 4 | digitize(c2);
	}
	dbtp->size = len;
	return (0);
}

/*
 * digitize --
 *	Convert a character to an integer.
 */
int
digitize(c)
	int c;
{
	switch (c) {			/* Don't depend on ASCII ordering. */
	case '0': return (0);
	case '1': return (1);
	case '2': return (2);
	case '3': return (3);
	case '4': return (4);
	case '5': return (5);
	case '6': return (6);
	case '7': return (7);
	case '8': return (8);
	case '9': return (9);
	case 'a': return (10);
	case 'b': return (11);
	case 'c': return (12);
	case 'd': return (13);
	case 'e': return (14);
	case 'f': return (15);
	}

	err(1, "unexpected hexadecimal value");
	/* NOTREACHED */

	return (0);
}

/*
 * badnum --
 *	Display the bad number message.
 */
void
badnum()
{
	err(1, "boolean name=value pairs require a value of 0 or 1");
}

/*
 * usage --
 *	Display the usage message.
 */
void
usage()
{
	(void)fprintf(stderr, "%s\n\t%s\n",
	    "usage: db_load [-nT]",
    "[-c name=value] [-f file] [-h home] [-t btree | hash | recno] db_file");
	exit(1);
}
