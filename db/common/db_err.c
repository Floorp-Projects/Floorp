/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)db_err.c	10.25 (Sleepycat) 5/2/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#endif

#include "db_int.h"
#include "common_ext.h"

static int __db_keyempty __P((const DB_ENV *));
static int __db_rdonly __P((const DB_ENV *, const char *));

/*
 * __db_err --
 *	Standard DB error routine.
 *
 * PUBLIC: #ifdef __STDC__
 * PUBLIC: void __db_err __P((const DB_ENV *dbenv, const char *fmt, ...));
 * PUBLIC: #else
 * PUBLIC: void __db_err();
 * PUBLIC: #endif
 */
void
#ifdef __STDC__
__db_err(const DB_ENV *dbenv, const char *fmt, ...)
#else
__db_err(dbenv, fmt, va_alist)
	const DB_ENV *dbenv;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;
	char errbuf[2048];	/* XXX: END OF THE STACK DON'T TRUST SPRINTF. */

	if (dbenv == NULL)
		return;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	if (dbenv->db_errcall != NULL) {
		(void)vsnprintf(errbuf, sizeof(errbuf), fmt, ap);
		dbenv->db_errcall(dbenv->db_errpfx, errbuf);
	}
	if (dbenv->db_errfile != NULL) {
		if (dbenv->db_errpfx != NULL)
			(void)fprintf(dbenv->db_errfile, "%s: ",
			    dbenv->db_errpfx);
		(void)vfprintf(dbenv->db_errfile, fmt, ap);
		(void)fprintf(dbenv->db_errfile, "\n");
		(void)fflush(dbenv->db_errfile);
	}
	va_end(ap);
}

/*
 * XXX
 * Provide ANSI C prototypes for the panic functions.  Some compilers, (e.g.,
 * MS VC 4.2) get upset if they aren't here, even though the K&R declaration
 * appears before the assignment in the __db__panic() call.
 */
static int __db_ecursor __P((DB *, DB_TXN *, DBC **));
static int __db_edel __P((DB *, DB_TXN *, DBT *, u_int32_t));
static int __db_efd __P((DB *, int *));
static int __db_egp __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
static int __db_estat __P((DB *, void *, void *(*)(size_t), u_int32_t));
static int __db_esync __P((DB *, u_int32_t));

/*
 * __db_ecursor --
 *	After-panic cursor routine.
 */
static int
__db_ecursor(a, b, c)
	DB *a;
	DB_TXN *b;
	DBC **c;
{
	COMPQUIET(a, NULL);
	COMPQUIET(b, NULL);
	COMPQUIET(c, NULL);

	return (EPERM);
}

/*
 * __db_edel --
 *	After-panic delete routine.
 */
static int
__db_edel(a, b, c, d)
	DB *a;
	DB_TXN *b;
	DBT *c;
	u_int32_t d;
{
	COMPQUIET(a, NULL);
	COMPQUIET(b, NULL);
	COMPQUIET(c, NULL);
	COMPQUIET(d, 0);

	return (EPERM);
}

/*
 * __db_efd --
 *	After-panic fd routine.
 */
static int
__db_efd(a, b)
	DB *a;
	int *b;
{
	COMPQUIET(a, NULL);
	COMPQUIET(b, NULL);

	return (EPERM);
}

/*
 * __db_egp --
 *	After-panic get/put routine.
 */
static int
__db_egp(a, b, c, d, e)
	DB *a;
	DB_TXN *b;
	DBT *c, *d;
	u_int32_t e;
{
	COMPQUIET(a, NULL);
	COMPQUIET(b, NULL);
	COMPQUIET(c, NULL);
	COMPQUIET(d, NULL);
	COMPQUIET(e, 0);

	return (EPERM);
}

/*
 * __db_estat --
 *	After-panic stat routine.
 */
static int
__db_estat(a, b, c, d)
	DB *a;
	void *b;
	void *(*c) __P((size_t));
	u_int32_t d;
{
	COMPQUIET(a, NULL);
	COMPQUIET(b, NULL);
	COMPQUIET(c, NULL);
	COMPQUIET(d, 0);

	return (EPERM);
}

/*
 * __db_esync --
 *	After-panic sync routine.
 */
static int
__db_esync(a, b)
	DB *a;
	u_int32_t b;
{
	COMPQUIET(a, NULL);
	COMPQUIET(b, 0);

	return (EPERM);
}

/*
 * __db_panic --
 *	Lock out the tree due to unrecoverable error.
 *
 * PUBLIC: int __db_panic __P((DB *));
 */
