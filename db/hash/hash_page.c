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
static const char sccsid[] = "@(#)hash_page.c	10.40 (Sleepycat) 6/2/98";
#endif /* not lint */

/*
 * PACKAGE:  hashing
 *
 * DESCRIPTION:
 *      Page manipulation for hashing package.
 *
 * ROUTINES:
 *
 * External
 *      __get_page
 *      __add_ovflpage
 *      __overflow_page
 * Internal
 *      open_temp
 */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "hash.h"

static int __ham_lock_bucket __P((DB *, HASH_CURSOR *, db_lockmode_t));

#ifdef DEBUG_SLOW
static void  __account_page(HTAB *, db_pgno_t, int);
#endif

/*
 * PUBLIC: int __ham_item __P((HTAB *, HASH_CURSOR *, db_lockmode_t));
 */
int
__ham_item(hashp, cursorp, mode)
	HTAB *hashp;
	HASH_CURSOR *cursorp;
	db_lockmode_t mode;
{
	db_pgno_t next_pgno;
	int ret;

	if (F_ISSET(cursorp, H_DELETED))
		return (EINVAL);
	F_CLR(cursorp, H_OK | H_NOMORE);

	/* Check if we need to get a page for this cursor. */
	if ((ret = __ham_get_cpage(hashp, cursorp, mode)) != 0)
		return (ret);

	/* Check if we are looking for space in which to insert an item. */
	if (cursorp->seek_size && cursorp->seek_found_page == PGNO_INVALID
	    && cursorp->seek_size < P_FREESPACE(cursorp->pagep))
		cursorp->seek_found_page = cursorp->pgno;

	/* Check if we need to go on to the next page. */
	if (F_ISSET(cursorp, H_ISDUP) && cursorp->dpgno == PGNO_INVALID)
		/*
		 * ISDUP is set, and offset is at the beginning of the datum.
		 * We need to grab the length of the datum, then set the datum
		 * pointer to be the beginning of the datum.
		 */
		memcpy(&cursorp->dup_len,
		    HKEYDATA_DATA(H_PAIRDATA(cursorp->pagep, cursorp->bndx)) +
		    cursorp->dup_off, sizeof(db_indx_t));
	else if (F_ISSET(cursorp, H_ISDUP)) {
		/* Make sure we're not about to run off the page. */
		if (cursorp->dpagep == NULL && (ret = __ham_get_page(hashp->dbp,
		    cursorp->dpgno, &cursorp->dpagep)) != 0)
			return (ret);

		if (cursorp->dndx >= NUM_ENT(cursorp->dpagep)) {
			if (NEXT_PGNO(cursorp->dpagep) == PGNO_INVALID) {
				if ((ret = __ham_put_page(hashp->dbp,
				    cursorp->dpagep, 0)) != 0)
					return (ret);
				F_CLR(cursorp, H_ISDUP);
				cursorp->dpagep = NULL;
				cursorp->dpgno = PGNO_INVALID;
				cursorp->dndx = NDX_INVALID;
				cursorp->bndx++;
			} else if ((ret = __ham_next_cpage(hashp, cursorp,
			    NEXT_PGNO(cursorp->dpagep), 0, H_ISDUP)) != 0)
				return (ret);
		}
	}

	if (cursorp->bndx >= (db_indx_t)H_NUMPAIRS(cursorp->pagep)) {
		/* Fetch next page. */
		if (NEXT_PGNO(cursorp->pagep) == PGNO_INVALID) {
			F_SET(cursorp, H_NOMORE);
			if (cursorp->dpagep != NULL &&
			    (ret = __ham_put_page(hashp->dbp,
			    cursorp->dpagep, 0)) != 0)
				return (ret);
			cursorp->dpgno = PGNO_INVALID;
			return (DB_NOTFOUND);
		}
		next_pgno = NEXT_PGNO(cursorp->pagep);
		cursorp->bndx = 0;
		if ((ret = __ham_next_cpage(hashp,
		    cursorp, next_pgno, 0, 0)) != 0)
			return (ret);
	}

	F_SET(cursorp, H_OK);
	return (0);
}

/*
 * PUBLIC: int __ham_item_reset __P((HTAB *, HASH_CURSOR *));
 */
int
__ham_item_reset(hashp, cursorp)
	HTAB *hashp;
	HASH_CURSOR *cursorp;
{
	int ret;

	if (cursorp->pagep)
		ret = __ham_put_page(hashp->dbp, cursorp->pagep, 0);
	else
		ret = 0;

	__ham_item_init(cursorp);
	return (ret);
}

/*
 * PUBLIC: void __ham_item_init __P((HASH_CURSOR *));
 */
void
__ham_item_init(cursorp)
	HASH_CURSOR *cursorp;
{
	cursorp->pagep = NULL;
	cursorp->bucket = BUCKET_INVALID;
	cursorp->lock = 0;
	cursorp->bndx = NDX_INVALID;
	cursorp->pgno = PGNO_INVALID;
	cursorp->dpgno = PGNO_INVALID;
	cursorp->dndx = NDX_INVALID;
	cursorp->dpagep = NULL;
	cursorp->flags = 0;
	cursorp->seek_size = 0;
	cursorp->seek_found_page = PGNO_INVALID;
}

/*
 * PUBLIC: int __ham_item_done __P((HTAB *, HASH_CURSOR *, int));
 */
int
__ham_item_done(hashp, cursorp, dirty)
	HTAB *hashp;
	HASH_CURSOR *cursorp;
	int dirty;
{
	int ret, t_ret;

	t_ret = ret = 0;

	if (cursorp->pagep)
		ret = __ham_put_page(hashp->dbp, cursorp->pagep,
		    dirty && cursorp->dpagep == NULL);
	cursorp->pagep = NULL;

	if (cursorp->dpagep)
		t_ret = __ham_put_page(hashp->dbp, cursorp->dpagep, dirty);
	cursorp->dpagep = NULL;

	if (ret == 0 && t_ret != 0)
		ret = t_ret;

	/*
	 * If we are running with transactions, then we must
	 * not relinquish locks explicitly.
	 */
	if (cursorp->lock && hashp->dbp->txn == NULL)
	    t_ret = lock_put(hashp->dbp->dbenv->lk_info, cursorp->lock);
	cursorp->lock = 0;


	/*
	 * We don't throw out the page number since we might want to
	 * continue getting on this page.
	 */
	return (ret != 0 ? ret : t_ret);
}

/*
 * Returns the last item in a bucket.
 *
 * PUBLIC: int __ham_item_last __P((HTAB *, HASH_CURSOR *, db_lockmode_t));
 */
int
__ham_item_last(hashp, cursorp, mode)
	HTAB *hashp;
	HASH_CURSOR *cursorp;
	db_lockmode_t mode;
{
	int ret;

	if ((ret = __ham_item_reset(hashp, cursorp)) != 0)
		return (ret);

	cursorp->bucket = hashp->hdr->max_bucket;
	F_SET(cursorp, H_OK);
	return (__ham_item_prev(hashp, cursorp, mode));
}

/*
 * PUBLIC: int __ham_item_first __P((HTAB *, HASH_CURSOR *, db_lockmode_t));
 */
int
__ham_item_first(hashp, cursorp, mode)
	HTAB *hashp;
	HASH_CURSOR *cursorp;
	db_lockmode_t mode;
{
	int ret;

	if ((ret = __ham_item_reset(hashp, cursorp)) != 0)
		return (ret);
	F_SET(cursorp, H_OK);
	cursorp->bucket = 0;
	return (__ham_item_next(hashp, cursorp, mode));
}

/*
 * __ham_item_prev --
 *	Returns a pointer to key/data pair on a page.  In the case of
 *	bigkeys, just returns the page number and index of the bigkey
 *	pointer pair.
 *
 * PUBLIC: int __ham_item_prev __P((HTAB *, HASH_CURSOR *, db_lockmode_t));
 */
