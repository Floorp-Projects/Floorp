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
static const char sccsid[] = "@(#)bt_split.c	10.23 (Sleepycat) 5/23/98";
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

static int __bam_page __P((DB *, EPG *, EPG *));
static int __bam_pinsert __P((DB *, EPG *, PAGE *, PAGE *));
static int __bam_psplit __P((DB *, EPG *, PAGE *, PAGE *, int));
static int __bam_root __P((DB *, EPG *));

/*
 * __bam_split --
 *	Split a page.
 *
 * PUBLIC: int __bam_split __P((DB *, void *));
 */
int
__bam_split(dbp, arg)
	DB *dbp;
	void *arg;
{
	BTREE *t;
	enum { UP, DOWN } dir;
	int exact, level, ret;

	t = dbp->internal;

	/*
	 * The locking protocol we use to avoid deadlock to acquire locks by
	 * walking down the tree, but we do it as lazily as possible, locking
	 * the root only as a last resort.  We expect all stack pages to have
	 * been discarded before we're called; we discard all short-term locks.
	 *
	 * When __bam_split is first called, we know that a leaf page was too
	 * full for an insert.  We don't know what leaf page it was, but we
	 * have the key/recno that caused the problem.  We call XX_search to
	 * reacquire the leaf page, but this time get both the leaf page and
	 * its parent, locked.  We then split the leaf page and see if the new
	 * internal key will fit into the parent page.  If it will, we're done.
	 *
	 * If it won't, we discard our current locks and repeat the process,
	 * only this time acquiring the parent page and its parent, locked.
	 * This process repeats until we succeed in the split, splitting the
	 * root page as the final resort.  The entire process then repeats,
	 * as necessary, until we split a leaf page.
	 *
	 * XXX
	 * A traditional method of speeding this up is to maintain a stack of
	 * the pages traversed in the original search.  You can detect if the
	 * stack is correct by storing the page's LSN when it was searched and
	 * comparing that LSN with the current one when it's locked during the
	 * split.  This would be an easy change for this code, but I have no
	 * numbers that indicate it's worthwhile.
	 */
	for (dir = UP, level = LEAFLEVEL;; dir == UP ? ++level : --level) {
		/*
		 * Acquire a page and its parent, locked.
		 */
		if ((ret = (dbp->type == DB_BTREE ?
		    __bam_search(dbp, arg, S_WRPAIR, level, NULL, &exact) :
		    __bam_rsearch(dbp,
		        (db_recno_t *)arg, S_WRPAIR, level, &exact))) != 0)
			return (ret);

		/* Split the page. */
		ret = t->bt_csp[0].page->pgno == PGNO_ROOT ?
		    __bam_root(dbp, &t->bt_csp[0]) :
		    __bam_page(dbp, &t->bt_csp[-1], &t->bt_csp[0]);

		switch (ret) {
		case 0:
			/* Once we've split the leaf page, we're done. */
			if (level == LEAFLEVEL)
				return (0);

			/* Switch directions. */
			if (dir == UP)
				dir = DOWN;
			break;
		case DB_NEEDSPLIT:
			/*
			 * It's possible to fail to split repeatedly, as other
			 * threads may be modifying the tree, or the page usage
			 * is sufficiently bad that we don't get enough space
			 * the first time.
			 */
			if (dir == DOWN)
				dir = UP;
			break;
		default:
			return (ret);
		}
	}
	/* NOTREACHED */
}

/*
 * __bam_root --
 *	Split the root page of a btree.
 */
