/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Olson.
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
static const char sccsid[] = "@(#)bt_put.c	10.45 (Sleepycat) 5/25/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"

static int __bam_fixed __P((BTREE *, DBT *));
static int __bam_isdeleted __P((DB *, PAGE *, u_int32_t, int *));
static int __bam_lookup __P((DB *, DBT *, int *));
static int __bam_ndup __P((DB *, PAGE *, u_int32_t));
static int __bam_ovput __P((DB *, PAGE *, u_int32_t, DBT *));
static int __bam_partial __P((DB *, DBT *, PAGE *, u_int32_t, u_int32_t));
static u_int32_t __bam_partsize __P((DBT *, PAGE *, u_int32_t));

/*
 * __bam_put --
 *	Add a new key/data pair or replace an existing pair (btree).
 *
 * PUBLIC: int __bam_put __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
 */
int
__bam_put(argdbp, txn, key, data, flags)
	DB *argdbp;
	DB_TXN *txn;
	DBT *key, *data;
	u_int32_t flags;
{
	BTREE *t;
	CURSOR c;
	DB *dbp;
	PAGE *h;
	db_indx_t indx;
	u_int32_t iitem_flags, insert_flags;
	int exact, isdeleted, newkey, ret, stack;

	DEBUG_LWRITE(argdbp, txn, "bam_put", key, data, flags);

	/* Check flags. */
	if ((ret = __db_putchk(argdbp, key, data, flags,
	    F_ISSET(argdbp, DB_AM_RDONLY), F_ISSET(argdbp, DB_AM_DUP))) != 0)
		return (ret);

	GETHANDLE(argdbp, txn, &dbp, ret);
	t = dbp->internal;

retry:	/*
	 * Find the location at which to insert.  The call to __bam_lookup
	 * leaves the returned page pinned.
	 */
	if ((ret = __bam_lookup(dbp, key, &exact)) != 0) {
		PUTHANDLE(dbp);
		return (ret);
	}
	h = t->bt_csp->page;
	indx = t->bt_csp->indx;
	stack = 1;

	/*
	 * If DB_NOOVERWRITE is set and there's an identical key in the tree,
	 * return an error unless the data item has already been marked for
	 * deletion, or, all the remaining data items have already been marked
	 * for deletion in the case of duplicates.  If all the data items have
	 * been marked for deletion, we do a replace, otherwise, it has to be
	 * a set of duplicates, and we simply append a new one to the set.
	 */
	isdeleted = 0;
	if (exact) {
		if ((ret = __bam_isdeleted(dbp, h, indx, &isdeleted)) != 0)
			goto err;
		if (isdeleted)
			__bam_ca_replace(dbp, h->pgno, indx, REPLACE_SETUP);
		else
			if (flags == DB_NOOVERWRITE) {
				ret = DB_KEYEXIST;
				goto err;
			}
	}

	/*
	 * If we're inserting into the first or last page of the tree,
	 * remember where we did it so we can do fast lookup next time.
	 *
	 * XXX
	 * Does reverse order still work (did it ever!?!?)
	 */
	t->bt_lpgno =
	    h->next_pgno == PGNO_INVALID || h->prev_pgno == PGNO_INVALID ?
	    h->pgno : PGNO_INVALID;

	/*
	 * Select the arguments for __bam_iitem() and do the insert.  If the
	 * key is an exact match, we're either adding a new duplicate at the
	 * end of the duplicate set, or we're replacing the data item with a
	 * new data item.  If the key isn't an exact match, we're inserting
	 * a new key/data pair, before the search location.
	 */
	newkey = dbp->type == DB_BTREE && !exact;
	if (exact) {
		if (!isdeleted && F_ISSET(dbp, DB_AM_DUP)) {
			/*
			 * Make sure that we're not looking at a page of
			 * duplicates -- if so, move to the last entry on
			 * that page.
			 */
			c.page = h;
			c.pgno = h->pgno;
			c.indx = indx;
			c.dpgno = PGNO_INVALID;
			c.dindx = 0;
			if ((ret =
			    __bam_ovfl_chk(dbp, &c, indx + O_INDX, 1)) != 0)
				goto err;
			if (c.dpgno != PGNO_INVALID) {
				/*
				 * XXX
				 * The __bam_ovfl_chk() routine memp_fput() the
				 * current page and acquired a new one, but did
				 * not do anything about the lock we're holding.
				 */
				t->bt_csp->page = h = c.page;
				indx = c.dindx;
			}
			insert_flags = DB_AFTER;
		} else
			insert_flags = DB_CURRENT;
	} else
		insert_flags = DB_BEFORE;

	/*
	 * The pages we're using may be modified by __bam_iitem(), so make
	 * sure we reset the stack.
	 */
	iitem_flags = 0;
	if (newkey)
		iitem_flags |= BI_NEWKEY;
	if (isdeleted)
		iitem_flags |= BI_DOINCR;
	ret = __bam_iitem(dbp, &h, &indx, key, data, insert_flags, iitem_flags);
	t->bt_csp->page = h;
	t->bt_csp->indx = indx;

	switch (ret) {
	case 0:
		/* Done.  Clean up the cursor. */
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

		if ((ret = __bam_split(dbp, key)) != 0)
			break;

		goto retry;
		/* NOTREACHED */
	default:
		if (isdeleted)
			__bam_ca_replace(dbp, h->pgno, indx, REPLACE_FAILED);
		break;
	}

err:	if (stack)
		(void)__bam_stkrel(dbp);

	PUTHANDLE(dbp);
	return (ret);
}