int
__ham_item_prev(hashp, cursorp, mode)
	HTAB *hashp;
	HASH_CURSOR *cursorp;
	db_lockmode_t mode;
{
	db_pgno_t next_pgno;
	int ret;

	/*
	 * There are N cases for backing up in a hash file.
	 * Case 1: In the middle of a page, no duplicates, just dec the index.
	 * Case 2: In the middle of a duplicate set, back up one.
	 * Case 3: At the beginning of a duplicate set, get out of set and
	 *	back up to next key.
	 * Case 4: At the beginning of a page; go to previous page.
	 * Case 5: At the beginning of a bucket; go to prev bucket.
	 */
	F_CLR(cursorp, H_OK | H_NOMORE | H_DELETED);

	/*
	 * First handle the duplicates.  Either you'll get the key here
	 * or you'll exit the duplicate set and drop into the code below
	 * to handle backing up through keys.
	 */
	if (F_ISSET(cursorp, H_ISDUP)) {
		if (cursorp->dpgno == PGNO_INVALID) {
			/* Duplicates are on-page. */
			if (cursorp->dup_off != 0)
				if ((ret = __ham_get_cpage(hashp,
				    cursorp, mode)) != 0)
					return (ret);
				else {
					HASH_CURSOR *h;
					h = cursorp;
					memcpy(&h->dup_len, HKEYDATA_DATA(
					    H_PAIRDATA(h->pagep, h->bndx))
					    + h->dup_off - sizeof(db_indx_t),
					    sizeof(db_indx_t));
					cursorp->dup_off -=
					    DUP_SIZE(cursorp->dup_len);
					cursorp->dndx--;
					return (__ham_item(hashp,
					    cursorp, mode));
				}
		} else if (cursorp->dndx > 0) {	/* Duplicates are off-page. */
			cursorp->dndx--;
			return (__ham_item(hashp, cursorp, mode));
		} else if ((ret = __ham_get_cpage(hashp, cursorp, mode)) != 0)
			return (ret);
		else if (PREV_PGNO(cursorp->dpagep) == PGNO_INVALID) {
			F_CLR(cursorp, H_ISDUP); /* End of dups */
			cursorp->dpgno = PGNO_INVALID;
			if (cursorp->dpagep != NULL)
				(void)__ham_put_page(hashp->dbp,
				    cursorp->dpagep, 0);
			cursorp->dpagep = NULL;
		} else if ((ret = __ham_next_cpage(hashp, cursorp,
		    PREV_PGNO(cursorp->dpagep), 0, H_ISDUP)) != 0)
			return (ret);
		else {
			cursorp->dndx = NUM_ENT(cursorp->pagep) - 1;
			return (__ham_item(hashp, cursorp, mode));
		}
	}

	/*
	 * If we get here, we are not in a duplicate set, and just need
	 * to back up the cursor.  There are still three cases:
	 * midpage, beginning of page, beginning of bucket.
	 */

	if (cursorp->bndx == 0) { 		/* Beginning of page. */
		if ((ret = __ham_get_cpage(hashp, cursorp, mode)) != 0)
			return (ret);
		cursorp->pgno = PREV_PGNO(cursorp->pagep);
		if (cursorp->pgno == PGNO_INVALID) {
			/* Beginning of bucket. */
			F_SET(cursorp, H_NOMORE);
			return (DB_NOTFOUND);
		} else if ((ret = __ham_next_cpage(hashp,
		    cursorp, cursorp->pgno, 0, 0)) != 0)
			return (ret);
		else
			cursorp->bndx = H_NUMPAIRS(cursorp->pagep);
	}

	/*
	 * Either we've got the cursor set up to be decremented, or we
	 * have to find the end of a bucket.
	 */
	if (cursorp->bndx == NDX_INVALID) {
		if (cursorp->pagep == NULL)
			next_pgno = BUCKET_TO_PAGE(hashp, cursorp->bucket);
		else
			goto got_page;

		do {
			if ((ret = __ham_next_cpage(hashp,
			    cursorp, next_pgno, 0, 0)) != 0)
				return (ret);
got_page:		next_pgno = NEXT_PGNO(cursorp->pagep);
			cursorp->bndx = H_NUMPAIRS(cursorp->pagep);
		} while (next_pgno != PGNO_INVALID);

		if (cursorp->bndx == 0) {
			/* Bucket was empty. */
			F_SET(cursorp, H_NOMORE);
			return (DB_NOTFOUND);
		}
	}

	cursorp->bndx--;

	return (__ham_item(hashp, cursorp, mode));
}

/*
 * Sets the cursor to the next key/data pair on a page.
 *
 * PUBLIC: int __ham_item_next __P((HTAB *, HASH_CURSOR *, db_lockmode_t));
 */
int
__ham_item_next(hashp, cursorp, mode)
	HTAB *hashp;
	HASH_CURSOR *cursorp;
	db_lockmode_t mode;
{
	/*
	 * Deleted on-page duplicates are a weird case. If we delete the last
	 * one, then our cursor is at the very end of a duplicate set and
	 * we actually need to go on to the next key.
	 */
	if (F_ISSET(cursorp, H_DELETED)) {
		if (cursorp->bndx != NDX_INVALID &&
		    F_ISSET(cursorp, H_ISDUP) &&
		    cursorp->dpgno == PGNO_INVALID &&
		    cursorp->dup_tlen == cursorp->dup_off) {
			F_CLR(cursorp, H_ISDUP);
			cursorp->dpgno = PGNO_INVALID;
			cursorp->bndx++;
		}
		F_CLR(cursorp, H_DELETED);
	} else if (cursorp->bndx == NDX_INVALID) {
		cursorp->bndx = 0;
		cursorp->dpgno = PGNO_INVALID;
		F_CLR(cursorp, H_ISDUP);
	} else if (F_ISSET(cursorp, H_ISDUP) && cursorp->dpgno != PGNO_INVALID)
		cursorp->dndx++;
	else if (F_ISSET(cursorp, H_ISDUP)) {
		cursorp->dndx++;
		cursorp->dup_off += DUP_SIZE(cursorp->dup_len);
		if (cursorp->dup_off >= cursorp->dup_tlen) {
			F_CLR(cursorp, H_ISDUP);
			cursorp->dpgno = PGNO_INVALID;
			cursorp->bndx++;
		}
	} else
		cursorp->bndx++;

	return (__ham_item(hashp, cursorp, mode));
}

/*
 * PUBLIC: void __ham_putitem __P((PAGE *p, const DBT *, int));
 *
 * This is a little bit sleazy in that we're overloading the meaning
 * of the H_OFFPAGE type here.  When we recover deletes, we have the
 * entire entry instead of having only the DBT, so we'll pass type
 * H_OFFPAGE to mean, "copy the whole entry" as opposed to constructing
 * an H_KEYDATA around it.
 */
void
__ham_putitem(p, dbt, type)
	PAGE *p;
	const DBT *dbt;
	int type;
{
	u_int16_t n, off;

	n = NUM_ENT(p);

	/* Put the item element on the page. */
	if (type == H_OFFPAGE) {
		off = HOFFSET(p) - dbt->size;
		HOFFSET(p) = p->inp[n] = off;
		memcpy(P_ENTRY(p, n), dbt->data, dbt->size);
	} else {
		off = HOFFSET(p) - HKEYDATA_SIZE(dbt->size);
		HOFFSET(p) = p->inp[n] = off;
		PUT_HKEYDATA(P_ENTRY(p, n), dbt->data, dbt->size, type);
	}

	/* Adjust page info. */
	NUM_ENT(p) += 1;
}

/*
 * PUBLIC: void __ham_reputpair
 * PUBLIC:    __P((PAGE *p, u_int32_t, u_int32_t, const DBT *, const DBT *));
 *
 * This is a special case to restore a key/data pair to its original
 * location during recovery.  We are guaranteed that the pair fits
 * on the page and is not the last pair on the page (because if it's
 * the last pair, the normal insert works).
 */
