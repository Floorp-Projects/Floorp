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
static const char sccsid[] = "@(#)bt_delete.c	10.31 (Sleepycat) 5/6/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"

static int __bam_dpages __P((DB *, BTREE *));

/*
 * __bam_delete --
 *	Delete the items referenced by a key.
 *
 * PUBLIC: int __bam_delete __P((DB *, DB_TXN *, DBT *, u_int32_t));
 */
int
__bam_delete(argdbp, txn, key, flags)
	DB *argdbp;
	DB_TXN *txn;
	DBT *key;
	u_int32_t flags;
{
	BTREE *t;
	DB *dbp;
	PAGE *h;
	db_indx_t cnt, i, indx;
	int dpage, exact, ret, stack;

	DEBUG_LWRITE(argdbp, txn, "bam_delete", key, NULL, flags);

	stack = 0;

	/* Check for invalid flags. */
	if ((ret = __db_delchk(argdbp,
	    key, flags, F_ISSET(argdbp, DB_AM_RDONLY))) != 0)
		return (ret);

	GETHANDLE(argdbp, txn, &dbp, ret);
	t = dbp->internal;

	/* Search the tree for the key; delete only deletes exact matches. */
	if ((ret = __bam_search(dbp, key, S_DELETE, 1, NULL, &exact)) != 0)
		goto err;
	stack = 1;
	h = t->bt_csp->page;
	indx = t->bt_csp->indx;

	/* Delete the key/data pair, including any on-or-off page duplicates. */
	for (cnt = 1, i = indx;; ++cnt)
		if ((i += P_INDX) >= NUM_ENT(h) || h->inp[i] != h->inp[indx])
			break;
	for (; cnt > 0; --cnt, ++t->lstat.bt_deleted)
		if (__bam_ca_delete(dbp, h->pgno, indx, NULL, 1) == 0) {
			/*
			 * XXX
			 * Delete the key item first, otherwise the duplicate
			 * checks in __bam_ditem() won't work!
			 */
			if ((ret = __bam_ditem(dbp, h, indx)) != 0)
				goto err;
			if ((ret = __bam_ditem(dbp, h, indx)) != 0)
				goto err;
		} else {
			B_DSET(GET_BKEYDATA(h, indx + O_INDX)->type);
			indx += P_INDX;
		}

	/* If we're using record numbers, update internal page record counts. */
	if (F_ISSET(dbp, DB_BT_RECNUM) && (ret = __bam_adjust(dbp, t, -1)) != 0)
		goto err;

	/* If the page is now empty, delete it. */
	dpage = NUM_ENT(h) == 0 && h->pgno != PGNO_ROOT;

	__bam_stkrel(dbp);
	stack = 0;

	ret = dpage ? __bam_dpage(dbp, key) : 0;

err:	if (stack)
		__bam_stkrel(dbp);
	PUTHANDLE(dbp);
	return (ret);
}

/*
 * __ram_delete --
 *	Delete the items referenced by a key.
 *
 * PUBLIC: int __ram_delete __P((DB *, DB_TXN *, DBT *, u_int32_t));
 */