int
__db_panic(dbp)
	DB *dbp;
{
	/*
	 * XXX
	 * We should shut down all of the process's cursors, too.
	 *
	 * We should call mpool and have it shut down the file, so we get
	 * other processes sharing this file as well.
	 *
	 *	Chaos reigns within.
	 *	Reflect, repent, and reboot.
	 *	Order shall return.
	 */
	dbp->cursor = __db_ecursor;
	dbp->del = __db_edel;
	dbp->fd = __db_efd;
	dbp->get = __db_egp;
	dbp->put = __db_egp;
	dbp->stat = __db_estat;
	dbp->sync = __db_esync;

	return (EPERM);
}

/* Check for invalid flags. */
#undef	DB_CHECK_FLAGS
#define	DB_CHECK_FLAGS(dbenv, name, flags, ok_flags)			\
	if ((flags) & ~(ok_flags))					\
		return (__db_ferr(dbenv, name, 0));
/* Check for invalid flag combinations. */
#undef	DB_CHECK_FCOMBO
#define	DB_CHECK_FCOMBO(dbenv, name, flags, flag1, flag2)		\
	if ((flags) & (flag1) && (flags) & (flag2))			\
		return (__db_ferr(dbenv, name, 1));

/*
 * __db_fchk --
 *	General flags checking routine.
 *
 * PUBLIC: int __db_fchk __P((DB_ENV *, const char *, u_int32_t, u_int32_t));
 */
int
__db_fchk(dbenv, name, flags, ok_flags)
	DB_ENV *dbenv;
	const char *name;
	u_int32_t flags, ok_flags;
{
	DB_CHECK_FLAGS(dbenv, name, flags, ok_flags);
	return (0);
}

/*
 * __db_fcchk --
 *	General combination flags checking routine.
 *
 * PUBLIC: int __db_fcchk
 * PUBLIC:    __P((DB_ENV *, const char *, u_int32_t, u_int32_t, u_int32_t));
 */
int
__db_fcchk(dbenv, name, flags, flag1, flag2)
	DB_ENV *dbenv;
	const char *name;
	u_int32_t flags, flag1, flag2;
{
	DB_CHECK_FCOMBO(dbenv, name, flags, flag1, flag2);
	return (0);
}

/*
 * __db_cdelchk --
 *	Common cursor delete argument checking routine.
 *
 * PUBLIC: int __db_cdelchk __P((const DB *, u_int32_t, int, int));
 */
int
__db_cdelchk(dbp, flags, isrdonly, isvalid)
	const DB *dbp;
	u_int32_t flags;
	int isrdonly, isvalid;
{
	/* Check for changes to a read-only tree. */
	if (isrdonly)
		return (__db_rdonly(dbp->dbenv, "c_del"));

	/* Check for invalid dbc->c_del() function flags. */
	DB_CHECK_FLAGS(dbp->dbenv, "c_del", flags, 0);

	/*
	 * The cursor must be initialized, return -1 for an invalid cursor,
	 * otherwise 0.
	 */
	return (isvalid ? 0 : EINVAL);
}

/*
 * __db_cgetchk --
 *	Common cursor get argument checking routine.
 *
 * PUBLIC: int __db_cgetchk __P((const DB *, DBT *, DBT *, u_int32_t, int));
 */
int
__db_cgetchk(dbp, key, data, flags, isvalid)
	const DB *dbp;
	DBT *key, *data;
	u_int32_t flags;
	int isvalid;
{
	int key_einval, key_flags;

	key_flags = key_einval = 0;

	/* Check for invalid dbc->c_get() function flags. */
	switch (flags) {
	case DB_CURRENT:
	case DB_FIRST:
	case DB_LAST:
	case DB_NEXT:
	case DB_PREV:
		key_flags = 1;
		break;
	case DB_SET_RANGE:
		key_einval = key_flags = 1;
		break;
	case DB_SET:
		key_einval = 1;
		break;
	case DB_GET_RECNO:
		if (!F_ISSET(dbp, DB_BT_RECNUM))
			goto err;
		break;
	case DB_SET_RECNO:
		if (!F_ISSET(dbp, DB_BT_RECNUM))
			goto err;
		key_einval = key_flags = 1;
		break;
	default:
err:		return (__db_ferr(dbp->dbenv, "c_get", 0));
	}

	/* Check for invalid key/data flags. */
	if (key_flags)
		DB_CHECK_FLAGS(dbp->dbenv, "key", key->flags,
		    DB_DBT_MALLOC | DB_DBT_USERMEM | DB_DBT_PARTIAL);
	DB_CHECK_FLAGS(dbp->dbenv, "data", data->flags,
	    DB_DBT_MALLOC | DB_DBT_USERMEM | DB_DBT_PARTIAL);

	/* Check dbt's for valid flags when multi-threaded. */
	if (F_ISSET(dbp, DB_AM_THREAD)) {
		if (!F_ISSET(data, DB_DBT_USERMEM | DB_DBT_MALLOC))
			return (__db_ferr(dbp->dbenv, "threaded data", 1));
		if (key_flags &&
		    !F_ISSET(key, DB_DBT_USERMEM | DB_DBT_MALLOC))
			return (__db_ferr(dbp->dbenv, "threaded key", 1));
	}

	/* Check for missing keys. */
	if (key_einval && (key->data == NULL || key->size == 0))
		return (__db_keyempty(dbp->dbenv));

	/*
	 * The cursor must be initialized for DB_CURRENT, return -1 for an
	 * invalid cursor, otherwise 0.
	 */
	return (isvalid || flags != DB_CURRENT ? 0 : EINVAL);
}

