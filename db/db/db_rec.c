/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)db_rec.c	10.16 (Sleepycat) 4/28/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_page.h"
#include "log.h"
#include "hash.h"
#include "btree.h"

/*
 * PUBLIC: int __db_addrem_recover
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 *
 * This log message is generated whenever we add or remove a duplicate
 * to/from a duplicate page.  On recover, we just do the opposite.
 */
int
__db_addrem_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__db_addrem_args *argp;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	u_int32_t change;
	int cmp_n, cmp_p, ret;

	REC_PRINT(__db_addrem_print);
	REC_INTRO(__db_addrem_read);

	if ((ret = memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (!redo) {
			/*
			 * We are undoing and the page doesn't exist.  That
			 * is equivalent to having a pagelsn of 0, so we
			 * would not have to undo anything.  In this case,
			 * don't bother creating a page.
			 */
			*lsnp = argp->prev_lsn;
			ret = 0;
			goto out;
		} else
			if ((ret = memp_fget(mpf,
			    &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0)
				goto out;
	}

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	change = 0;
	if ((cmp_p == 0 && redo && argp->opcode == DB_ADD_DUP) ||
	    (cmp_n == 0 && !redo && argp->opcode == DB_REM_DUP)) {

		/* Need to redo an add, or undo a delete. */
		if ((ret = __db_pitem(file_dbp, pagep, argp->indx, argp->nbytes,
		    argp->hdr.size == 0 ? NULL : &argp->hdr,
		    argp->dbt.size == 0 ? NULL : &argp->dbt)) != 0)
			goto out;

		change = DB_MPOOL_DIRTY;

	} else if ((cmp_n == 0 && !redo && argp->opcode == DB_ADD_DUP) ||
	    (cmp_p == 0 && redo && argp->opcode == DB_REM_DUP)) {
		/* Need to undo an add, or redo a delete. */
		if ((ret = __db_ditem(file_dbp,
		    pagep, argp->indx, argp->nbytes)) != 0)
			goto out;
		change = DB_MPOOL_DIRTY;
	}

	if (change)
		if (redo)
			LSN(pagep) = *lsnp;
		else
			LSN(pagep) = argp->pagelsn;

	if ((ret = memp_fput(mpf, pagep, change)) == 0)
		*lsnp = argp->prev_lsn;

out:	REC_CLOSE;
}

/*
 * PUBLIC: int __db_split_recover __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__db_split_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__db_split_args *argp;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int change, cmp_n, cmp_p, ret;

	REC_PRINT(__db_split_print);
	REC_INTRO(__db_split_read);

	if ((ret = memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (!redo) {
			/*
			 * We are undoing and the page doesn't exist.  That
			 * is equivalent to having a pagelsn of 0, so we
			 * would not have to undo anything.  In this case,
			 * don't bother creating a page.
			 */
			*lsnp = argp->prev_lsn;
			ret = 0;
			goto out;
		} else
			if ((ret = memp_fget(mpf,
			    &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0)
				goto out;
	}

	/*
	 * There are two types of log messages here, one for the old page
	 * and one for the new pages created.  The original image in the
	 * SPLITOLD record is used for undo.  The image in the SPLITNEW
	 * is used for redo.  We should never have a case where there is
	 * a redo operation and the SPLITOLD record is on disk, but not
	 * the SPLITNEW record.  Therefore, we only redo NEW messages
	 * and only undo OLD messages.
	 */

	change = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	if (cmp_p == 0 && redo) {
		if (argp->opcode == DB_SPLITNEW) {
			/* Need to redo the split described. */
			memcpy(pagep,
			    argp->pageimage.data, argp->pageimage.size);
		}
		LSN(pagep) = *lsnp;
		change = DB_MPOOL_DIRTY;
	} else if (cmp_n == 0 && !redo) {
		if (argp->opcode == DB_SPLITOLD) {
			/* Put back the old image. */
			memcpy(pagep,
			    argp->pageimage.data, argp->pageimage.size);
		}
		LSN(pagep) = argp->pagelsn;
		change = DB_MPOOL_DIRTY;
	}
	if ((ret = memp_fput(mpf, pagep, change)) == 0)
		*lsnp = argp->prev_lsn;

out:	REC_CLOSE;
}

/*
 * PUBLIC: int __db_big_recover __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__db_big_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__db_big_args *argp;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	u_int32_t change;
	int cmp_n, cmp_p, ret;

	REC_PRINT(__db_big_print);
	REC_INTRO(__db_big_read);

	if ((ret = memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (!redo) {
			/*
			 * We are undoing and the page doesn't exist.  That
			 * is equivalent to having a pagelsn of 0, so we
			 * would not have to undo anything.  In this case,
			 * don't bother creating a page.
			 */
			ret = 0;
			goto ppage;
		} else
			if ((ret = memp_fget(mpf,
			    &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0)
			goto out;
	}

	/*
	 * There are three pages we need to check.  The one on which we are
	 * adding data, the previous one whose next_pointer may have
	 * been updated, and the next one whose prev_pointer may have
	 * been updated.
	 */
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	change = 0;
	if ((cmp_p == 0 && redo && argp->opcode == DB_ADD_BIG) ||
	    (cmp_n == 0 && !redo && argp->opcode == DB_REM_BIG)) {
		/* We are either redo-ing an add, or undoing a delete. */
		P_INIT(pagep, file_dbp->pgsize, argp->pgno, argp->prev_pgno,
			argp->next_pgno, 0, P_OVERFLOW);
		OV_LEN(pagep) = argp->dbt.size;
		OV_REF(pagep) = 1;
		memcpy((u_int8_t *)pagep + P_OVERHEAD, argp->dbt.data,
		    argp->dbt.size);
		PREV_PGNO(pagep) = argp->prev_pgno;
		change = DB_MPOOL_DIRTY;
	} else if ((cmp_n == 0 && !redo && argp->opcode == DB_ADD_BIG) ||
	    (cmp_p == 0 && redo && argp->opcode == DB_REM_BIG)) {
		/*
		 * We are either undo-ing an add or redo-ing a delete.
		 * The page is about to be reclaimed in either case, so
		 * there really isn't anything to do here.
		 */
		change = DB_MPOOL_DIRTY;
	}
	if (change)
		LSN(pagep) = redo ? *lsnp : argp->pagelsn;

	if ((ret = memp_fput(mpf, pagep, change)) != 0)
		goto out;

	/* Now check the previous page. */
ppage:	if (argp->prev_pgno != PGNO_INVALID) {
		change = 0;
		if ((ret = memp_fget(mpf, &argp->prev_pgno, 0, &pagep)) != 0)
			if (!redo) {
				/*
				 * We are undoing and the page doesn't exist.
				 * That is equivalent to having a pagelsn of 0,
				 * so we would not have to undo anything.  In
				 * this case, don't bother creating a page.
				 */
				*lsnp = argp->prev_lsn;
				ret = 0;
				goto npage;
			} else
				if ((ret = memp_fget(mpf, &argp->prev_pgno,
				    DB_MPOOL_CREATE, &pagep)) != 0)
					goto out;

		cmp_n = log_compare(lsnp, &LSN(pagep));
		cmp_p = log_compare(&LSN(pagep), &argp->prevlsn);

		if ((cmp_p == 0 && redo && argp->opcode == DB_ADD_BIG) ||
		    (cmp_n == 0 && !redo && argp->opcode == DB_REM_BIG)) {
			/* Redo add, undo delete. */
			NEXT_PGNO(pagep) = argp->pgno;
			change = DB_MPOOL_DIRTY;
		} else if ((cmp_n == 0 &&
		    !redo && argp->opcode == DB_ADD_BIG) ||
		    (cmp_p == 0 && redo && argp->opcode == DB_REM_BIG)) {
			/* Redo delete, undo add. */
			NEXT_PGNO(pagep) = argp->next_pgno;
			change = DB_MPOOL_DIRTY;
		}
		if (change)
			LSN(pagep) = redo ? *lsnp : argp->prevlsn;
		if ((ret = memp_fput(mpf, pagep, change)) != 0)
			goto out;
	}

	/* Now check the next page.  Can only be set on a delete. */
npage:	if (argp->next_pgno != PGNO_INVALID) {
		change = 0;
		if ((ret = memp_fget(mpf, &argp->next_pgno, 0, &pagep)) != 0)
			if (!redo) {
				/*
				 * We are undoing and the page doesn't exist.
				 * That is equivalent to having a pagelsn of 0,
				 * so we would not have to undo anything.  In
				 * this case, don't bother creating a page.
				 */
				*lsnp = argp->prev_lsn;
				ret = 0;
				goto out;
			} else
				if ((ret = memp_fget(mpf, &argp->next_pgno,
				    DB_MPOOL_CREATE, &pagep)) != 0)
					goto out;

		cmp_n = log_compare(lsnp, &LSN(pagep));
		cmp_p = log_compare(&LSN(pagep), &argp->nextlsn);
		if (cmp_p == 0 && redo) {
			PREV_PGNO(pagep) = PGNO_INVALID;
			change = DB_MPOOL_DIRTY;
		} else if (cmp_n == 0 && !redo) {
			PREV_PGNO(pagep) = argp->pgno;
			change = DB_MPOOL_DIRTY;
		}
		if (change)
			LSN(pagep) = redo ? *lsnp : argp->nextlsn;
		if ((ret = memp_fput(mpf, pagep, change)) != 0)
			goto out;
	}

	*lsnp = argp->prev_lsn;

out:	REC_CLOSE;
}

