/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)bt_recno.c	10.37 (Sleepycat) 5/23/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"

static int __ram_add __P((DB *, db_recno_t *, DBT *, u_int32_t, u_int32_t));
static int __ram_c_close __P((DBC *));
static int __ram_c_del __P((DBC *, u_int32_t));
static int __ram_c_get __P((DBC *, DBT *, DBT *, u_int32_t));
static int __ram_c_put __P((DBC *, DBT *, DBT *, u_int32_t));
static int __ram_fmap __P((DB *, db_recno_t));
static int __ram_get __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
static int __ram_iget __P((DB *, DBT *, DBT *));
static int __ram_put __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
static int __ram_source __P((DB *, RECNO *, const char *));
static int __ram_sync __P((DB *, u_int32_t));
static int __ram_update __P((DB *, db_recno_t, int));
static int __ram_vmap __P((DB *, db_recno_t));
static int __ram_writeback __P((DB *));

/*
 * If we're renumbering records, then we have to detect in the cursor that a
 * record was deleted, and adjust the cursor as necessary.  If not renumbering
 * records, then we can detect this by looking at the actual record, so we
 * ignore the cursor delete flag.
 */
#define	CD_SET(dbp, cp) {						\
	if (F_ISSET(dbp, DB_RE_RENUMBER))				\
		F_SET(cp, CR_DELETED);					\
}
#define	CD_CLR(dbp, cp) {						\
	if (F_ISSET(dbp, DB_RE_RENUMBER))				\
		F_CLR(cp, CR_DELETED);					\
}
#define	CD_ISSET(dbp, cp)						\
	(F_ISSET(dbp, DB_RE_RENUMBER) && F_ISSET(cp, CR_DELETED))

/*
 * __ram_open --
 *	Recno open function.
 *
 * PUBLIC: int __ram_open __P((DB *, DBTYPE, DB_INFO *));
 */
int
__ram_open(dbp, type, dbinfo)
	DB *dbp;
	DBTYPE type;
	DB_INFO *dbinfo;
{
	BTREE *t;
	RECNO *rp;
	int ret;

	COMPQUIET(type, DB_RECNO);

	ret = 0;

	/* Allocate and initialize the private RECNO structure. */
	if ((rp = (RECNO *)__db_calloc(1, sizeof(*rp))) == NULL)
		return (ENOMEM);

	if (dbinfo != NULL) {
		/*
		 * If the user specified a source tree, open it and map it in.
		 *
		 * !!!
		 * We don't complain if the user specified transactions or
		 * threads.  It's possible to make it work, but you'd better
		 * know what you're doing!
		 */
		if (dbinfo->re_source == NULL) {
			rp->re_fd = -1;
			F_SET(rp, RECNO_EOF);
		} else {
			if ((ret =
			    __ram_source(dbp, rp, dbinfo->re_source)) != 0)
			goto err;
		}

		/* Copy delimiter, length and padding values. */
		rp->re_delim =
		    F_ISSET(dbp, DB_RE_DELIMITER) ? dbinfo->re_delim : '\n';
		rp->re_pad = F_ISSET(dbp, DB_RE_PAD) ? dbinfo->re_pad : ' ';

		if (F_ISSET(dbp, DB_RE_FIXEDLEN)) {
			if ((rp->re_len = dbinfo->re_len) == 0) {
				__db_err(dbp->dbenv,
				    "record length must be greater than 0");
				ret = EINVAL;
				goto err;
			}
		} else
			rp->re_len = 0;
	} else {
		rp->re_delim = '\n';
		rp->re_pad = ' ';
		rp->re_fd = -1;
		F_SET(rp, RECNO_EOF);
	}

	/* Open the underlying btree. */
	if ((ret = __bam_open(dbp, DB_RECNO, dbinfo)) != 0)
		goto err;

	/* Set the routines necessary to make it look like a recno tree. */
	dbp->cursor = __ram_cursor;
	dbp->del = __ram_delete;
	dbp->get = __ram_get;
	dbp->put = __ram_put;
	dbp->sync = __ram_sync;

	/* Link in the private recno structure. */
	((BTREE *)dbp->internal)->bt_recno = rp;

	/* If we're snapshotting an underlying source file, do it now. */
	if (dbinfo != NULL && F_ISSET(dbinfo, DB_SNAPSHOT))
		if ((ret = __ram_snapshot(dbp)) != 0 && ret != DB_NOTFOUND)
			goto err;

	return (0);

err:	/* If we mmap'd a source file, discard it. */
	if (rp->re_smap != NULL)
		(void)__db_unmapfile(rp->re_smap, rp->re_msize);

	/* If we opened a source file, discard it. */
	if (rp->re_fd != -1)
		(void)__db_close(rp->re_fd);
	if (rp->re_source != NULL)
		FREES(rp->re_source);

	/* If we allocated room for key/data return, discard it. */
	t = dbp->internal;
	if (t != NULL && t->bt_rkey.data != NULL)
		__db_free(t->bt_rkey.data);

	FREE(rp, sizeof(*rp));

	return (ret);
}