/*
 * __db_cputchk --
 *	Common cursor put argument checking routine.
 *
 * PUBLIC: int __db_cputchk __P((const DB *,
 * PUBLIC:    const DBT *, DBT *, u_int32_t, int, int));
 */
int
__db_cputchk(dbp, key, data, flags, isrdonly, isvalid)
	const DB *dbp;
	const DBT *key;
	DBT *data;
	u_int32_t flags;
	int isrdonly, isvalid;
{
	int key_einval, key_flags;

	/* Check for changes to a read-only tree. */
	if (isrdonly)
		return (__db_rdonly(dbp->dbenv, "c_put"));

	/* Check for invalid dbc->c_put() function flags. */
	key_einval = key_flags = 0;
	switch (flags) {
	case DB_AFTER:
	case DB_BEFORE:
		if (dbp->type == DB_RECNO && !F_ISSET(dbp, DB_RE_RENUMBER))
			goto err;
		if (dbp->type != DB_RECNO && !F_ISSET(dbp, DB_AM_DUP))
			goto err;
		break;
	case DB_CURRENT:
		break;
	case DB_KEYFIRST:
	case DB_KEYLAST:
		if (dbp->type == DB_RECNO)
			goto err;
		key_einval = key_flags = 1;
		break;
	default:
err:		return (__db_ferr(dbp->dbenv, "c_put", 0));
	}

	/* Check for invalid key/data flags. */
	if (key_flags)
		DB_CHECK_FLAGS(dbp->dbenv, "key", key->flags,
		    DB_DBT_MALLOC | DB_DBT_USERMEM | DB_DBT_PARTIAL);
	DB_CHECK_FLAGS(dbp->dbenv, "data", data->flags,
	    DB_DBT_MALLOC | DB_DBT_USERMEM | DB_DBT_PARTIAL);

	/* Check for missing keys. */
	if (key_einval && (key->data == NULL || key->size == 0))
		return (__db_keyempty(dbp->dbenv));

	/*
	 * The cursor must be initialized for anything other than DB_KEYFIRST
	 * and DB_KEYLAST, return -1 for an invalid cursor, otherwise 0.
	 */
	return (isvalid ||
	    (flags != DB_KEYFIRST && flags != DB_KEYLAST) ? 0 : EINVAL);
}

/*
 * __db_delchk --
 *	Common delete argument checking routine.
 *
 * PUBLIC: int __db_delchk __P((const DB *, DBT *, u_int32_t, int));
 */
int
__db_delchk(dbp, key, flags, isrdonly)
	const DB *dbp;
	DBT *key;
	u_int32_t flags;
	int isrdonly;
{
	/* Check for changes to a read-only tree. */
	if (isrdonly)
		return (__db_rdonly(dbp->dbenv, "delete"));

	/* Check for invalid db->del() function flags. */
	DB_CHECK_FLAGS(dbp->dbenv, "delete", flags, 0);

	/* Check for missing keys. */
	if (key->data == NULL || key->size == 0)
		return (__db_keyempty(dbp->dbenv));

	return (0);
}

/*
 * __db_getchk --
 *	Common get argument checking routine.
 *
 * PUBLIC: int __db_getchk __P((const DB *, const DBT *, DBT *, u_int32_t));
 */