static int
__bam_root(dbp, cp)
	DB *dbp;
	EPG *cp;
{
	BTREE *t;
	PAGE *lp, *rp;
	int ret;

	t = dbp->internal;

	/* Yeah, right. */
	if (cp->page->level >= MAXBTREELEVEL) {
		ret = ENOSPC;
		goto err;
	}

	/* Create new left and right pages for the split. */
	lp = rp = NULL;
	if ((ret = __bam_new(dbp, TYPE(cp->page), &lp)) != 0 ||
	    (ret = __bam_new(dbp, TYPE(cp->page), &rp)) != 0)
		goto err;
	P_INIT(lp, dbp->pgsize, lp->pgno,
	    PGNO_INVALID, ISINTERNAL(cp->page) ? PGNO_INVALID : rp->pgno,
	    cp->page->level, TYPE(cp->page));
	P_INIT(rp, dbp->pgsize, rp->pgno,
	    ISINTERNAL(cp->page) ?  PGNO_INVALID : lp->pgno, PGNO_INVALID,
	    cp->page->level, TYPE(cp->page));

	/* Split the page. */
	if ((ret = __bam_psplit(dbp, cp, lp, rp, 1)) != 0)
		goto err;

	/* Log the change. */
	if (DB_LOGGING(dbp)) {
		DBT __a;
		DB_LSN __lsn;
		memset(&__a, 0, sizeof(__a));
		__a.data = cp->page;
		__a.size = dbp->pgsize;
		ZERO_LSN(__lsn);
		if ((ret = __bam_split_log(dbp->dbenv->lg_info, dbp->txn,
		    &LSN(cp->page), 0, dbp->log_fileid, PGNO(lp), &LSN(lp),
		    PGNO(rp), &LSN(rp), (u_int32_t)NUM_ENT(lp), 0, &__lsn,
		    &__a)) != 0)
			goto err;
		LSN(lp) = LSN(rp) = LSN(cp->page);
	}

	/* Clean up the new root page. */
	if ((ret = (dbp->type == DB_RECNO ?
	    __ram_root(dbp, cp->page, lp, rp) :
	    __bam_broot(dbp, cp->page, lp, rp))) != 0)
		goto err;

	/* Success -- write the real pages back to the store. */
	(void)memp_fput(dbp->mpf, cp->page, DB_MPOOL_DIRTY);
	(void)__BT_TLPUT(dbp, cp->lock);
	(void)memp_fput(dbp->mpf, lp, DB_MPOOL_DIRTY);
	(void)memp_fput(dbp->mpf, rp, DB_MPOOL_DIRTY);

	++t->lstat.bt_split;
	++t->lstat.bt_rootsplit;
	return (0);

err:	if (lp != NULL)
		(void)__bam_free(dbp, lp);
	if (rp != NULL)
		(void)__bam_free(dbp, rp);
	(void)memp_fput(dbp->mpf, cp->page, 0);
	(void)__BT_TLPUT(dbp, cp->lock);
	return (ret);
}

/*
 * __bam_page --
 *	Split the non-root page of a btree.
 */