/*
 * __bam_isdeleted --
 *	Return if the only remaining data item for the element has been
 *	deleted.
 */
static int
__bam_isdeleted(dbp, h, indx, isdeletedp)
	DB *dbp;
	PAGE *h;
	u_int32_t indx;
	int *isdeletedp;
{
	BKEYDATA *bk;
	db_pgno_t pgno;
	int ret;

	*isdeletedp = 1;
	for (;;) {
		bk = GET_BKEYDATA(h, indx + O_INDX);
		switch (B_TYPE(bk->type)) {
		case B_KEYDATA:
		case B_OVERFLOW:
			if (!B_DISSET(bk->type)) {
				*isdeletedp = 0;
				return (0);
			}
			break;
		case B_DUPLICATE:
			/*
			 * If the data item referencing the off-page duplicates
			 * is flagged as deleted, we're done.  Else, we have to
			 * walk the chain of duplicate pages.
			 */
			if (B_DISSET(bk->type))
				return (0);
			goto dupchk;
		default:
			return (__db_pgfmt(dbp, h->pgno));
		}

		/*
		 * If there are no more on-page duplicate items, then every
		 * data item for this key must have been deleted.
		 */
		if (indx + P_INDX >= (u_int32_t)NUM_ENT(h))
			return (0);
		if (h->inp[indx] != h->inp[indx + P_INDX])
			return (0);

		/* Check the next item. */
		indx += P_INDX;
	}
	/* NOTREACHED */

dupchk:	/* Check a chain of duplicate pages. */
	pgno = ((BOVERFLOW *)bk)->pgno;
	for (;;) {
		/* Acquire the next page in the duplicate chain. */
		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &h)) != 0)
			return (ret);

		/* Check each item for a delete flag. */
		for (indx = 0; indx < NUM_ENT(h); ++indx)
			if (!B_DISSET(GET_BKEYDATA(h, indx)->type)) {
				*isdeletedp = 0;
				goto done;
			}
		/*
		 * If we reach the end of the duplicate pages, then every
		 * item we reviewed must have been deleted.
		 */
		if ((pgno = NEXT_PGNO(h)) == PGNO_INVALID)
			goto done;

		(void)memp_fput(dbp->mpf, h, 0);
	}
	/* NOTREACHED */

done:	(void)memp_fput(dbp->mpf, h, 0);
	return (0);
}

/*
 * __bam_lookup --
 *	Find the right location in the tree for the key.
 */
