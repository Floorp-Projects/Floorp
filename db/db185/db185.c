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
static const char sccsid[] = "@(#)db185.c	8.17 (Sleepycat) 5/7/98";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "db185_int.h"
#include "common_ext.h"

static int db185_close __P((DB185 *));
static int db185_del __P((const DB185 *, const DBT185 *, u_int));
static int db185_fd __P((const DB185 *));
static int db185_get __P((const DB185 *, const DBT185 *, DBT185 *, u_int));
static int db185_put __P((const DB185 *, DBT185 *, const DBT185 *, u_int));
static int db185_seq __P((const DB185 *, DBT185 *, DBT185 *, u_int));
static int db185_sync __P((const DB185 *, u_int));

DB185 *
dbopen(file, oflags, mode, type, openinfo)
	const char *file;
	int oflags, mode;
	DBTYPE type;
	const void *openinfo;
{
	const BTREEINFO *bi;
	const HASHINFO *hi;
	const RECNOINFO *ri;
	DB *dbp;
	DB185 *db185p;
	DB_INFO dbinfo, *dbinfop;
	int s_errno;

	if ((db185p = (DB185 *)__db_calloc(1, sizeof(DB185))) == NULL)
		return (NULL);
	dbinfop = NULL;
	memset(&dbinfo, 0, sizeof(dbinfo));

	/*
	 * !!!
	 * The DBTYPE enum wasn't initialized in DB 185, so it's off-by-one
	 * from DB 2.0.
	 */
	switch (type) {
	case 0:					/* DB_BTREE */
		type = DB_BTREE;
		if ((bi = openinfo) != NULL) {
			dbinfop = &dbinfo;
			if (bi->flags & ~R_DUP)
				goto einval;
			if (bi->flags & R_DUP)
				dbinfop->flags |= DB_DUP;
			dbinfop->db_cachesize = bi->cachesize;
			dbinfop->bt_maxkey = bi->maxkeypage;
			dbinfop->bt_minkey = bi->minkeypage;
			dbinfop->db_pagesize = bi->psize;
			/*
			 * !!!
			 * Comparisons and prefix calls work because the DBT
			 * structures in 1.85 and 2.0 have the same initial
			 * fields.
			 */
			dbinfop->bt_compare = bi->compare;
			dbinfop->bt_prefix = bi->prefix;
			dbinfop->db_lorder = bi->lorder;
		}
		break;
	case 1:					/* DB_HASH */
		type = DB_HASH;
		if ((hi = openinfo) != NULL) {
			dbinfop = &dbinfo;
			dbinfop->db_pagesize = hi->bsize;
			dbinfop->h_ffactor = hi->ffactor;
			dbinfop->h_nelem = hi->nelem;
			dbinfop->db_cachesize = hi->cachesize;
			dbinfop->h_hash = hi->hash;
			dbinfop->db_lorder = hi->lorder;
		}

		break;
	case 2:					/* DB_RECNO */
		type = DB_RECNO;
		dbinfop = &dbinfo;

		/* DB 1.85 did renumbering by default. */
		dbinfop->flags |= DB_RENUMBER;

		/*
		 * !!!
		 * The file name given to DB 1.85 recno is the name of the DB
		 * 2.0 backing file.  If the file doesn't exist, create it if
		 * the user has the O_CREAT flag set, DB 1.85 did it for you,
		 * and DB 2.0 doesn't.
		 *
		 * !!!
		 * Setting the file name to NULL specifies that we're creating
		 * a temporary backing file, in DB 2.X.  If we're opening the
		 * DB file read-only, change the flags to read-write, because
		 * temporary backing files cannot be opened read-only, and DB
		 * 2.X will return an error.  We are cheating here -- if the
		 * application does a put on the database, it will succeed --
		 * although that would be a stupid thing for the application
		 * to do.
		 *
		 * !!!
		 * Note, the file name in DB 1.85 was a const -- we don't do
		 * that in DB 2.0, so do that cast.
		 */
		if (file != NULL) {
			if (oflags & O_CREAT && __db_exists(file, NULL) != 0)
				(void)__os_close(__os_open(file, oflags, mode));
			dbinfop->re_source = (char *)file;

			if (O_RDONLY)
				oflags &= ~O_RDONLY;
			oflags |= O_RDWR;
			file = NULL;
		}

		if ((ri = openinfo) != NULL) {
			/*
			 * !!!
			 * We can't support the bfname field.
			 */
#define	BFMSG	"DB: DB 1.85's recno bfname field is not supported.\n"
			if (ri->bfname != NULL) {
				(void)__os_write(2, BFMSG, sizeof(BFMSG) - 1);
				goto einval;
			}

			if (ri->flags & ~(R_FIXEDLEN | R_NOKEY | R_SNAPSHOT))
				goto einval;
			if (ri->flags & R_FIXEDLEN) {
				dbinfop->flags |= DB_FIXEDLEN;
				if (ri->bval != 0) {
					dbinfop->flags |= DB_PAD;
					dbinfop->re_pad = ri->bval;
				}
			} else
				if (ri->bval != 0) {
					dbinfop->flags |= DB_DELIMITER;
					dbinfop->re_delim = ri->bval;
				}

			/*
			 * !!!
			 * We ignore the R_NOKEY flag, but that's okay, it was
			 * only an optimization that was never implemented.
			 */

			if (ri->flags & R_SNAPSHOT)
				dbinfop->flags |= DB_SNAPSHOT;

			dbinfop->db_cachesize = ri->cachesize;
			dbinfop->db_pagesize = ri->psize;
			dbinfop->db_lorder = ri->lorder;
			dbinfop->re_len = ri->reclen;
		}
		break;
	default:
		goto einval;
	}

	db185p->close = db185_close;
	db185p->del = db185_del;
	db185p->fd = db185_fd;
	db185p->get = db185_get;
	db185p->put = db185_put;
	db185p->seq = db185_seq;
	db185p->sync = db185_sync;

	/*
	 * !!!
	 * Store the returned pointer to the real DB 2.0 structure in the
	 * internal pointer.  Ugly, but we're not going for pretty, here.
	 */
	if ((errno = db_open(file,
	    type, __db_oflags(oflags), mode, NULL, dbinfop, &dbp)) != 0) {
		__db_free(db185p);
		return (NULL);
	}

	/* Create the cursor used for sequential ops. */
	if ((errno = dbp->cursor(dbp, NULL, &((DB185 *)db185p)->dbc)) != 0) {
		s_errno = errno;
		(void)dbp->close(dbp, 0);
		__db_free(db185p);
		errno = s_errno;
		return (NULL);
	}

	db185p->internal = dbp;
	return (db185p);

einval:	__db_free(db185p);
	errno = EINVAL;
	return (NULL);
}