static int
__bam_page(dbp, pp, cp)
	DB *dbp;
	EPG *pp, *cp;
{
	DB_LOCK tplock;
	PAGE *lp, *rp, *tp;
	int ret;

	lp = rp = tp = NULL;
	ret = -1;

	/* Create new right page for the split. */
	if ((ret = __bam_new(dbp, TYPE(cp->page), &rp)) != 0)
		goto err;
	P_INIT(rp, dbp->pgsize, rp->pgno,
	    ISINTERNAL(cp->page) ? PGNO_INVALID : cp->page->pgno,
	    ISINTERNAL(cp->page) ? PGNO_INVALID : cp->page->next_pgno,
	    cp->page->level, TYPE(cp->page));

	/* Create new left page for the split. */
	if ((lp = (PAGE *)__db_malloc(dbp->pgsize)) == NULL) {
		ret = ENOMEM;
		goto err;
	}
#ifdef DIAGNOSTIC
	memset(lp, 0xff, dbp->pgsize);
#endif
	P_INIT(lp, dbp->pgsize, cp->page->pgno,
	    ISINTERNAL(cp->page) ?  PGNO_INVALID : cp->page->prev_pgno,
	    ISINTERNAL(cp->page) ?  PGNO_INVALID : rp->pgno,
	    cp->page->level, TYPE(cp->page));
	ZERO_LSN(lp->lsn);

	/*
	 * Split right.
	 *
	 * Only the indices are sorted on the page, i.e., the key/data pairs
	 * aren't, so it's simpler to copy the data from the split page onto
	 * two new pages instead of copying half the data to the right page
	 * and compacting the left page in place.  Since the left page can't
	 * change, we swap the original and the allocated left page after the
	 * split.
	 */
	if ((ret = __bam_psplit(dbp, cp, lp, rp, 0)) != 0)
		goto err;

	/*
	 * Fix up the previous pointer of any leaf page following the split
	 * page.
	 *
	 * !!!
	 * There are interesting deadlock situations here as we write-lock a
	 * page that's not in our direct ancestry.  Consider a cursor walking
	 * through the leaf pages, that has the previous page read-locked and
	 * is waiting on a lock for the page we just split.  It will deadlock
	 * here.  If this is a problem, we can fail in the split; it's not a
	 * problem as the split will succeed after the cursor passes through
	 * the page we're splitting.
	 */
	if (TYPE(cp->page) == P_LBTREE && rp->next_pgno != PGNO_INVALID) {
		if ((ret = __bam_lget(dbp,
		    0, rp->next_pgno, DB_LOCK_WRITE, &tplock)) != 0)
			goto err;
		if ((ret = __bam_pget(dbp, &tp, &rp->next_pgno, 0)) != 0)
			goto err;
	}

	/* Insert the new pages into the parent page. */
	if ((ret = __bam_pinsert(dbp, pp, lp, rp)) != 0)
		goto err;

	/* Log the change. */
	if (DB_LOGGING(dbp)) {
		DBT __a;
		DB_LSN __lsn;
		memset(&__a, 0, sizeof(__a));
		__a.data = cp->page;
		__a.size = dbp->pgsize;
		if (tp == NULL)
			ZERO_LSN(__lsn);
		if ((ret = __bam_split_log(dbp->dbenv->lg_info, dbp->txn,
		    &cp->page->lsn, 0, dbp->log_fileid, PGNO(cp->page),
		    &LSN(cp->page), PGNO(rp), &LSN(rp), (u_int32_t)NUM_ENT(lp),
		    tp == NULL ? 0 : PGNO(tp),
		    tp == NULL ? &__lsn : &LSN(tp), &__a)) != 0)
			goto err;

		LSN(lp) = LSN(rp) = LSN(cp->page);
		if (tp != NULL)
			LSN(tp) = LSN(cp->page);
	}

	/* Copy the allocated page into place. */
	memcpy(cp->page, lp, LOFFSET(lp));
	memcpy((u_int8_t *)cp->page + HOFFSET(lp),
	    (u_int8_t *)lp + HOFFSET(lp), dbp->pgsize - HOFFSET(lp));
	FREE(lp, dbp->pgsize);
	lp = NULL;

	/* Finish the next-page link. */
	if (tp != NULL)
		tp->prev_pgno = rp->pgno;

	/* Success -- write the real pages back to the store. */
	(void)memp_fput(dbp->mpf, pp->page, DB_MPOOL_DIRTY);
	(void)__BT_TLPUT(dbp, pp->lock);
	(void)memp_fput(dbp->mpf, cp->page, DB_MPOOL_DIRTY);
	(void)__BT_TLPUT(dbp, cp->lock);
	(void)memp_fput(dbp->mpf, rp, DB_MPOOL_DIRTY);
	if (tp != NULL) {
		(void)memp_fput(dbp->mpf, tp, DB_MPOOL_DIRTY);
		(void)__BT_TLPUT(dbp, tplock);
	}
	return (0);

err:	if (lp != NULL)
		FREE(lp, dbp->pgsize);
	if (rp != NULL)
		(void)__bam_free(dbp, rp);
	if (tp != NULL) {
		(void)memp_fput(dbp->mpf, tp, 0);
		(void)__BT_TLPUT(dbp, tplock);
	}
	(void)memp_fput(dbp->mpf, pp->page, 0);
	(void)__BT_TLPUT(dbp, pp->lock);
	(void)memp_fput(dbp->mpf, cp->page, 0);
	(void)__BT_TLPUT(dbp, cp->lock);
	return (ret);
}

/*
 * __bam_broot --
 *	Fix up the btree root page after it has been split.
 *
 * PUBLIC: int __bam_broot __P((DB *, PAGE *, PAGE *, PAGE *));
 */
