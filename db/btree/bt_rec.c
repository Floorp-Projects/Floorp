/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)bt_rec.c	10.21 (Sleepycat) 4/28/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "shqueue.h"
#include "hash.h"
#include "btree.h"
#include "log.h"
#include "common_ext.h"

/*
 * __bam_pg_alloc_recover --
 *	Recovery function for pg_alloc.
 *
 * PUBLIC: int __bam_pg_alloc_recover
 * PUBLIC:   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__bam_pg_alloc_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__bam_pg_alloc_args *argp;
	BTMETA *meta;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	DB *file_dbp, *mdbp;
	db_pgno_t pgno;
	int cmp_n, cmp_p, modified, ret;

	REC_PRINT(__bam_pg_alloc_print);
	REC_INTRO(__bam_pg_alloc_read);

	/*
	 * Fix up the allocated page.  If we're redoing the operation, we have
	 * to get the page (creating it if it doesn't exist), and update its
	 * LSN.  If we're undoing the operation, we have to reset the page's
	 * LSN and put it on the free list.
	 *
	 * Fix up the metadata page.  If we're redoing the operation, we have
	 * to get the metadata page and update its LSN and its free pointer.
	 * If we're undoing the operation and the page was ever created, we put
	 * it on the freelist.
	 */
	pgno = PGNO_METADATA;
	if ((ret = memp_fget(mpf, &pgno, 0, &meta)) != 0) {
		/* The metadata page must always exist. */
		(void)__db_pgerr(file_dbp, pgno);
		goto out;
	}
	if ((ret = memp_fget(mpf, &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0) {
		/*
		 * We specify creation and check for it later, because this
		 * operation was supposed to create the page, and even in
		 * the undo case it's going to get linked onto the freelist
		 * which we're also fixing up.
		 */
		(void)__db_pgerr(file_dbp, argp->pgno);
		(void)memp_fput(mpf, meta, 0);
		goto out;
	}

	/* Fix up the allocated page. */
	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->page_lsn);
	if (cmp_p == 0 && redo) {
		/* Need to redo update described. */
		P_INIT(pagep, file_dbp->pgsize,
		    argp->pgno, PGNO_INVALID, PGNO_INVALID, 0, argp->ptype);

		pagep->lsn = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && !redo) {
		/* Need to undo update described. */
		P_INIT(pagep, file_dbp->pgsize,
		    argp->pgno, PGNO_INVALID, meta->free, 0, P_INVALID);

		pagep->lsn = argp->page_lsn;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0) {
		(void)__db_panic(file_dbp);
		(void)memp_fput(mpf, meta, 0);
		goto out;
	}

	/* Fix up the metadata page. */
	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(meta));
	cmp_p = log_compare(&LSN(meta), &argp->meta_lsn);
	if (cmp_p == 0 && redo) {
		/* Need to redo update described. */
		meta->lsn = *lsnp;
		meta->free = argp->next;
		modified = 1;
	} else if (cmp_n == 0 && !redo) {
		/* Need to undo update described. */
		meta->lsn = argp->meta_lsn;
		meta->free = argp->pgno;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, meta, modified ? DB_MPOOL_DIRTY : 0)) != 0) {
		(void)__db_panic(file_dbp);
		goto out;
	}

	*lsnp = argp->prev_lsn;
	ret = 0;

out:	REC_CLOSE;
}