void
__ham_reputpair(p, psize, ndx, key, data)
	PAGE *p;
	u_int32_t psize, ndx;
	const DBT *key, *data;
{
	db_indx_t i, movebytes, newbytes;
	u_int8_t *from;

	/* First shuffle the existing items up on the page.  */
	movebytes =
	    (ndx == 0 ? psize : p->inp[H_DATAINDEX(ndx - 1)]) - HOFFSET(p);
	newbytes = key->size + data->size;
	from = (u_int8_t *)p + HOFFSET(p);
	memmove(from - newbytes, from, movebytes);

	/*
	 * Adjust the indices and move them up 2 spaces. Note that we
	 * have to check the exit condition inside the loop just in case
	 * we are dealing with index 0 (db_indx_t's are unsigned).
	 */
	for (i = NUM_ENT(p) - 1; ; i-- ) {
		p->inp[i + 2] = p->inp[i] - newbytes;
		if (i == H_KEYINDEX(ndx))
			break;
	}

	/* Put the key and data on the page. */
	p->inp[H_KEYINDEX(ndx)] =
	    (ndx == 0 ? psize : p->inp[H_DATAINDEX(ndx - 1)]) - key->size;
	p->inp[H_DATAINDEX(ndx)] = p->inp[H_KEYINDEX(ndx)] - data->size;
	memcpy(P_ENTRY(p, H_KEYINDEX(ndx)), key->data, key->size);
	memcpy(P_ENTRY(p, H_DATAINDEX(ndx)), data->data, data->size);

	/* Adjust page info. */
	HOFFSET(p) -= newbytes;
	NUM_ENT(p) += 2;
}


/*
 * PUBLIC: int __ham_del_pair __P((HTAB *, HASH_CURSOR *, int));
 *
 * XXX
 * TODO: if the item is an offdup, delete the other pages and then remove
 * the pair. If the offpage page is 0, then you can just remove the pair.
 */
int
__ham_del_pair(hashp, cursorp, reclaim_page)
	HTAB *hashp;
	HASH_CURSOR *cursorp;
	int reclaim_page;
{
	DBT data_dbt, key_dbt;
	DB_ENV *dbenv;
	DB_LSN new_lsn, *n_lsn, tmp_lsn;
	PAGE *p;
	db_indx_t ndx;
	db_pgno_t chg_pgno, pgno;
	int ret, tret;

	dbenv = hashp->dbp->dbenv;
	ndx = cursorp->bndx;
	if (cursorp->pagep == NULL && (ret =
	    __ham_get_page(hashp->dbp, cursorp->pgno, &cursorp->pagep)) != 0)
		return (ret);

	p = cursorp->pagep;

	/*
	 * We optimize for the normal case which is when neither the key nor
	 * the data are large.  In this case, we write a single log record
	 * and do the delete.  If either is large, we'll call __big_delete
	 * to remove the big item and then update the page to remove the
	 * entry referring to the big item.
	 */
	ret = 0;
	if (HPAGE_PTYPE(H_PAIRKEY(p, ndx)) == H_OFFPAGE) {
		memcpy(&pgno, HOFFPAGE_PGNO(P_ENTRY(p, H_KEYINDEX(ndx))),
		    sizeof(db_pgno_t));
		ret = __db_doff(hashp->dbp, pgno, __ham_del_page);
	}

	if (ret == 0)
		switch (HPAGE_PTYPE(H_PAIRDATA(p, ndx))) {
		case H_OFFPAGE:
			memcpy(&pgno,
			    HOFFPAGE_PGNO(P_ENTRY(p, H_DATAINDEX(ndx))),
			    sizeof(db_pgno_t));
			ret = __db_doff(hashp->dbp, pgno, __ham_del_page);
			break;
		case H_OFFDUP:
			memcpy(&pgno,
			    HOFFDUP_PGNO(P_ENTRY(p, H_DATAINDEX(ndx))),
			    sizeof(db_pgno_t));
			ret = __db_ddup(hashp->dbp, pgno, __ham_del_page);
			F_CLR(cursorp, H_ISDUP);
			break;
		case H_DUPLICATE:
			/*
			 * If we delete a pair that is/was a duplicate, then
			 * we had better clear the flag so that we update the
			 * cursor appropriately.
			 */
			F_CLR(cursorp, H_ISDUP);
			break;
		}

	if (ret)
		return (ret);

	/* Now log the delete off this page. */
	if (DB_LOGGING(hashp->dbp)) {
		key_dbt.data = P_ENTRY(p, H_KEYINDEX(ndx));
		key_dbt.size =
		    LEN_HITEM(p, hashp->hdr->pagesize, H_KEYINDEX(ndx));
		data_dbt.data = P_ENTRY(p, H_DATAINDEX(ndx));
		data_dbt.size =
		    LEN_HITEM(p, hashp->hdr->pagesize, H_DATAINDEX(ndx));

		if ((ret = __ham_insdel_log(dbenv->lg_info,
		    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0, DELPAIR,
		    hashp->dbp->log_fileid, PGNO(p), (u_int32_t)ndx,
		    &LSN(p), &key_dbt, &data_dbt)) != 0)
			return (ret);

		/* Move lsn onto page. */
		LSN(p) = new_lsn;
	}

	__ham_dpair(hashp->dbp, p, ndx);

	/*
	 * If we are locking, we will not maintain this.
	 * XXXX perhaps we can retain incremental numbers and apply them
	 * later.
	 */
	if (!F_ISSET(hashp->dbp, DB_AM_LOCKING))
		--hashp->hdr->nelem;

	/*
	 * If we need to reclaim the page, then check if the page is empty.
	 * There are two cases.  If it's empty and it's not the first page
	 * in the bucket (i.e., the bucket page) then we can simply remove
	 * it. If it is the first chain in the bucket, then we need to copy
	 * the second page into it and remove the second page.
	 */
	if (reclaim_page && NUM_ENT(p) == 0 && PREV_PGNO(p) == PGNO_INVALID &&
	    NEXT_PGNO(p) != PGNO_INVALID) {
		PAGE *n_pagep, *nn_pagep;
		db_pgno_t tmp_pgno;

		/*
		 * First page in chain is empty and we know that there
		 * are more pages in the chain.
		 */
		if ((ret =
		    __ham_get_page(hashp->dbp, NEXT_PGNO(p), &n_pagep)) != 0)
			return (ret);

		if (NEXT_PGNO(n_pagep) != PGNO_INVALID) {
			if ((ret =
			    __ham_get_page(hashp->dbp, NEXT_PGNO(n_pagep),
			    &nn_pagep)) != 0) {
				(void) __ham_put_page(hashp->dbp, n_pagep, 0);
				return (ret);
			}
		}

		if (DB_LOGGING(hashp->dbp)) {
			key_dbt.data = n_pagep;
			key_dbt.size = hashp->hdr->pagesize;
			if ((ret = __ham_copypage_log(dbenv->lg_info,
			    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0,
			    hashp->dbp->log_fileid, PGNO(p), &LSN(p),
			    PGNO(n_pagep), &LSN(n_pagep), NEXT_PGNO(n_pagep),
			    NEXT_PGNO(n_pagep) == PGNO_INVALID ? NULL :
			    &LSN(nn_pagep), &key_dbt)) != 0)
				return (ret);

			/* Move lsn onto page. */
			LSN(p) = new_lsn;	/* Structure assignment. */
			LSN(n_pagep) = new_lsn;
			if (NEXT_PGNO(n_pagep) != PGNO_INVALID)
				LSN(nn_pagep) = new_lsn;
		}
		if (NEXT_PGNO(n_pagep) != PGNO_INVALID) {
			PREV_PGNO(nn_pagep) = PGNO(p);
			(void)__ham_put_page(hashp->dbp, nn_pagep, 1);
		}

		tmp_pgno = PGNO(p);
		tmp_lsn = LSN(p);
		memcpy(p, n_pagep, hashp->hdr->pagesize);
		PGNO(p) = tmp_pgno;
		LSN(p) = tmp_lsn;
		PREV_PGNO(p) = PGNO_INVALID;

		/*
		 * Cursor is advanced to the beginning of the next page.
		 */
		cursorp->bndx = 0;
		cursorp->pgno = PGNO(p);
		F_SET(cursorp, H_DELETED);
		chg_pgno = PGNO(p);
		if ((ret = __ham_dirty_page(hashp, p)) != 0 ||
		    (ret = __ham_del_page(hashp->dbp, n_pagep)) != 0)
			return (ret);
	} else if (reclaim_page &&
	    NUM_ENT(p) == 0 && PREV_PGNO(p) != PGNO_INVALID) {
		PAGE *n_pagep, *p_pagep;

		if ((ret =
		    __ham_get_page(hashp->dbp, PREV_PGNO(p), &p_pagep)) != 0)
			return (ret);

		if (NEXT_PGNO(p) != PGNO_INVALID) {
			if ((ret = __ham_get_page(hashp->dbp,
			    NEXT_PGNO(p), &n_pagep)) != 0) {
				(void)__ham_put_page(hashp->dbp, p_pagep, 0);
				return (ret);
			}
			n_lsn = &LSN(n_pagep);
		} else {
			n_pagep = NULL;
			n_lsn = NULL;
		}

		NEXT_PGNO(p_pagep) = NEXT_PGNO(p);
		if (n_pagep != NULL)
			PREV_PGNO(n_pagep) = PGNO(p_pagep);

		if (DB_LOGGING(hashp->dbp)) {
			if ((ret = __ham_newpage_log(dbenv->lg_info,
			    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0, DELOVFL,
			    hashp->dbp->log_fileid, PREV_PGNO(p), &LSN(p_pagep),
			    PGNO(p), &LSN(p), NEXT_PGNO(p), n_lsn)) != 0)
				return (ret);

			/* Move lsn onto page. */
			LSN(p_pagep) = new_lsn;	/* Structure assignment. */
			if (n_pagep)
				LSN(n_pagep) = new_lsn;
			LSN(p) = new_lsn;
		}
		cursorp->pgno = NEXT_PGNO(p);
		cursorp->bndx = 0;
		/*
		 * Since we are about to delete the cursor page and we have
		 * just moved the cursor, we need to make sure that the
		 * old page pointer isn't left hanging around in the cursor.
		 */
		cursorp->pagep = NULL;
		chg_pgno = PGNO(p);
		ret = __ham_del_page(hashp->dbp, p);
		if ((tret = __ham_put_page(hashp->dbp, p_pagep, 1)) != 0 &&
		    ret == 0)
			ret = tret;
		if (n_pagep != NULL &&
		    (tret = __ham_put_page(hashp->dbp, n_pagep, 1)) != 0 &&
		    ret == 0)
			ret = tret;
		if (ret != 0)
			return (ret);
	} else {
		/*
		 * Mark item deleted so that we don't try to return it, and
		 * so that we update the cursor correctly on the next call
		 * to next.
		 */
		F_SET(cursorp, H_DELETED);
		chg_pgno = cursorp->pgno;
		ret = __ham_dirty_page(hashp, p);
	}
	__ham_c_update(cursorp, chg_pgno, 0, 0, 0);

	/*
	 * Since we just deleted a pair from the master page, anything
	 * in cursorp->dpgno should be cleared.
	 */
	cursorp->dpgno = PGNO_INVALID;

	F_CLR(cursorp, H_OK);
	return (ret);
}