static int
db185_close(db185p)
	DB185 *db185p;
{
	DB *dbp;

	dbp = (DB *)db185p->internal;

	errno = dbp->close(dbp, 0);

	__db_free(db185p);

	return (errno == 0 ? 0 : -1);
}

static int
db185_del(db185p, key185, flags)
	const DB185 *db185p;
	const DBT185 *key185;
	u_int flags;
{
	DB *dbp;
	DBT key;

	dbp = (DB *)db185p->internal;

	memset(&key, 0, sizeof(key));
	key.data = key185->data;
	key.size = key185->size;

	if (flags & ~R_CURSOR)
		goto einval;
	if (flags & R_CURSOR)
		errno = db185p->dbc->c_del(db185p->dbc, 0);
	else
		errno = dbp->del(dbp, NULL, &key, 0);

	switch (errno) {
	case 0:
		return (0);
	case DB_NOTFOUND:
		return (1);
	}
	return (-1);

einval:	errno = EINVAL;
	return (-1);
}

static int
db185_fd(db185p)
	const DB185 *db185p;
{
	DB *dbp;
	int fd;

	dbp = (DB *)db185p->internal;

	return ((errno = dbp->fd(dbp, &fd)) == 0 ? fd : -1);
}

static int
db185_get(db185p, key185, data185, flags)
	const DB185 *db185p;
	const DBT185 *key185;
	DBT185 *data185;
	u_int flags;
{
	DB *dbp;
	DBT key, data;

	dbp = (DB *)db185p->internal;

	memset(&key, 0, sizeof(key));
	key.data = key185->data;
	key.size = key185->size;
	memset(&data, 0, sizeof(data));
	data.data = data185->data;
	data.size = data185->size;

	if (flags)
		goto einval;

	switch (errno = dbp->get(dbp, NULL, &key, &data, 0)) {
	case 0:
		data185->data = data.data;
		data185->size = data.size;
		return (0);
	case DB_NOTFOUND:
		return (1);
	}
	return (-1);

einval:	errno = EINVAL;
	return (-1);
}