static int
__bam_lookup(dbp, key, exactp)
	DB *dbp;
	DBT *key;
	int *exactp;
{
	BTREE *t;
	DB_LOCK lock;
	EPG e;
	PAGE *h;
	db_indx_t indx;
	int cmp, ret;

	t = dbp->internal;
	h = NULL;

	/*
	 * Record numbers can't be fast-tracked, we have to lock the entire
	 * tree.
	 */
	if (F_ISSET(dbp, DB_BT_RECNUM))
		goto slow;

	/* Check to see if we've been seeing sorted input. */
	if (t->bt_lpgno == PGNO_INVALID)
		goto slow;

	/*
	 * Retrieve the page on which we did the last insert.  It's okay if
	 * it doesn't exist, or if it's not the page type we expect, it just
	 * means that the world changed.
	 */
	if (__bam_lget(dbp, 0, t->bt_lpgno, DB_LOCK_WRITE, &lock))
		goto miss;
	if (__bam_pget(dbp, &h, &t->bt_lpgno, 0)) {
		(void)__BT_LPUT(dbp, lock);
		goto miss;
	}
	if (TYPE(h) != P_LBTREE)
		goto miss;
	if (NUM_ENT(h) == 0)
		goto miss;

	/*
	 * We have to be at the end or beginning of the tree to know that
	 * we're inserting in a sort order.  If that's the case and we're
	 * in the right order in comparison to the first/last key/data pair,
	 * we have the right position.
	 */
	if (h->next_pgno == PGNO_INVALID) {
		e.page = h;
		e.indx = NUM_ENT(h) - P_INDX;
		if ((cmp = __bam_cmp(dbp, key, &e)) >= 0) {
			if (cmp > 0)
				e.indx += P_INDX;
			goto fast;
		}
	}
	if (h->prev_pgno == PGNO_INVALID) {
		e.page = h;
		e.indx = 0;
		if ((cmp = __bam_cmp(dbp, key, &e)) <= 0) {
			/*
			 * We're doing a put, so we want to insert as the last
			 * of any set of duplicates.
			 */
			if (cmp == 0) {
				for (indx = 0;
				    indx < (db_indx_t)(NUM_ENT(h) - P_INDX) &&
				    h->inp[indx] == h->inp[indx + P_INDX];
				    indx += P_INDX)
					;
				e.indx = indx;
			}
			goto fast;
		}
	}
	goto miss;

	/* Set the exact match flag in case we've already inserted this key. */
fast:	*exactp = cmp == 0;

	/* Enter the entry in the stack. */
	BT_STK_CLR(t);
	BT_STK_ENTER(t, e.page, e.indx, lock, ret);
	if (ret != 0)
		return (ret);

	++t->lstat.bt_cache_hit;
	return (0);

miss:	++t->lstat.bt_cache_miss;
	if (h != NULL) {
		(void)memp_fput(dbp->mpf, h, 0);
		(void)__BT_LPUT(dbp, lock);
	}

slow:	return (__bam_search(dbp, key, S_INSERT, 1, NULL, exactp));
}

/*
 * __bam_iitem --
 *	Insert an item into the tree.
 *
 * PUBLIC: int __bam_iitem __P((DB *,
 * PUBLIC:    PAGE **, db_indx_t *, DBT *, DBT *, u_int32_t, u_int32_t));
 */