/*
 * __bam_pg_free_recover --
 *	Recovery function for pg_free.
 *
 * PUBLIC: int __bam_pg_free_recover
 * PUBLIC:   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__bam_pg_free_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__bam_pg_free_args *argp;
	BTMETA *meta;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	db_pgno_t pgno;
	int cmp_n, cmp_p, modified, ret;

	REC_PRINT(__bam_pg_free_print);
	REC_INTRO(__bam_pg_free_read);

	/*
	 * Fix up the freed page.  If we're redoing the operation we get the
	 * page and explicitly discard its contents, then update its LSN.  If
	 * we're undoing the operation, we get the page and restore its header.
	 */
	if ((ret = memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		/*
		 * We don't automatically create the page.  The only way the
		 * page might not exist is if the alloc never happened, and
		 * the only way the alloc might never have happened is if we
		 * are undoing, in which case there's no reason to create the
		 * page.
		 */
		if (!redo)
			goto done;
		(void)__db_pgerr(file_dbp, argp->pgno);
		goto out;
	}
	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &LSN(argp->header.data));
	if (cmp_p == 0 && redo) {
		/* Need to redo update described. */
		P_INIT(pagep, file_dbp->pgsize,
		    pagep->pgno, PGNO_INVALID, argp->next, 0, P_INVALID);
		pagep->lsn = *lsnp;

		modified = 1;
	} else if (cmp_n == 0 && !redo) {
		/* Need to undo update described. */
		memcpy(pagep, argp->header.data, argp->header.size);

		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0) {
		(void)__db_panic(file_dbp);
		goto out;
	}

	/*
	 * Fix up the metadata page.  If we're redoing or undoing the operation
	 * we get the page and update its LSN and free pointer.
	 */
	pgno = PGNO_METADATA;
	if ((ret = memp_fget(mpf, &pgno, 0, &meta)) != 0) {
		/* The metadata page must always exist. */
		(void)__db_pgerr(file_dbp, pgno);
		goto out;
	}

	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(meta));
	cmp_p = log_compare(&LSN(meta), &argp->meta_lsn);
	if (cmp_p == 0 && redo) {
		/* Need to redo update described. */
		meta->free = argp->pgno;

		meta->lsn = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && !redo) {
		/* Need to undo update described. */
		meta->free = argp->next;

		meta->lsn = argp->meta_lsn;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, meta, modified ? DB_MPOOL_DIRTY : 0)) != 0) {
		(void)__db_panic(file_dbp);
		goto out;
	}

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	REC_CLOSE;
}