/*
 * __ram_cursor --
 *	Recno db->cursor function.
 *
 * PUBLIC: int __ram_cursor __P((DB *, DB_TXN *, DBC **));
 */
int
__ram_cursor(dbp, txn, dbcp)
	DB *dbp;
	DB_TXN *txn;
	DBC **dbcp;
{
	RCURSOR *cp;
	DBC *dbc;

	DEBUG_LWRITE(dbp, txn, "ram_cursor", NULL, NULL, 0);

	if ((dbc = (DBC *)__db_calloc(1, sizeof(DBC))) == NULL)
		return (ENOMEM);
	if ((cp = (RCURSOR *)__db_calloc(1, sizeof(RCURSOR))) == NULL) {
		__db_free(dbc);
		return (ENOMEM);
	}

	cp->dbc = dbc;
	cp->recno = RECNO_OOB;

	dbc->dbp = dbp;
	dbc->txn = txn;
	dbc->internal = cp;
	dbc->c_close = __ram_c_close;
	dbc->c_del = __ram_c_del;
	dbc->c_get = __ram_c_get;
	dbc->c_put = __ram_c_put;

	/*
	 * All cursors are queued from the master DB structure.  Add the
	 * cursor to that queue.
	 */
	CURSOR_SETUP(dbp);
	TAILQ_INSERT_HEAD(&dbp->curs_queue, dbc, links);
	CURSOR_TEARDOWN(dbp);

	*dbcp = dbc;
	return (0);
}

/*
 * __ram_get --
 *	Recno db->get function.
 */
static int
__ram_get(argdbp, txn, key, data, flags)
	DB *argdbp;
	DB_TXN *txn;
	DBT *key, *data;
	u_int32_t flags;
{
	DB *dbp;
	int ret;

	DEBUG_LWRITE(argdbp, txn, "ram_get", key, NULL, flags);

	/* Check for invalid flags. */
	if ((ret = __db_getchk(argdbp, key, data, flags)) != 0)
		return (ret);

	GETHANDLE(argdbp, txn, &dbp, ret);

	ret = __ram_iget(dbp, key, data);

	PUTHANDLE(dbp);
	return (ret);
}

/*
 * __ram_iget --
 *	Internal ram get function, called for both standard and cursor
 *	get after the flags have been checked.
 */
static int
__ram_iget(dbp, key, data)
	DB *dbp;
	DBT *key, *data;
{
	BTREE *t;
	PAGE *h;
	db_indx_t indx;
	db_recno_t recno;
	int exact, ret, stack;

	stack = 0;
	t = dbp->internal;

	/* Check the user's record number and fill in as necessary. */
	if ((ret = __ram_getno(dbp, key, &recno, 0)) != 0)
		goto done;

	/* Search the tree for the record. */
	if ((ret = __bam_rsearch(dbp, &recno, S_FIND, 1, &exact)) != 0)
		goto done;
	if (!exact)
		return (DB_NOTFOUND);
	stack = 1;

	h = t->bt_csp->page;
	indx = t->bt_csp->indx;

	/* If the record has already been deleted, we couldn't have found it. */
	if (B_DISSET(GET_BKEYDATA(h, indx)->type)) {
		ret = DB_KEYEMPTY;
		goto done;
	}

	/* Return the data item. */
	ret = __db_ret(dbp,
	    h, indx, data, &t->bt_rdata.data, &t->bt_rdata.ulen);
	++t->lstat.bt_get;

done:	/* Discard the stack. */
	if (stack)
		__bam_stkrel(dbp);

	return (ret);
}

/*
 * __ram_put --
 *	Recno db->put function.
 */
static int
__ram_put(argdbp, txn, key, data, flags)
	DB *argdbp;
	DB_TXN *txn;
	DBT *key, *data;
	u_int32_t flags;
{
	BTREE *t;
	DB *dbp;
	db_recno_t recno;
	int ret;

	DEBUG_LWRITE(argdbp, txn, "ram_put", key, data, flags);

	/* Check for invalid flags. */
	if ((ret = __db_putchk(argdbp,
	    key, data, flags, F_ISSET(argdbp, DB_AM_RDONLY), 0)) != 0)
		return (ret);

	GETHANDLE(argdbp, txn, &dbp, ret);

	/*
	 * If we're appending to the tree, make sure we've read in all of
	 * the backing source file.  Otherwise, check the user's record
	 * number and fill in as necessary.
	 */
	ret = LF_ISSET(DB_APPEND) ?
	    __ram_snapshot(dbp) : __ram_getno(dbp, key, &recno, 1);

	/* Add the record. */
	if (ret == 0)
		ret = __ram_add(dbp, &recno, data, flags, 0);

	/* If we're appending to the tree, we have to return the record. */
	if (ret == 0 && LF_ISSET(DB_APPEND)) {
		t = dbp->internal;
		ret = __db_retcopy(key, &recno, sizeof(recno),
		    &t->bt_rkey.data, &t->bt_rkey.ulen, dbp->db_malloc);
	}

	PUTHANDLE(dbp);
	return (ret);
}