/*
 * __ham_replpair --
 *	Given the key data indicated by the cursor, replace part/all of it
 *	according to the fields in the dbt.
 *
 * PUBLIC: int __ham_replpair __P((HTAB *, HASH_CURSOR *, DBT *, u_int32_t));
 */
int
__ham_replpair(hashp, hcp, dbt, make_dup)
	HTAB *hashp;
	HASH_CURSOR *hcp;
	DBT *dbt;
	u_int32_t make_dup;
{
	DBT old_dbt, tdata, tmp;
	DB_LSN	new_lsn;
	int32_t change;			/* XXX: Possible overflow. */
	u_int32_t len;
	int is_big, ret, type;
	u_int8_t *beg, *dest, *end, *hk, *src;

	/*
	 * Big item replacements are handled in generic code.
	 * Items that fit on the current page fall into 4 classes.
	 * 1. On-page element, same size
	 * 2. On-page element, new is bigger (fits)
	 * 3. On-page element, new is bigger (does not fit)
	 * 4. On-page element, old is bigger
	 * Numbers 1, 2, and 4 are essentially the same (and should
	 * be the common case).  We handle case 3 as a delete and
	 * add.
	 */

	/*
	 * We need to compute the number of bytes that we are adding or
	 * removing from the entry.  Normally, we can simply substract
	 * the number of bytes we are replacing (dbt->dlen) from the
	 * number of bytes we are inserting (dbt->size).  However, if
	 * we are doing a partial put off the end of a record, then this
	 * formula doesn't work, because we are essentially adding
	 * new bytes.
	 */
	change = dbt->size - dbt->dlen;

	hk = H_PAIRDATA(hcp->pagep, hcp->bndx);
	is_big = HPAGE_PTYPE(hk) == H_OFFPAGE;

	if (is_big)
		memcpy(&len, HOFFPAGE_TLEN(hk), sizeof(u_int32_t));
	else
		len = LEN_HKEYDATA(hcp->pagep,
		    hashp->dbp->pgsize, H_DATAINDEX(hcp->bndx));

	if (dbt->doff + dbt->dlen > len)
		change += dbt->doff + dbt->dlen - len;


	if (change > (int32_t)P_FREESPACE(hcp->pagep) || is_big) {
		/*
		 * Case 3 -- two subcases.
		 * A. This is not really a partial operation, but an overwrite.
		 *    Simple del and add works.
		 * B. This is a partial and we need to construct the data that
		 *    we are really inserting (yuck).
		 * In both cases, we need to grab the key off the page (in
		 * some cases we could do this outside of this routine; for
		 * cleanliness we do it here.  If you happen to be on a big
		 * key, this could be a performance hit).
		 */
		tmp.flags = 0;
		F_SET(&tmp, DB_DBT_MALLOC | DB_DBT_INTERNAL);
		if ((ret =
		    __db_ret(hashp->dbp, hcp->pagep, H_KEYINDEX(hcp->bndx),
		    &tmp, &hcp->big_key, &hcp->big_keylen)) != 0)
			return (ret);

		if (dbt->doff == 0 && dbt->dlen == len) {
			ret = __ham_del_pair(hashp, hcp, 0);
			if (ret == 0)
			    ret = __ham_add_el(hashp,
			        hcp, &tmp, dbt, H_KEYDATA);
		} else {					/* Case B */
			type = HPAGE_PTYPE(hk) != H_OFFPAGE ?
			    HPAGE_PTYPE(hk) : H_KEYDATA;
			tdata.flags = 0;
			F_SET(&tdata, DB_DBT_MALLOC | DB_DBT_INTERNAL);

			if ((ret = __db_ret(hashp->dbp, hcp->pagep,
			    H_DATAINDEX(hcp->bndx), &tdata, &hcp->big_data,
			    &hcp->big_datalen)) != 0)
				goto err;

			/* Now we can delete the item. */
			if ((ret = __ham_del_pair(hashp, hcp, 0)) != 0) {
				__db_free(tdata.data);
				goto err;
			}

			/* Now shift old data around to make room for new. */
			if (change > 0) {
				tdata.data = (void *)__db_realloc(tdata.data,
				    tdata.size + change);
				memset((u_int8_t *)tdata.data + tdata.size,
				    0, change);
			}
			if (tdata.data == NULL)
				return (ENOMEM);
			end = (u_int8_t *)tdata.data + tdata.size;

			src = (u_int8_t *)tdata.data + dbt->doff + dbt->dlen;
			if (src < end && tdata.size > dbt->doff + dbt->dlen) {
				len = tdata.size - dbt->doff - dbt->dlen;
				dest = src + change;
				memmove(dest, src, len);
			}
			memcpy((u_int8_t *)tdata.data + dbt->doff,
			    dbt->data, dbt->size);
			tdata.size += change;

			/* Now add the pair. */
			ret = __ham_add_el(hashp, hcp, &tmp, &tdata, type);
			__db_free(tdata.data);
		}
err:		__db_free(tmp.data);
		return (ret);
	}

	/*
	 * Set up pointer into existing data. Do it before the log
	 * message so we can use it inside of the log setup.
	 */
	beg = HKEYDATA_DATA(H_PAIRDATA(hcp->pagep, hcp->bndx));
	beg += dbt->doff;

	/*
	 * If we are going to have to move bytes at all, figure out
	 * all the parameters here.  Then log the call before moving
	 * anything around.
	 */
	if (DB_LOGGING(hashp->dbp)) {
		old_dbt.data = beg;
		old_dbt.size = dbt->dlen;
		if ((ret = __ham_replace_log(hashp->dbp->dbenv->lg_info,
		    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0,
		    hashp->dbp->log_fileid, PGNO(hcp->pagep),
		    (u_int32_t)H_DATAINDEX(hcp->bndx), &LSN(hcp->pagep),
		    (u_int32_t)dbt->doff, &old_dbt, dbt, make_dup)) != 0)
			return (ret);

		LSN(hcp->pagep) = new_lsn;	/* Structure assignment. */
	}

	__ham_onpage_replace(hcp->pagep, hashp->dbp->pgsize,
	    (u_int32_t)H_DATAINDEX(hcp->bndx), (int32_t)dbt->doff, change, dbt);

	return (0);
}

