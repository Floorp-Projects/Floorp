/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994
 *	Margo Seltzer.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)hash.c	10.45 (Sleepycat) 5/11/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_page.h"
#include "db_am.h"
#include "db_ext.h"
#include "hash.h"
#include "log.h"

static int  __ham_c_close __P((DBC *));
static int  __ham_c_del __P((DBC *, u_int32_t));
static int  __ham_c_get __P((DBC *, DBT *, DBT *, u_int32_t));
static int  __ham_c_put __P((DBC *, DBT *, DBT *, u_int32_t));
static int  __ham_c_init __P((DB *, DB_TXN *, DBC **));
static int  __ham_cursor __P((DB *, DB_TXN *, DBC **));
static int  __ham_delete __P((DB *, DB_TXN *, DBT *, u_int32_t));
static int  __ham_dup_return __P((HTAB *, HASH_CURSOR *, DBT *, u_int32_t));
static int  __ham_get __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
static void __ham_init_htab __P((HTAB *, u_int32_t, u_int32_t));
static int  __ham_lookup __P((HTAB *,
		HASH_CURSOR *, const DBT *, u_int32_t, db_lockmode_t));
static int  __ham_overwrite __P((HTAB *, HASH_CURSOR *, DBT *));
static int  __ham_put __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
static int  __ham_sync __P((DB *, u_int32_t));

/************************** INTERFACE ROUTINES ***************************/
/* OPEN/CLOSE */

/*
 * __ham_open --
 *
 * PUBLIC: int __ham_open __P((DB *, DB_INFO *));
 */
int
__ham_open(dbp, dbinfo)
	DB *dbp;
	DB_INFO *dbinfo;
{
	DB_ENV *dbenv;
	DBC *curs;
	HTAB *hashp;
	int file_existed, ret;

	dbenv = dbp->dbenv;

	if ((hashp = (HTAB *)__db_calloc(1, sizeof(HTAB))) == NULL)
		return (ENOMEM);
	hashp->dbp = dbp;

	/* Set the hash function if specified by the user. */
	if (dbinfo != NULL && dbinfo->h_hash != NULL)
		hashp->hash = dbinfo->h_hash;

	/*
	 * Initialize the remaining fields of the dbp.  The type, close and
	 * fd functions are all set in db_open.
	 */
	dbp->internal = hashp;
	dbp->cursor = __ham_cursor;
	dbp->del = __ham_delete;
	dbp->get = __ham_get;
	dbp->put = __ham_put;
	dbp->sync = __ham_sync;

	/* If locking is turned on, lock the meta data page. */
	if (F_ISSET(dbp, DB_AM_LOCKING)) {
		dbp->lock.pgno = BUCKET_INVALID;
		if ((ret = lock_get(dbenv->lk_info, dbp->locker,
		    0, &dbp->lock_dbt, DB_LOCK_READ, &hashp->hlock)) != 0) {
			if (ret < 0)
				ret = EAGAIN;
			goto out;
		}
	}

	/*
	 * Now, we can try to read the meta-data page and figure out
	 * if we set up locking and get the meta-data page properly.
	 * If this is a new file, initialize it, and put it back dirty.
	 */
	if ((ret = __ham_get_page(hashp->dbp, 0, (PAGE **)&hashp->hdr)) != 0)
		goto out;

	/* Initialize the hashp structure */
	if (hashp->hdr->magic == DB_HASHMAGIC) {
		file_existed = 1;
		/* File exists, verify the data in the header. */
		if (hashp->hash == NULL)
			hashp->hash =
			    hashp->hdr->version < 5 ? __ham_func4 : __ham_func5;
		if (hashp->hash(CHARKEY, sizeof(CHARKEY)) !=
		    hashp->hdr->h_charkey) {
			__db_err(hashp->dbp->dbenv,
			    "hash: incompatible hash function");
			ret = EINVAL;
			goto out;
		}
		if (F_ISSET(hashp->hdr, DB_HASH_DUP))
			F_SET(dbp, DB_AM_DUP);
	} else {
		/*
		 * File does not exist, we must initialize the header.  If
		 * locking is enabled that means getting a write lock first.
		 */
		file_existed = 0;
		if (F_ISSET(dbp, DB_AM_LOCKING) &&
		    ((ret = lock_put(dbenv->lk_info, hashp->hlock)) != 0 ||
		    (ret = lock_get(dbenv->lk_info, dbp->locker, 0,
		        &dbp->lock_dbt, DB_LOCK_WRITE, &hashp->hlock)) != 0)) {
			if (ret < 0)
				ret = EAGAIN;
			goto out;
		}

		__ham_init_htab(hashp,
		    dbinfo != NULL ? dbinfo->h_nelem : 0,
		    dbinfo != NULL ? dbinfo->h_ffactor : 0);
		if (F_ISSET(dbp, DB_AM_DUP))
			F_SET(hashp->hdr, DB_HASH_DUP);
		if ((ret = __ham_dirty_page(hashp, (PAGE *)hashp->hdr)) != 0)
			goto out;
	}

	/* Initialize the default cursor. */
	__ham_c_init(dbp, NULL, &curs);
	TAILQ_INSERT_TAIL(&dbp->curs_queue, curs, links);

	/* Allocate memory for our split buffer. */
	if ((hashp->split_buf = (PAGE *)__db_malloc(dbp->pgsize)) == NULL) {
		ret = ENOMEM;
		goto out;
	}

#ifdef NO_STATISTICS_FOR_DB_ERR
	__db_err(dbp->dbenv,
	    "%s%lx\n%s%ld\n%s%ld\n%s%ld\n%s%ld\n%s0x%lx\n%s0x%lx\n%s%ld\n%s%ld\n%s0x%lx",
	    "TABLE POINTER   ", (long)hashp,
	    "BUCKET SIZE     ", (long)hashp->hdr->pagesize,
	    "FILL FACTOR     ", (long)hashp->hdr->ffactor,
	    "MAX BUCKET      ", (long)hashp->hdr->max_bucket,
	    "OVFL POINT      ", (long)hashp->hdr->ovfl_point,
	    "LAST FREED      ", (long)hashp->hdr->last_freed,
	    "HIGH MASK       ", (long)hashp->hdr->high_mask,
	    "LOW  MASK       ", (long)hashp->hdr->low_mask,
	    "NELEM           ", (long)hashp->hdr->nelem,
	    "FLAGS           ", (long)hashp->hdr->flags);
#endif

	/* Release the meta data page */
	(void)__ham_put_page(hashp->dbp, (PAGE *)hashp->hdr, 0);
	if (F_ISSET(dbp, DB_AM_LOCKING) &&
	    (ret = lock_put(dbenv->lk_info, hashp->hlock)) != 0) {
		if (ret < 0)
			ret = EAGAIN;
		goto out;
	}

	hashp->hlock = 0;
	hashp->hdr = NULL;
	/* Sync the file so that we know that the meta data goes to disk. */
	if (!file_existed && (ret = dbp->sync(dbp, 0)) != 0)
		goto out;
	return (0);

out:	(void)__ham_close(dbp);
	return (ret);
}