/*
 * __db_ovref_recover --
 *	Recovery function for __db_ovref().
 *
 * PUBLIC: int __db_ovref_recover __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__db_ovref_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__db_ovref_args *argp;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int modified, ret;

	REC_PRINT(__db_ovref_print);
	REC_INTRO(__db_ovref_read);

	if ((ret = memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		(void)__db_pgerr(file_dbp, argp->pgno);
		goto out;
	}

	modified = 0;
	if (log_compare(&LSN(pagep), &argp->lsn) == 0 && redo) {
		/* Need to redo update described. */
		OV_REF(pagep) += argp->adjust;

		pagep->lsn = *lsnp;
		modified = 1;
	} else if (log_compare(lsnp, &LSN(pagep)) == 0 && !redo) {
		/* Need to undo update described. */
		OV_REF(pagep) -= argp->adjust;

		pagep->lsn = argp->lsn;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) == 0)
		*lsnp = argp->prev_lsn;

out:	REC_CLOSE;
}

/*
 * __db_relink_recover --
 *	Recovery function for relink.
 *
 * PUBLIC: int __db_relink_recover
 * PUBLIC:   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__db_relink_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__db_relink_args *argp;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int modified, ret;

	REC_PRINT(__db_relink_print);
	REC_INTRO(__db_relink_read);

	/*
	 * There are three pages we need to check -- the page, and the
	 * previous and next pages, if they existed.
	 */
	if ((ret = memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		if (redo) {
			(void)__db_pgerr(file_dbp, argp->pgno);
			goto out;
		}
		goto next;
	}
	modified = 0;
	if (log_compare(&LSN(pagep), &argp->lsn) == 0 && redo) {
		/* Redo the relink. */
		pagep->lsn = *lsnp;
		modified = 1;
	} else if (log_compare(lsnp, &LSN(pagep)) == 0 && !redo) {
		/* Undo the relink. */
		pagep->next_pgno = argp->next;
		pagep->prev_pgno = argp->prev;

		pagep->lsn = argp->lsn;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0) {
		(void)__db_panic(file_dbp);
		goto out;
	}

next:	if ((ret = memp_fget(mpf, &argp->next, 0, &pagep)) != 0) {
		if (redo) {
			(void)__db_pgerr(file_dbp, argp->next);
			goto out;
		}
		goto prev;
	}
	modified = 0;
	if (log_compare(&LSN(pagep), &argp->lsn_next) == 0 && redo) {
		/* Redo the relink. */
		pagep->prev_pgno = argp->prev;

		pagep->lsn = *lsnp;
		modified = 1;
	} else if (log_compare(lsnp, &LSN(pagep)) == 0 && !redo) {
		/* Undo the relink. */
		pagep->prev_pgno = argp->pgno;

		pagep->lsn = argp->lsn_next;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0) {
		(void)__db_panic(file_dbp);
		goto out;
	}

prev:	if ((ret = memp_fget(mpf, &argp->prev, 0, &pagep)) != 0) {
		if (redo) {
			(void)__db_pgerr(file_dbp, argp->prev);
			goto out;
		}
		goto done;
	}
	modified = 0;
	if (log_compare(&LSN(pagep), &argp->lsn_prev) == 0 && redo) {
		/* Redo the relink. */
		pagep->next_pgno = argp->next;

		pagep->lsn = *lsnp;
		modified = 1;
	} else if (log_compare(lsnp, &LSN(pagep)) == 0 && !redo) {
		/* Undo the relink. */
		pagep->next_pgno = argp->pgno;

		pagep->lsn = argp->lsn_prev;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0) {
		(void) __db_panic(file_dbp);
		goto out;
	}

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	REC_CLOSE;
}