/*
 * __ram_sync --
 *	Recno db->sync function.
 */
static int
__ram_sync(argdbp, flags)
	DB *argdbp;
	u_int32_t flags;
{
	DB *dbp;
	int ret;

	DEBUG_LWRITE(argdbp, NULL, "ram_sync", NULL, NULL, flags);

	/* Sync the underlying btree. */
	if ((ret = __bam_sync(argdbp, flags)) != 0)
		return (ret);

	/* Copy back the backing source file. */
	GETHANDLE(argdbp, NULL, &dbp, ret);
	ret = __ram_writeback(dbp);
	PUTHANDLE(dbp);

	return (ret);
}

/*
 * __ram_close --
 *	Recno db->close function.
 *
 * PUBLIC: int __ram_close __P((DB *));
 */
int
__ram_close(argdbp)
	DB *argdbp;
{
	RECNO *rp;

	DEBUG_LWRITE(argdbp, NULL, "ram_close", NULL, NULL, 0);

	rp = ((BTREE *)argdbp->internal)->bt_recno;

	/* Close any underlying mmap region. */
	if (rp->re_smap != NULL)
		(void)__db_unmapfile(rp->re_smap, rp->re_msize);

	/* Close any backing source file descriptor. */
	if (rp->re_fd != -1)
		(void)__db_close(rp->re_fd);

	/* Free any backing source file name. */
	if (rp->re_source != NULL)
		FREES(rp->re_source);

	/* Free allocated memory. */
	FREE(rp, sizeof(RECNO));
	((BTREE *)argdbp->internal)->bt_recno = NULL;

	/* Close the underlying btree. */
	return (__bam_close(argdbp));
}

/*
 * __ram_c_close --
 *	Recno cursor->close function.
 */
static int
__ram_c_close(dbc)
	DBC *dbc;
{
	DEBUG_LWRITE(dbc->dbp, dbc->txn, "ram_c_close", NULL, NULL, 0);

	return (__ram_c_iclose(dbc->dbp, dbc));
}

/*
 * __ram_c_iclose --
 *	Close a single cursor -- internal version.
 *
 * PUBLIC: int __ram_c_iclose __P((DB *, DBC *));
 */
int
__ram_c_iclose(dbp, dbc)
	DB *dbp;
	DBC *dbc;
{
	/* Remove the cursor from the queue. */
	CURSOR_SETUP(dbp);
	TAILQ_REMOVE(&dbp->curs_queue, dbc, links);
	CURSOR_TEARDOWN(dbp);

	/* Discard the structures. */
	FREE(dbc->internal, sizeof(RCURSOR));
	FREE(dbc, sizeof(DBC));

	return (0);
}

/*
 * __ram_c_del --
 *	Recno cursor->c_del function.
 */
static int
__ram_c_del(dbc, flags)
	DBC *dbc;
	u_int32_t flags;
{
	DBT key;
	RCURSOR *cp;
	int ret;

	DEBUG_LWRITE(dbc->dbp, dbc->txn, "ram_c_del", NULL, NULL, flags);

	cp = dbc->internal;

	/* Check for invalid flags. */
	if ((ret = __db_cdelchk(dbc->dbp, flags,
	    F_ISSET(dbc->dbp, DB_AM_RDONLY), cp->recno != RECNO_OOB)) != 0)
		return (ret);

	/* If already deleted, return failure. */
	if (CD_ISSET(dbc->dbp, cp))
		return (DB_KEYEMPTY);

	/* Build a normal delete request. */
	memset(&key, 0, sizeof(key));
	key.data = &cp->recno;
	key.size = sizeof(db_recno_t);
	if ((ret = __ram_delete(dbc->dbp, dbc->txn, &key, 0)) == 0)
		CD_SET(dbc->dbp, cp);

	return (ret);
}

/*
 * __ram_c_get --
 *	Recno cursor->c_get function.
 */