int
__bam_iitem(dbp, hp, indxp, key, data, op, flags)
	DB *dbp;
	PAGE **hp;
	db_indx_t *indxp;
	DBT *key, *data;
	u_int32_t op, flags;
{
	BTREE *t;
	BKEYDATA *bk;
	DBT tdbt;
	PAGE *h;
	db_indx_t indx, nbytes;
	u_int32_t data_size, have_bytes, need_bytes, needed;
	int bigkey, bigdata, dupadjust, replace, ret;

	COMPQUIET(bk, NULL);

	t = dbp->internal;
	h = *hp;
	indx = *indxp;
	dupadjust = replace = 0;

	/*
	 * If it's a page of duplicates, call the common code to do the work.
	 *
	 * !!!
	 * Here's where the hp and indxp are important.  The duplicate code
	 * may decide to rework/rearrange the pages and indices we're using,
	 * so the caller must understand that the page stack may change.
	 */
	if (TYPE(h) == P_DUPLICATE) {
		/* Adjust the index for the new item if it's a DB_AFTER op. */
		if (op == DB_AFTER)
			++*indxp;

		/* Remove the current item if it's a DB_CURRENT op. */
		if (op == DB_CURRENT) {
			bk = GET_BKEYDATA(*hp, *indxp);
			switch (B_TYPE(bk->type)) {
			case B_KEYDATA:
				nbytes = BKEYDATA_SIZE(bk->len);
				break;
			case B_OVERFLOW:
				nbytes = BOVERFLOW_SIZE;
				break;
			default:
				return (__db_pgfmt(dbp, h->pgno));
			}
			if ((ret = __db_ditem(dbp, *hp, *indxp, nbytes)) != 0)
				return (ret);
		}

		/* Put the new/replacement item onto the page. */
		if ((ret = __db_dput(dbp, data, hp, indxp, __bam_new)) != 0)
			return (ret);

		goto done;
	}

	/* Handle fixed-length records: build the real record. */
	if (F_ISSET(dbp, DB_RE_FIXEDLEN) && data->size != t->bt_recno->re_len) {
		tdbt = *data;
		if ((ret = __bam_fixed(t, &tdbt)) != 0)
			return (ret);
		data = &tdbt;
	}

	/*
	 * Figure out how much space the data will take, including if it's a
	 * partial record.  If either of the key or data items won't fit on
	 * a page, we'll have to store them on overflow pages.
	 */
	bigkey = LF_ISSET(BI_NEWKEY) && key->size > t->bt_ovflsize;
	data_size = F_ISSET(data, DB_DBT_PARTIAL) ?
	    __bam_partsize(data, h, indx) : data->size;
	bigdata = data_size > t->bt_ovflsize;

	needed = 0;
	if (LF_ISSET(BI_NEWKEY)) {
		/* If BI_NEWKEY is set we're adding a new key and data pair. */
		if (bigkey)
			needed += BOVERFLOW_PSIZE;
		else
			needed += BKEYDATA_PSIZE(key->size);
		if (bigdata)
			needed += BOVERFLOW_PSIZE;
		else
			needed += BKEYDATA_PSIZE(data_size);
	} else {
		/*
		 * We're either overwriting the data item of a key/data pair
		 * or we're adding the data item only, i.e. a new duplicate.
		 */
		if (op == DB_CURRENT) {
			bk = GET_BKEYDATA(h,
			    indx + (TYPE(h) == P_LBTREE ? O_INDX : 0));
			if (B_TYPE(bk->type) == B_KEYDATA)
				have_bytes = BKEYDATA_PSIZE(bk->len);
			else
				have_bytes = BOVERFLOW_PSIZE;
			need_bytes = 0;
		} else {
			have_bytes = 0;
			need_bytes = sizeof(db_indx_t);
		}
		if (bigdata)
			need_bytes += BOVERFLOW_PSIZE;
		else
			need_bytes += BKEYDATA_PSIZE(data_size);

		if (have_bytes < need_bytes)
			needed += need_bytes - have_bytes;
	}

	/*
	 * If there's not enough room, or the user has put a ceiling on the
	 * number of keys permitted in the page, split the page.
	 *
	 * XXX
	 * The t->bt_maxkey test here may be insufficient -- do we have to
	 * check in the btree split code, so we don't undo it there!?!?
	 */
	if (P_FREESPACE(h) < needed ||
	    (t->bt_maxkey != 0 && NUM_ENT(h) > t->bt_maxkey))
		return (DB_NEEDSPLIT);

	/* Handle partial puts: build the real record. */
	if (F_ISSET(data, DB_DBT_PARTIAL)) {
		tdbt = *data;
		if ((ret = __bam_partial(dbp, &tdbt, h, indx, data_size)) != 0)
			return (ret);
		data = &tdbt;
	}

	/*
	 * The code breaks it up into six cases:
	 *
	 * 1. Append a new key/data pair.
	 * 2. Insert a new key/data pair.
	 * 3. Append a new data item (a new duplicate).
	 * 4. Insert a new data item (a new duplicate).
	 * 5. Overflow item: delete and re-add the data item.
	 * 6. Replace the data item.
	 */
	if (LF_ISSET(BI_NEWKEY)) {
		switch (op) {
		case DB_AFTER:		/* 1. Append a new key/data pair. */
			indx += 2;
			*indxp += 2;
			break;
		case DB_BEFORE:		/* 2. Insert a new key/data pair. */
			break;
		default:
			return (EINVAL);
		}

		/* Add the key. */
		if (bigkey) {
			if ((ret = __bam_ovput(dbp, h, indx, key)) != 0)
				return (ret);
		} else
			if ((ret = __db_pitem(dbp, h, indx,
			    BKEYDATA_SIZE(key->size), NULL, key)) != 0)
				return (ret);
		++indx;
	} else {
		switch (op) {
		case DB_AFTER:		/* 3. Append a new data item. */
			if (TYPE(h) == P_LBTREE) {
				/*
				 * Adjust the cursor and copy in the key for
				 * the duplicate.
				 */
				if ((ret = __bam_adjindx(dbp,
				    h, indx + P_INDX, indx, 1)) != 0)
					return (ret);

				indx += 3;
				dupadjust = 1;

				*indxp += 2;
			} else {
				++indx;
				__bam_ca_di(dbp, h->pgno, indx, 1);

				*indxp += 1;
			}
			break;
		case DB_BEFORE:		/* 4. Insert a new data item. */
			if (TYPE(h) == P_LBTREE) {
				/*
				 * Adjust the cursor and copy in the key for
				 * the duplicate.
				 */
				if ((ret =
				    __bam_adjindx(dbp, h, indx, indx, 1)) != 0)
					return (ret);

				++indx;
				dupadjust = 1;
			} else
				__bam_ca_di(dbp, h->pgno, indx, 1);
			break;
		case DB_CURRENT:
			if (TYPE(h) == P_LBTREE)
				++indx;

			/*
			 * 5. Delete/re-add the data item.
			 *
			 * If we're dealing with offpage items, we have to
			 * delete and then re-add the item.
			 */
			if (bigdata || B_TYPE(bk->type) != B_KEYDATA) {
				if ((ret = __bam_ditem(dbp, h, indx)) != 0)
					return (ret);
				break;
			}

			/* 6. Replace the data item. */
			replace = 1;
			break;
		default:
			return (EINVAL);
		}
	}

	/* Add the data. */
	if (bigdata) {
		if ((ret = __bam_ovput(dbp, h, indx, data)) != 0)
			return (ret);
	} else {
		BKEYDATA __bk;
		DBT __hdr;

		if (LF_ISSET(BI_DELETED)) {
			B_TSET(__bk.type, B_KEYDATA, 1);
			__bk.len = data->size;
			__hdr.data = &__bk;
			__hdr.size = SSZA(BKEYDATA, data);
			ret = __db_pitem(dbp, h, indx,
			    BKEYDATA_SIZE(data->size), &__hdr, data);
		} else if (replace)
			ret = __bam_ritem(dbp, h, indx, data);
		else
			ret = __db_pitem(dbp, h, indx,
			    BKEYDATA_SIZE(data->size), NULL, data);
		if (ret != 0)
			return (ret);
	}

	if ((ret = memp_fset(dbp->mpf, h, DB_MPOOL_DIRTY)) != 0)
		return (ret);

	/*
	 * If the page is at least 50% full, and we added a duplicate, see if
	 * that set of duplicates takes up at least 25% of the space.  If it
	 * does, move it off onto its own page.
	 */
	if (dupadjust && P_FREESPACE(h) <= dbp->pgsize / 2) {
		--indx;
		if ((ret = __bam_ndup(dbp, h, indx)) != 0)
			return (ret);
	}

	/*
	 * If we've changed the record count, update the tree.  Record counts
	 * need to be updated in recno databases and in btree databases where
	 * we are supporting records.  In both cases, adjust the count if the
	 * operation wasn't performed on the current record or when the caller
	 * overrides and wants the adjustment made regardless.
	 */
done:	if (LF_ISSET(BI_DOINCR) ||
	    (op != DB_CURRENT &&
	    (F_ISSET(dbp, DB_BT_RECNUM) || dbp->type == DB_RECNO)))
		if ((ret = __bam_adjust(dbp, t, 1)) != 0)
			return (ret);

	/* If we've modified a recno file, set the flag */
	if (t->bt_recno != NULL)
		F_SET(t->bt_recno, RECNO_MODIFIED);

	++t->lstat.bt_added;

	return (ret);
}