/*
 * __bam_split_recover --
 *	Recovery function for split.
 *
 * PUBLIC: int __bam_split_recover
 * PUBLIC:   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__bam_split_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__bam_split_args *argp;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	PAGE *_lp, *lp, *np, *pp, *_rp, *rp, *sp;
	db_pgno_t pgno;
	int l_update, p_update, r_update, ret, rootsplit, t_ret;

	REC_PRINT(__bam_split_print);

	mpf = NULL;
	_lp = lp = np = pp = _rp = rp = NULL;

	REC_INTRO(__bam_split_read);

	/*
	 * There are two kinds of splits that we have to recover from.  The
	 * first is a root-page split, where the root page is split from a
	 * leaf page into an internal page and two new leaf pages are created.
	 * The second is where a page is split into two pages, and a new key
	 * is inserted into the parent page.
	 */
	sp = argp->pg.data;
	pgno = PGNO(sp);
	rootsplit = pgno == PGNO_ROOT;
	if (memp_fget(mpf, &argp->left, 0, &lp) != 0)
		lp = NULL;
	if (memp_fget(mpf, &argp->right, 0, &rp) != 0)
		rp = NULL;

	if (redo) {
		l_update = r_update = p_update = 0;
		/*
		 * Decide if we need to resplit the page.
		 *
		 * If this is a root split, then the root has to exist, it's
		 * the page we're splitting and it gets modified.  If this is
		 * not a root split, then the left page has to exist, for the
		 * same reason.
		 */
		if (rootsplit) {
			if ((ret = memp_fget(mpf, &pgno, 0, &pp)) != 0) {
				(void)__db_pgerr(file_dbp, pgno);
				pp = NULL;
				goto out;
			}
			p_update =
			    log_compare(&LSN(pp), &LSN(argp->pg.data)) == 0;
		} else
			if (lp == NULL) {
				(void)__db_pgerr(file_dbp, argp->left);
				goto out;
			}
		if (lp == NULL || log_compare(&LSN(lp), &argp->llsn) == 0)
			l_update = 1;
		if (rp == NULL || log_compare(&LSN(rp), &argp->rlsn) == 0)
			r_update = 1;
		if (!p_update && !l_update && !r_update)
			goto done;

		/* Allocate and initialize new left/right child pages. */
		if ((_lp = (PAGE *)__db_malloc(file_dbp->pgsize)) == NULL ||
		    (_rp = (PAGE *)__db_malloc(file_dbp->pgsize)) == NULL) {
			ret = ENOMEM;
			__db_err(file_dbp->dbenv, "%s", strerror(ret));
			goto out;
		}
		if (rootsplit) {
			P_INIT(_lp, file_dbp->pgsize, argp->left,
			    PGNO_INVALID,
			    ISINTERNAL(sp) ? PGNO_INVALID : argp->right,
			    LEVEL(sp), TYPE(sp));
			P_INIT(_rp, file_dbp->pgsize, argp->right,
			    ISINTERNAL(sp) ?  PGNO_INVALID : argp->left,
			    PGNO_INVALID, LEVEL(sp), TYPE(sp));
		} else {
			P_INIT(_lp, file_dbp->pgsize, PGNO(sp),
			    ISINTERNAL(sp) ? PGNO_INVALID : PREV_PGNO(sp),
			    ISINTERNAL(sp) ? PGNO_INVALID : argp->right,
			    LEVEL(sp), TYPE(sp));
			P_INIT(_rp, file_dbp->pgsize, argp->right,
			    ISINTERNAL(sp) ? PGNO_INVALID : sp->pgno,
			    ISINTERNAL(sp) ? PGNO_INVALID : NEXT_PGNO(sp),
			    LEVEL(sp), TYPE(sp));
		}

		/* Split the page. */
		if ((ret = __bam_copy(file_dbp, sp, _lp, 0, argp->indx)) != 0 ||
		    (ret = __bam_copy(file_dbp, sp, _rp, argp->indx,
		    NUM_ENT(sp))) != 0)
			goto out;

		/* If the left child is wrong, update it. */
		if (lp == NULL && (ret =
		    memp_fget(mpf, &argp->left, DB_MPOOL_CREATE, &lp)) != 0) {
			(void)__db_pgerr(file_dbp, argp->left);
			lp = NULL;
			goto out;
		}
		if (l_update) {
			memcpy(lp, _lp, file_dbp->pgsize);
			lp->lsn = *lsnp;
			if ((ret = memp_fput(mpf, lp, DB_MPOOL_DIRTY)) != 0)
				goto fatal;
			lp = NULL;
		}

		/* If the right child is wrong, update it. */
		if (rp == NULL && (ret = memp_fget(mpf,
		    &argp->right, DB_MPOOL_CREATE, &rp)) != 0) {
			(void)__db_pgerr(file_dbp, argp->right);
			rp = NULL;
			goto out;
		}
		if (r_update) {
			memcpy(rp, _rp, file_dbp->pgsize);
			rp->lsn = *lsnp;
			if ((ret = memp_fput(mpf, rp, DB_MPOOL_DIRTY)) != 0)
				goto fatal;
			rp = NULL;
		}

		/*
		 * If the parent page is wrong, update it.  This is of interest
		 * only if it was a root split, since root splits create parent
		 * pages.  All other splits modify a parent page, but those are
		 * separately logged and recovered.
		 */
		if (rootsplit && p_update) {
			if (file_dbp->type == DB_BTREE)
				P_INIT(pp, file_dbp->pgsize,
				    PGNO_ROOT, PGNO_INVALID, PGNO_INVALID,
				    _lp->level + 1, P_IBTREE);
			else
				P_INIT(pp, file_dbp->pgsize,
				    PGNO_ROOT, PGNO_INVALID, PGNO_INVALID,
				    _lp->level + 1, P_IRECNO);
			RE_NREC_SET(pp,
			    file_dbp->type == DB_RECNO ||
			    F_ISSET(file_dbp, DB_BT_RECNUM) ?
			    __bam_total(_lp) + __bam_total(_rp) : 0);
			pp->lsn = *lsnp;
			if ((ret = memp_fput(mpf, pp, DB_MPOOL_DIRTY)) != 0)
				goto fatal;
			pp = NULL;
		}

		/*
		 * Finally, redo the next-page link if necessary.  This is of
		 * interest only if it wasn't a root split -- inserting a new
		 * page in the tree requires that any following page have its
		 * previous-page pointer updated to our new page.  The next
		 * page must exist because we're redoing the operation.
		 */
		if (!rootsplit && !IS_ZERO_LSN(argp->nlsn)) {
			if ((ret = memp_fget(mpf, &argp->npgno, 0, &np)) != 0) {
				(void)__db_pgerr(file_dbp, argp->npgno);
				np = NULL;
				goto out;
			}
			if (log_compare(&LSN(np), &argp->nlsn) == 0) {
				PREV_PGNO(np) = argp->right;
				np->lsn = *lsnp;
				if ((ret = memp_fput(mpf,
				    np, DB_MPOOL_DIRTY)) != 0)
					goto fatal;
				np = NULL;
			}
		}
	} else {
		/*
		 * If the split page is wrong, replace its contents with the
		 * logged page contents.  If the page doesn't exist, it means
		 * that the create of the page never happened, nor did any of
		 * the adds onto the page that caused the split, and there's
		 * really no undo-ing to be done.
		 */
		if ((ret = memp_fget(mpf, &pgno, 0, &pp)) != 0) {
			pp = NULL;
			goto lrundo;
		}
		if (log_compare(lsnp, &LSN(pp)) == 0) {
			memcpy(pp, argp->pg.data, argp->pg.size);
			if ((ret = memp_fput(mpf, pp, DB_MPOOL_DIRTY)) != 0)
				goto fatal;
			pp = NULL;
		}

		/*
		 * If it's a root split and the left child ever existed, update
		 * its LSN.  (If it's not a root split, we've updated the left
		 * page already -- it's the same as the split page.) If the
		 * right child ever existed, root split or not, update its LSN.
		 * The undo of the page allocation(s) will restore them to the
		 * free list.
		 */
lrundo:		if ((rootsplit && lp != NULL) || rp != NULL) {
			if (rootsplit && lp != NULL &&
			    log_compare(lsnp, &LSN(lp)) == 0) {
				lp->lsn = argp->llsn;
				if ((ret =
				    memp_fput(mpf, lp, DB_MPOOL_DIRTY)) != 0)
					goto fatal;
				lp = NULL;
			}
			if (rp != NULL &&
			    log_compare(lsnp, &LSN(rp)) == 0) {
				rp->lsn = argp->rlsn;
				if ((ret =
				    memp_fput(mpf, rp, DB_MPOOL_DIRTY)) != 0)
					goto fatal;
				rp = NULL;
			}
		}

		/*
		 * Finally, undo the next-page link if necessary.  This is of
		 * interest only if it wasn't a root split -- inserting a new
		 * page in the tree requires that any following page have its
		 * previous-page pointer updated to our new page.  Since it's
		 * possible that the next-page never existed, we ignore it as
		 * if there's nothing to undo.
		 */
		if (!rootsplit && !IS_ZERO_LSN(argp->nlsn)) {
			if ((ret = memp_fget(mpf, &argp->npgno, 0, &np)) != 0) {
				np = NULL;
				goto done;
			}
			if (log_compare(lsnp, &LSN(np)) == 0) {
				PREV_PGNO(np) = argp->left;
				np->lsn = argp->nlsn;
				if (memp_fput(mpf, np, DB_MPOOL_DIRTY))
					goto fatal;
				np = NULL;
			}
		}
	}