static int
__ram_c_get(dbc, key, data, flags)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
{
	BTREE *t;
	DB *dbp;
	RCURSOR *cp, copy;
	int ret;

	DEBUG_LREAD(dbc->dbp, dbc->txn, "ram_c_get",
	    flags == DB_SET || flags == DB_SET_RANGE ? key : NULL,
	    NULL, flags);

	cp = dbc->internal;
	dbp = dbc->dbp;

	/* Check for invalid flags. */
	if ((ret = __db_cgetchk(dbc->dbp,
	    key, data, flags, cp->recno != RECNO_OOB)) != 0)
		return (ret);

	GETHANDLE(dbc->dbp, dbc->txn, &dbp, ret);
	t = dbp->internal;

	/* Initialize the cursor for a new retrieval. */
	copy = *cp;

retry:	/* Update the record number. */
	switch (flags) {
	case DB_CURRENT:
		if (CD_ISSET(dbp, cp)) {
			PUTHANDLE(dbp);
			return (DB_KEYEMPTY);
		}
		break;
	case DB_NEXT:
		if (CD_ISSET(dbp, cp))
			break;
		if (cp->recno != RECNO_OOB) {
			++cp->recno;
			break;
		}
		/* FALLTHROUGH */
	case DB_FIRST:
		flags = DB_NEXT;
		cp->recno = 1;
		break;
	case DB_PREV:
		if (cp->recno != RECNO_OOB) {
			if (cp->recno == 1)
				return (DB_NOTFOUND);
			--cp->recno;
			break;
		}
		/* FALLTHROUGH */
	case DB_LAST:
		flags = DB_PREV;
		if (((ret = __ram_snapshot(dbp)) != 0) && ret != DB_NOTFOUND)
			goto err;
		if ((ret = __bam_nrecs(dbp, &cp->recno)) != 0)
			goto err;
		if (cp->recno == 0)
			return (DB_NOTFOUND);
		break;
	case DB_SET:
	case DB_SET_RANGE:
		if ((ret = __ram_getno(dbp, key, &cp->recno, 0)) != 0)
			goto err;
		break;
	}

	/*
	 * Return the key if the user didn't give us one, and then pass it
	 * into __ram_iget().
	 */
	if (flags != DB_SET && flags != DB_SET_RANGE &&
	    (ret = __db_retcopy(key, &cp->recno, sizeof(cp->recno),
	    &t->bt_rkey.data, &t->bt_rkey.ulen, dbp->db_malloc)) != 0)
		return (ret);

	/*
	 * The cursor was reset, so the delete adjustment is no
	 * longer necessary.
	 */
	CD_CLR(dbp, cp);

	/*
	 * Retrieve the record.
	 *
	 * Skip any keys that don't really exist.
	 */
	if ((ret = __ram_iget(dbp, key, data)) != 0)
		if (ret == DB_KEYEMPTY &&
		    (flags == DB_NEXT || flags == DB_PREV))
			goto retry;

err:	if (ret != 0)
		*cp = copy;

	PUTHANDLE(dbp);
	return (ret);
}

/*
 * __ram_c_put --
 *	Recno cursor->c_put function.
 */
static int
__ram_c_put(dbc, key, data, flags)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
{
	BTREE *t;
	RCURSOR *cp, copy;
	DB *dbp;
	int exact, ret;
	void *arg;

	DEBUG_LWRITE(dbc->dbp, dbc->txn, "ram_c_put", NULL, data, flags);

	cp = dbc->internal;

	if ((ret = __db_cputchk(dbc->dbp, key, data, flags,
	    F_ISSET(dbc->dbp, DB_AM_RDONLY), cp->recno != RECNO_OOB)) != 0)
		return (ret);

	GETHANDLE(dbc->dbp, dbc->txn, &dbp, ret);
	t = dbp->internal;

	/* Initialize the cursor for a new retrieval. */
	copy = *cp;

	/*
	 * To split, we need a valid key for the page.  Since it's a cursor,
	 * we have to build one.
	 *
	 * The split code discards all short-term locks and stack pages.
	 */
	if (0) {
split:		arg = &cp->recno;
		if ((ret = __bam_split(dbp, arg)) != 0)
			goto err;
	}

	if ((ret = __bam_rsearch(dbp, &cp->recno, S_INSERT, 1, &exact)) != 0)
		goto err;
	if (!exact) {
		ret = DB_NOTFOUND;
		goto err;
	}
	if ((ret = __bam_iitem(dbp, &t->bt_csp->page,
	    &t->bt_csp->indx, key, data, flags, 0)) == DB_NEEDSPLIT) {
		if ((ret = __bam_stkrel(dbp)) != 0)
			goto err;
		goto split;
	}
	if ((ret = __bam_stkrel(dbp)) != 0)
		goto err;

	switch (flags) {
	case DB_AFTER:
		/* Adjust the cursors. */
		__ram_ca(dbp, cp->recno, CA_IAFTER);

		/* Set this cursor to reference the new record. */
		cp->recno = copy.recno + 1;
		break;
	case DB_BEFORE:
		/* Adjust the cursors. */
		__ram_ca(dbp, cp->recno, CA_IBEFORE);

		/* Set this cursor to reference the new record. */
		cp->recno = copy.recno;
		break;
	}

	/*
	 * The cursor was reset, so the delete adjustment is no
	 * longer necessary.
	 */
	CD_CLR(dbp, cp);

err:	if (ret != 0)
		*cp = copy;

	PUTHANDLE(dbp);
	return (ret);
}