int
__bam_broot(dbp, rootp, lp, rp)
	DB *dbp;
	PAGE *rootp, *lp, *rp;
{
	BINTERNAL bi, *child_bi;
	BKEYDATA *child_bk;
	DBT hdr, data;
	int ret;

	/*
	 * If the root page was a leaf page, change it into an internal page.
	 * We copy the key we split on (but not the key's data, in the case of
	 * a leaf page) to the new root page.
	 */
	P_INIT(rootp, dbp->pgsize,
	    PGNO_ROOT, PGNO_INVALID, PGNO_INVALID, lp->level + 1, P_IBTREE);

	memset(&data, 0, sizeof(data));
	memset(&hdr, 0, sizeof(hdr));

	/*
	 * The btree comparison code guarantees that the left-most key on any
	 * level of the tree is never used, so it doesn't need to be filled in.
	 */
	memset(&bi, 0, sizeof(bi));
	bi.len = 0;
	B_TSET(bi.type, B_KEYDATA, 0);
	bi.pgno = lp->pgno;
	if (F_ISSET(dbp, DB_BT_RECNUM)) {
		bi.nrecs = __bam_total(lp);
		RE_NREC_SET(rootp, bi.nrecs);
	}
	hdr.data = &bi;
	hdr.size = SSZA(BINTERNAL, data);
	if ((ret =
	    __db_pitem(dbp, rootp, 0, BINTERNAL_SIZE(0), &hdr, NULL)) != 0)
		return (ret);

	switch (TYPE(rp)) {
	case P_IBTREE:
		/* Copy the first key of the child page onto the root page. */
		child_bi = GET_BINTERNAL(rp, 0);

		bi.len = child_bi->len;
		B_TSET(bi.type, child_bi->type, 0);
		bi.pgno = rp->pgno;
		if (F_ISSET(dbp, DB_BT_RECNUM)) {
			bi.nrecs = __bam_total(rp);
			RE_NREC_ADJ(rootp, bi.nrecs);
		}
		hdr.data = &bi;
		hdr.size = SSZA(BINTERNAL, data);
		data.data = child_bi->data;
		data.size = child_bi->len;
		if ((ret = __db_pitem(dbp, rootp, 1,
		    BINTERNAL_SIZE(child_bi->len), &hdr, &data)) != 0)
			return (ret);

		/* Increment the overflow ref count. */
		if (B_TYPE(child_bi->type) == B_OVERFLOW)
			if ((ret = __db_ovref(dbp,
			    ((BOVERFLOW *)(child_bi->data))->pgno, 1)) != 0)
				return (ret);
		break;
	case P_LBTREE:
		/* Copy the first key of the child page onto the root page. */
		child_bk = GET_BKEYDATA(rp, 0);
		switch (B_TYPE(child_bk->type)) {
		case B_KEYDATA:
			bi.len = child_bk->len;
			B_TSET(bi.type, child_bk->type, 0);
			bi.pgno = rp->pgno;
			if (F_ISSET(dbp, DB_BT_RECNUM)) {
				bi.nrecs = __bam_total(rp);
				RE_NREC_ADJ(rootp, bi.nrecs);
			}
			hdr.data = &bi;
			hdr.size = SSZA(BINTERNAL, data);
			data.data = child_bk->data;
			data.size = child_bk->len;
			if ((ret = __db_pitem(dbp, rootp, 1,
			    BINTERNAL_SIZE(child_bk->len), &hdr, &data)) != 0)
				return (ret);
			break;
		case B_DUPLICATE:
		case B_OVERFLOW:
			bi.len = BOVERFLOW_SIZE;
			B_TSET(bi.type, child_bk->type, 0);
			bi.pgno = rp->pgno;
			if (F_ISSET(dbp, DB_BT_RECNUM)) {
				bi.nrecs = __bam_total(rp);
				RE_NREC_ADJ(rootp, bi.nrecs);
			}
			hdr.data = &bi;
			hdr.size = SSZA(BINTERNAL, data);
			data.data = child_bk;
			data.size = BOVERFLOW_SIZE;
			if ((ret = __db_pitem(dbp, rootp, 1,
			    BINTERNAL_SIZE(BOVERFLOW_SIZE), &hdr, &data)) != 0)
				return (ret);

			/* Increment the overflow ref count. */
			if (B_TYPE(child_bk->type) == B_OVERFLOW)
				if ((ret = __db_ovref(dbp,
				    ((BOVERFLOW *)child_bk)->pgno, 1)) != 0)
					return (ret);
			break;
		default:
			return (__db_pgfmt(dbp, rp->pgno));
		}
		break;
	default:
		return (__db_pgfmt(dbp, rp->pgno));
	}
	return (0);
}

/*
 * __ram_root --
 *	Fix up the recno root page after it has been split.
 *
 * PUBLIC: int __ram_root __P((DB *, PAGE *, PAGE *, PAGE *));
 */