/*
 * PUBLIC: int __db_addpage_recover
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__db_addpage_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__db_addpage_args *argp;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	u_int32_t change;
	int cmp_n, cmp_p, ret;

	REC_PRINT(__db_addpage_print);
	REC_INTRO(__db_addpage_read);

	/*
	 * We need to check two pages: the old one and the new one onto
	 * which we're going to add duplicates.  Do the old one first.
	 */
	if ((ret = memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0)
		goto out;

	change = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->lsn);
	if (cmp_p == 0 && redo) {
		NEXT_PGNO(pagep) = argp->nextpgno;

		LSN(pagep) = *lsnp;
		change = DB_MPOOL_DIRTY;
	} else if (cmp_n == 0 && !redo) {
		NEXT_PGNO(pagep) = PGNO_INVALID;

		LSN(pagep) = argp->lsn;
		change = DB_MPOOL_DIRTY;
	}
	if ((ret = memp_fput(mpf, pagep, change)) != 0)
		goto out;

	if ((ret = memp_fget(mpf, &argp->nextpgno, 0, &pagep)) != 0)
		if (!redo) {
			/*
			 * We are undoing and the page doesn't exist.  That
			 * is equivalent to having a pagelsn of 0, so we
			 * would not have to undo anything.  In this case,
			 * don't bother creating a page.
			 */
			ret = 0;
			goto out;
		} else
			if ((ret = memp_fget(mpf,
			    &argp->nextpgno, DB_MPOOL_CREATE, &pagep)) != 0)
				goto out;

	change = 0;
	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->nextlsn);
	if (cmp_p == 0 && redo) {
		PREV_PGNO(pagep) = argp->pgno;

		LSN(pagep) = *lsnp;
		change = DB_MPOOL_DIRTY;
	} else if (cmp_n == 0 && !redo) {
		PREV_PGNO(pagep) = PGNO_INVALID;

		LSN(pagep) = argp->nextlsn;
		change = DB_MPOOL_DIRTY;
	}
	ret = memp_fput(mpf, pagep, change);