/*
 * __bam_partsize --
 *	Figure out how much space a partial data item is in total.
 */
static u_int32_t
__bam_partsize(data, h, indx)
	DBT *data;
	PAGE *h;
	u_int32_t indx;
{
	BKEYDATA *bk;
	u_int32_t nbytes;

	/*
	 * Figure out how much total space we'll need.  If the record doesn't
	 * already exist, it's simply the data we're provided.
	 */
	if (indx >= NUM_ENT(h))
		return (data->doff + data->size);

	/*
	 * Otherwise, it's the data provided plus any already existing data
	 * that we're not replacing.
	 */
	bk = GET_BKEYDATA(h, indx + (TYPE(h) == P_LBTREE ? O_INDX : 0));
	nbytes =
	    B_TYPE(bk->type) == B_OVERFLOW ? ((BOVERFLOW *)bk)->tlen : bk->len;

	/*
	 * There are really two cases here:
	 *
	 * Case 1: We are replacing some bytes that do not exist (i.e., they
	 * are past the end of the record).  In this case the number of bytes
	 * we are replacing is irrelevant and all we care about is how many
	 * bytes we are going to add from offset.  So, the new record length
	 * is going to be the size of the new bytes (size) plus wherever those
	 * new bytes begin (doff).
	 *
	 * Case 2: All the bytes we are replacing exist.  Therefore, the new
	 * size is the oldsize (nbytes) minus the bytes we are replacing (dlen)
	 * plus the bytes we are adding (size).
	 */
	if (nbytes < data->doff + data->dlen)		/* Case 1 */
		return (data->doff + data->size);

	return (nbytes + data->size - data->dlen);	/* Case 2 */
}