int
__ram_root(dbp, rootp, lp, rp)
	DB *dbp;
	PAGE *rootp, *lp, *rp;
{
	DBT hdr;
	RINTERNAL ri;
	int ret;

	/* Initialize the page. */
	P_INIT(rootp, dbp->pgsize,
	    PGNO_ROOT, PGNO_INVALID, PGNO_INVALID, lp->level + 1, P_IRECNO);

	/* Initialize the header. */
	memset(&hdr, 0, sizeof(hdr));
	hdr.data = &ri;
	hdr.size = RINTERNAL_SIZE;

	/* Insert the left and right keys, set the header information. */
	ri.pgno = lp->pgno;
	ri.nrecs = __bam_total(lp);
	if ((ret = __db_pitem(dbp, rootp, 0, RINTERNAL_SIZE, &hdr, NULL)) != 0)
		return (ret);
	RE_NREC_SET(rootp, ri.nrecs);
	ri.pgno = rp->pgno;
	ri.nrecs = __bam_total(rp);
	if ((ret = __db_pitem(dbp, rootp, 1, RINTERNAL_SIZE, &hdr, NULL)) != 0)
		return (ret);
	RE_NREC_ADJ(rootp, ri.nrecs);
	return (0);
}

/*
 * __bam_pinsert --
 *	Insert a new key into a parent page, completing the split.
 */