out:	if (ret == 0)
		*lsnp = argp->prev_lsn;
	REC_CLOSE;
}

/*
 * __db_debug_recover --
 *	Recovery function for debug.
 *
 * PUBLIC: int __db_debug_recover __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__db_debug_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__db_debug_args *argp;
	int ret;

	COMPQUIET(redo, 0);
	COMPQUIET(logp, NULL);

	REC_PRINT(__db_debug_print);
	REC_NOOP_INTRO(__db_debug_read);

	*lsnp = argp->prev_lsn;
	ret = 0;

	REC_NOOP_CLOSE;
}

/*
 * __db_noop_recover --
 *	Recovery function for noop.
 *
 * PUBLIC: int __db_noop_recover __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__db_noop_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__db_noop_args *argp;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	u_int32_t change;
	int cmp_n, cmp_p, ret;

	REC_PRINT(__db_noop_print);
	REC_INTRO(__db_noop_read);

	if ((ret = memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0)
		goto out;

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->prevlsn);
	change = 0;
	if (cmp_p == 0 && redo) {
		LSN(pagep) = *lsnp;
		change = DB_MPOOL_DIRTY;
	} else if (cmp_n == 0 && !redo) {
		LSN(pagep) = argp->prevlsn;
		change = DB_MPOOL_DIRTY;
	}
	*lsnp = argp->prev_lsn;
	ret = memp_fput(mpf, pagep, change);

out:	REC_CLOSE;
}