/*
 * OVPUT --
 *	Copy an overflow item onto a page.
 */
#undef	OVPUT
#define	OVPUT(h, indx, bo) do {						\
	DBT __hdr;							\
	memset(&__hdr, 0, sizeof(__hdr));				\
	__hdr.data = &bo;						\
	__hdr.size = BOVERFLOW_SIZE;					\
	if ((ret = __db_pitem(dbp,					\
	    h, indx, BOVERFLOW_SIZE, &__hdr, NULL)) != 0)		\
		return (ret);						\
} while (0)

/*
 * __bam_ovput --
 *	Build an overflow item and put it on the page.
 */
static int
__bam_ovput(dbp, h, indx, item)
	DB *dbp;
	PAGE *h;
	u_int32_t indx;
	DBT *item;
{
	BOVERFLOW bo;
	int ret;

	B_TSET(bo.type, B_OVERFLOW, 0);
	bo.tlen = item->size;
	if ((ret = __db_poff(dbp, item, &bo.pgno, __bam_new)) != 0)
		return (ret);

	OVPUT(h, indx, bo);

	return (0);
}

/*
 * __bam_ritem --
 *	Replace an item on a page.
 *
 * PUBLIC: int __bam_ritem __P((DB *, PAGE *, u_int32_t, DBT *));
 */
int
__bam_ritem(dbp, h, indx, data)
	DB *dbp;
	PAGE *h;
	u_int32_t indx;
	DBT *data;
{
	BKEYDATA *bk;
	DBT orig, repl;
	db_indx_t cnt, lo, ln, min, off, prefix, suffix;
	int32_t nbytes;
	int ret;
	u_int8_t *p, *t;

	/*
	 * Replace a single item onto a page.  The logic figuring out where
	 * to insert and whether it fits is handled in the caller.  All we do
	 * here is manage the page shuffling.
	 */
	bk = GET_BKEYDATA(h, indx);

	/* Log the change. */
	if (DB_LOGGING(dbp)) {
		/*
		 * We might as well check to see if the two data items share
		 * a common prefix and suffix -- it can save us a lot of log
		 * message if they're large.
		 */
		min = data->size < bk->len ? data->size : bk->len;
		for (prefix = 0,
		    p = bk->data, t = data->data;
		    prefix < min && *p == *t; ++prefix, ++p, ++t)
			;

		min -= prefix;
		for (suffix = 0,
		    p = (u_int8_t *)bk->data + bk->len - 1,
		    t = (u_int8_t *)data->data + data->size - 1;
		    suffix < min && *p == *t; ++suffix, --p, --t)
			;

		/* We only log the parts of the keys that have changed. */
		orig.data = (u_int8_t *)bk->data + prefix;
		orig.size = bk->len - (prefix + suffix);
		repl.data = (u_int8_t *)data->data + prefix;
		repl.size = data->size - (prefix + suffix);
		if ((ret = __bam_repl_log(dbp->dbenv->lg_info, dbp->txn,
		    &LSN(h), 0, dbp->log_fileid, PGNO(h), &LSN(h),
		    (u_int32_t)indx, (u_int32_t)B_DISSET(bk->type),
		    &orig, &repl, (u_int32_t)prefix, (u_int32_t)suffix)) != 0)
			return (ret);
	}

	/*
	 * Set references to the first in-use byte on the page and the
	 * first byte of the item being replaced.
	 */
	p = (u_int8_t *)h + HOFFSET(h);
	t = (u_int8_t *)bk;

	/*
	 * If the entry is growing in size, shift the beginning of the data
	 * part of the page down.  If the entry is shrinking in size, shift
	 * the beginning of the data part of the page up.  Use memmove(3),
	 * the regions overlap.
	 */
	lo = BKEYDATA_SIZE(bk->len);
	ln = BKEYDATA_SIZE(data->size);
	if (lo != ln) {
		nbytes = lo - ln;		/* Signed difference. */
		if (p == t)			/* First index is fast. */
			h->inp[indx] += nbytes;
		else {				/* Else, shift the page. */
			memmove(p + nbytes, p, t - p);

			/* Adjust the indices' offsets. */
			off = h->inp[indx];
			for (cnt = 0; cnt < NUM_ENT(h); ++cnt)
				if (h->inp[cnt] <= off)
					h->inp[cnt] += nbytes;
		}

		/* Clean up the page and adjust the item's reference. */
		HOFFSET(h) += nbytes;
		t += nbytes;
	}

	/* Copy the new item onto the page. */
	bk = (BKEYDATA *)t;
	B_TSET(bk->type, B_KEYDATA, 0);
	bk->len = data->size;
	memcpy(bk->data, data->data, data->size);

	return (0);
}