/*
 * Replace data on a page with new data, possibly growing or shrinking what's
 * there.  This is called on two different occasions. On one (from replpair)
 * we are interested in changing only the data.  On the other (from recovery)
 * we are replacing the entire data (header and all) with a new element.  In
 * the latter case, the off argument is negative.
 * pagep: the page that we're changing
 * ndx: page index of the element that is growing/shrinking.
 * off: Offset at which we are beginning the replacement.
 * change: the number of bytes (+ or -) that the element is growing/shrinking.
 * dbt: the new data that gets written at beg.
 * PUBLIC: void __ham_onpage_replace __P((PAGE *, size_t, u_int32_t, int32_t,
 * PUBLIC:     int32_t,  DBT *));
 */
void
__ham_onpage_replace(pagep, pgsize, ndx, off, change, dbt)
	PAGE *pagep;
	size_t pgsize;
	u_int32_t ndx;
	int32_t off;
	int32_t change;
	DBT *dbt;
{
	db_indx_t i;
	int32_t len;
	u_int8_t *src, *dest;
	int zero_me;

	if (change != 0) {
		zero_me = 0;
		src = (u_int8_t *)(pagep) + HOFFSET(pagep);
		if (off < 0)
			len = pagep->inp[ndx] - HOFFSET(pagep);
		else if ((u_int32_t)off >= LEN_HKEYDATA(pagep, pgsize, ndx)) {
			len = HKEYDATA_DATA(P_ENTRY(pagep, ndx)) +
			    LEN_HKEYDATA(pagep, pgsize, ndx) - src;
			zero_me = 1;
		} else
			len = (HKEYDATA_DATA(P_ENTRY(pagep, ndx)) + off) - src;
		dest = src - change;
		memmove(dest, src, len);
		if (zero_me)
			memset(dest + len, 0, change);

		/* Now update the indices. */
		for (i = ndx; i < NUM_ENT(pagep); i++)
			pagep->inp[i] -= change;
		HOFFSET(pagep) -= change;
	}
	if (off >= 0)
		memcpy(HKEYDATA_DATA(P_ENTRY(pagep, ndx)) + off,
		    dbt->data, dbt->size);
	else
		memcpy(P_ENTRY(pagep, ndx), dbt->data, dbt->size);
}

/*
 * PUBLIC: int __ham_split_page __P((HTAB *, u_int32_t, u_int32_t));
 */
int
__ham_split_page(hashp, obucket, nbucket)
	HTAB *hashp;
	u_int32_t obucket, nbucket;
{
	DBT key, page_dbt;
	DB_ENV *dbenv;
	DB_LSN new_lsn;
	PAGE **pp, *old_pagep, *temp_pagep, *new_pagep;
	db_indx_t n;
	db_pgno_t bucket_pgno, next_pgno;
	u_int32_t big_len, len;
	int ret, tret;
	void *big_buf;

	dbenv = hashp->dbp->dbenv;
	temp_pagep = old_pagep = new_pagep = NULL;

	bucket_pgno = BUCKET_TO_PAGE(hashp, obucket);
	if ((ret = __ham_get_page(hashp->dbp, bucket_pgno, &old_pagep)) != 0)
		return (ret);
	if ((ret = __ham_new_page(hashp, BUCKET_TO_PAGE(hashp, nbucket), P_HASH,
	    &new_pagep)) != 0)
		goto err;

	temp_pagep = hashp->split_buf;
	memcpy(temp_pagep, old_pagep, hashp->hdr->pagesize);

	if (DB_LOGGING(hashp->dbp)) {
		page_dbt.size = hashp->hdr->pagesize;
		page_dbt.data = old_pagep;
		if ((ret = __ham_splitdata_log(dbenv->lg_info,
		    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0,
		    hashp->dbp->log_fileid, SPLITOLD, PGNO(old_pagep),
		    &page_dbt, &LSN(old_pagep))) != 0)
			goto err;
	}

	P_INIT(old_pagep, hashp->hdr->pagesize, PGNO(old_pagep), PGNO_INVALID,
	    PGNO_INVALID, 0, P_HASH);

	if (DB_LOGGING(hashp->dbp))
		LSN(old_pagep) = new_lsn;	/* Structure assignment. */

	big_len = 0;
	big_buf = NULL;
	key.flags = 0;
	while (temp_pagep != NULL) {
		for (n = 0; n < (db_indx_t)H_NUMPAIRS(temp_pagep); n++) {
			if ((ret =
			    __db_ret(hashp->dbp, temp_pagep, H_KEYINDEX(n),
			    &key, &big_buf, &big_len)) != 0)
				goto err;

			if (__ham_call_hash(hashp, key.data, key.size)
			    == obucket)
				pp = &old_pagep;
			else
				pp = &new_pagep;

			/*
			 * Figure out how many bytes we need on the new
			 * page to store the key/data pair.
			 */

			len = LEN_HITEM(temp_pagep, hashp->hdr->pagesize,
			    H_DATAINDEX(n)) +
			    LEN_HITEM(temp_pagep, hashp->hdr->pagesize,
			    H_KEYINDEX(n)) +
			    2 * sizeof(db_indx_t);

			if (P_FREESPACE(*pp) < len) {
				if (DB_LOGGING(hashp->dbp)) {
					page_dbt.size = hashp->hdr->pagesize;
					page_dbt.data = *pp;
					if ((ret = __ham_splitdata_log(
					    dbenv->lg_info,
					    (DB_TXN *)hashp->dbp->txn,
					    &new_lsn, 0,
					    hashp->dbp->log_fileid, SPLITNEW,
					    PGNO(*pp), &page_dbt,
					    &LSN(*pp))) != 0)
						goto err;
					LSN(*pp) = new_lsn;
				}
				if ((ret = __ham_add_ovflpage(hashp,
				    *pp, 1, pp)) != 0)
					goto err;
			}
			__ham_copy_item(hashp, temp_pagep, H_KEYINDEX(n), *pp);
			__ham_copy_item(hashp, temp_pagep, H_DATAINDEX(n), *pp);
		}
		next_pgno = NEXT_PGNO(temp_pagep);

		/* Clear temp_page; if it's a link overflow page, free it. */
		if (PGNO(temp_pagep) != bucket_pgno && (ret =
		    __ham_del_page(hashp->dbp, temp_pagep)) != 0)
			goto err;

		if (next_pgno == PGNO_INVALID)
			temp_pagep = NULL;
		else if ((ret =
		    __ham_get_page(hashp->dbp, next_pgno, &temp_pagep)) != 0)
			goto err;

		if (temp_pagep != NULL && DB_LOGGING(hashp->dbp)) {
			page_dbt.size = hashp->hdr->pagesize;
			page_dbt.data = temp_pagep;
			if ((ret = __ham_splitdata_log(dbenv->lg_info,
			    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0,
			    hashp->dbp->log_fileid, SPLITOLD, PGNO(temp_pagep),
			    &page_dbt, &LSN(temp_pagep))) != 0)
				goto err;
			LSN(temp_pagep) = new_lsn;
		}
	}
	if (big_buf != NULL)
		__db_free(big_buf);

	/*
	 * If the original bucket spanned multiple pages, then we've got
	 * a pointer to a page that used to be on the bucket chain.  It
	 * should be deleted.
	 */
	if (temp_pagep != NULL && PGNO(temp_pagep) != bucket_pgno &&
	    (ret = __ham_del_page(hashp->dbp, temp_pagep)) != 0)
		goto err;

	/*
	 * Write new buckets out.
	 */
	if (DB_LOGGING(hashp->dbp)) {
		page_dbt.size = hashp->hdr->pagesize;
		page_dbt.data = old_pagep;
		if ((ret = __ham_splitdata_log(dbenv->lg_info,
		    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0,
		    hashp->dbp->log_fileid, SPLITNEW, PGNO(old_pagep),
		    &page_dbt, &LSN(old_pagep))) != 0)
			goto err;
		LSN(old_pagep) = new_lsn;

		page_dbt.data = new_pagep;
		if ((ret = __ham_splitdata_log(dbenv->lg_info,
		    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0,
		    hashp->dbp->log_fileid, SPLITNEW, PGNO(new_pagep),
		    &page_dbt, &LSN(new_pagep))) != 0)
			goto err;
		LSN(new_pagep) = new_lsn;
	}
	ret = __ham_put_page(hashp->dbp, old_pagep, 1);
	if ((tret = __ham_put_page(hashp->dbp, new_pagep, 1)) != 0 &&
	    ret == 0)
		ret = tret;

	if (0) {
err:		if (old_pagep != NULL)
			(void)__ham_put_page(hashp->dbp, old_pagep, 1);
		if (new_pagep != NULL)
			(void)__ham_put_page(hashp->dbp, new_pagep, 1);
		if (temp_pagep != NULL && PGNO(temp_pagep) != bucket_pgno)
			(void)__ham_put_page(hashp->dbp, temp_pagep, 1);
	}
	return (ret);
}