/*
 * __ram_ca --
 *	Adjust cursors.
 *
 * PUBLIC: void __ram_ca __P((DB *, db_recno_t, ca_recno_arg));
 */
void
__ram_ca(dbp, recno, op)
	DB *dbp;
	db_recno_t recno;
	ca_recno_arg op;
{
	DBC *dbc;
	RCURSOR *cp;

	/*
	 * Adjust the cursors.  See the comment in __bam_ca_delete().
	 */
	CURSOR_SETUP(dbp);
	for (dbc = TAILQ_FIRST(&dbp->curs_queue);
	    dbc != NULL; dbc = TAILQ_NEXT(dbc, links)) {
		cp = (RCURSOR *)dbc->internal;
		switch (op) {
		case CA_DELETE:
			if (recno > cp->recno)
				--cp->recno;
			break;
		case CA_IAFTER:
			if (recno > cp->recno)
				++cp->recno;
			break;
		case CA_IBEFORE:
			if (recno >= cp->recno)
				++cp->recno;
			break;
		}
	}
	CURSOR_TEARDOWN(dbp);
}

#ifdef DEBUG
/*
 * __ram_cprint --
 *	Display the current recno cursor list.
 *
 * PUBLIC: int __ram_cprint __P((DB *));
 */
int
__ram_cprint(dbp)
	DB *dbp;
{
	DBC *dbc;
	RCURSOR *cp;

	CURSOR_SETUP(dbp);
	for (dbc = TAILQ_FIRST(&dbp->curs_queue);
	    dbc != NULL; dbc = TAILQ_NEXT(dbc, links)) {
		cp = (RCURSOR *)dbc->internal;
		fprintf(stderr,
		    "%#0x: recno: %lu\n", (u_int)cp, (u_long)cp->recno);
	}
	CURSOR_TEARDOWN(dbp);

	return (0);
}
#endif /* DEBUG */

/*
 * __ram_getno --
 *	Check the user's record number, and make sure we've seen it.
 *
 * PUBLIC: int __ram_getno __P((DB *, const DBT *, db_recno_t *, int));
 */
int
__ram_getno(dbp, key, rep, can_create)
	DB *dbp;
	const DBT *key;
	db_recno_t *rep;
	int can_create;
{
	db_recno_t recno;

	/* Check the user's record number. */
	if ((recno = *(db_recno_t *)key->data) == 0) {
		__db_err(dbp->dbenv, "illegal record number of 0");
		return (EINVAL);
	}
	if (rep != NULL)
		*rep = recno;

	/*
	 * Btree can neither create records or read them in.  Recno can
	 * do both, see if we can find the record.
	 */
	return (dbp->type == DB_RECNO ?
	    __ram_update(dbp, recno, can_create) : 0);
}

/*
 * __ram_snapshot --
 *	Read in any remaining records from the backing input file.
 *
 * PUBLIC: int __ram_snapshot __P((DB *));
 */
int
__ram_snapshot(dbp)
	DB *dbp;
{
	return (__ram_update(dbp, DB_MAX_RECORDS, 0));
}

/*
 * __ram_update --
 *	Ensure the tree has records up to and including the specified one.
 */