/*
 * __bam_ndup --
 *	Check to see if the duplicate set at indx should have its own page.
 *	If it should, create it.
 */
static int
__bam_ndup(dbp, h, indx)
	DB *dbp;
	PAGE *h;
	u_int32_t indx;
{
	BKEYDATA *bk;
	BOVERFLOW bo;
	DBT hdr;
	PAGE *cp;
	db_indx_t cnt, cpindx, first, sz;
	int ret;

	while (indx > 0 && h->inp[indx] == h->inp[indx - P_INDX])
		indx -= P_INDX;
	for (cnt = 0, sz = 0, first = indx;; ++cnt, indx += P_INDX) {
		if (indx >= NUM_ENT(h) || h->inp[first] != h->inp[indx])
			break;
		bk = GET_BKEYDATA(h, indx);
		sz += B_TYPE(bk->type) == B_KEYDATA ?
		    BKEYDATA_PSIZE(bk->len) : BOVERFLOW_PSIZE;
		bk = GET_BKEYDATA(h, indx + O_INDX);
		sz += B_TYPE(bk->type) == B_KEYDATA ?
		    BKEYDATA_PSIZE(bk->len) : BOVERFLOW_PSIZE;
	}

	/*
	 * If this set of duplicates is using more than 25% of the page, move
	 * them off.  The choice of 25% is a WAG, but it has to be small enough
	 * that we can always split regardless of the presence of duplicates.
	 */
	if (sz < dbp->pgsize / 4)
		return (0);

	/* Get a new page. */
	if ((ret = __bam_new(dbp, P_DUPLICATE, &cp)) != 0)
		return (ret);

	/*
	 * Move this set of duplicates off the page.  First points to the first
	 * key of the first duplicate key/data pair, cnt is the number of pairs
	 * we're dealing with.
	 */
	memset(&hdr, 0, sizeof(hdr));
	for (indx = first + O_INDX, cpindx = 0;; ++cpindx) {
		/* Copy the entry to the new page. */
		bk = GET_BKEYDATA(h, indx);
		hdr.data = bk;
		hdr.size = B_TYPE(bk->type) == B_KEYDATA ?
		    BKEYDATA_SIZE(bk->len) : BOVERFLOW_SIZE;
		if ((ret =
		    __db_pitem(dbp, cp, cpindx, hdr.size, &hdr, NULL)) != 0)
			goto err;

		/*
		 * Move cursors referencing the old entry to the new entry.
		 * Done after the page put because __db_pitem() adjusts
		 * cursors on the new page, and before the delete because
		 * __db_ditem adjusts cursors on the old page.
		 */
		__bam_ca_dup(dbp,
		    PGNO(h), first, indx - O_INDX, PGNO(cp), cpindx);

		/* Delete the data item. */
		if ((ret = __db_ditem(dbp, h, indx, hdr.size)) != 0)
			goto err;

		/* Delete all but the first reference to the key. */
		if (--cnt == 0)
			break;
		if ((ret = __bam_adjindx(dbp, h, indx, first, 0)) != 0)
			goto err;
	}

	/* Put in a new data item that points to the duplicates page. */
	B_TSET(bo.type, B_DUPLICATE, 0);
	bo.pgno = cp->pgno;
	bo.tlen = 0;

	OVPUT(h, indx, bo);

	return (memp_fput(dbp->mpf, cp, DB_MPOOL_DIRTY));

err:	(void)__bam_free(dbp, cp);
	return (ret);
}

/*
 * __bam_fixed --
 *	Build the real record for a fixed length put.
 */