/*
 * Add the given pair to the page.  The page in question may already be
 * held (i.e. it was already gotten).  If it is, then the page is passed
 * in via the pagep parameter.  On return, pagep will contain the page
 * to which we just added something.  This allows us to link overflow
 * pages and return the new page having correctly put the last page.
 *
 * PUBLIC: int __ham_add_el
 * PUBLIC:    __P((HTAB *, HASH_CURSOR *, const DBT *, const DBT *, int));
 */
int
__ham_add_el(hashp, hcp, key, val, type)
	HTAB *hashp;
	HASH_CURSOR *hcp;
	const DBT *key, *val;
	int type;
{
	const DBT *pkey, *pdata;
	DBT key_dbt, data_dbt;
	DB_LSN new_lsn;
	HOFFPAGE doff, koff;
	db_pgno_t next_pgno;
	u_int32_t data_size, key_size, pairsize, rectype;
	int do_expand, is_keybig, is_databig, ret;
	int key_type, data_type;

	do_expand = 0;

	if (hcp->pagep == NULL && (ret = __ham_get_page(hashp->dbp,
	    hcp->seek_found_page != PGNO_INVALID ?  hcp->seek_found_page :
	    hcp->pgno, &hcp->pagep)) != 0)
		return (ret);

	key_size = HKEYDATA_PSIZE(key->size);
	data_size = HKEYDATA_PSIZE(val->size);
	is_keybig = ISBIG(hashp, key->size);
	is_databig = ISBIG(hashp, val->size);
	if (is_keybig)
		key_size = HOFFPAGE_PSIZE;
	if (is_databig)
		data_size = HOFFPAGE_PSIZE;

	pairsize = key_size + data_size;

	/* Advance to first page in chain with room for item. */
	while (H_NUMPAIRS(hcp->pagep) && NEXT_PGNO(hcp->pagep) !=
	    PGNO_INVALID) {
		/*
		 * This may not be the end of the chain, but the pair may fit
		 * anyway.  Check if it's a bigpair that fits or a regular
		 * pair that fits.
		 */
		if (P_FREESPACE(hcp->pagep) >= pairsize)
			break;
		next_pgno = NEXT_PGNO(hcp->pagep);
		if ((ret =
		    __ham_next_cpage(hashp, hcp, next_pgno, 0, 0)) != 0)
			return (ret);
	}

	/*
	 * Check if we need to allocate a new page.
	 */
	if (P_FREESPACE(hcp->pagep) < pairsize) {
		do_expand = 1;
		if ((ret = __ham_add_ovflpage(hashp,
		    hcp->pagep, 1, &hcp->pagep)) !=  0)
			return (ret);
		hcp->pgno = PGNO(hcp->pagep);
	}

	/*
	 * Update cursor.
	 */
	hcp->bndx = H_NUMPAIRS(hcp->pagep);
	F_CLR(hcp, H_DELETED);
	if (is_keybig) {
		if ((ret = __db_poff(hashp->dbp,
		    key, &koff.pgno, __ham_overflow_page)) != 0)
			return (ret);
		koff.type = H_OFFPAGE;
		koff.tlen = key->size;
		key_dbt.data = &koff;
		key_dbt.size = sizeof(koff);
		pkey = &key_dbt;
		key_type = H_OFFPAGE;
	} else {
		pkey = key;
		key_type = H_KEYDATA;
	}

	if (is_databig) {
		if ((ret = __db_poff(hashp->dbp,
		    val, &doff.pgno, __ham_overflow_page)) != 0)
			return (ret);
		doff.type = H_OFFPAGE;
		doff.tlen = val->size;
		data_dbt.data = &doff;
		data_dbt.size = sizeof(doff);
		pdata = &data_dbt;
		data_type = H_OFFPAGE;
	} else {
		pdata = val;
		data_type = type;
	}

	if (DB_LOGGING(hashp->dbp)) {
		rectype = PUTPAIR;
		if (is_databig)
			rectype |= PAIR_DATAMASK;
		if (is_keybig)
			rectype |= PAIR_KEYMASK;

		if ((ret = __ham_insdel_log(hashp->dbp->dbenv->lg_info,
		    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0, rectype,
		    hashp->dbp->log_fileid, PGNO(hcp->pagep),
		    (u_int32_t)H_NUMPAIRS(hcp->pagep),
		    &LSN(hcp->pagep), pkey, pdata)) != 0)
			return (ret);

		/* Move lsn onto page. */
		LSN(hcp->pagep) = new_lsn;	/* Structure assignment. */
	}

	__ham_putitem(hcp->pagep, pkey, key_type);
	__ham_putitem(hcp->pagep, pdata, data_type);

	/*
	 * For splits, we are going to update item_info's page number
	 * field, so that we can easily return to the same page the
	 * next time we come in here.  For other operations, this shouldn't
	 * matter, since odds are this is the last thing that happens before
	 * we return to the user program.
	 */
	hcp->pgno = PGNO(hcp->pagep);

	/*
	 * XXX Maybe keep incremental numbers here
	 */
	if (!F_ISSET(hashp->dbp, DB_AM_LOCKING))
		hashp->hdr->nelem++;

	if (do_expand || (hashp->hdr->ffactor != 0 &&
	    (u_int32_t)H_NUMPAIRS(hcp->pagep) > hashp->hdr->ffactor))
		F_SET(hcp, H_EXPAND);
	return (0);
}


/*
 * Special __putitem call used in splitting -- copies one entry to
 * another.  Works for all types of hash entries (H_OFFPAGE, H_KEYDATA,
 * H_DUPLICATE, H_OFFDUP).  Since we log splits at a high level, we
 * do not need to do any logging here.
 *
 * PUBLIC: void __ham_copy_item __P((HTAB *, PAGE *, u_int32_t, PAGE *));
 */
void
__ham_copy_item(hashp, src_page, src_ndx, dest_page)
	HTAB *hashp;
	PAGE *src_page;
	u_int32_t src_ndx;
	PAGE *dest_page;
{
	u_int32_t len;
	void *src, *dest;

	/*
	 * Copy the key and data entries onto this new page.
	 */
	src = P_ENTRY(src_page, src_ndx);

	/* Set up space on dest. */
	len = LEN_HITEM(src_page, hashp->hdr->pagesize, src_ndx);
	HOFFSET(dest_page) -= len;
	dest_page->inp[NUM_ENT(dest_page)] = HOFFSET(dest_page);
	dest = P_ENTRY(dest_page, NUM_ENT(dest_page));
	NUM_ENT(dest_page)++;

	memcpy(dest, src, len);
}

/*
 *
 * Returns:
 *      pointer on success
 *      NULL on error
 *
 * PUBLIC: int __ham_add_ovflpage __P((HTAB *, PAGE *, int, PAGE **));
 */