static int
__ram_update(dbp, recno, can_create)
	DB *dbp;
	db_recno_t recno;
	int can_create;
{
	BTREE *t;
	RECNO *rp;
	db_recno_t nrecs;
	int ret;

	t = dbp->internal;
	rp = t->bt_recno;

	/*
	 * If we can't create records and we've read the entire backing input
	 * file, we're done.
	 */
	if (!can_create && F_ISSET(rp, RECNO_EOF))
		return (0);

	/*
	 * If we haven't seen this record yet, try to get it from the original
	 * file.
	 */
	if ((ret = __bam_nrecs(dbp, &nrecs)) != 0)
		return (ret);
	if (!F_ISSET(rp, RECNO_EOF) && recno > nrecs) {
		if ((ret = rp->re_irec(dbp, recno)) != 0)
			return (ret);
		if ((ret = __bam_nrecs(dbp, &nrecs)) != 0)
			return (ret);
	}

	/*
	 * If we can create records, create empty ones up to the requested
	 * record.
	 */
	if (!can_create || recno <= nrecs + 1)
		return (0);

	t->bt_rdata.dlen = 0;
	t->bt_rdata.doff = 0;
	t->bt_rdata.flags = 0;
	if (F_ISSET(dbp, DB_RE_FIXEDLEN)) {
		if (t->bt_rdata.ulen < rp->re_len) {
			t->bt_rdata.data = t->bt_rdata.data == NULL ?
			    (void *)__db_malloc(rp->re_len) :
			    (void *)__db_realloc(t->bt_rdata.data, rp->re_len);
			if (t->bt_rdata.data == NULL) {
				t->bt_rdata.ulen = 0;
				return (ENOMEM);
			}
			t->bt_rdata.ulen = rp->re_len;
		}
		t->bt_rdata.size = rp->re_len;
		memset(t->bt_rdata.data, rp->re_pad, rp->re_len);
	} else
		t->bt_rdata.size = 0;

	while (recno > ++nrecs)
		if ((ret = __ram_add(dbp,
		    &nrecs, &t->bt_rdata, 0, BI_DELETED)) != 0)
			return (ret);
	return (0);
}

/*
 * __ram_source --
 *	Load information about the backing file.
 */
static int
__ram_source(dbp, rp, fname)
	DB *dbp;
	RECNO *rp;
	const char *fname;
{
	size_t size;
	u_int32_t bytes, mbytes, oflags;
	int ret;

	if ((ret = __db_appname(dbp->dbenv,
	    DB_APP_DATA, NULL, fname, 0, NULL, &rp->re_source)) != 0)
		return (ret);

	oflags = F_ISSET(dbp, DB_AM_RDONLY) ? DB_RDONLY : 0;
	if ((ret =
	    __db_open(rp->re_source, oflags, oflags, 0, &rp->re_fd)) != 0) {
		__db_err(dbp->dbenv, "%s: %s", rp->re_source, strerror(ret));
		goto err;
	}

	/*
	 * XXX
	 * We'd like to test to see if the file is too big to mmap.  Since we
	 * don't know what size or type off_t's or size_t's are, or the largest
	 * unsigned integral type is, or what random insanity the local C
	 * compiler will perpetrate, doing the comparison in a portable way is
	 * flatly impossible.  Hope that mmap fails if the file is too large.
	 */
	if ((ret = __db_ioinfo(rp->re_source,
	    rp->re_fd, &mbytes, &bytes, NULL)) != 0) {
		__db_err(dbp->dbenv, "%s: %s", rp->re_source, strerror(ret));
		goto err;
	}
	if (mbytes == 0 && bytes == 0) {
		F_SET(rp, RECNO_EOF);
		return (0);
	}

	size = mbytes * MEGABYTE + bytes;
	if ((ret = __db_mapfile(rp->re_source,
	    rp->re_fd, (size_t)size, 1, &rp->re_smap)) != 0)
		goto err;
	rp->re_cmap = rp->re_smap;
	rp->re_emap = (u_int8_t *)rp->re_smap + (rp->re_msize = size);
	rp->re_irec = F_ISSET(dbp, DB_RE_FIXEDLEN) ?  __ram_fmap : __ram_vmap;
	return (0);

err:	FREES(rp->re_source)
	return (ret);
}

/*
 * __ram_writeback --
 *	Rewrite the backing file.
 */