int
__db_getchk(dbp, key, data, flags)
	const DB *dbp;
	const DBT *key;
	DBT *data;
	u_int32_t flags;
{
	/* Check for invalid db->get() function flags. */
	DB_CHECK_FLAGS(dbp->dbenv,
	    "get", flags, F_ISSET(dbp, DB_BT_RECNUM) ? DB_SET_RECNO : 0);

	/* Check for invalid key/data flags. */
	DB_CHECK_FLAGS(dbp->dbenv, "key", key->flags, 0);
	DB_CHECK_FLAGS(dbp->dbenv, "data", data->flags,
	    DB_DBT_MALLOC | DB_DBT_USERMEM | DB_DBT_PARTIAL);
	DB_CHECK_FCOMBO(dbp->dbenv,
	    "data", data->flags, DB_DBT_MALLOC, DB_DBT_USERMEM);
	if (F_ISSET(dbp, DB_AM_THREAD) &&
	    !F_ISSET(data, DB_DBT_MALLOC | DB_DBT_USERMEM))
		return (__db_ferr(dbp->dbenv, "threaded data", 1));

	/* Check for missing keys. */
	if (key->data == NULL || key->size == 0)
		return (__db_keyempty(dbp->dbenv));

	return (0);
}

/*
 * __db_putchk --
 *	Common put argument checking routine.
 *
 * PUBLIC: int __db_putchk
 * PUBLIC:    __P((const DB *, DBT *, const DBT *, u_int32_t, int, int));
 */
int
__db_putchk(dbp, key, data, flags, isrdonly, isdup)
	const DB *dbp;
	DBT *key;
	const DBT *data;
	u_int32_t flags;
	int isrdonly, isdup;
{
	/* Check for changes to a read-only tree. */
	if (isrdonly)
		return (__db_rdonly(dbp->dbenv, "put"));

	/* Check for invalid db->put() function flags. */
	DB_CHECK_FLAGS(dbp->dbenv, "put", flags,
	    DB_NOOVERWRITE | (dbp->type == DB_RECNO ? DB_APPEND : 0));

	/* Check for invalid key/data flags. */
	DB_CHECK_FLAGS(dbp->dbenv, "key", key->flags, 0);
	DB_CHECK_FLAGS(dbp->dbenv, "data", data->flags,
	    DB_DBT_MALLOC | DB_DBT_USERMEM | DB_DBT_PARTIAL);
	DB_CHECK_FCOMBO(dbp->dbenv,
	    "data", data->flags, DB_DBT_MALLOC, DB_DBT_USERMEM);

	/* Check for missing keys. */
	if (key->data == NULL || key->size == 0)
		return (__db_keyempty(dbp->dbenv));

	/* Check for partial puts in the presence of duplicates. */
	if (isdup && F_ISSET(data, DB_DBT_PARTIAL)) {
		__db_err(dbp->dbenv,
"a partial put in the presence of duplicates requires a cursor operation");
		return (EINVAL);
	}

	return (0);
}

/*
 * __db_statchk --
 *	Common stat argument checking routine.
 *
 * PUBLIC: int __db_statchk __P((const DB *, u_int32_t));
 */
int
__db_statchk(dbp, flags)
	const DB *dbp;
	u_int32_t flags;
{
	/* Check for invalid db->stat() function flags. */
	DB_CHECK_FLAGS(dbp->dbenv, "stat", flags, DB_RECORDCOUNT);

	if (LF_ISSET(DB_RECORDCOUNT) &&
	    dbp->type == DB_BTREE && !F_ISSET(dbp, DB_BT_RECNUM))
		return (__db_ferr(dbp->dbenv, "stat", 0));

	return (0);
}

/*
 * __db_syncchk --
 *	Common sync argument checking routine.
 *
 * PUBLIC: int __db_syncchk __P((const DB *, u_int32_t));
 */
int
__db_syncchk(dbp, flags)
	const DB *dbp;
	u_int32_t flags;
{
	/* Check for invalid db->sync() function flags. */
	DB_CHECK_FLAGS(dbp->dbenv, "sync", flags, 0);

	return (0);
}

/*
 * __db_ferr --
 *	Common flag errors.
 *
 * PUBLIC: int __db_ferr __P((const DB_ENV *, const char *, int));
 */
int
__db_ferr(dbenv, name, iscombo)
	const DB_ENV *dbenv;
	const char *name;
	int iscombo;
{
	__db_err(dbenv, "illegal flag %sspecified to %s",
	    iscombo ? "combination " : "", name);
	return (EINVAL);
}

/*
 * __db_rdonly --
 *	Common readonly message.
 */
static int
__db_rdonly(dbenv, name)
	const DB_ENV *dbenv;
	const char *name;
{
	__db_err(dbenv, "%s: attempt to modify a read-only tree", name);
	return (EACCES);
}

/*
 * __db_keyempty --
 *	Common missing or empty key value message.
 */
static int
__db_keyempty(dbenv)
	const DB_ENV *dbenv;
{
	__db_err(dbenv, "missing or empty key value specified");
	return (EINVAL);
}