int
__ram_delete(argdbp, txn, key, flags)
	DB *argdbp;
	DB_TXN *txn;
	DBT *key;
	u_int32_t flags;
{
	BKEYDATA bk;
	BTREE *t;
	DB *dbp;
	DBT hdr, data;
	PAGE *h;
	db_indx_t indx;
	db_recno_t recno;
	int exact, ret, stack;

	stack = 0;

	/* Check for invalid flags. */
	if ((ret = __db_delchk(argdbp,
	    key, flags, F_ISSET(argdbp, DB_AM_RDONLY))) != 0)
		return (ret);

	GETHANDLE(argdbp, txn, &dbp, ret);
	t = dbp->internal;

	/* Check the user's record number and fill in as necessary. */
	if ((ret = __ram_getno(argdbp, key, &recno, 0)) != 0)
		goto err;

	/* Search the tree for the key; delete only deletes exact matches. */
	if ((ret = __bam_rsearch(dbp, &recno, S_DELETE, 1, &exact)) != 0)
		goto err;
	if (!exact) {
		ret = DB_NOTFOUND;
		goto err;
	}

	h = t->bt_csp->page;
	indx = t->bt_csp->indx;
	stack = 1;

	/* If the record has already been deleted, we couldn't have found it. */
	if (B_DISSET(GET_BKEYDATA(h, indx)->type)) {
		ret = DB_KEYEMPTY;
		goto done;
	}

	/*
	 * If we're not renumbering records, replace the record with a marker
	 * and return.
	 */
	if (!F_ISSET(dbp, DB_RE_RENUMBER)) {
		if ((ret = __bam_ditem(dbp, h, indx)) != 0)
			goto err;

		B_TSET(bk.type, B_KEYDATA, 1);
		bk.len = 0;
		memset(&hdr, 0, sizeof(hdr));
		hdr.data = &bk;
		hdr.size = SSZA(BKEYDATA, data);
		memset(&data, 0, sizeof(data));
		data.data = (char *)"";
		data.size = 0;
		if ((ret = __db_pitem(dbp,
		    h, indx, BKEYDATA_SIZE(0), &hdr, &data)) != 0)
			goto err;

		++t->lstat.bt_deleted;
		goto done;
	}

	/* Delete the item. */
	if ((ret = __bam_ditem(dbp, h, indx)) != 0)
		goto err;

	++t->lstat.bt_deleted;
	if (t->bt_recno != NULL)
		F_SET(t->bt_recno, RECNO_MODIFIED);

	/* Adjust the counts. */
	__bam_adjust(dbp, t, -1);

	/* Adjust the cursors. */
	__ram_ca(dbp, recno, CA_DELETE);

	/*
	 * If the page is now empty, delete it -- we have the whole tree
	 * locked, so there are no preparations to make.  Else, release
	 * the pages.
	 */
	if (NUM_ENT(h) == 0 && h->pgno != PGNO_ROOT) {
		stack = 0;
		ret = __bam_dpages(dbp, t);
	}

done:
err:	if (stack)
		__bam_stkrel(dbp);

	PUTHANDLE(dbp);
	return (ret);
}

/*
 * __bam_ditem --
 *	Delete one or more entries from a page.
 *
 * PUBLIC: int __bam_ditem __P((DB *, PAGE *, u_int32_t));
 */
int
__bam_ditem(dbp, h, indx)
	DB *dbp;
	PAGE *h;
	u_int32_t indx;
{
	BINTERNAL *bi;
	BKEYDATA *bk;
	BOVERFLOW *bo;
	u_int32_t nbytes;
	int ret;

	switch (TYPE(h)) {
	case P_IBTREE:
		bi = GET_BINTERNAL(h, indx);
		switch (B_TYPE(bi->type)) {
		case B_DUPLICATE:
		case B_OVERFLOW:
			nbytes = BINTERNAL_SIZE(bi->len);
			bo = (BOVERFLOW *)bi->data;
			goto offpage;
		case B_KEYDATA:
			nbytes = BINTERNAL_SIZE(bi->len);
			break;
		default:
			return (__db_pgfmt(dbp, h->pgno));
		}
		break;
	case P_IRECNO:
		nbytes = RINTERNAL_SIZE;
		break;
	case P_LBTREE:
		/*
		 * If it's a duplicate key, discard the index and don't touch
		 * the actual page item.
		 *
		 * XXX
		 * This works because no data item can have an index matching
		 * any other index so even if the data item is in a key "slot",
		 * it won't match any other index.
		 */
		if ((indx % 2) == 0) {
			/*
			 * Check for a duplicate after us on the page.  NOTE:
			 * we have to delete the key item before deleting the
			 * data item, otherwise the "indx + P_INDX" calculation
			 * won't work!
			 */
			if (indx + P_INDX < (u_int32_t)NUM_ENT(h) &&
			    h->inp[indx] == h->inp[indx + P_INDX])
				return (__bam_adjindx(dbp,
				    h, indx, indx + O_INDX, 0));
			/*
			 * Check for a duplicate before us on the page.  It
			 * doesn't matter if we delete the key item before or
			 * after the data item for the purposes of this one.
			 */
			if (indx > 0 && h->inp[indx] == h->inp[indx - P_INDX])
				return (__bam_adjindx(dbp,
				    h, indx, indx - P_INDX, 0));
		}
		/* FALLTHROUGH */
	case P_LRECNO:
		bk = GET_BKEYDATA(h, indx);
		switch (B_TYPE(bk->type)) {
		case B_DUPLICATE:
		case B_OVERFLOW:
			nbytes = BOVERFLOW_SIZE;
			bo = GET_BOVERFLOW(h, indx);

offpage:		/* Delete duplicate/offpage chains. */
			if (B_TYPE(bo->type) == B_DUPLICATE) {
				if ((ret =
				    __db_ddup(dbp, bo->pgno, __bam_free)) != 0)
					return (ret);
			} else
				if ((ret =
				    __db_doff(dbp, bo->pgno, __bam_free)) != 0)
					return (ret);
			break;
		case B_KEYDATA:
			nbytes = BKEYDATA_SIZE(bk->len);
			break;
		default:
			return (__db_pgfmt(dbp, h->pgno));
		}
		break;
	default:
		return (__db_pgfmt(dbp, h->pgno));
	}

	/* Delete the item. */
	if ((ret = __db_ditem(dbp, h, indx, nbytes)) != 0)
		return (ret);

	/* Mark the page dirty. */
	return (memp_fset(dbp->mpf, h, DB_MPOOL_DIRTY));
}