static int
__ram_writeback(dbp)
	DB *dbp;
{
	RECNO *rp;
	DBT key, data;
	db_recno_t keyno;
	ssize_t nw;
	int fd, ret, t_ret;
	u_int8_t delim, *pad;

	rp = ((BTREE *)dbp->internal)->bt_recno;

	/* If the file wasn't modified, we're done. */
	if (!F_ISSET(rp, RECNO_MODIFIED))
		return (0);

	/* If there's no backing source file, we're done. */
	if (rp->re_source == NULL) {
		F_CLR(rp, RECNO_MODIFIED);
		return (0);
	}

	/*
	 * Read any remaining records into the tree.
	 *
	 * XXX
	 * This is why we can't support transactions when applications specify
	 * backing (re_source) files.  At this point we have to read in the
	 * rest of the records from the file so that we can write all of the
	 * records back out again, which could modify a page for which we'd
	 * have to log changes and which we don't have locked.  This could be
	 * partially fixed by taking a snapshot of the entire file during the
	 * db_open(), or, since db_open() isn't transaction protected, as part
	 * of the first DB operation.  But, if a checkpoint occurs then, the
	 * part of the log holding the copy of the file could be discarded, and
	 * that would make it impossible to recover in the face of disaster.
	 * This could all probably be fixed, but it would require transaction
	 * protecting the backing source file, i.e. mpool would have to know
	 * about it, and we don't want to go there.
	 */
	if ((ret = __ram_snapshot(dbp)) != 0 && ret != DB_NOTFOUND)
		return (ret);

	/*
	 * !!!
	 * Close any underlying mmap region.  This is required for Windows NT
	 * (4.0, Service Pack 2) -- if the file is still mapped, the following
	 * open will fail.
	 */
	if (rp->re_smap != NULL) {
		(void)__db_unmapfile(rp->re_smap, rp->re_msize);
		rp->re_smap = NULL;
	}

	/* Get rid of any backing file descriptor, just on GP's. */
	if (rp->re_fd != -1) {
		(void)__db_close(rp->re_fd);
		rp->re_fd = -1;
	}

	/* Open the file, truncating it. */
	if ((ret = __db_open(rp->re_source,
	    DB_SEQUENTIAL | DB_TRUNCATE,
	    DB_SEQUENTIAL | DB_TRUNCATE, 0, &fd)) != 0) {
		__db_err(dbp->dbenv, "%s: %s", rp->re_source, strerror(ret));
		return (ret);
	}

	/*
	 * We step through the records, writing each one out.  Use the record
	 * number and the dbp->get() function, instead of a cursor, so we find
	 * and write out "deleted" or non-existent records.
	 */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.size = sizeof(db_recno_t);
	key.data = &keyno;

	/*
	 * We'll need the delimiter if we're doing variable-length records,
	 * and the pad character if we're doing fixed-length records.
	 */
	delim = rp->re_delim;
	if (F_ISSET(dbp, DB_RE_FIXEDLEN)) {
		if ((pad = (u_int8_t *)__db_malloc(rp->re_len)) == NULL) {
			ret = ENOMEM;
			goto err;
		}
		memset(pad, rp->re_pad, rp->re_len);
	} else
		COMPQUIET(pad, NULL);
	for (keyno = 1;; ++keyno) {
		switch (ret = dbp->get(dbp, NULL, &key, &data, 0)) {
		case 0:
			if ((ret =
			    __db_write(fd, data.data, data.size, &nw)) != 0)
				goto err;
			if (nw != (ssize_t)data.size) {
				ret = EIO;
				goto err;
			}
			break;
		case DB_KEYEMPTY:
			if (F_ISSET(dbp, DB_RE_FIXEDLEN)) {
				if ((ret =
				    __db_write(fd, pad, rp->re_len, &nw)) != 0)
					goto err;
				if (nw != (ssize_t)rp->re_len) {
					ret = EIO;
					goto err;
				}
			}
			break;
		case DB_NOTFOUND:
			ret = 0;
			goto done;
		}
		if (!F_ISSET(dbp, DB_RE_FIXEDLEN)) {
			if ((ret = __db_write(fd, &delim, 1, &nw)) != 0)
				goto err;
			if (nw != 1) {
				ret = EIO;
				goto err;
			}
		}
	}

err:
done:	/* Close the file descriptor. */
	if ((t_ret = __db_close(fd)) != 0 || ret == 0)
		ret = t_ret;

	if (ret == 0)
		F_CLR(rp, RECNO_MODIFIED);
	return (ret);
}

/*
 * __ram_fmap --
 *	Get fixed length records from a file.
 */
static int
__ram_fmap(dbp, top)
	DB *dbp;
	db_recno_t top;
{
	BTREE *t;
	DBT data;
	RECNO *rp;
	db_recno_t recno;
	u_int32_t len;
	u_int8_t *sp, *ep, *p;
	int ret;

	if ((ret = __bam_nrecs(dbp, &recno)) != 0)
		return (ret);

	t = dbp->internal;
	rp = t->bt_recno;
	if (t->bt_rdata.ulen < rp->re_len) {
		t->bt_rdata.data = t->bt_rdata.data == NULL ?
		    (void *)__db_malloc(rp->re_len) :
		    (void *)__db_realloc(t->bt_rdata.data, rp->re_len);
		if (t->bt_rdata.data == NULL) {
			t->bt_rdata.ulen = 0;
			return (ENOMEM);
		}
		t->bt_rdata.ulen = rp->re_len;
	}

	memset(&data, 0, sizeof(data));
	data.data = t->bt_rdata.data;
	data.size = rp->re_len;

	sp = (u_int8_t *)rp->re_cmap;
	ep = (u_int8_t *)rp->re_emap;
	while (recno < top) {
		if (sp >= ep) {
			F_SET(rp, RECNO_EOF);
			return (DB_NOTFOUND);
		}
		len = rp->re_len;
		for (p = t->bt_rdata.data;
		    sp < ep && len > 0; *p++ = *sp++, --len)
			;

		/*
		 * Another process may have read this record from the input
		 * file and stored it into the database already, in which
		 * case we don't need to repeat that operation.  We detect
		 * this by checking if the last record we've read is greater
		 * or equal to the number of records in the database.
		 *
		 * XXX
		 * We should just do a seek, since the records are fixed
		 * length.
		 */
		if (rp->re_last >= recno) {
			if (len != 0)
				memset(p, rp->re_pad, len);

			++recno;
			if ((ret = __ram_add(dbp, &recno, &data, 0, 0)) != 0)
				return (ret);
		}
		++rp->re_last;
	}
	rp->re_cmap = sp;
	return (0);
}