/*
 * PUBLIC: int __ham_close __P((DB *));
 */
int
__ham_close(dbp)
	DB *dbp;
{
	HTAB *hashp;
	int ret, t_ret;

	DEBUG_LWRITE(dbp, NULL, "ham_close", NULL, NULL, 0);
	hashp = (HTAB *)dbp->internal;
	ret = 0;

	/* Free the split page. */
	if (hashp->split_buf)
		FREE(hashp->split_buf, dbp->pgsize);

	if (hashp->hdr && (t_ret = __ham_put_page(hashp->dbp,
	    (PAGE *)hashp->hdr, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (hashp->hlock && (t_ret = lock_put(hashp->dbp->dbenv->lk_info,
	    hashp->hlock)) != 0 && ret == 0)
		ret = t_ret;

	FREE(hashp, sizeof(HTAB));
	dbp->internal = NULL;
	return (ret);
}

/************************** LOCAL CREATION ROUTINES **********************/
/*
 * Returns 0 on No Error
 */
static void
__ham_init_htab(hashp, nelem, ffactor)
	HTAB *hashp;
	u_int32_t nelem, ffactor;
{
	int32_t l2, nbuckets;

	memset(hashp->hdr, 0, sizeof(HASHHDR));
	hashp->hdr->ffactor = ffactor;
	hashp->hdr->pagesize = hashp->dbp->pgsize;
	ZERO_LSN(hashp->hdr->lsn);
	hashp->hdr->magic = DB_HASHMAGIC;
	hashp->hdr->version = DB_HASHVERSION;
	if (hashp->hash == NULL)
		hashp->hash =
		    hashp->hdr->version < 5 ? __ham_func4 : __ham_func5;
	hashp->hdr->h_charkey = hashp->hash(CHARKEY, sizeof(CHARKEY));
	if (nelem != 0 && hashp->hdr->ffactor != 0) {
		nelem = (nelem - 1) / hashp->hdr->ffactor + 1;
		l2 = __db_log2(nelem > 2 ? nelem : 2);
	} else
		l2 = 2;

	nbuckets = 1 << l2;

	hashp->hdr->ovfl_point = l2;
	hashp->hdr->last_freed = PGNO_INVALID;

	hashp->hdr->max_bucket = hashp->hdr->high_mask = nbuckets - 1;
	hashp->hdr->low_mask = (nbuckets >> 1) - 1;
	memcpy(hashp->hdr->uid, hashp->dbp->lock.fileid, DB_FILE_ID_LEN);
}

/********************** DESTROY/CLOSE ROUTINES ************************/


/*
 * Write modified pages to disk
 *
 * Returns:
 *	 0 == OK
 *	-1 ERROR
 */
static int
__ham_sync(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	int ret;

	DEBUG_LWRITE(dbp, NULL, "ham_sync", NULL, NULL, flags);
	if ((ret = __db_syncchk(dbp, flags)) != 0)
		return (ret);
	if (F_ISSET(dbp, DB_AM_RDONLY))
		return (0);

	if ((ret = memp_fsync(dbp->mpf)) == DB_INCOMPLETE)
		ret = 0;

	return (ret);
}

/*******************************SEARCH ROUTINES *****************************/
/*
 * All the access routines return
 *
 * Returns:
 *	 0 on SUCCESS
 *	 1 to indicate an external ERROR (i.e. key not found, etc)
 *	-1 to indicate an internal ERROR (i.e. out of memory, etc)
 */

static int
__ham_get(dbp, txn, key, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key;
	DBT *data;
	u_int32_t flags;
{
	DB *ldbp;
	HTAB *hashp;
	HASH_CURSOR *hcp;
	int ret, t_ret;

	DEBUG_LREAD(dbp, txn, "ham_get", key, NULL, flags);
	if ((ret = __db_getchk(dbp, key, data, flags)) != 0)
		return (ret);

	ldbp = dbp;
	if (F_ISSET(dbp, DB_AM_THREAD) &&
	    (ret = __db_gethandle(dbp, __ham_hdup, &ldbp)) != 0)
		return (ret);

	hashp = (HTAB *)ldbp->internal;
	SET_LOCKER(ldbp, txn);
	GET_META(ldbp, hashp);

	hashp->hash_accesses++;
	hcp = (HASH_CURSOR *)TAILQ_FIRST(&ldbp->curs_queue)->internal;
	if ((ret = __ham_lookup(hashp, hcp, key, 0, DB_LOCK_READ)) == 0)
		if (F_ISSET(hcp, H_OK))
			ret = __ham_dup_return(hashp, hcp, data, DB_FIRST);
		else /* Key was not found */
			ret = DB_NOTFOUND;

	if ((t_ret = __ham_item_done(hashp, hcp, 0)) != 0 && ret == 0)
		ret = t_ret;
	RELEASE_META(ldbp, hashp);
	if (F_ISSET(dbp, DB_AM_THREAD))
		__db_puthandle(ldbp);
	return (ret);
}

static int
__ham_put(dbp, txn, key, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key;
	DBT *data;
	u_int32_t flags;
{
	DB *ldbp;
	DBT tmp_val, *myval;
	HASH_CURSOR *hcp;
	HTAB *hashp;
	u_int32_t nbytes;
	int ret, t_ret;

	DEBUG_LWRITE(dbp, txn, "ham_put", key, data, flags);
	if ((ret = __db_putchk(dbp, key, data,
	    flags, F_ISSET(dbp, DB_AM_RDONLY), F_ISSET(dbp, DB_AM_DUP))) != 0)
		return (ret);

	ldbp = dbp;
	if (F_ISSET(dbp, DB_AM_THREAD) &&
	    (ret = __db_gethandle(dbp, __ham_hdup, &ldbp)) != 0)
		return (ret);

	hashp = (HTAB *)ldbp->internal;
	SET_LOCKER(ldbp, txn);
	GET_META(ldbp, hashp);
	hcp = TAILQ_FIRST(&ldbp->curs_queue)->internal;

	nbytes = (ISBIG(hashp, key->size) ? HOFFPAGE_PSIZE :
	    HKEYDATA_PSIZE(key->size)) +
	    (ISBIG(hashp, data->size) ? HOFFPAGE_PSIZE :
	    HKEYDATA_PSIZE(data->size));

	hashp->hash_accesses++;
	ret = __ham_lookup(hashp, hcp, key, nbytes, DB_LOCK_WRITE);

	if (ret == DB_NOTFOUND) {
		ret = 0;
		if (hcp->seek_found_page != PGNO_INVALID &&
		    hcp->seek_found_page != hcp->pgno) {
			if ((ret = __ham_item_done(hashp, hcp, 0)) != 0)
				goto out;
			hcp->pgno = hcp->seek_found_page;
			hcp->bndx = NDX_INVALID;
		}

		if (F_ISSET(data, DB_DBT_PARTIAL) && data->doff != 0) {
			/*
			 * Doing a partial put, but the key does not exist
			 * and we are not beginning the write at 0.  We
			 * must create a data item padded up to doff and
			 * then write the new bytes represented by val.
			 */
			ret = __ham_init_dbt(&tmp_val, data->size + data->doff,
			    &hcp->big_data, &hcp->big_datalen);
			if (ret == 0) {
				memset(tmp_val.data, 0, data->doff);
				memcpy((u_int8_t *)tmp_val.data + data->doff,
				    data->data, data->size);
				myval = &tmp_val;
			}
		} else
			myval = (DBT *)data;

		if (ret == 0)
			ret = __ham_add_el(hashp, hcp, key, myval, H_KEYDATA);
	} else if (ret == 0 && F_ISSET(hcp, H_OK)) {
		if (flags == DB_NOOVERWRITE)
			ret = DB_KEYEXIST;
		else if (F_ISSET(ldbp, DB_AM_DUP))
			ret = __ham_add_dup(hashp, hcp, data, DB_KEYLAST);
		else
			ret = __ham_overwrite(hashp, hcp, data);
	}

	/* Free up all the cursor pages. */
	if ((t_ret = __ham_item_done(hashp, hcp, ret == 0)) != 0 && ret == 0)
		ret = t_ret;
	/* Now check if we have to grow. */
out:	if (ret == 0 && F_ISSET(hcp, H_EXPAND)) {
		ret = __ham_expand_table(hashp);
		F_CLR(hcp, H_EXPAND);
	}

	if ((t_ret = __ham_item_done(hashp, hcp, ret == 0)) != 0 && ret == 0)
		ret = t_ret;
	RELEASE_META(ldbp, hashp);
	if (F_ISSET(dbp, DB_AM_THREAD))
		__db_puthandle(ldbp);
	return (ret);
}

static int
__ham_cursor(dbp, txnid, dbcp)
	DB *dbp;
	DB_TXN *txnid;
	DBC **dbcp;
{
	int ret;

	DEBUG_LWRITE(dbp, txnid, "ham_cursor", NULL, NULL, 0);
	if ((ret = __ham_c_init(dbp, txnid, dbcp)) != 0)
		return (ret);

	DB_THREAD_LOCK(dbp);
	TAILQ_INSERT_TAIL(&dbp->curs_queue, *dbcp, links);
	DB_THREAD_UNLOCK(dbp);
	return (ret);
}

static int
__ham_c_init(dbp, txnid, dbcp)
	DB *dbp;
	DB_TXN *txnid;
	DBC **dbcp;
{
	DBC *db_curs;
	HASH_CURSOR *new_curs;

	if ((db_curs = (DBC *)__db_calloc(sizeof(DBC), 1)) == NULL)
		return (ENOMEM);

	if ((new_curs =
	    (HASH_CURSOR *)__db_calloc(sizeof(struct cursor_t), 1)) == NULL) {
		FREE(db_curs, sizeof(DBC));
		return (ENOMEM);
	}

	db_curs->internal = new_curs;
	db_curs->c_close = __ham_c_close;
	db_curs->c_del = __ham_c_del;
	db_curs->c_get = __ham_c_get;
	db_curs->c_put = __ham_c_put;
	db_curs->txn = txnid;
	db_curs->dbp = dbp;

	new_curs->db_cursor = db_curs;
	__ham_item_init(new_curs);

	if (dbcp != NULL)
		*dbcp = db_curs;
	return (0);
}

static int
__ham_delete(dbp, txn, key, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key;
	u_int32_t flags;
{
	DB *ldbp;
	HTAB *hashp;
	HASH_CURSOR *hcp;
	int ret, t_ret;

	DEBUG_LWRITE(dbp, txn, "ham_delete", key, NULL, flags);
	if ((ret =
	    __db_delchk(dbp, key, flags, F_ISSET(dbp, DB_AM_RDONLY))) != 0)
		return (ret);

	ldbp = dbp;
	if (F_ISSET(dbp, DB_AM_THREAD) &&
	    (ret = __db_gethandle(dbp, __ham_hdup, &ldbp)) != 0)
		return (ret);
	hashp = (HTAB *)ldbp->internal;
	SET_LOCKER(ldbp, txn);
	GET_META(ldbp, hashp);
	hcp = TAILQ_FIRST(&ldbp->curs_queue)->internal;

	hashp->hash_accesses++;
	if ((ret = __ham_lookup(hashp, hcp, key, 0, DB_LOCK_WRITE)) == 0)
		if (F_ISSET(hcp, H_OK))
			ret = __ham_del_pair(hashp, hcp, 1);
		else
			ret = DB_NOTFOUND;

	if ((t_ret = __ham_item_done(hashp, hcp, ret == 0)) != 0 && ret == 0)
		ret = t_ret;
	RELEASE_META(ldbp, hashp);
	if (F_ISSET(dbp, DB_AM_THREAD))
		__db_puthandle(ldbp);
	return (ret);
}

/* ****************** CURSORS ********************************** */
static int
__ham_c_close(cursor)
	DBC *cursor;
{
	DB  *ldbp;
	int ret;

	DEBUG_LWRITE(cursor->dbp, cursor->txn, "ham_c_close", NULL, NULL, 0);
	/*
	 * If the pagep, dpagep, and lock fields of the cursor are all NULL,
	 * then there really isn't a need to get a handle here.  However,
	 * the normal case is that at least one of those fields is non-NULL,
	 * and putting those checks in here would couple the ham_item_done
	 * functionality with cursor close which would be pretty disgusting.
	 * Instead, we pay the overhead here of always getting the handle.
	 */
	ldbp = cursor->dbp;
	if (F_ISSET(cursor->dbp, DB_AM_THREAD) &&
	    (ret = __db_gethandle(cursor->dbp, __ham_hdup, &ldbp)) != 0)
		return (ret);

	ret = __ham_c_iclose(ldbp, cursor);

	if (F_ISSET(ldbp, DB_AM_THREAD))
		__db_puthandle(ldbp);
	return (ret);
}
/*
 * __ham_c_iclose --
 *
 * Internal cursor close routine; assumes it is being passed the correct
 * handle, rather than getting and putting a handle.
 *
 * PUBLIC: int __ham_c_iclose __P((DB *, DBC *));
 */
int
__ham_c_iclose(dbp, dbc)
	DB *dbp;
	DBC *dbc;
{
	HASH_CURSOR *hcp;
	HTAB *hashp;
	int ret;

	hashp = (HTAB *)dbp->internal;
	hcp = (HASH_CURSOR *)dbc->internal;
	ret = __ham_item_done(hashp, hcp, 0);

	if (hcp->big_key)
		FREE(hcp->big_key, hcp->big_keylen);
	if (hcp->big_data)
		FREE(hcp->big_data, hcp->big_datalen);

	/*
	 * All cursors (except the default ones) are linked off the master.
	 * Therefore, when we close the cursor, we have to remove it from
	 * the master, not the local one.
	 * XXX I am always removing from the master; what about local cursors?
	 */
	DB_THREAD_LOCK(dbc->dbp);
	TAILQ_REMOVE(&dbc->dbp->curs_queue, dbc, links);
	DB_THREAD_UNLOCK(dbc->dbp);

	FREE(hcp, sizeof(HASH_CURSOR));
	FREE(dbc, sizeof(DBC));

	return (ret);
}

static int
__ham_c_del(cursor, flags)
	DBC *cursor;
	u_int32_t flags;
{
	DB *ldbp;
	HASH_CURSOR *hcp;
	HASH_CURSOR save_curs;
	HTAB *hashp;
	db_pgno_t ppgno, chg_pgno;
	int ret, t_ret;

	DEBUG_LWRITE(cursor->dbp, cursor->txn, "ham_c_del", NULL, NULL, flags);
	ldbp = cursor->dbp;
	if (F_ISSET(cursor->dbp, DB_AM_THREAD) &&
	    (ret = __db_gethandle(cursor->dbp, __ham_hdup, &ldbp)) != 0)
		return (ret);
	hashp = (HTAB *)ldbp->internal;
	hcp = (HASH_CURSOR *)cursor->internal;
	save_curs = *hcp;
	if ((ret = __db_cdelchk(ldbp, flags,
	    F_ISSET(ldbp, DB_AM_RDONLY), IS_VALID(hcp))) != 0)
		return (ret);
	if (F_ISSET(hcp, H_DELETED))
		return (DB_NOTFOUND);

	SET_LOCKER(hashp->dbp, cursor->txn);
	GET_META(hashp->dbp, hashp);
	hashp->hash_accesses++;
	if ((ret = __ham_get_cpage(hashp, hcp, DB_LOCK_WRITE)) != 0)
		goto out;
	if (F_ISSET(hcp, H_ISDUP) && hcp->dpgno != PGNO_INVALID) {
		/*
		 * We are about to remove a duplicate from offpage.
		 *
		 * There are 4 cases.
		 * 1. We will remove an item on a page, but there are more
		 *    items on that page.
		 * 2. We will remove the last item on a page, but there is a
		 *    following page of duplicates.
		 * 3. We will remove the last item on a page, this page was the
		 *    last page in a duplicate set, but there were dups before
		 *    it.
		 * 4. We will remove the last item on a page, removing the last
		 *    duplicate.
		 * In case 1 hcp->dpagep is unchanged.
		 * In case 2 hcp->dpagep comes back pointing to the next dup
		 *     page.
		 * In case 3 hcp->dpagep comes back NULL.
		 * In case 4 hcp->dpagep comes back NULL.
		 *
		 * Case 4 results in deleting the pair off the master page.
		 * The normal code for doing this knows how to delete the
		 * duplicates, so we will handle this case in the normal code.
		 */
		ppgno = PREV_PGNO(hcp->dpagep);
		if (ppgno == PGNO_INVALID &&
		    NEXT_PGNO(hcp->dpagep) == PGNO_INVALID &&
		    NUM_ENT(hcp->dpagep) == 1)
			goto normal;

		/* Remove item from duplicate page. */
		chg_pgno = hcp->dpgno;
		if ((ret = __db_drem(hashp->dbp,
		    &hcp->dpagep, hcp->dndx, __ham_del_page)) != 0)
			goto out;

		if (hcp->dpagep == NULL) {
			if (ppgno != PGNO_INVALID) {		/* Case 3 */
				hcp->dpgno = ppgno;
				if ((ret = __ham_get_cpage(hashp, hcp,
				    DB_LOCK_READ)) != 0)
					goto out;
				hcp->dndx = NUM_ENT(hcp->dpagep);
				F_SET(hcp, H_DELETED);
			} else {				/* Case 4 */
				ret = __ham_del_pair(hashp, hcp, 1);
				hcp->dpgno = PGNO_INVALID;
				/*
				 * Delpair updated the cursor queue, so we
				 * don't have to do that here.
				 */
				chg_pgno = PGNO_INVALID;
			}
		} else if (PGNO(hcp->dpagep) != hcp->dpgno) {
			hcp->dndx = 0;				/* Case 2 */
			hcp->dpgno = PGNO(hcp->dpagep);
			if (ppgno == PGNO_INVALID)
				memcpy(HOFFDUP_PGNO(P_ENTRY(hcp->pagep,
				    H_DATAINDEX(hcp->bndx))),
				    &hcp->dpgno, sizeof(db_pgno_t));
			F_SET(hcp, H_DELETED);
		} else						/* Case 1 */
			F_SET(hcp, H_DELETED);
		if (chg_pgno != PGNO_INVALID)
			__ham_c_update(hcp, chg_pgno, 0, 0, 1);
	} else if (F_ISSET(hcp, H_ISDUP)) {			/* on page */
		if (hcp->dup_off == 0 && DUP_SIZE(hcp->dup_len) ==
		    LEN_HDATA(hcp->pagep, hashp->hdr->pagesize, hcp->bndx))
			ret = __ham_del_pair(hashp, hcp, 1);
		else {
			DBT repldbt;

			repldbt.flags = 0;
			F_SET(&repldbt, DB_DBT_PARTIAL);
			repldbt.doff = hcp->dup_off;
			repldbt.dlen = DUP_SIZE(hcp->dup_len);
			repldbt.size = 0;
			ret = __ham_replpair(hashp, hcp, &repldbt, 0);
			hcp->dup_tlen -= DUP_SIZE(hcp->dup_len);
			F_SET(hcp, H_DELETED);
			__ham_c_update(hcp, hcp->pgno,
			    DUP_SIZE(hcp->dup_len), 0, 1);
		}

	} else
		/* Not a duplicate */
normal:		ret = __ham_del_pair(hashp, hcp, 1);

out:	if ((t_ret = __ham_item_done(hashp, hcp, ret == 0)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0)
		*hcp = save_curs;
	RELEASE_META(hashp->dbp, hashp);
	if (F_ISSET(cursor->dbp, DB_AM_THREAD))
		__db_puthandle(ldbp);
	return (ret);
}

static int
__ham_c_get(cursor, key, data, flags)
	DBC *cursor;
	DBT *key;
	DBT *data;
	u_int32_t flags;
{
	DB *ldbp;
	HTAB *hashp;
	HASH_CURSOR *hcp, save_curs;
	int get_key, ret, t_ret;

	DEBUG_LREAD(cursor->dbp, cursor->txn, "ham_c_get",
	    flags == DB_SET || flags == DB_SET_RANGE ? key : NULL,
	    NULL, flags);
	ldbp = cursor->dbp;
	if (F_ISSET(cursor->dbp, DB_AM_THREAD) &&
	    (ret = __db_gethandle(cursor->dbp, __ham_hdup, &ldbp)) != 0)
		return (ret);
	hashp = (HTAB *)(ldbp->internal);
	hcp = (HASH_CURSOR *)cursor->internal;
	save_curs = *hcp;
	if ((ret =
	    __db_cgetchk(hashp->dbp, key, data, flags, IS_VALID(hcp))) != 0)
		return (ret);

	SET_LOCKER(hashp->dbp, cursor->txn);
	GET_META(hashp->dbp, hashp);
	hashp->hash_accesses++;

	hcp->seek_size = 0;

	ret = 0;
	get_key = 1;
	switch (flags) {
	case DB_PREV:
		if (hcp->bucket != BUCKET_INVALID) {
			ret = __ham_item_prev(hashp, hcp, DB_LOCK_READ);
			break;
		}
		/* FALLTHROUGH */
	case DB_LAST:
		ret = __ham_item_last(hashp, hcp, DB_LOCK_READ);
		break;
	case DB_FIRST:
		ret = __ham_item_first(hashp, hcp, DB_LOCK_READ);
		break;
	case DB_NEXT:
		if (hcp->bucket == BUCKET_INVALID)
			hcp->bucket = 0;
		ret = __ham_item_next(hashp, hcp, DB_LOCK_READ);
		break;
	case DB_SET:
	case DB_SET_RANGE:
		ret = __ham_lookup(hashp, hcp, key, 0, DB_LOCK_READ);
		get_key = 0;
		break;
	case DB_CURRENT:
		if (F_ISSET(hcp, H_DELETED)) {
			ret = DB_KEYEMPTY;
			goto out;
		}

		ret = __ham_item(hashp, hcp, DB_LOCK_READ);
		break;
	}

	/*
	 * Must always enter this loop to do error handling and
	 * check for big key/data pair.
	 */
	while (1) {
		if (ret != 0 && ret != DB_NOTFOUND)
			goto out1;
		else if (F_ISSET(hcp, H_OK)) {
			/* Get the key. */
			if (get_key && (ret = __db_ret(hashp->dbp, hcp->pagep,
			    H_KEYINDEX(hcp->bndx), key, &hcp->big_key,
			    &hcp->big_keylen)) != 0)
				goto out1;

			ret = __ham_dup_return(hashp, hcp, data, flags);
			break;
		} else if (!F_ISSET(hcp, H_NOMORE)) {
			abort();
			break;
		}

		/*
		 * Ran out of entries in a bucket; change buckets.
		 */
		switch (flags) {
			case DB_LAST:
			case DB_PREV:
				ret = __ham_item_done(hashp, hcp, 0);
				if (hcp->bucket == 0) {
					ret = DB_NOTFOUND;
					goto out1;
				}
				hcp->bucket--;
				hcp->bndx = NDX_INVALID;
				if (ret == 0)
					ret = __ham_item_prev(hashp,
					    hcp, DB_LOCK_READ);
				break;
			case DB_FIRST:
			case DB_NEXT:
				ret = __ham_item_done(hashp, hcp, 0);
				hcp->bndx = NDX_INVALID;
				hcp->bucket++;
				hcp->pgno = PGNO_INVALID;
				hcp->pagep = NULL;
				if (hcp->bucket > hashp->hdr->max_bucket) {
					ret = DB_NOTFOUND;
					goto out1;
				}
				if (ret == 0)
					ret = __ham_item_next(hashp,
					    hcp, DB_LOCK_READ);
				break;
			case DB_SET:
			case DB_SET_RANGE:
				/* Key not found. */
				ret = DB_NOTFOUND;
				goto out1;
		}
	}
out1:	if ((t_ret = __ham_item_done(hashp, hcp, 0)) != 0 && ret == 0)
		ret = t_ret;
out:	if (ret)
		*hcp = save_curs;
	RELEASE_META(hashp->dbp, hashp);
	if (F_ISSET(cursor->dbp, DB_AM_THREAD))
		__db_puthandle(ldbp);
	return (ret);
}

static int
__ham_c_put(cursor, key, data, flags)
	DBC *cursor;
	DBT *key;
	DBT *data;
	u_int32_t flags;
{
	DB *ldbp;
	HASH_CURSOR *hcp, save_curs;
	HTAB *hashp;
	u_int32_t nbytes;
	int ret, t_ret;

	DEBUG_LWRITE(cursor->dbp, cursor->txn, "ham_c_put",
	    flags == DB_KEYFIRST || flags == DB_KEYLAST ? key : NULL,
	    data, flags);
	ldbp = cursor->dbp;
	if (F_ISSET(cursor->dbp, DB_AM_THREAD) &&
	    (ret = __db_gethandle(cursor->dbp, __ham_hdup, &ldbp)) != 0)
		return (ret);
	hashp = (HTAB *)(ldbp->internal);
	hcp = (HASH_CURSOR *)cursor->internal;
	save_curs = *hcp;

	if ((ret = __db_cputchk(hashp->dbp, key, data, flags,
	    F_ISSET(ldbp, DB_AM_RDONLY), IS_VALID(hcp))) != 0)
		return (ret);
	if (F_ISSET(hcp, H_DELETED))
		return (DB_NOTFOUND);

	SET_LOCKER(hashp->dbp, cursor->txn);
	GET_META(hashp->dbp, hashp);
	ret = 0;

	switch (flags) {
	case DB_KEYLAST:
	case DB_KEYFIRST:
		nbytes = (ISBIG(hashp, key->size) ? HOFFPAGE_PSIZE :
		    HKEYDATA_PSIZE(key->size)) +
		    (ISBIG(hashp, data->size) ? HOFFPAGE_PSIZE :
		    HKEYDATA_PSIZE(data->size));
		ret = __ham_lookup(hashp, hcp, key, nbytes, DB_LOCK_WRITE);
		break;
	case DB_BEFORE:
	case DB_AFTER:
	case DB_CURRENT:
		ret = __ham_item(hashp, hcp, DB_LOCK_WRITE);
		break;
	}

	if (ret == 0) {
		if (flags == DB_CURRENT && !F_ISSET(ldbp, DB_AM_DUP))
			ret = __ham_overwrite(hashp, hcp, data);
		else
			ret = __ham_add_dup(hashp, hcp, data, flags);
	}

	if (ret == 0 && F_ISSET(hcp, H_EXPAND)) {
		ret = __ham_expand_table(hashp);
		F_CLR(hcp, H_EXPAND);
	}

	if ((t_ret = __ham_item_done(hashp, hcp, ret == 0)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0)
		*hcp = save_curs;
	RELEASE_META(hashp->dbp, hashp);
	if (F_ISSET(cursor->dbp, DB_AM_THREAD))
		__db_puthandle(ldbp);
	return (ret);
}

/********************************* UTILITIES ************************/

/*
 * __ham_expand_table --
 *
 * PUBLIC: int __ham_expand_table __P((HTAB *));
 */
int
__ham_expand_table(hashp)
	HTAB *hashp;
{
	DB_LSN new_lsn;
	u_int32_t old_bucket, new_bucket, spare_ndx;
	int ret;

	ret = 0;
	DIRTY_META(hashp, ret);
	if (ret)
		return (ret);

	/*
	 * If the split point is about to increase, make sure that we
	 * have enough extra pages.  The calculation here is weird.
	 * We'd like to do this after we've upped max_bucket, but it's
	 * too late then because we've logged the meta-data split.  What
	 * we'll do between then and now is increment max bucket and then
	 * see what the log of one greater than that is; here we have to
	 * look at the log of max + 2.  VERY NASTY STUFF.
	 */
	if (__db_log2(hashp->hdr->max_bucket + 2) > hashp->hdr->ovfl_point) {
		/*
		 * We are about to shift the split point.  Make sure that
		 * if the next doubling is going to be big (more than 8
		 * pages), we have some extra pages around.
		 */
		if (hashp->hdr->max_bucket + 1 >= 8 &&
		    hashp->hdr->spares[hashp->hdr->ovfl_point] <
		    hashp->hdr->spares[hashp->hdr->ovfl_point - 1] +
		    hashp->hdr->ovfl_point + 1)
			__ham_init_ovflpages(hashp);
	}

	/* Now we can log the meta-data split. */
	if (DB_LOGGING(hashp->dbp)) {
		if ((ret = __ham_splitmeta_log(hashp->dbp->dbenv->lg_info,
		    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0,
		    hashp->dbp->log_fileid,
		    hashp->hdr->max_bucket, hashp->hdr->ovfl_point,
		    hashp->hdr->spares[hashp->hdr->ovfl_point],
		    &hashp->hdr->lsn)) != 0)
			return (ret);

		hashp->hdr->lsn = new_lsn;
	}

	hashp->hash_expansions++;
	new_bucket = ++hashp->hdr->max_bucket;
	old_bucket = (hashp->hdr->max_bucket & hashp->hdr->low_mask);

	/*
	 * If the split point is increasing, copy the current contents
	 * of the spare split bucket to the next bucket.
	 */
	spare_ndx = __db_log2(hashp->hdr->max_bucket + 1);
	if (spare_ndx > hashp->hdr->ovfl_point) {
		hashp->hdr->spares[spare_ndx] =
		    hashp->hdr->spares[hashp->hdr->ovfl_point];
		hashp->hdr->ovfl_point = spare_ndx;
	}

	if (new_bucket > hashp->hdr->high_mask) {
		/* Starting a new doubling */
		hashp->hdr->low_mask = hashp->hdr->high_mask;
		hashp->hdr->high_mask = new_bucket | hashp->hdr->low_mask;
	}

	if (BUCKET_TO_PAGE(hashp, new_bucket) > MAX_PAGES(hashp)) {
		__db_err(hashp->dbp->dbenv,
		    "hash: Cannot allocate new bucket.  Pages exhausted.");
		return (ENOSPC);
	}

	/* Relocate records to the new bucket */
	return (__ham_split_page(hashp, old_bucket, new_bucket));
}

/*
 * PUBLIC: u_int32_t __ham_call_hash __P((HTAB *, u_int8_t *, int32_t));
 */
u_int32_t
__ham_call_hash(hashp, k, len)
	HTAB *hashp;
	u_int8_t *k;
	int32_t len;
{
	u_int32_t n, bucket;

	n = (u_int32_t)hashp->hash(k, len);
	bucket = n & hashp->hdr->high_mask;
	if (bucket > hashp->hdr->max_bucket)
		bucket = bucket & hashp->hdr->low_mask;
	return (bucket);
}

/*
 * Check for duplicates, and call __db_ret appropriately.  Release
 * everything held by the cursor.
 */
static int
__ham_dup_return(hashp, hcp, val, flags)
	HTAB *hashp;
	HASH_CURSOR *hcp;
	DBT *val;
	u_int32_t flags;
{
	PAGE *pp;
	DBT *myval, tmp_val;
	db_indx_t ndx;
	db_pgno_t pgno;
	u_int8_t *hk, type;
	int ret;
	db_indx_t len;

	/* Check for duplicate and return the first one. */
	ndx = H_DATAINDEX(hcp->bndx);
	type = HPAGE_TYPE(hcp->pagep, ndx);
	pp = hcp->pagep;
	myval = val;

	/*
	 * There are 3 cases:
	 * 1. We are not in duplicate, simply call db_ret.
	 * 2. We are looking at keys and stumbled onto a duplicate.
	 * 3. We are in the middle of a duplicate set. (ISDUP set)
	 */

	/*
	 * Here we check for the case where we just stumbled onto a
	 * duplicate.  In this case, we do initialization and then
	 * let the normal duplicate code handle it.
	 */
	if (!F_ISSET(hcp, H_ISDUP))
		if (type == H_DUPLICATE) {
			F_SET(hcp, H_ISDUP);
			hcp->dup_tlen = LEN_HDATA(hcp->pagep,
			    hashp->hdr->pagesize, hcp->bndx);
			hk = H_PAIRDATA(hcp->pagep, hcp->bndx);
			if (flags == DB_LAST || flags == DB_PREV) {
				hcp->dndx = 0;
				hcp->dup_off = 0;
				do {
					memcpy(&len,
					    HKEYDATA_DATA(hk) + hcp->dup_off,
					    sizeof(db_indx_t));
					hcp->dup_off += DUP_SIZE(len);
					hcp->dndx++;
				} while (hcp->dup_off < hcp->dup_tlen);
				hcp->dup_off -= DUP_SIZE(len);
				hcp->dndx--;
			} else {
				memcpy(&len,
				    HKEYDATA_DATA(hk), sizeof(db_indx_t));
				hcp->dup_off = 0;
				hcp->dndx = 0;
			}
			hcp->dup_len = len;
		} else if (type == H_OFFDUP) {
			F_SET(hcp, H_ISDUP);
			memcpy(&pgno, HOFFDUP_PGNO(P_ENTRY(hcp->pagep, ndx)),
			    sizeof(db_pgno_t));
			if (flags == DB_LAST || flags == DB_PREV) {
				if ((ret = __db_dend(hashp->dbp,
				    pgno, &hcp->dpagep)) != 0)
					return (ret);
				hcp->dpgno = PGNO(hcp->dpagep);
				hcp->dndx = NUM_ENT(hcp->dpagep) - 1;
			} else if ((ret = __ham_next_cpage(hashp,
			    hcp, pgno, 0, H_ISDUP)) != 0)
				return (ret);
		}


	/*
	 * Now, everything is initialized, grab a duplicate if
	 * necessary.
	 */
	if (F_ISSET(hcp, H_ISDUP))
		if (hcp->dpgno != PGNO_INVALID) {
			pp = hcp->dpagep;
			ndx = hcp->dndx;
		} else {
			/*
			 * Copy the DBT in case we are retrieving into
			 * user memory and we need the parameters for
			 * it.
			 */
			memcpy(&tmp_val, val, sizeof(*val));
			F_SET(&tmp_val, DB_DBT_PARTIAL);
			tmp_val.dlen = hcp->dup_len;
			tmp_val.doff = hcp->dup_off + sizeof(db_indx_t);
			myval = &tmp_val;
		}


	/*
	 * Finally, if we had a duplicate, pp, ndx, and myval should be
	 * set appropriately.
	 */
	if ((ret = __db_ret(hashp->dbp, pp, ndx, myval, &hcp->big_data,
	    &hcp->big_datalen)) != 0)
		return (ret);

	/*
	 * In case we sent a temporary off to db_ret, set the real
	 * return values.
	 */
	val->data = myval->data;
	val->size = myval->size;

	return (0);
}

static int
__ham_overwrite(hashp, hcp, nval)
	HTAB *hashp;
	HASH_CURSOR *hcp;
	DBT *nval;
{
	DBT *myval, tmp_val;
	u_int8_t *hk;

	if (F_ISSET(hashp->dbp, DB_AM_DUP))
		return (__ham_add_dup(hashp, hcp, nval, DB_KEYLAST));
	else if (!F_ISSET(nval, DB_DBT_PARTIAL)) {
		/* Put/overwrite */
		memcpy(&tmp_val, nval, sizeof(*nval));
		F_SET(&tmp_val, DB_DBT_PARTIAL);
		tmp_val.doff = 0;
		hk = H_PAIRDATA(hcp->pagep, hcp->bndx);
		if (HPAGE_PTYPE(hk) == H_OFFPAGE)
			memcpy(&tmp_val.dlen,
			    HOFFPAGE_TLEN(hk), sizeof(u_int32_t));
		else
			tmp_val.dlen = LEN_HDATA(hcp->pagep,
			    hashp->hdr->pagesize,hcp->bndx);
		myval = &tmp_val;
	} else /* Regular partial put */
		myval = nval;

	return (__ham_replpair(hashp, hcp, myval, 0));
}

/*
 * Given a key and a cursor, sets the cursor to the page/ndx on which
 * the key resides.  If the key is found, the cursor H_OK flag is set
 * and the pagep, bndx, pgno (dpagep, dndx, dpgno) fields are set.
 * If the key is not found, the H_OK flag is not set.  If the sought
 * field is non-0, the pagep, bndx, pgno (dpagep, dndx, dpgno) fields
 * are set indicating where an add might take place.  If it is 0,
 * non of the cursor pointer field are valid.
 */
static int
__ham_lookup(hashp, hcp, key, sought, mode)
	HTAB *hashp;
	HASH_CURSOR *hcp;
	const DBT *key;
	u_int32_t sought;
	db_lockmode_t mode;
{
	db_pgno_t pgno;
	u_int32_t tlen;
	int match, ret, t_ret;
	u_int8_t *hk;

	/*
	 * Set up cursor so that we're looking for space to add an item
	 * as we cycle through the pages looking for the key.
	 */
	if ((ret = __ham_item_reset(hashp, hcp)) != 0)
		return (ret);
	hcp->seek_size = sought;

	hcp->bucket = __ham_call_hash(hashp, (u_int8_t *)key->data, key->size);
	while (1) {
		if ((ret = __ham_item_next(hashp, hcp, mode)) != 0)
			return (ret);

		if (F_ISSET(hcp, H_NOMORE))
			break;

		hk = H_PAIRKEY(hcp->pagep, hcp->bndx);
		switch (HPAGE_PTYPE(hk)) {
		case H_OFFPAGE:
			memcpy(&tlen, HOFFPAGE_TLEN(hk), sizeof(u_int32_t));
			if (tlen == key->size) {
				memcpy(&pgno,
				    HOFFPAGE_PGNO(hk), sizeof(db_pgno_t));
				match = __db_moff(hashp->dbp, key, pgno);
				if (match == 0) {
					F_SET(hcp, H_OK);
					return (0);
				}
			}
			break;
		case H_KEYDATA:
			if (key->size == LEN_HKEY(hcp->pagep,
			    hashp->hdr->pagesize, hcp->bndx) &&
			    memcmp(key->data,
			    HKEYDATA_DATA(hk), key->size) == 0) {
				F_SET(hcp, H_OK);
				return (0);
			}
			break;
		case H_DUPLICATE:
		case H_OFFDUP:
			/*
			 * These are errors because keys are never
			 * duplicated, only data items are.
			 */
			return (__db_pgfmt(hashp->dbp, PGNO(hcp->pagep)));
		}
		hashp->hash_collisions++;
	}

	/*
	 * Item was not found, adjust cursor properly.
	 */

	if (sought != 0)
		return (ret);

	if ((t_ret = __ham_item_done(hashp, hcp, 0)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * Initialize a dbt using some possibly already allocated storage
 * for items.
 * PUBLIC: int __ham_init_dbt __P((DBT *, u_int32_t, void **, u_int32_t *));
 */
int
__ham_init_dbt(dbt, size, bufp, sizep)
	DBT *dbt;
	u_int32_t size;
	void **bufp;
	u_int32_t *sizep;
{
	memset(dbt, 0, sizeof(*dbt));
	if (*sizep < size) {
		if ((*bufp = (void *)(*bufp == NULL ?
		    __db_malloc(size) : __db_realloc(*bufp, size))) == NULL) {
			*sizep = 0;
			return (ENOMEM);
		}
		*sizep = size;
	}
	dbt->data = *bufp;
	dbt->size = size;
	return (0);
}

/*
 * Adjust the cursor after an insert or delete.  The cursor passed is
 * the one that was operated upon; we just need to check any of the
 * others.
 *
 * len indicates the length of the item added/deleted
 * add indicates if the item indicated by the cursor has just been
 * added (add == 1) or deleted (add == 0).
 * dup indicates if the addition occurred into a duplicate set.
 *
 * PUBLIC: void __ham_c_update
 * PUBLIC:    __P((HASH_CURSOR *, db_pgno_t, u_int32_t, int, int));
 */
void
__ham_c_update(hcp, chg_pgno, len, add, is_dup)
	HASH_CURSOR *hcp;
	db_pgno_t chg_pgno;
	u_int32_t len;
	int add, is_dup;
{
	DBC *cp;
	HTAB *hp;
	HASH_CURSOR *lcp;
	int page_deleted;

	/*
	 * Regular adds are always at the end of a given page, so we never
	 * have to adjust anyone's cursor after a regular add.
	 */
	if (!is_dup && add)
		return;

	/*
	 * Determine if a page was deleted.    If this is a regular update
	 * (i.e., not is_dup) then the deleted page's number will be that in
	 * chg_pgno, and the pgno in the cursor will be different.  If this
	 * was an onpage-duplicate, then the same conditions apply.  If this
	 * was an off-page duplicate, then we need to verify if hcp->dpgno
	 * is the same (no delete) or different (delete) than chg_pgno.
	 */
	if (!is_dup || hcp->dpgno == PGNO_INVALID)
		page_deleted =
		    chg_pgno != PGNO_INVALID && chg_pgno != hcp->pgno;
	else
		page_deleted =
		    chg_pgno != PGNO_INVALID && chg_pgno != hcp->dpgno;

	hp = hcp->db_cursor->dbp->master->internal;
	DB_THREAD_LOCK(hp->dbp);

	for (cp = TAILQ_FIRST(&hp->dbp->curs_queue); cp != NULL;
	    cp = TAILQ_NEXT(cp, links)) {
		if (cp->internal == hcp)
			continue;

		lcp = (HASH_CURSOR *)cp->internal;

		if (!is_dup && lcp->pgno != chg_pgno)
			continue;

		if (is_dup) {
			if (F_ISSET(hcp, H_DELETED) && lcp->pgno != chg_pgno)
				continue;
			if (!F_ISSET(hcp, H_DELETED) && lcp->dpgno != chg_pgno)
				continue;
		}

		if (page_deleted) {
			if (is_dup) {
				lcp->dpgno = hcp->dpgno;
				lcp->dndx = hcp->dndx;
			} else {
				lcp->pgno = hcp->pgno;
				lcp->bndx = hcp->bndx;
				lcp->bucket = hcp->bucket;
			}
			F_CLR(lcp, H_ISDUP);
			continue;
		}

		if (!is_dup && lcp->bndx > hcp->bndx)
			lcp->bndx--;
		else if (!is_dup && lcp->bndx == hcp->bndx)
			F_SET(lcp, H_DELETED);
		else if (is_dup && lcp->bndx == hcp->bndx) {
			/* Assign dpgno in case there was page conversion. */
			lcp->dpgno = hcp->dpgno;
			if (add && lcp->dndx >= hcp->dndx )
				lcp->dndx++;
			else if (!add && lcp->dndx > hcp->dndx)
				lcp->dndx--;
			else if (!add && lcp->dndx == hcp->dndx)
				F_SET(lcp, H_DELETED);

			/* Now adjust on-page information. */
			if (lcp->dpgno == PGNO_INVALID)
				if (add) {
					lcp->dup_tlen += len;
					if (lcp->dndx > hcp->dndx)
						lcp->dup_off += len;
				} else {
					lcp->dup_tlen -= len;
					if (lcp->dndx > hcp->dndx)
						lcp->dup_off -= len;
				}
		}
	}
	DB_THREAD_UNLOCK(hp->dbp);
}

/*
 * __ham_hdup --
 *	This function gets called when we create a duplicate handle for a
 *	threaded DB.  It should create the private part of the DB structure.
 *
 * PUBLIC: int  __ham_hdup __P((DB *, DB *));
 */
int
__ham_hdup(orig, new)
	DB *orig, *new;
{
	DBC *curs;
	HTAB *hashp;
	int ret;

	if ((hashp = (HTAB *)__db_malloc(sizeof(HTAB))) == NULL)
		return (ENOMEM);

	new->internal = hashp;

	hashp->dbp = new;
	hashp->hlock = 0;
	hashp->hdr = NULL;
	hashp->hash = ((HTAB *)orig->internal)->hash;
	if ((hashp->split_buf = (PAGE *)__db_malloc(orig->pgsize)) == NULL)
		return (ENOMEM);
	hashp->local_errno = 0;
	hashp->hash_accesses = 0;
	hashp->hash_collisions = 0;
	hashp->hash_expansions = 0;
	hashp->hash_overflows = 0;
	hashp->hash_bigpages = 0;
	/* Initialize the cursor queue. */
	ret = __ham_c_init(new, NULL, &curs);
	TAILQ_INSERT_TAIL(&new->curs_queue, curs, links);
	return (ret);
}