int
__ham_add_ovflpage(hashp, pagep, release, pp)
	HTAB *hashp;
	PAGE *pagep;
	int release;
	PAGE **pp;
{
	DB_ENV *dbenv;
	DB_LSN new_lsn;
	PAGE *new_pagep;
	int ret;

	dbenv = hashp->dbp->dbenv;

	if ((ret = __ham_overflow_page(hashp->dbp, P_HASH, &new_pagep)) != 0)
		return (ret);

	if (DB_LOGGING(hashp->dbp)) {
		if ((ret = __ham_newpage_log(dbenv->lg_info,
		    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0, PUTOVFL,
		    hashp->dbp->log_fileid, PGNO(pagep), &LSN(pagep),
		    PGNO(new_pagep), &LSN(new_pagep), PGNO_INVALID, NULL)) != 0)
			return (ret);

		/* Move lsn onto page. */
		LSN(pagep) = LSN(new_pagep) = new_lsn;
	}
	NEXT_PGNO(pagep) = PGNO(new_pagep);
	PREV_PGNO(new_pagep) = PGNO(pagep);

	if (release)
		ret = __ham_put_page(hashp->dbp, pagep, 1);

	hashp->hash_overflows++;
	*pp = new_pagep;
	return (ret);
}


/*
 * PUBLIC: int __ham_new_page __P((HTAB *, u_int32_t, u_int32_t, PAGE **));
 */
int
__ham_new_page(hashp, addr, type, pp)
	HTAB *hashp;
	u_int32_t addr, type;
	PAGE **pp;
{
	PAGE *pagep;
	int ret;

	if ((ret = memp_fget(hashp->dbp->mpf,
	    &addr, DB_MPOOL_CREATE, &pagep)) != 0)
		return (ret);

#ifdef DEBUG_SLOW
	__account_page(hashp, addr, 1);
#endif
	/* This should not be necessary because page-in should do it. */
	P_INIT(pagep,
	    hashp->hdr->pagesize, addr, PGNO_INVALID, PGNO_INVALID, 0, type);

	*pp = pagep;
	return (0);
}

/*
 * PUBLIC: int __ham_del_page __P((DB *, PAGE *));
 */
int
__ham_del_page(dbp, pagep)
	DB *dbp;
	PAGE *pagep;
{
	DB_LSN new_lsn;
	HTAB *hashp;
	int ret;

	hashp = (HTAB *)dbp->internal;
	ret = 0;
	DIRTY_META(hashp, ret);
	if (ret != 0) {
		if (ret != EAGAIN)
			__db_err(hashp->dbp->dbenv,
			    "free_ovflpage: unable to lock meta data page %s\n",
			    strerror(ret));
		/*
		 * If we are going to return an error, then we should free
		 * the page, so it doesn't stay pinned forever.
		 */
		(void)__ham_put_page(hashp->dbp, pagep, 0);
		return (ret);
	}

	if (DB_LOGGING(hashp->dbp)) {
		if ((ret = __ham_newpgno_log(hashp->dbp->dbenv->lg_info,
		    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0, DELPGNO,
		    hashp->dbp->log_fileid, PGNO(pagep), hashp->hdr->last_freed,
		    (u_int32_t)TYPE(pagep), NEXT_PGNO(pagep), P_INVALID,
		    &LSN(pagep), &hashp->hdr->lsn)) != 0)
			return (ret);

		hashp->hdr->lsn = new_lsn;
		LSN(pagep) = new_lsn;
	}

#ifdef DIAGNOSTIC
	{
		db_pgno_t __pgno;
		DB_LSN __lsn;
		__pgno = pagep->pgno;
		__lsn = pagep->lsn;
		memset(pagep, 0xff, dbp->pgsize);
		pagep->pgno = __pgno;
		pagep->lsn = __lsn;
	}
#endif
	TYPE(pagep) = P_INVALID;
	NEXT_PGNO(pagep) = hashp->hdr->last_freed;
	hashp->hdr->last_freed = PGNO(pagep);

	return (__ham_put_page(hashp->dbp, pagep, 1));
}


/*
 * PUBLIC: int __ham_put_page __P((DB *, PAGE *, int32_t));
 */
int
__ham_put_page(dbp, pagep, is_dirty)
	DB *dbp;
	PAGE *pagep;
	int32_t is_dirty;
{
#ifdef DEBUG_SLOW
	__account_page((HTAB *)dbp->cookie,
	    ((BKT *)((char *)pagep - sizeof(BKT)))->pgno, -1);
#endif
	return (memp_fput(dbp->mpf, pagep, (is_dirty ? DB_MPOOL_DIRTY : 0)));
}

/*
 * __ham_dirty_page --
 *	Mark a page dirty.
 *
 * PUBLIC: int __ham_dirty_page __P((HTAB *, PAGE *));
 */
int
__ham_dirty_page(hashp, pagep)
	HTAB *hashp;
	PAGE *pagep;
{
	return (memp_fset(hashp->dbp->mpf, pagep, DB_MPOOL_DIRTY));
}

/*
 * PUBLIC: int __ham_get_page __P((DB *, db_pgno_t, PAGE **));
 */
int
__ham_get_page(dbp, addr, pagep)
	DB *dbp;
	db_pgno_t addr;
	PAGE **pagep;
{
	int ret;

	ret = memp_fget(dbp->mpf, &addr, DB_MPOOL_CREATE, pagep);
#ifdef DEBUG_SLOW
	if (*pagep != NULL)
		__account_page((HTAB *)dbp->internal, addr, 1);
#endif
	return (ret);
}

/*
 * PUBLIC: int __ham_overflow_page __P((DB *, u_int32_t, PAGE **));
 */
int
__ham_overflow_page(dbp, type, pp)
	DB *dbp;
	u_int32_t type;
	PAGE **pp;
{
	DB_LSN *lsnp, new_lsn;
	HTAB *hashp;
	PAGE *p;
	db_pgno_t new_addr, next_free, newalloc_flag;
	u_int32_t offset, splitnum;
	int ret;

	hashp = (HTAB *)dbp->internal;

	ret = 0;
	DIRTY_META(hashp, ret);
	if (ret != 0)
		return (ret);

	/*
	 * This routine is split up into two parts.  First we have
	 * to figure out the address of the new page that we are
	 * allocating.  Then we have to log the allocation.  Only
	 * after the log do we get to complete allocation of the
	 * new page.
	 */
	new_addr = hashp->hdr->last_freed;
	if (new_addr != PGNO_INVALID) {
		if ((ret = __ham_get_page(hashp->dbp, new_addr, &p)) != 0)
			return (ret);
		next_free = NEXT_PGNO(p);
		lsnp = &LSN(p);
		newalloc_flag = 0;
	} else {
		splitnum = hashp->hdr->ovfl_point;
		hashp->hdr->spares[splitnum]++;
		offset = hashp->hdr->spares[splitnum] -
		    (splitnum ? hashp->hdr->spares[splitnum - 1] : 0);
		new_addr = PGNO_OF(hashp, hashp->hdr->ovfl_point, offset);
		if (new_addr > MAX_PAGES(hashp)) {
			__db_err(hashp->dbp->dbenv, "hash: out of file pages");
			hashp->hdr->spares[splitnum]--;
			return (ENOMEM);
		}
		next_free = PGNO_INVALID;
		p = NULL;
		lsnp = NULL;
		newalloc_flag = 1;
	}

	if (DB_LOGGING(hashp->dbp)) {
		if ((ret = __ham_newpgno_log(hashp->dbp->dbenv->lg_info,
		    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0, ALLOCPGNO,
		    hashp->dbp->log_fileid, new_addr, next_free,
		    0, newalloc_flag, type, lsnp, &hashp->hdr->lsn)) != 0)
			return (ret);

		hashp->hdr->lsn = new_lsn;
		if (lsnp != NULL)
			*lsnp = new_lsn;
	}

	if (p != NULL) {
		/* We just took something off the free list, initialize it. */
		hashp->hdr->last_freed = next_free;
		P_INIT(p, hashp->hdr->pagesize, PGNO(p), PGNO_INVALID,
		    PGNO_INVALID, 0, (u_int8_t)type);
	} else {
		/* Get the new page. */
		if ((ret = __ham_new_page(hashp, new_addr, type, &p)) != 0)
			return (ret);
	}
	if (DB_LOGGING(hashp->dbp))
		LSN(p) = new_lsn;

	*pp = p;
	return (0);
}

#ifdef DEBUG
/*
 * PUBLIC: #ifdef DEBUG
 * PUBLIC: db_pgno_t __bucket_to_page __P((HTAB *, db_pgno_t));
 * PUBLIC: #endif
 */
db_pgno_t
__bucket_to_page(hashp, n)
	HTAB *hashp;
	db_pgno_t n;
{
	int ret_val;

	ret_val = n + 1;
	if (n != 0)
		ret_val += hashp->hdr->spares[__db_log2(n + 1) - 1];
	return (ret_val);
}
#endif