/*
 * __ram_vmap --
 *	Get variable length records from a file.
 */
static int
__ram_vmap(dbp, top)
	DB *dbp;
	db_recno_t top;
{
	BTREE *t;
	DBT data;
	RECNO *rp;
	db_recno_t recno;
	u_int8_t *sp, *ep;
	int delim, ret;

	t = dbp->internal;
	rp = t->bt_recno;

	if ((ret = __bam_nrecs(dbp, &recno)) != 0)
		return (ret);

	memset(&data, 0, sizeof(data));

	delim = rp->re_delim;

	sp = (u_int8_t *)rp->re_cmap;
	ep = (u_int8_t *)rp->re_emap;
	while (recno < top) {
		if (sp >= ep) {
			F_SET(rp, RECNO_EOF);
			return (DB_NOTFOUND);
		}
		for (data.data = sp; sp < ep && *sp != delim; ++sp)
			;

		/*
		 * Another process may have read this record from the input
		 * file and stored it into the database already, in which
		 * case we don't need to repeat that operation.  We detect
		 * this by checking if the last record we've read is greater
		 * or equal to the number of records in the database.
		 */
		if (rp->re_last >= recno) {
			data.size = sp - (u_int8_t *)data.data;
			++recno;
			if ((ret = __ram_add(dbp, &recno, &data, 0, 0)) != 0)
				return (ret);
		}
		++rp->re_last;
		++sp;
	}
	rp->re_cmap = sp;
	return (0);
}

/*
 * __ram_add --
 *	Add records into the tree.
 */
static int
__ram_add(dbp, recnop, data, flags, bi_flags)
	DB *dbp;
	db_recno_t *recnop;
	DBT *data;
	u_int32_t flags, bi_flags;
{
	BKEYDATA *bk;
	BTREE *t;
	PAGE *h;
	db_indx_t indx;
	int exact, isdeleted, ret, stack;

	t = dbp->internal;

retry:	/* Find the slot for insertion. */
	if ((ret = __bam_rsearch(dbp, recnop,
	    S_INSERT | (LF_ISSET(DB_APPEND) ? S_APPEND : 0), 1, &exact)) != 0)
		return (ret);
	h = t->bt_csp->page;
	indx = t->bt_csp->indx;
	stack = 1;

	/*
	 * If DB_NOOVERWRITE is set and the item already exists in the tree,
	 * return an error unless the item has been marked for deletion.
	 */
	isdeleted = 0;
	if (exact) {
		bk = GET_BKEYDATA(h, indx);
		if (B_DISSET(bk->type)) {
			isdeleted = 1;
			__bam_ca_replace(dbp, h->pgno, indx, REPLACE_SETUP);
		} else
			if (LF_ISSET(DB_NOOVERWRITE)) {
				ret = DB_KEYEXIST;
				goto err;
			}
	}

	/*
	 * Select the arguments for __bam_iitem() and do the insert.  If the
	 * key is an exact match, or we're replacing the data item with a
	 * new data item, replace the current item.  If the key isn't an exact
	 * match, we're inserting a new key/data pair, before the search
	 * location.
	 */
	switch (ret = __bam_iitem(dbp,
	    &h, &indx, NULL, data, exact ? DB_CURRENT : DB_BEFORE, bi_flags)) {
	case 0:
		/*
		 * Done.  Clean up the cursor and adjust the internal page
		 * counts.
		 */
		if (isdeleted)
			__bam_ca_replace(dbp, h->pgno, indx, REPLACE_SUCCESS);
		break;
	case DB_NEEDSPLIT:
		/*
		 * We have to split the page.  Back out the cursor setup,
		 * discard the stack of pages, and do the split.
		 */
		if (isdeleted)
			__bam_ca_replace(dbp, h->pgno, indx, REPLACE_FAILED);

		(void)__bam_stkrel(dbp);
		stack = 0;

		if ((ret = __bam_split(dbp, recnop)) != 0)
			break;

		goto retry;
		/* NOTREACHED */
	default:
		if (isdeleted)
			__bam_ca_replace(dbp, h->pgno, indx, REPLACE_FAILED);
		break;
	}

err:	if (stack)
		__bam_stkrel(dbp);

	return (ret);
}