/*
 * __bam_adjindx --
 *	Adjust an index on the page.
 *
 * PUBLIC: int __bam_adjindx __P((DB *, PAGE *, u_int32_t, u_int32_t, int));
 */
int
__bam_adjindx(dbp, h, indx, indx_copy, is_insert)
	DB *dbp;
	PAGE *h;
	u_int32_t indx, indx_copy;
	int is_insert;
{
	db_indx_t copy;
	int ret;

	/* Log the change. */
	if (DB_LOGGING(dbp) &&
	    (ret = __bam_adj_log(dbp->dbenv->lg_info, dbp->txn, &LSN(h),
	    0, dbp->log_fileid, PGNO(h), &LSN(h), indx, indx_copy,
	    (u_int32_t)is_insert)) != 0)
		return (ret);

	if (is_insert) {
		copy = h->inp[indx_copy];
		if (indx != NUM_ENT(h))
			memmove(&h->inp[indx + O_INDX], &h->inp[indx],
			    sizeof(db_indx_t) * (NUM_ENT(h) - indx));
		h->inp[indx] = copy;
		++NUM_ENT(h);
	} else {
		--NUM_ENT(h);
		if (indx != NUM_ENT(h))
			memmove(&h->inp[indx], &h->inp[indx + O_INDX],
			    sizeof(db_indx_t) * (NUM_ENT(h) - indx));
	}

	/* Mark the page dirty. */
	ret = memp_fset(dbp->mpf, h, DB_MPOOL_DIRTY);

	/* Adjust the cursors. */
	__bam_ca_di(dbp, h->pgno, indx, is_insert ? 1 : -1);
	return (0);
}

/*
 * __bam_dpage --
 *	Delete a page from the tree.
 *
 * PUBLIC: int __bam_dpage __P((DB *, const DBT *));
 */