done:	*lsnp = argp->prev_lsn;
	ret = 0;

	if (0) {
fatal:		(void)__db_panic(file_dbp);
	}
out:	/* Free any pages that weren't dirtied. */
	if (pp != NULL && (t_ret = memp_fput(mpf, pp, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (lp != NULL && (t_ret = memp_fput(mpf, lp, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (np != NULL && (t_ret = memp_fput(mpf, np, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (rp != NULL && (t_ret = memp_fput(mpf, rp, 0)) != 0 && ret == 0)
		ret = t_ret;

	/* Free any allocated space. */
	if (_lp != NULL)
		__db_free(_lp);
	if (_rp != NULL)
		__db_free(_rp);

	REC_CLOSE;
}

/*
 * __bam_rsplit_recover --
 *	Recovery function for a reverse split.
 *
 * PUBLIC: int __bam_rsplit_recover
 * PUBLIC:   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__bam_rsplit_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__bam_rsplit_args *argp;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	db_pgno_t pgno;
	int cmp_n, cmp_p, modified, ret;

	REC_PRINT(__bam_rsplit_print);
	REC_INTRO(__bam_rsplit_read);

	/* Fix the root page. */
	pgno = PGNO_ROOT;
	if ((ret = memp_fget(mpf, &pgno, 0, &pagep)) != 0) {
		/* The root page must always exist. */
		__db_pgerr(file_dbp, pgno);
		goto out;
	}
	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->rootlsn);
	if (cmp_p == 0 && redo) {
		/* Need to redo update described. */
		memcpy(pagep, argp->pgdbt.data, argp->pgdbt.size);
		pagep->pgno = PGNO_ROOT;
		pagep->lsn = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && !redo) {
		/* Need to undo update described. */
		P_INIT(pagep, file_dbp->pgsize, PGNO_ROOT,
		    argp->nrec, PGNO_INVALID, pagep->level + 1,
		    file_dbp->type == DB_BTREE ? P_IBTREE : P_IRECNO);
		if ((ret = __db_pitem(file_dbp, pagep, 0,
		    argp->rootent.size, &argp->rootent, NULL)) != 0)
			goto out;
		pagep->lsn = argp->rootlsn;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0) {
		(void)__db_panic(file_dbp);
		goto out;
	}

	/*
	 * Fix the page copied over the root page.  It's possible that the
	 * page never made it to disk, so if we're undo-ing and the page
	 * doesn't exist, it's okay and there's nothing further to do.
	 */
	if ((ret = memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (!redo)
			goto done;
		(void)__db_pgerr(file_dbp, argp->pgno);
		goto out;
	}
	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &LSN(argp->pgdbt.data));
	if (cmp_p == 0 && redo) {
		/* Need to redo update described. */
		pagep->lsn = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && !redo) {
		/* Need to undo update described. */
		memcpy(pagep, argp->pgdbt.data, argp->pgdbt.size);
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0) {
		(void)__db_panic(file_dbp);
		goto out;
	}

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	REC_CLOSE;
}

/*
 * __bam_adj_recover --
 *	Recovery function for adj.
 *
 * PUBLIC: int __bam_adj_recover
 * PUBLIC:   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__bam_adj_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__bam_adj_args *argp;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int cmp_n, cmp_p, modified, ret;

	REC_PRINT(__bam_adj_print);
	REC_INTRO(__bam_adj_read);

	/* Get the page; if it never existed and we're undoing, we're done. */
	if ((ret = memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (!redo)
			goto done;
		(void)__db_pgerr(file_dbp, argp->pgno);
		goto out;
	}

	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->lsn);
	if (cmp_p == 0 && redo) {
		/* Need to redo update described. */
		if ((ret = __bam_adjindx(file_dbp,
		    pagep, argp->indx, argp->indx_copy, argp->is_insert)) != 0)
			goto err;

		LSN(pagep) = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && !redo) {
		/* Need to undo update described. */
		if ((ret = __bam_adjindx(file_dbp,
		    pagep, argp->indx, argp->indx_copy, !argp->is_insert)) != 0)
			goto err;

		LSN(pagep) = argp->lsn;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

	if (0) {
err:		(void)memp_fput(mpf, pagep, 0);
	}
out:	REC_CLOSE;
}

/*
 * __bam_cadjust_recover --
 *	Recovery function for the adjust of a count change in an internal
 *	page.
 *
 * PUBLIC: int __bam_cadjust_recover
 * PUBLIC:   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__bam_cadjust_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__bam_cadjust_args *argp;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int cmp_n, cmp_p, modified, ret;

	REC_PRINT(__bam_cadjust_print);
	REC_INTRO(__bam_cadjust_read);

	/* Get the page; if it never existed and we're undoing, we're done. */
	if ((ret = memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (!redo)
			goto done;
		(void)__db_pgerr(file_dbp, argp->pgno);
		goto out;
	}

	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->lsn);
	if (cmp_p == 0 && redo) {
		/* Need to redo update described. */
		if (file_dbp->type == DB_BTREE &&
		    F_ISSET(file_dbp, DB_BT_RECNUM)) {
			GET_BINTERNAL(pagep, argp->indx)->nrecs += argp->adjust;
			if (argp->total && PGNO(pagep) == PGNO_ROOT)
				RE_NREC_ADJ(pagep, argp->adjust);
		}
		if (file_dbp->type == DB_RECNO) {
			GET_RINTERNAL(pagep, argp->indx)->nrecs += argp->adjust;
			if (argp->total && PGNO(pagep) == PGNO_ROOT)
				RE_NREC_ADJ(pagep, argp->adjust);
		}

		LSN(pagep) = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && !redo) {
		/* Need to undo update described. */
		if (file_dbp->type == DB_BTREE &&
		    F_ISSET(file_dbp, DB_BT_RECNUM)) {
			GET_BINTERNAL(pagep, argp->indx)->nrecs -= argp->adjust;
			if (argp->total && PGNO(pagep) == PGNO_ROOT)
				RE_NREC_ADJ(pagep, argp->adjust);
		}
		if (file_dbp->type == DB_RECNO) {
			GET_RINTERNAL(pagep, argp->indx)->nrecs -= argp->adjust;
			if (argp->total && PGNO(pagep) == PGNO_ROOT)
				RE_NREC_ADJ(pagep, -(argp->adjust));
		}
		LSN(pagep) = argp->lsn;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	REC_CLOSE;
}

/*
 * __bam_cdel_recover --
 *	Recovery function for the intent-to-delete of a cursor record.
 *
 * PUBLIC: int __bam_cdel_recover
 * PUBLIC:   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__bam_cdel_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__bam_cdel_args *argp;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int cmp_n, cmp_p, modified, ret;

	REC_PRINT(__bam_cdel_print);
	REC_INTRO(__bam_cdel_read);

	/* Get the page; if it never existed and we're undoing, we're done. */
	if ((ret = memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (!redo)
			goto done;
		(void)__db_pgerr(file_dbp, argp->pgno);
		goto out;
	}

	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->lsn);
	if (cmp_p == 0 && redo) {
		/* Need to redo update described. */
		B_DSET(GET_BKEYDATA(pagep, argp->indx + O_INDX)->type);

		LSN(pagep) = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && !redo) {
		/* Need to undo update described. */
		B_DCLR(GET_BKEYDATA(pagep, argp->indx + O_INDX)->type);

		LSN(pagep) = argp->lsn;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	REC_CLOSE;
}

/*
 * __bam_repl_recover --
 *	Recovery function for page item replacement.
 *
 * PUBLIC: int __bam_repl_recover
 * PUBLIC:   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__bam_repl_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__bam_repl_args *argp;
	BKEYDATA *bk;
	DB *file_dbp, *mdbp;
	DBT dbt;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int cmp_n, cmp_p, modified, ret;
	u_int8_t *p;

	REC_PRINT(__bam_repl_print);
	REC_INTRO(__bam_repl_read);

	/* Get the page; if it never existed and we're undoing, we're done. */
	if ((ret = memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (!redo)
			goto done;
		(void)__db_pgerr(file_dbp, argp->pgno);
		goto out;
	}
	bk = GET_BKEYDATA(pagep, argp->indx);

	modified = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->lsn);
	if (cmp_p == 0 && redo) {
		/*
		 * Need to redo update described.
		 *
		 * Re-build the replacement item.
		 */
		memset(&dbt, 0, sizeof(dbt));
		dbt.size = argp->prefix + argp->suffix + argp->repl.size;
		if ((dbt.data = __db_malloc(dbt.size)) == NULL) {
			ret = ENOMEM;
			goto err;
		}
		p = dbt.data;
		memcpy(p, bk->data, argp->prefix);
		p += argp->prefix;
		memcpy(p, argp->repl.data, argp->repl.size);
		p += argp->repl.size;
		memcpy(p, bk->data + (bk->len - argp->suffix), argp->suffix);

		ret = __bam_ritem(file_dbp, pagep, argp->indx, &dbt);
		__db_free(dbt.data);
		if (ret != 0)
			goto err;

		LSN(pagep) = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && !redo) {
		/*
		 * Need to undo update described.
		 *
		 * Re-build the original item.
		 */
		memset(&dbt, 0, sizeof(dbt));
		dbt.size = argp->prefix + argp->suffix + argp->orig.size;
		if ((dbt.data = __db_malloc(dbt.size)) == NULL) {
			ret = ENOMEM;
			goto err;
		}
		p = dbt.data;
		memcpy(p, bk->data, argp->prefix);
		p += argp->prefix;
		memcpy(p, argp->orig.data, argp->orig.size);
		p += argp->orig.size;
		memcpy(p, bk->data + (bk->len - argp->suffix), argp->suffix);

		ret = __bam_ritem(file_dbp, pagep, argp->indx, &dbt);
		__db_free(dbt.data);
		if (ret != 0)
			goto err;

		/* Reset the deleted flag, if necessary. */
		if (argp->isdeleted)
			B_DSET(GET_BKEYDATA(pagep, argp->indx)->type);

		LSN(pagep) = argp->lsn;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

	if (0) {
err:		(void)memp_fput(mpf, pagep, 0);
	}
out:	REC_CLOSE;
}