static int
__bam_fixed(t, dbt)
	BTREE *t;
	DBT *dbt;
{
	RECNO *rp;

	rp = t->bt_recno;

	/*
	 * If database contains fixed-length records, and the record is long,
	 * return EINVAL.
	 */
	if (dbt->size > rp->re_len)
		return (EINVAL);

	/*
	 * The caller checked to see if it was just right, so we know it's
	 * short.  Pad it out.  We use the record data return memory, it's
	 * only a short-term use.
	 */
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
	memcpy(t->bt_rdata.data, dbt->data, dbt->size);
	memset((u_int8_t *)t->bt_rdata.data + dbt->size,
	    rp->re_pad, rp->re_len - dbt->size);

	/*
	 * Clean up our flags and other information just in case, and
	 * change the caller's DBT to reference our created record.
	 */
	t->bt_rdata.size = rp->re_len;
	t->bt_rdata.dlen = 0;
	t->bt_rdata.doff = 0;
	t->bt_rdata.flags = 0;
	*dbt = t->bt_rdata;

	return (0);
}

/*
 * __bam_partial --
 *	Build the real record for a partial put.
 */
static int
__bam_partial(dbp, dbt, h, indx, nbytes)
	DB *dbp;
	DBT *dbt;
	PAGE *h;
	u_int32_t indx, nbytes;
{
	BTREE *t;
	BKEYDATA *bk, tbk;
	BOVERFLOW *bo;
	DBT copy;
	u_int32_t len, tlen;
	u_int8_t *p;
	int ret;

	COMPQUIET(bo, NULL);

	t = dbp->internal;

	/* We use the record data return memory, it's only a short-term use. */
	if (t->bt_rdata.ulen < nbytes) {
		t->bt_rdata.data = t->bt_rdata.data == NULL ?
		    (void *)__db_malloc(nbytes) :
		    (void *)__db_realloc(t->bt_rdata.data, nbytes);
		if (t->bt_rdata.data == NULL) {
			t->bt_rdata.ulen = 0;
			return (ENOMEM);
		}
		t->bt_rdata.ulen = nbytes;
	}

	/* Find the current record. */
	if (indx < NUM_ENT(h)) {
		bk = GET_BKEYDATA(h, indx + (TYPE(h) == P_LBTREE ? O_INDX : 0));
		bo = (BOVERFLOW *)bk;
	} else {
		bk = &tbk;
		B_TSET(bk->type, B_KEYDATA, 0);
		bk->len = 0;
	}

	/*
	 * We use nul bytes for any part of the record that isn't specified,
	 * get it over with.
	 */
	memset(t->bt_rdata.data, 0, nbytes);

	if (B_TYPE(bk->type) == B_OVERFLOW) {
		/*
		 * In the case of an overflow record, we shift things around
		 * in the current record rather than allocate a separate copy.
		 */
		memset(&copy, 0, sizeof(copy));
		if ((ret = __db_goff(dbp, &copy, bo->tlen,
		    bo->pgno, &t->bt_rdata.data, &t->bt_rdata.ulen)) != 0)
			return (ret);

		/* Skip any leading data from the original record. */
		tlen = dbt->doff;
		p = (u_int8_t *)t->bt_rdata.data + dbt->doff;

		/*
		 * Copy in any trailing data from the original record.
		 *
		 * If the original record was larger than the original offset
		 * plus the bytes being deleted, there is trailing data in the
		 * original record we need to preserve.  If we aren't deleting
		 * the same number of bytes as we're inserting, copy it up or
		 * down, into place.
		 *
		 * Use memmove(), the regions may overlap.
		 */
		if (bo->tlen > dbt->doff + dbt->dlen) {
			len = bo->tlen - (dbt->doff + dbt->dlen);
			if (dbt->dlen != dbt->size)
				memmove(p + dbt->size, p + dbt->dlen, len);
			tlen += len;
		}

		/* Copy in the application provided data. */
		memcpy(p, dbt->data, dbt->size);
		tlen += dbt->size;
	} else {
		/* Copy in any leading data from the original record. */
		memcpy(t->bt_rdata.data,
		    bk->data, dbt->doff > bk->len ? bk->len : dbt->doff);
		tlen = dbt->doff;
		p = (u_int8_t *)t->bt_rdata.data + dbt->doff;

		/* Copy in the application provided data. */
		memcpy(p, dbt->data, dbt->size);
		tlen += dbt->size;

		/* Copy in any trailing data from the original record. */
		len = dbt->doff + dbt->dlen;
		if (bk->len > len) {
			memcpy(p + dbt->size, bk->data + len, bk->len - len);
			tlen += bk->len - len;
		}
	}

	/* Set the DBT to reference our new record. */
	t->bt_rdata.size = tlen;
	t->bt_rdata.dlen = 0;
	t->bt_rdata.doff = 0;
	t->bt_rdata.flags = 0;
	*dbt = t->bt_rdata;
	return (0);
}