static int
__bam_pinsert(dbp, parent, lchild, rchild)
	DB *dbp;
	EPG *parent;
	PAGE *lchild, *rchild;
{
	BINTERNAL bi, *child_bi;
	BKEYDATA *child_bk, *tmp_bk;
	BTREE *t;
	DBT a, b, hdr, data;
	PAGE *ppage;
	RINTERNAL ri;
	db_indx_t off;
	db_recno_t nrecs;
	u_int32_t n, nbytes, nksize;
	int ret;

	t = dbp->internal;
	ppage = parent->page;

	/* If handling record numbers, count records split to the right page. */
	nrecs = dbp->type == DB_RECNO || F_ISSET(dbp, DB_BT_RECNUM) ?
	    __bam_total(rchild) : 0;

	/*
	 * Now we insert the new page's first key into the parent page, which
	 * completes the split.  The parent points to a PAGE and a page index
	 * offset, where the new key goes ONE AFTER the index, because we split
	 * to the right.
	 *
	 * XXX
	 * Some btree algorithms replace the key for the old page as well as
	 * the new page.  We don't, as there's no reason to believe that the
	 * first key on the old page is any better than the key we have, and,
	 * in the case of a key being placed at index 0 causing the split, the
	 * key is unavailable.
	 */
	off = parent->indx + O_INDX;

	/*
	 * Calculate the space needed on the parent page.
	 *
	 * Prefix trees: space hack used when inserting into BINTERNAL pages.
	 * Retain only what's needed to distinguish between the new entry and
	 * the LAST entry on the page to its left.  If the keys compare equal,
	 * retain the entire key.  We ignore overflow keys, and the entire key
	 * must be retained for the next-to-leftmost key on the leftmost page
	 * of each level, or the search will fail.  Applicable ONLY to internal
	 * pages that have leaf pages as children.  Further reduction of the
	 * key between pairs of internal pages loses too much information.
	 */
	switch (TYPE(rchild)) {
	case P_IBTREE:
		child_bi = GET_BINTERNAL(rchild, 0);
		nbytes = BINTERNAL_PSIZE(child_bi->len);

		if (P_FREESPACE(ppage) < nbytes)
			return (DB_NEEDSPLIT);

		/* Add a new record for the right page. */
		memset(&bi, 0, sizeof(bi));
		bi.len = child_bi->len;
		B_TSET(bi.type, child_bi->type, 0);
		bi.pgno = rchild->pgno;
		bi.nrecs = nrecs;
		memset(&hdr, 0, sizeof(hdr));
		hdr.data = &bi;
		hdr.size = SSZA(BINTERNAL, data);
		memset(&data, 0, sizeof(data));
		data.data = child_bi->data;
		data.size = child_bi->len;
		if ((ret = __db_pitem(dbp, ppage, off,
		    BINTERNAL_SIZE(child_bi->len), &hdr, &data)) != 0)
			return (ret);

		/* Increment the overflow ref count. */
		if (B_TYPE(child_bi->type) == B_OVERFLOW)
			if ((ret = __db_ovref(dbp,
			    ((BOVERFLOW *)(child_bi->data))->pgno, 1)) != 0)
				return (ret);
		break;
	case P_LBTREE:
		child_bk = GET_BKEYDATA(rchild, 0);
		switch (B_TYPE(child_bk->type)) {
		case B_KEYDATA:
			nbytes = BINTERNAL_PSIZE(child_bk->len);
			nksize = child_bk->len;
			if (t->bt_prefix == NULL)
				goto noprefix;
			if (ppage->prev_pgno == PGNO_INVALID && off <= 1)
				goto noprefix;
			tmp_bk = GET_BKEYDATA(lchild, NUM_ENT(lchild) - P_INDX);
			if (B_TYPE(tmp_bk->type) != B_KEYDATA)
				goto noprefix;
			memset(&a, 0, sizeof(a));
			a.size = tmp_bk->len;
			a.data = tmp_bk->data;
			memset(&b, 0, sizeof(b));
			b.size = child_bk->len;
			b.data = child_bk->data;
			nksize = t->bt_prefix(&a, &b);
			if ((n = BINTERNAL_PSIZE(nksize)) < nbytes) {
				t->lstat.bt_pfxsaved += nbytes - n;
				nbytes = n;
			} else
noprefix:			nksize = child_bk->len;

			if (P_FREESPACE(ppage) < nbytes)
				return (DB_NEEDSPLIT);

			memset(&bi, 0, sizeof(bi));
			bi.len = nksize;
			B_TSET(bi.type, child_bk->type, 0);
			bi.pgno = rchild->pgno;
			bi.nrecs = nrecs;
			memset(&hdr, 0, sizeof(hdr));
			hdr.data = &bi;
			hdr.size = SSZA(BINTERNAL, data);
			memset(&data, 0, sizeof(data));
			data.data = child_bk->data;
			data.size = nksize;
			if ((ret = __db_pitem(dbp, ppage, off,
			    BINTERNAL_SIZE(nksize), &hdr, &data)) != 0)
				return (ret);
			break;
		case B_DUPLICATE:
		case B_OVERFLOW:
			nbytes = BINTERNAL_PSIZE(BOVERFLOW_SIZE);

			if (P_FREESPACE(ppage) < nbytes)
				return (DB_NEEDSPLIT);

			memset(&bi, 0, sizeof(bi));
			bi.len = BOVERFLOW_SIZE;
			B_TSET(bi.type, child_bk->type, 0);
			bi.pgno = rchild->pgno;
			bi.nrecs = nrecs;
			memset(&hdr, 0, sizeof(hdr));
			hdr.data = &bi;
			hdr.size = SSZA(BINTERNAL, data);
			memset(&data, 0, sizeof(data));
			data.data = child_bk;
			data.size = BOVERFLOW_SIZE;
			if ((ret = __db_pitem(dbp, ppage, off,
			    BINTERNAL_SIZE(BOVERFLOW_SIZE), &hdr, &data)) != 0)
				return (ret);

			/* Increment the overflow ref count. */
			if (B_TYPE(child_bk->type) == B_OVERFLOW)
				if ((ret = __db_ovref(dbp,
				    ((BOVERFLOW *)child_bk)->pgno, 1)) != 0)
					return (ret);
			break;
		default:
			return (__db_pgfmt(dbp, rchild->pgno));
		}
		break;
	case P_IRECNO:
	case P_LRECNO:
		nbytes = RINTERNAL_PSIZE;

		if (P_FREESPACE(ppage) < nbytes)
			return (DB_NEEDSPLIT);

		/* Add a new record for the right page. */
		memset(&hdr, 0, sizeof(hdr));
		hdr.data = &ri;
		hdr.size = RINTERNAL_SIZE;
		ri.pgno = rchild->pgno;
		ri.nrecs = nrecs;
		if ((ret = __db_pitem(dbp,
		    ppage, off, RINTERNAL_SIZE, &hdr, NULL)) != 0)
			return (ret);
		break;
	default:
		return (__db_pgfmt(dbp, rchild->pgno));
	}

	/* Adjust the parent page's left page record count. */
	if (dbp->type == DB_RECNO || F_ISSET(dbp, DB_BT_RECNUM)) {
		/* Log the change. */
		if (DB_LOGGING(dbp) &&
		    (ret = __bam_cadjust_log(dbp->dbenv->lg_info,
		    dbp->txn, &LSN(ppage), 0, dbp->log_fileid,
		    PGNO(ppage), &LSN(ppage), (u_int32_t)parent->indx,
		    -(int32_t)nrecs, (int32_t)0)) != 0)
			return (ret);

		/* Update the left page count. */
		if (dbp->type == DB_RECNO)
			GET_RINTERNAL(ppage, parent->indx)->nrecs -= nrecs;
		else
			GET_BINTERNAL(ppage, parent->indx)->nrecs -= nrecs;
	}

	return (0);
}