int
__bam_dpage(dbp, key)
	DB *dbp;
	const DBT *key;
{
	BTREE *t;
	DB_LOCK lock;
	PAGE *h;
	db_pgno_t pgno;
	int level;		/* !!!: has to hold number of tree levels. */
	int exact, ret;

	ret = 0;
	t = dbp->internal;

	/*
	 * The locking protocol is that we acquire locks by walking down the
	 * tree, to avoid the obvious deadlocks.
	 *
	 * Call __bam_search to reacquire the empty leaf page, but this time
	 * get both the leaf page and it's parent, locked.  Walk back up the
	 * tree, until we have the top pair of pages that we want to delete.
	 * Once we have the top page that we want to delete locked, lock the
	 * underlying pages and check to make sure they're still empty.  If
	 * they are, delete them.
	 */
	for (level = LEAFLEVEL;; ++level) {
		/* Acquire a page and its parent, locked. */
		if ((ret =
		    __bam_search(dbp, key, S_WRPAIR, level, NULL, &exact)) != 0)
			return (ret);

		/*
		 * If we reach the root or the page isn't going to be empty
		 * when we delete one record, quit.
		 */
		h = t->bt_csp[-1].page;
		if (h->pgno == PGNO_ROOT || NUM_ENT(h) != 1)
			break;

		/* Release the two locked pages. */
		(void)memp_fput(dbp->mpf, t->bt_csp[-1].page, 0);
		(void)__BT_TLPUT(dbp, t->bt_csp[-1].lock);
		(void)memp_fput(dbp->mpf, t->bt_csp[0].page, 0);
		(void)__BT_TLPUT(dbp, t->bt_csp[0].lock);
	}

	/*
	 * Leave the stack pointer one after the last entry, we may be about
	 * to push more items on the stack.
	 */
	++t->bt_csp;

	/*
	 * t->bt_csp[-2].page is the top page, which we're not going to delete,
	 * and t->bt_csp[-1].page is the first page we are going to delete.
	 *
	 * Walk down the chain, acquiring the rest of the pages until we've
	 * retrieved the leaf page.  If we find any pages that aren't going
	 * to be emptied by the delete, someone else added something while we
	 * were walking the tree, and we discontinue the delete.
	 */
	for (h = t->bt_csp[-1].page;;) {
		if (ISLEAF(h)) {
			if (NUM_ENT(h) != 0)
				goto release;
			break;
		} else
			if (NUM_ENT(h) != 1)
				goto release;

		/*
		 * Get the next page, write lock it and push it onto the stack.
		 * We know it's index 0, because it can only have one element.
		 */
		pgno = TYPE(h) == P_IBTREE ?
		    GET_BINTERNAL(h, 0)->pgno : GET_RINTERNAL(h, 0)->pgno;

		if ((ret = __bam_lget(dbp, 0, pgno, DB_LOCK_WRITE, &lock)) != 0)
			goto release;
		if ((ret = __bam_pget(dbp, &h, &pgno, 0)) != 0)
			goto release;
		BT_STK_PUSH(t, h, 0, lock, ret);
		if (ret != 0)
			goto release;
	}

	BT_STK_POP(t);
	return (__bam_dpages(dbp, t));

release:
	/* Discard any locked pages and return. */
	BT_STK_POP(t);
	__bam_stkrel(dbp);
	return (ret);
}

/*
 * __bam_dpages --
 *	Delete a set of locked pages.
 */