/*
 * Create a bunch of overflow pages at the current split point.
 * PUBLIC: void __ham_init_ovflpages __P((HTAB *));
 */
void
__ham_init_ovflpages(hp)
	HTAB *hp;
{
	DB_LSN new_lsn;
	PAGE *p;
	db_pgno_t last_pgno, new_pgno;
	u_int32_t i, curpages, numpages;

	curpages = hp->hdr->spares[hp->hdr->ovfl_point] -
	    hp->hdr->spares[hp->hdr->ovfl_point - 1];
	numpages = hp->hdr->ovfl_point + 1 - curpages;

	last_pgno = hp->hdr->last_freed;
	new_pgno = PGNO_OF(hp, hp->hdr->ovfl_point, curpages + 1);
	if (DB_LOGGING(hp->dbp)) {
		(void)__ham_ovfl_log(hp->dbp->dbenv->lg_info,
		    (DB_TXN *)hp->dbp->txn, &new_lsn, 0,
		    hp->dbp->log_fileid, new_pgno,
		    numpages, last_pgno, hp->hdr->ovfl_point, &hp->hdr->lsn);
		hp->hdr->lsn = new_lsn;
	} else
		ZERO_LSN(new_lsn);

	hp->hdr->spares[hp->hdr->ovfl_point] += numpages;
	for (i = numpages; i > 0; i--) {
		if (__ham_new_page(hp,
		    PGNO_OF(hp, hp->hdr->ovfl_point, curpages + i),
		    P_INVALID, &p) != 0)
			break;
		LSN(p) = new_lsn;
		NEXT_PGNO(p) = last_pgno;
		last_pgno = PGNO(p);
		(void)__ham_put_page(hp->dbp, p, 1);
	}
	hp->hdr->last_freed = last_pgno;
}

/*
 * PUBLIC: int __ham_get_cpage __P((HTAB *, HASH_CURSOR *, db_lockmode_t));
 */
int
__ham_get_cpage(hashp, hcp, mode)
	HTAB *hashp;
	HASH_CURSOR *hcp;
	db_lockmode_t mode;
{
	int ret;

	if (hcp->lock == 0 && F_ISSET(hashp->dbp, DB_AM_LOCKING) &&
	    (ret = __ham_lock_bucket(hashp->dbp, hcp, mode)) != 0)
		return (ret);

	if (hcp->pagep == NULL) {
		if (hcp->pgno == PGNO_INVALID) {
			hcp->pgno = BUCKET_TO_PAGE(hashp, hcp->bucket);
			hcp->bndx = 0;
		}

		if ((ret =
		    __ham_get_page(hashp->dbp, hcp->pgno, &hcp->pagep)) != 0)
			return (ret);
	}

	if (hcp->dpgno != PGNO_INVALID && hcp->dpagep == NULL)
		if ((ret =
		    __ham_get_page(hashp->dbp, hcp->dpgno, &hcp->dpagep)) != 0)
			return (ret);
	return (0);
}

/*
 * Get a new page at the cursor, putting the last page if necessary.
 * If the flag is set to H_ISDUP, then we are talking about the
 * duplicate page, not the main page.
 *
 * PUBLIC: int __ham_next_cpage
 * PUBLIC:    __P((HTAB *, HASH_CURSOR *, db_pgno_t, int, u_int32_t));
 */
int
__ham_next_cpage(hashp, hcp, pgno, dirty, flags)
	HTAB *hashp;
	HASH_CURSOR *hcp;
	db_pgno_t pgno;
	int dirty;
	u_int32_t flags;
{
	PAGE *p;
	int ret;

	if (LF_ISSET(H_ISDUP) && hcp->dpagep != NULL &&
	    (ret = __ham_put_page(hashp->dbp, hcp->dpagep, dirty)) != 0)
		return (ret);
	else if (!LF_ISSET(H_ISDUP) && hcp->pagep != NULL &&
	    (ret = __ham_put_page(hashp->dbp, hcp->pagep, dirty)) != 0)
		return (ret);

	if ((ret = __ham_get_page(hashp->dbp, pgno, &p)) != 0)
		return (ret);

	if (LF_ISSET(H_ISDUP)) {
		hcp->dpagep = p;
		hcp->dpgno = pgno;
		hcp->dndx = 0;
	} else {
		hcp->pagep = p;
		hcp->pgno = pgno;
		hcp->bndx = 0;
	}

	return (0);
}

/*
 * __ham_lock_bucket --
 *	Get the lock on a particular bucket.
 */
static int
__ham_lock_bucket(dbp, hcp, mode)
	DB *dbp;
	HASH_CURSOR *hcp;
	db_lockmode_t mode;
{
	int ret;

	/*
	 * What a way to trounce on the memory system.  It might be
	 * worth copying the lk_info into the hashp.
	 */
	ret = 0;
	dbp->lock.pgno = (db_pgno_t)(hcp->bucket);
	ret = lock_get(dbp->dbenv->lk_info,
	    dbp->txn == NULL ?  dbp->locker : dbp->txn->txnid, 0,
	    &dbp->lock_dbt, mode, &hcp->lock);

	return (ret < 0 ? EAGAIN : ret);
}

/*
 * __ham_dpair --
 *	Delete a pair on a page, paying no attention to what the pair
 *	represents.  The caller is responsible for freeing up duplicates
 *	or offpage entries that might be referenced by this pair.
 *
 * PUBLIC: void __ham_dpair __P((DB *, PAGE *, u_int32_t));
 */
void
__ham_dpair(dbp, p, pndx)
	DB *dbp;
	PAGE *p;
	u_int32_t pndx;
{
	db_indx_t delta, n;
	u_int8_t *dest, *src;

	/*
	 * Compute "delta", the amount we have to shift all of the
	 * offsets.  To find the delta, we just need to calculate
	 * the size of the pair of elements we are removing.
	 */
	delta = H_PAIRSIZE(p, dbp->pgsize, pndx);

	/*
	 * The hard case: we want to remove something other than
	 * the last item on the page.  We need to shift data and
	 * offsets down.
	 */
	if ((db_indx_t)pndx != H_NUMPAIRS(p) - 1) {
		/*
		 * Move the data: src is the first occupied byte on
		 * the page. (Length is delta.)
		 */
		src = (u_int8_t *)p + HOFFSET(p);

		/*
		 * Destination is delta bytes beyond src.  This might
		 * be an overlapping copy, so we have to use memmove.
		 */
		dest = src + delta;
		memmove(dest, src, p->inp[H_DATAINDEX(pndx)] - HOFFSET(p));
	}

	/* Adjust the offsets. */
	for (n = (db_indx_t)pndx; n < (db_indx_t)(H_NUMPAIRS(p) - 1); n++) {
		p->inp[H_KEYINDEX(n)] = p->inp[H_KEYINDEX(n+1)] + delta;
		p->inp[H_DATAINDEX(n)] = p->inp[H_DATAINDEX(n+1)] + delta;
	}

	/* Adjust page metadata. */
	HOFFSET(p) = HOFFSET(p) + delta;
	NUM_ENT(p) = NUM_ENT(p) - 2;
}

#ifdef DEBUG_SLOW
static void
__account_page(hashp, pgno, inout)
	HTAB *hashp;
	db_pgno_t pgno;
	int inout;
{
	static struct {
		db_pgno_t pgno;
		int times;
	} list[100];
	static int last;
	int i, j;

	if (inout == -1)			/* XXX: Kluge */
		inout = 0;

	/* Find page in list. */
	for (i = 0; i < last; i++)
		if (list[i].pgno == pgno)
			break;
	/* Not found. */
	if (i == last) {
		list[last].times = inout;
		list[last].pgno = pgno;
		last++;
	}
	list[i].times = inout;
	if (list[i].times == 0) {
		for (j = i; j < last; j++)
			list[j] = list[j + 1];
		last--;
	}
	for (i = 0; i < last; i++, list[i].times++)
		if (list[i].times > 20 &&
		    !__is_bitmap_pgno(hashp, list[i].pgno))
			(void)fprintf(stderr,
			    "Warning: pg %lu has been out for %d times\n",
			    (u_long)list[i].pgno, list[i].times);
}
#endif /* DEBUG_SLOW */