/*
 * __bam_psplit --
 *	Do the real work of splitting the page.
 */
static int
__bam_psplit(dbp, cp, lp, rp, cleft)
	DB *dbp;
	EPG *cp;
	PAGE *lp, *rp;
	int cleft;
{
	BTREE *t;
	PAGE *pp;
	db_indx_t half, nbytes, off, splitp, top;
	int adjust, cnt, isbigkey, ret;

	t = dbp->internal;
	pp = cp->page;
	adjust = TYPE(pp) == P_LBTREE ? P_INDX : O_INDX;

	/*
	 * If we're splitting the first (last) page on a level because we're
	 * inserting (appending) a key to it, it's likely that the data is
	 * sorted.  Moving a single item to the new page is less work and can
	 * push the fill factor higher than normal.  If we're wrong it's not
	 * a big deal, we'll just do the split the right way next time.
	 */
	off = 0;
	if (NEXT_PGNO(pp) == PGNO_INVALID &&
	    ((ISINTERNAL(pp) && cp->indx == NUM_ENT(cp->page) - 1) ||
	    (!ISINTERNAL(pp) && cp->indx == NUM_ENT(cp->page))))
		off = NUM_ENT(cp->page) - adjust;
	else if (PREV_PGNO(pp) == PGNO_INVALID && cp->indx == 0)
		off = adjust;

	++t->lstat.bt_split;
	if (off != 0) {
		++t->lstat.bt_fastsplit;
		goto sort;
	}

	/*
	 * Split the data to the left and right pages.  Try not to split on
	 * an overflow key.  (Overflow keys on internal pages will slow down
	 * searches.)  Refuse to split in the middle of a set of duplicates.
	 *
	 * First, find the optimum place to split.
	 *
	 * It's possible to try and split past the last record on the page if
	 * there's a very large record at the end of the page.  Make sure this
	 * doesn't happen by bounding the check at the next-to-last entry on
	 * the page.
	 *
	 * Note, we try and split half the data present on the page.  This is
	 * because another process may have already split the page and left
	 * it half empty.  We don't try and skip the split -- we don't know
	 * how much space we're going to need on the page, and we may need up
	 * to half the page for a big item, so there's no easy test to decide
	 * if we need to split or not.  Besides, if two threads are inserting
	 * data into the same place in the database, we're probably going to
	 * need more space soon anyway.
	 */
	top = NUM_ENT(pp) - adjust;
	half = (dbp->pgsize - HOFFSET(pp)) / 2;
	for (nbytes = 0, off = 0; off < top && nbytes < half; ++off)
		switch (TYPE(pp)) {
		case P_IBTREE:
			if (B_TYPE(GET_BINTERNAL(pp, off)->type) == B_KEYDATA)
				nbytes +=
				   BINTERNAL_SIZE(GET_BINTERNAL(pp, off)->len);
			else
				nbytes += BINTERNAL_SIZE(BOVERFLOW_SIZE);
			break;
		case P_LBTREE:
			if (B_TYPE(GET_BKEYDATA(pp, off)->type) == B_KEYDATA)
				nbytes +=
				    BKEYDATA_SIZE(GET_BKEYDATA(pp, off)->len);
			else
				nbytes += BOVERFLOW_SIZE;

			++off;
			if (B_TYPE(GET_BKEYDATA(pp, off)->type) == B_KEYDATA)
				nbytes +=
				    BKEYDATA_SIZE(GET_BKEYDATA(pp, off)->len);
			else
				nbytes += BOVERFLOW_SIZE;
			break;
		case P_IRECNO:
			nbytes += RINTERNAL_SIZE;
			break;
		case P_LRECNO:
			nbytes += BKEYDATA_SIZE(GET_BKEYDATA(pp, off)->len);
			break;
		default:
			return (__db_pgfmt(dbp, pp->pgno));
		}
sort:	splitp = off;

	/*
	 * Splitp is either at or just past the optimum split point.  If
	 * it's a big key, try and find something close by that's not.
	 */
	if (TYPE(pp) == P_IBTREE)
		isbigkey = B_TYPE(GET_BINTERNAL(pp, off)->type) != B_KEYDATA;
	else if (TYPE(pp) == P_LBTREE)
		isbigkey = B_TYPE(GET_BKEYDATA(pp, off)->type) != B_KEYDATA;
	else
		isbigkey = 0;
	if (isbigkey)
		for (cnt = 1; cnt <= 3; ++cnt) {
			off = splitp + cnt * adjust;
			if (off < (db_indx_t)NUM_ENT(pp) &&
			    ((TYPE(pp) == P_IBTREE &&
			    B_TYPE(GET_BINTERNAL(pp,off)->type) == B_KEYDATA) ||
			    B_TYPE(GET_BKEYDATA(pp, off)->type) == B_KEYDATA)) {
				splitp = off;
				break;
			}
			if (splitp <= (db_indx_t)(cnt * adjust))
				continue;
			off = splitp - cnt * adjust;
			if (TYPE(pp) == P_IBTREE ?
			    B_TYPE(GET_BINTERNAL(pp, off)->type) == B_KEYDATA :
			    B_TYPE(GET_BKEYDATA(pp, off)->type) == B_KEYDATA) {
				splitp = off;
				break;
			}
		}

	/*
	 * We can't split in the middle a set of duplicates.  We know that
	 * no duplicate set can take up more than about 25% of the page,
	 * because that's the point where we push it off onto a duplicate
	 * page set.  So, this loop can't be unbounded.
	 */
	if (F_ISSET(dbp, DB_AM_DUP) && TYPE(pp) == P_LBTREE &&
	    pp->inp[splitp] == pp->inp[splitp - adjust])
		for (cnt = 1;; ++cnt) {
			off = splitp + cnt * adjust;
			if (off < NUM_ENT(pp) &&
			    pp->inp[splitp] != pp->inp[off]) {
				splitp = off;
				break;
			}
			if (splitp <= (db_indx_t)(cnt * adjust))
				continue;
			off = splitp - cnt * adjust;
			if (pp->inp[splitp] != pp->inp[off]) {
				splitp = off + adjust;
				break;
			}
		}


	/* We're going to split at splitp. */
	if ((ret = __bam_copy(dbp, pp, lp, 0, splitp)) != 0)
		return (ret);
	if ((ret = __bam_copy(dbp, pp, rp, splitp, NUM_ENT(pp))) != 0)
		return (ret);

	/* Adjust the cursors. */
	__bam_ca_split(dbp, pp->pgno, lp->pgno, rp->pgno, splitp, cleft);
	return (0);
}