static int
__bam_dpages(dbp, t)
	DB *dbp;
	BTREE *t;
{
	DBT a, b;
	DB_LOCK lock;
	EPG *epg;
	PAGE *h;
	db_pgno_t pgno;
	db_recno_t rcnt;
	int ret;

	COMPQUIET(rcnt, 0);

	epg = t->bt_sp;

	/*
	 * !!!
	 * There is an interesting deadlock situation here.  We have to relink
	 * the leaf page chain around the leaf page being deleted.  Consider
	 * a cursor walking through the leaf pages, that has the previous page
	 * read-locked and is waiting on a lock for the page we're deleting.
	 * It will deadlock here.  This is a problem, because if our process is
	 * selected to resolve the deadlock, we'll leave an empty leaf page
	 * that we can never again access by walking down the tree.  So, before
	 * we unlink the subtree, we relink the leaf page chain.
	 */
	if ((ret = __db_relink(dbp, t->bt_csp->page, NULL, 1)) != 0)
		goto release;

	/*
	 * We have the entire stack of deletable pages locked.  Start from the
	 * top of the tree and move to the bottom, as it's better to release
	 * the inner pages as soon as possible.
	 */
	if ((ret = __bam_ditem(dbp, epg->page, epg->indx)) != 0)
		goto release;

	/*
	 * If we just deleted the last or next-to-last item from the root page,
	 * the tree can collapse a level.  Write lock the last page referenced
	 * by the root page and copy it over the root page.  If we can't get a
	 * write lock, that's okay, the tree just remains a level deeper than
	 * we'd like.
	 */
	h = epg->page;
	if (h->pgno == PGNO_ROOT && NUM_ENT(h) <= 1) {
		pgno = TYPE(epg->page) == P_IBTREE ?
		    GET_BINTERNAL(epg->page, 0)->pgno :
		    GET_RINTERNAL(epg->page, 0)->pgno;
		if ((ret = __bam_lget(dbp, 0, pgno, DB_LOCK_WRITE, &lock)) != 0)
			goto release;
		if ((ret = __bam_pget(dbp, &h, &pgno, 0)) != 0)
			goto release;

		/* Log the change. */
		if (DB_LOGGING(dbp)) {
			memset(&a, 0, sizeof(a));
			a.data = h;
			a.size = dbp->pgsize;
			memset(&b, 0, sizeof(b));
			b.data = P_ENTRY(epg->page, 0);
			b.size = BINTERNAL_SIZE(((BINTERNAL *)b.data)->len);
			__bam_rsplit_log(dbp->dbenv->lg_info, dbp->txn,
			   &h->lsn, 0, dbp->log_fileid, h->pgno, &a,
			   RE_NREC(epg->page), &b, &epg->page->lsn);
		}

		/*
		 * Make the switch.
		 *
		 * One fixup -- if the tree has record numbers and we're not
		 * converting to a leaf page, we have to preserve the total
		 * record count.
		 */
		if (TYPE(h) == P_IRECNO ||
		    (TYPE(h) == P_IBTREE && F_ISSET(dbp, DB_BT_RECNUM)))
			rcnt = RE_NREC(epg->page);
		memcpy(epg->page, h, dbp->pgsize);
		epg->page->pgno = PGNO_ROOT;
		if (TYPE(h) == P_IRECNO ||
		    (TYPE(h) == P_IBTREE && F_ISSET(dbp, DB_BT_RECNUM)))
			RE_NREC_SET(epg->page, rcnt);
		(void)memp_fset(dbp->mpf, epg->page, DB_MPOOL_DIRTY);

		/*
		 * Free the page copied onto the root page and discard its
		 * lock.  (The call to __bam_free() discards our reference
		 * to the page.)
		 *
		 * It's possible that the reverse split we're doing involves
		 * pages from the stack of pages we're deleting.  Don't free
		 * the page twice.
		 */
		 if (h->pgno == (epg + 1)->page->pgno)
			(void)memp_fput(dbp->mpf, h, 0);
		else {
			(void)__bam_free(dbp, h);
			++t->lstat.bt_freed;
		}
		(void)__BT_TLPUT(dbp, lock);

		/* Adjust the cursors. */
		__bam_ca_move(dbp, h->pgno, PGNO_ROOT);
	}

	/* Release the top page in the subtree. */
	(void)memp_fput(dbp->mpf, epg->page, 0);
	(void)__BT_TLPUT(dbp, epg->lock);

	/*
	 * Free the rest of the pages.
	 *
	 * XXX
	 * Don't bother checking for errors.  We've unlinked the subtree from
	 * the tree, and there's no possibility of recovery.
	 */
	while (++epg <= t->bt_csp) {
		/*
		 * XXX
		 * Why do we need to do this?  Isn't the page already empty?
		 */
		if (NUM_ENT(epg->page) != 0)
			(void)__bam_ditem(dbp, epg->page, epg->indx);

		(void)__bam_free(dbp, epg->page);
		(void)__BT_TLPUT(dbp, epg->lock);
		++t->lstat.bt_freed;
	}
	return (0);

release:
	/* Discard any remaining pages and return. */
	for (; epg <= t->bt_csp; ++epg) {
		(void)memp_fput(dbp->mpf, epg->page, 0);
		(void)__BT_TLPUT(dbp, epg->lock);
	}
	return (ret);
}