static int
db185_put(db185p, key185, data185, flags)
	const DB185 *db185p;
	DBT185 *key185;
	const DBT185 *data185;
	u_int flags;
{
	DB *dbp;
	DBC *dbcp_put;
	DBT key, data;
	int s_errno;

	dbp = (DB *)db185p->internal;

	memset(&key, 0, sizeof(key));
	key.data = key185->data;
	key.size = key185->size;
	memset(&data, 0, sizeof(data));
	data.data = data185->data;
	data.size = data185->size;

	switch (flags) {
	case 0:
		errno = dbp->put(dbp, NULL, &key, &data, 0);
		break;
	case R_CURSOR:
		errno =
		    db185p->dbc->c_put(db185p->dbc, &key, &data, DB_CURRENT);
		break;
	case R_IAFTER:
	case R_IBEFORE:
		if (dbp->type != DB_RECNO)
			goto einval;

		if ((errno = dbp->cursor(dbp, NULL, &dbcp_put)) != 0)
			return (-1);
		if ((errno =
		    dbcp_put->c_get(dbcp_put, &key, &data, DB_SET)) != 0) {
			s_errno = errno;
			(void)dbcp_put->c_close(dbcp_put);
			errno = s_errno;
			return (-1);
		}
		memset(&data, 0, sizeof(data));
		data.data = data185->data;
		data.size = data185->size;
		errno = dbcp_put->c_put(dbcp_put,
		    &key, &data, flags == R_IAFTER ? DB_AFTER : DB_BEFORE);
		s_errno = errno;
		(void)dbcp_put->c_close(dbcp_put);
		errno = s_errno;
		break;
	case R_NOOVERWRITE:
		errno = dbp->put(dbp, NULL, &key, &data, DB_NOOVERWRITE);
		break;
	case R_SETCURSOR:
		if (dbp->type != DB_BTREE && dbp->type != DB_RECNO)
			goto einval;

		if ((errno = dbp->put(dbp, NULL, &key, &data, 0)) != 0)
			break;
		errno =
		    db185p->dbc->c_get(db185p->dbc, &key, &data, DB_SET_RANGE);
		break;
	default:
		goto einval;
	}

	switch (errno) {
	case 0:
		key185->data = key.data;
		key185->size = key.size;
		return (0);
	case DB_KEYEXIST:
		return (1);
	}
	return (-1);

einval:	errno = EINVAL;
	return (-1);
}

static int
db185_seq(db185p, key185, data185, flags)
	const DB185 *db185p;
	DBT185 *key185, *data185;
	u_int flags;
{
	DB *dbp;
	DBT key, data;

	dbp = (DB *)db185p->internal;

	memset(&key, 0, sizeof(key));
	key.data = key185->data;
	key.size = key185->size;
	memset(&data, 0, sizeof(data));
	data.data = data185->data;
	data.size = data185->size;

	switch (flags) {
	case R_CURSOR:
		flags = DB_SET_RANGE;
		break;
	case R_FIRST:
		flags = DB_FIRST;
		break;
	case R_LAST:
		if (dbp->type != DB_BTREE && dbp->type != DB_RECNO)
			goto einval;
		flags = DB_LAST;
		break;
	case R_NEXT:
		flags = DB_NEXT;
		break;
	case R_PREV:
		if (dbp->type != DB_BTREE && dbp->type != DB_RECNO)
			goto einval;
		flags = DB_PREV;
		break;
	default:
		goto einval;
	}
	switch (errno = db185p->dbc->c_get(db185p->dbc, &key, &data, flags)) {
	case 0:
		key185->data = key.data;
		key185->size = key.size;
		data185->data = data.data;
		data185->size = data.size;
		return (0);
	case DB_NOTFOUND:
		return (1);
	}
	return (-1);

einval:	errno = EINVAL;
	return (-1);
}

static int
db185_sync(db185p, flags)
	const DB185 *db185p;
	u_int flags;
{
	DB *dbp;

	dbp = (DB *)db185p->internal;

	switch (flags) {
	case 0:
		break;
	case R_RECNOSYNC:
		/*
		 * !!!
		 * We can't support the R_RECNOSYNC flag.
		 */
#define	RSMSG	"DB: DB 1.85's R_RECNOSYNC sync flag is not supported.\n"
		(void)__os_write(2, RSMSG, sizeof(RSMSG) - 1);
		goto einval;
	default:
		goto einval;
	}

	return ((errno = dbp->sync(dbp, 0)) == 0 ? 0 : -1);

einval:	errno = EINVAL;
	return (-1);
}