/*
 * __bam_copy --
 *	Copy a set of records from one page to another.
 *
 * PUBLIC: int __bam_copy __P((DB *, PAGE *, PAGE *, u_int32_t, u_int32_t));
 */
int
__bam_copy(dbp, pp, cp, nxt, stop)
	DB *dbp;
	PAGE *pp, *cp;
	u_int32_t nxt, stop;
{
	db_indx_t nbytes, off;

	/*
	 * Copy the rest of the data to the right page.  Nxt is the next
	 * offset placed on the target page.
	 */
	for (off = 0; nxt < stop; ++nxt, ++NUM_ENT(cp), ++off) {
		switch (TYPE(pp)) {
		case P_IBTREE:
			if (B_TYPE(GET_BINTERNAL(pp, nxt)->type) == B_KEYDATA)
				nbytes =
				    BINTERNAL_SIZE(GET_BINTERNAL(pp, nxt)->len);
			else
				nbytes = BINTERNAL_SIZE(BOVERFLOW_SIZE);
			break;
		case P_LBTREE:
			/*
			 * If we're on a key and it's a duplicate, just copy
			 * the offset.
			 */
			if (off != 0 && (nxt % P_INDX) == 0 &&
			    pp->inp[nxt] == pp->inp[nxt - P_INDX]) {
				cp->inp[off] = cp->inp[off - P_INDX];
				continue;
			}
			/* FALLTHROUGH */
		case P_LRECNO:
			if (B_TYPE(GET_BKEYDATA(pp, nxt)->type) == B_KEYDATA)
				nbytes =
				    BKEYDATA_SIZE(GET_BKEYDATA(pp, nxt)->len);
			else
				nbytes = BOVERFLOW_SIZE;
			break;
		case P_IRECNO:
			nbytes = RINTERNAL_SIZE;
			break;
		default:
			return (__db_pgfmt(dbp, pp->pgno));
		}
		cp->inp[off] = HOFFSET(cp) -= nbytes;
		memcpy(P_ENTRY(cp, off), P_ENTRY(pp, nxt), nbytes);
	}
	return (0);
}
