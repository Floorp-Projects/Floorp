/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1995, 1996
 *	Margo Seltzer.  All rights reserved.
 */
/*
 * Copyright (c) 1995, 1996
 *	The President and Fellows of Harvard University.  All rights reserved.
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
static const char sccsid[] = "@(#)hash_rec.c	10.19 (Sleepycat) 5/23/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_page.h"
#include "hash.h"
#include "btree.h"
#include "log.h"
#include "common_ext.h"

/*
 * __ham_insdel_recover --
 *
 * PUBLIC: int __ham_insdel_recover
 * PUBLIC:     __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_insdel_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__ham_insdel_args *argp;
	DB *mdbp, *file_dbp;
	DB_MPOOLFILE *mpf;
	HTAB *hashp;
	PAGE *pagep;
	u_int32_t op;
	int cmp_n, cmp_p, getmeta, ret;

	getmeta = 0;
	hashp = NULL;				/* XXX: shut the compiler up. */
	REC_PRINT(__ham_insdel_print);
	REC_INTRO(__ham_insdel_read);

	ret = memp_fget(mpf, &argp->pgno, 0, &pagep);
	if (ret != 0)
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
		} else if ((ret = memp_fget(mpf, &argp->pgno,
		    DB_MPOOL_CREATE, &pagep)) != 0)
			goto out;


	hashp = (HTAB *)file_dbp->internal;
	GET_META(file_dbp, hashp);
	getmeta = 1;

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	/*
	 * Two possible things going on:
	 * redo a delete/undo a put: delete the item from the page.
	 * redo a put/undo a delete: add the item to the page.
	 * If we are undoing a delete, then the information logged is the
	 * entire entry off the page, not just the data of a dbt.  In
	 * this case, we want to copy it back onto the page verbatim.
	 * We do this by calling __putitem with the type H_OFFPAGE instead
	 * of H_KEYDATA.
	 */
	op = OPCODE_OF(argp->opcode);

	if ((op == DELPAIR && cmp_n == 0 && !redo) ||
	    (op == PUTPAIR && cmp_p == 0 && redo)) {
		/*
		 * Need to redo a PUT or undo a delete.  If we are undoing a
		 * delete, we've got to restore the item back to its original
		 * position.  That's a royal pain in the butt (because we do
		 * not store item lengths on the page), but there's no choice.
		 */
		if (op != DELPAIR ||
		    argp->ndx == (u_int32_t)H_NUMPAIRS(pagep)) {
			__ham_putitem(pagep, &argp->key,
			    !redo || PAIR_ISKEYBIG(argp->opcode) ?
			    H_OFFPAGE : H_KEYDATA);
			__ham_putitem(pagep, &argp->data,
			    !redo || PAIR_ISDATABIG(argp->opcode) ?
			    H_OFFPAGE : H_KEYDATA);
		} else
			(void) __ham_reputpair(pagep, hashp->hdr->pagesize,
			    argp->ndx, &argp->key, &argp->data);

		LSN(pagep) = redo ? *lsnp : argp->pagelsn;
		if ((ret = __ham_put_page(file_dbp, pagep, 1)) != 0)
			goto out;

	} else if ((op == DELPAIR && cmp_p == 0 && redo)
	    || (op == PUTPAIR && cmp_n == 0 && !redo)) {
		/* Need to undo a put or redo a delete. */
		__ham_dpair(file_dbp, pagep, argp->ndx);
		LSN(pagep) = redo ? *lsnp : argp->pagelsn;
		if ((ret = __ham_put_page(file_dbp, (PAGE *)pagep, 1)) != 0)
			goto out;
	} else
		if ((ret = __ham_put_page(file_dbp, (PAGE *)pagep, 0)) != 0)
			goto out;

	/* Return the previous LSN. */
	*lsnp = argp->prev_lsn;

out:	if (getmeta)
		RELEASE_META(file_dbp, hashp);
	REC_CLOSE;
}

/*
 * __ham_newpage_recover --
 *	This log message is used when we add/remove overflow pages.  This
 *	message takes care of the pointer chains, not the data on the pages.
 *
 * PUBLIC: int __ham_newpage_recover
 * PUBLIC:     __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_newpage_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__ham_newpage_args *argp;
	DB *mdbp, *file_dbp;
	DB_MPOOLFILE *mpf;
	HTAB *hashp;
	PAGE *pagep;
	int cmp_n, cmp_p, change, getmeta, ret;

	getmeta = 0;
	hashp = NULL;				/* XXX: shut the compiler up. */
	REC_PRINT(__ham_newpage_print);
	REC_INTRO(__ham_newpage_read);

	ret = memp_fget(mpf, &argp->new_pgno, 0, &pagep);
	if (ret != 0)
		if (!redo) {
			/*
			 * We are undoing and the page doesn't exist.  That
			 * is equivalent to having a pagelsn of 0, so we
			 * would not have to undo anything.  In this case,
			 * don't bother creating a page.
			 */
			ret = 0;
			goto ppage;
		} else if ((ret = memp_fget(mpf, &argp->new_pgno,
		    DB_MPOOL_CREATE, &pagep)) != 0)
			goto out;

	hashp = (HTAB *)file_dbp->internal;
	GET_META(file_dbp, hashp);
	getmeta = 1;

	/*
	 * There are potentially three pages we need to check: the one
	 * that we created/deleted, the one before it and the one after
	 * it.
	 */

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);
	change = 0;

	if ((cmp_p == 0 && redo && argp->opcode == PUTOVFL) ||
	    (cmp_n == 0 && !redo && argp->opcode == DELOVFL)) {
		/* Redo a create new page or undo a delete new page. */
		P_INIT(pagep, file_dbp->pgsize, argp->new_pgno,
		    argp->prev_pgno, argp->next_pgno, 0, P_HASH);
		change = 1;
	} else if ((cmp_p == 0 && redo && argp->opcode == DELOVFL) ||
	    (cmp_n == 0 && !redo && argp->opcode == PUTOVFL)) {
		/*
		 * Redo a delete or undo a create new page.  All we
		 * really need to do is change the LSN.
		 */
		change = 1;
	}

	if (!change) {
		if ((ret = __ham_put_page(file_dbp, (PAGE *)pagep, 0)) != 0)
			goto out;
	} else {
		LSN(pagep) = redo ? *lsnp : argp->pagelsn;
		if ((ret = __ham_put_page(file_dbp, (PAGE *)pagep, 1)) != 0)
			goto out;
	}

	/* Now do the prev page. */
ppage:	if (argp->prev_pgno != PGNO_INVALID) {
		ret = memp_fget(mpf, &argp->prev_pgno, 0, &pagep);

		if (ret != 0)
			if (!redo) {
				/*
				 * We are undoing and the page doesn't exist.
				 * That is equivalent to having a pagelsn of 0,
				 * so we would not have to undo anything.  In
				 * this case, don't bother creating a page.
				 */
				ret = 0;
				goto npage;
			} else if ((ret =
			    memp_fget(mpf, &argp->prev_pgno,
			    DB_MPOOL_CREATE, &pagep)) != 0)
				goto out;

		cmp_n = log_compare(lsnp, &LSN(pagep));
		cmp_p = log_compare(&LSN(pagep), &argp->prevlsn);
		change = 0;

		if ((cmp_p == 0 && redo && argp->opcode == PUTOVFL) ||
		    (cmp_n == 0 && !redo && argp->opcode == DELOVFL)) {
			/* Redo a create new page or undo a delete new page. */
			pagep->next_pgno = argp->new_pgno;
			change = 1;
		} else if ((cmp_p == 0 && redo && argp->opcode == DELOVFL) ||
		    (cmp_n == 0 && !redo && argp->opcode == PUTOVFL)) {
			/* Redo a delete or undo a create new page. */
			pagep->next_pgno = argp->next_pgno;
			change = 1;
		}

		if (!change) {
			if ((ret = __ham_put_page(file_dbp, (PAGE *)pagep, 0)) != 0)
				goto out;
		} else {
			LSN(pagep) = redo ? *lsnp : argp->prevlsn;
			if ((ret = __ham_put_page(file_dbp, (PAGE *)pagep, 1)) != 0)
				goto out;
		}
	}

	/* Now time to do the next page */
npage:	if (argp->next_pgno != PGNO_INVALID) {
		ret = memp_fget(mpf, &argp->next_pgno, 0, &pagep);

		if (ret != 0)
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
			} else if ((ret =
			    memp_fget(mpf, &argp->next_pgno,
			    DB_MPOOL_CREATE, &pagep)) != 0)
				goto out;

		cmp_n = log_compare(lsnp, &LSN(pagep));
		cmp_p = log_compare(&LSN(pagep), &argp->nextlsn);
		change = 0;

		if ((cmp_p == 0 && redo && argp->opcode == PUTOVFL) ||
		    (cmp_n == 0 && !redo && argp->opcode == DELOVFL)) {
			/* Redo a create new page or undo a delete new page. */
			pagep->prev_pgno = argp->new_pgno;
			change = 1;
		} else if ((cmp_p == 0 && redo && argp->opcode == DELOVFL) ||
		    (cmp_n == 0 && !redo && argp->opcode == PUTOVFL)) {
			/* Redo a delete or undo a create new page. */
			pagep->prev_pgno = argp->prev_pgno;
			change = 1;
		}

		if (!change) {
			if ((ret =
			    __ham_put_page(file_dbp, (PAGE *)pagep, 0)) != 0)
				goto out;
		} else {
			LSN(pagep) = redo ? *lsnp : argp->nextlsn;
			if ((ret =
			    __ham_put_page(file_dbp, (PAGE *)pagep, 1)) != 0)
				goto out;
		}
	}
	*lsnp = argp->prev_lsn;

out:	if (getmeta)
		RELEASE_META(file_dbp, hashp);
	REC_CLOSE;
}


/*
 * __ham_replace_recover --
 *	This log message refers to partial puts that are local to a single
 *	page.  You can think of them as special cases of the more general
 *	insdel log message.
 *
 * PUBLIC: int __ham_replace_recover
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_replace_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__ham_replace_args *argp;
	DB *mdbp, *file_dbp;
	DB_MPOOLFILE *mpf;
	DBT dbt;
	HTAB *hashp;
	PAGE *pagep;
	int32_t grow;
	int change, cmp_n, cmp_p, getmeta, ret;
	u_int8_t *hk;

	getmeta = 0;
	hashp = NULL;				/* XXX: shut the compiler up. */
	REC_PRINT(__ham_replace_print);
	REC_INTRO(__ham_replace_read);

	ret = memp_fget(mpf, &argp->pgno, 0, &pagep);
	if (ret != 0)
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
		} else if ((ret = memp_fget(mpf, &argp->pgno,
		    DB_MPOOL_CREATE, &pagep)) != 0)
			goto out;

	hashp = (HTAB *)file_dbp->internal;
	GET_META(file_dbp, hashp);
	getmeta = 1;

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);

	if (cmp_p == 0 && redo) {
		change = 1;
		/* Reapply the change as specified. */
		dbt.data = argp->newitem.data;
		dbt.size = argp->newitem.size;
		grow = argp->newitem.size - argp->olditem.size;
		LSN(pagep) = *lsnp;
	} else if (cmp_n == 0 && !redo) {
		change = 1;
		/* Undo the already applied change. */
		dbt.data = argp->olditem.data;
		dbt.size = argp->olditem.size;
		grow = argp->olditem.size - argp->newitem.size;
		LSN(pagep) = argp->pagelsn;
	} else {
		change = 0;
		grow = 0;
	}

	if (change) {
		__ham_onpage_replace(pagep,
		    file_dbp->pgsize, argp->ndx, argp->off, grow, &dbt);
		if (argp->makedup) {
			hk = P_ENTRY(pagep, argp->ndx);
			if (redo)
				HPAGE_PTYPE(hk) = H_DUPLICATE;
			else
				HPAGE_PTYPE(hk) = H_KEYDATA;
		}
	}

	if ((ret = __ham_put_page(file_dbp, pagep, change)) != 0)
		goto out;

	*lsnp = argp->prev_lsn;

out:	if (getmeta)
		RELEASE_META(file_dbp, hashp);
	REC_CLOSE;
}

/*
 * __ham_newpgno_recover --
 *	This log message is used when allocating or deleting an overflow
 *	page.  It takes care of modifying the meta data.
 *
 * PUBLIC: int __ham_newpgno_recover
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_newpgno_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__ham_newpgno_args *argp;
	DB *mdbp, *file_dbp;
	DB_MPOOLFILE *mpf;
	HTAB *hashp;
	PAGE *pagep;
	int change, cmp_n, cmp_p, getmeta, ret;

	getmeta = 0;
	hashp = NULL;				/* XXX: shut the compiler up. */
	REC_PRINT(__ham_newpgno_print);
	REC_INTRO(__ham_newpgno_read);

	hashp = (HTAB *)file_dbp->internal;
	GET_META(file_dbp, hashp);
	getmeta = 1;

	/*
	 * There are two phases to the recovery here.  First we need
	 * to update the meta data; then we need to update the page.
	 * We'll do the meta-data first.
	 */
	cmp_n = log_compare(lsnp, &hashp->hdr->lsn);
	cmp_p = log_compare(&hashp->hdr->lsn, &argp->metalsn);

	change = 0;
	if ((cmp_p == 0 && redo && argp->opcode == ALLOCPGNO) ||
	    (cmp_n == 0 && !redo && argp->opcode == DELPGNO)) {
		/* Need to redo an allocation or undo a deletion. */
		hashp->hdr->last_freed = argp->free_pgno;
		if (redo && argp->old_pgno != 0) /* Must be ALLOCPGNO */
			hashp->hdr->spares[hashp->hdr->ovfl_point]++;
		change = 1;
	} else if (cmp_p == 0 && redo && argp->opcode == DELPGNO) {
		/* Need to redo a deletion */
		hashp->hdr->last_freed = argp->pgno;
		change = 1;
	} else if (cmp_n == 0 && !redo && argp->opcode == ALLOCPGNO) {
		/* undo an allocation. */
		if (argp->old_pgno == 0)
			hashp->hdr->last_freed = argp->pgno;
		else {
			hashp->hdr->spares[hashp->hdr->ovfl_point]--;
			hashp->hdr->last_freed = 0;
		}
		change = 1;
	}
	if (change) {
		hashp->hdr->lsn = redo ? *lsnp : argp->metalsn;
		F_SET(file_dbp, DB_HS_DIRTYMETA);
	}


	/* Now check the newly allocated/freed page. */
	ret = memp_fget(mpf, &argp->pgno, 0, &pagep);

	if (ret != 0)
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
		} else if ((ret = memp_fget(mpf, &argp->pgno,
		    DB_MPOOL_CREATE, &pagep)) != 0)
			goto out;

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);

	change = 0;
	if (cmp_p == 0 && redo && argp->opcode == ALLOCPGNO) {
		/* Need to redo an allocation. */
		P_INIT(pagep, file_dbp->pgsize, argp->pgno, PGNO_INVALID,
		    PGNO_INVALID, 0, argp->new_type);
		change = 1;
	} else if (cmp_n == 0 && !redo && argp->opcode == DELPGNO) {
		/* Undoing a delete. */
		P_INIT(pagep, file_dbp->pgsize, argp->pgno, PGNO_INVALID,
		    argp->old_pgno, 0, argp->old_type);
		change = 1;
	} else if ((cmp_p == 0 && redo && argp->opcode == DELPGNO) ||
	    (cmp_n == 0 && !redo && argp->opcode == ALLOCPGNO)) {
		/* Need to redo a deletion or undo an allocation. */
		NEXT_PGNO(pagep) = argp->free_pgno;
		TYPE(pagep) = P_INVALID;
		change = 1;
	}
	if (change)
		LSN(pagep) = redo ? *lsnp : argp->pagelsn;

	if ((ret = __ham_put_page(file_dbp, pagep, change)) != 0)
		goto out;

	*lsnp = argp->prev_lsn;

out:	if (getmeta)
		RELEASE_META(file_dbp, hashp);
	REC_CLOSE;

}

/*
 * __ham_splitmeta_recover --
 *	This is the meta-data part of the split.  Records the new and old
 *	bucket numbers and the new/old mask information.
 *
 * PUBLIC: int __ham_splitmeta_recover
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_splitmeta_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__ham_splitmeta_args *argp;
	DB *mdbp, *file_dbp;
	DB_MPOOLFILE *mpf;
	HTAB *hashp;
	int change, cmp_n, cmp_p, getmeta, ret;
	u_int32_t pow;

	getmeta = 0;
	hashp = NULL;				/* XXX: shut the compiler up. */
	REC_PRINT(__ham_splitmeta_print);
	REC_INTRO(__ham_splitmeta_read);

	hashp = (HTAB *)file_dbp->internal;
	GET_META(file_dbp, hashp);
	getmeta = 1;

	/*
	 * There are two phases to the recovery here.  First we need
	 * to update the meta data; then we need to update the page.
	 * We'll do the meta-data first.
	 */
	cmp_n = log_compare(lsnp, &hashp->hdr->lsn);
	cmp_p = log_compare(&hashp->hdr->lsn, &argp->metalsn);

	change = 0;
	if (cmp_p == 0 && redo) {
		/* Need to redo the split information. */
		hashp->hdr->max_bucket = argp->bucket + 1;
		pow = __db_log2(hashp->hdr->max_bucket + 1);
		if (pow > hashp->hdr->ovfl_point) {
			hashp->hdr->spares[pow] =
				hashp->hdr->spares[hashp->hdr->ovfl_point];
			hashp->hdr->ovfl_point = pow;
		}
		if (hashp->hdr->max_bucket > hashp->hdr->high_mask) {
			hashp->hdr->low_mask = hashp->hdr->high_mask;
			hashp->hdr->high_mask =
			    hashp->hdr->max_bucket | hashp->hdr->low_mask;
		}
		change = 1;
	} else if (cmp_n == 0 && !redo) {
		/* Need to undo the split information. */
		hashp->hdr->max_bucket = argp->bucket;
		hashp->hdr->ovfl_point = argp->ovflpoint;
		hashp->hdr->spares[hashp->hdr->ovfl_point] = argp->spares;
		pow = 1 << __db_log2(hashp->hdr->max_bucket + 1);
		hashp->hdr->high_mask = pow - 1;
		hashp->hdr->low_mask = (pow >> 1) - 1;
		change = 1;
	}
	if (change) {
		hashp->hdr->lsn = redo ? *lsnp : argp->metalsn;
		F_SET(file_dbp, DB_HS_DIRTYMETA);
	}
	*lsnp = argp->prev_lsn;

out:	if (getmeta)
		RELEASE_META(file_dbp, hashp);
	REC_CLOSE;
}

/*
 * __ham_splitdata_recover --
 *
 * PUBLIC: int __ham_splitdata_recover
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_splitdata_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__ham_splitdata_args *argp;
	DB *mdbp, *file_dbp;
	DB_MPOOLFILE *mpf;
	HTAB *hashp;
	PAGE *pagep;
	int change, cmp_n, cmp_p, getmeta, ret;

	getmeta = 0;
	hashp = NULL;				/* XXX: shut the compiler up. */
	REC_PRINT(__ham_splitdata_print);
	REC_INTRO(__ham_splitdata_read);

	ret = memp_fget(mpf, &argp->pgno, 0, &pagep);
	if (ret != 0)
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
		} else if ((ret = memp_fget(mpf, &argp->pgno,
		    DB_MPOOL_CREATE, &pagep)) != 0)
			goto out;

	hashp = (HTAB *)file_dbp->internal;
	GET_META(file_dbp, hashp);
	getmeta = 1;

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);

	/*
	 * There are two types of log messages here, one for the old page
	 * and one for the new pages created.  The original image in the
	 * SPLITOLD record is used for undo.  The image in the SPLITNEW
	 * is used for redo.  We should never have a case where there is
	 * a redo operation and the SPLITOLD record is on disk, but not
	 * the SPLITNEW record.  Therefore, we only have work to do when
	 * redo NEW messages and undo OLD messages, but we have to update
	 * LSNs in both cases.
	 */
	change = 0;
	if (cmp_p == 0 && redo) {
		if (argp->opcode == SPLITNEW)
			/* Need to redo the split described. */
			memcpy(pagep, argp->pageimage.data,
			    argp->pageimage.size);
		LSN(pagep) = *lsnp;
		change = 1;
	} else if (cmp_n == 0 && !redo) {
		if (argp->opcode == SPLITOLD) {
			/* Put back the old image. */
			memcpy(pagep, argp->pageimage.data,
			    argp->pageimage.size);
		} else
			P_INIT(pagep, file_dbp->pgsize, argp->pgno,
			    PGNO_INVALID, PGNO_INVALID, 0, P_HASH);
		LSN(pagep) = argp->pagelsn;
		change = 1;
	}
	if ((ret = __ham_put_page(file_dbp, pagep, change)) != 0)
		goto out;

	*lsnp = argp->prev_lsn;

out:	if (getmeta)
		RELEASE_META(file_dbp, hashp);
	REC_CLOSE;
}

/*
 * __ham_ovfl_recover --
 *	This message is generated when we initialize a set of overflow pages.
 *
 * PUBLIC: int __ham_ovfl_recover
 * PUBLIC:     __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_ovfl_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__ham_ovfl_args *argp;
	DB *mdbp, *file_dbp;
	DB_MPOOLFILE *mpf;
	HTAB *hashp;
	PAGE *pagep;
	db_pgno_t max_pgno, pgno;
	int cmp_n, cmp_p, getmeta, ret;

	getmeta = 0;
	hashp = NULL;				/* XXX: shut the compiler up. */
	REC_PRINT(__ham_ovfl_print);
	REC_INTRO(__ham_ovfl_read);

	hashp = (HTAB *)file_dbp->internal;
	GET_META(file_dbp, hashp);
	getmeta = 1;

	cmp_n = log_compare(lsnp, &hashp->hdr->lsn);
	cmp_p = log_compare(&hashp->hdr->lsn, &argp->metalsn);

	if (cmp_p == 0 && redo) {
		/* Redo the allocation. */
		hashp->hdr->last_freed = argp->start_pgno;
		hashp->hdr->spares[argp->ovflpoint] += argp->npages;
		hashp->hdr->lsn = *lsnp;
		F_SET(file_dbp, DB_HS_DIRTYMETA);
	} else if (cmp_n == 0 && !redo) {
		hashp->hdr->last_freed = argp->free_pgno;
		hashp->hdr->spares[argp->ovflpoint] -= argp->npages;
		hashp->hdr->lsn = argp->metalsn;
		F_SET(file_dbp, DB_HS_DIRTYMETA);
	}

	max_pgno = argp->start_pgno + argp->npages - 1;
	ret = 0;
	for (pgno = argp->start_pgno; pgno <= max_pgno; pgno++) {
		ret = memp_fget(mpf, &pgno, 0, &pagep);
		if (ret != 0) {
			if (redo && (ret = memp_fget(mpf, &pgno,
			    DB_MPOOL_CREATE, &pagep)) != 0)
				goto out;
			else if (!redo) {
				(void)__ham_put_page(file_dbp, pagep, 0);
				continue;
			}
		}
		if (redo && log_compare((const DB_LSN *)lsnp,
		    (const DB_LSN *)&LSN(pagep)) > 0) {
			P_INIT(pagep, file_dbp->pgsize, pgno, PGNO_INVALID,
			    pgno == max_pgno ? argp->free_pgno : pgno + 1,
			    0, P_HASH);
			LSN(pagep) = *lsnp;
			ret = __ham_put_page(file_dbp, pagep, 1);
		} else if (!redo) {
			ZERO_LSN(pagep->lsn);
			ret = __ham_put_page(file_dbp, pagep, 1);
		} else
			ret = __ham_put_page(file_dbp, pagep, 0);
		if (ret)
			goto out;
	}

	*lsnp = argp->prev_lsn;
out:	if (getmeta)
		RELEASE_META(file_dbp, hashp);
	REC_CLOSE;
}

/*
 * __ham_copypage_recover --
 *	Recovery function for copypage.
 *
 * PUBLIC: int __ham_copypage_recover
 * PUBLIC:   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_copypage_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__ham_copypage_args *argp;
	DB *file_dbp, *mdbp;
	DB_MPOOLFILE *mpf;
	HTAB *hashp;
	PAGE *pagep;
	int cmp_n, cmp_p, getmeta, modified, ret;

	getmeta = 0;
	hashp = NULL;				/* XXX: shut the compiler up. */
	REC_PRINT(__ham_copypage_print);
	REC_INTRO(__ham_copypage_read);

	hashp = (HTAB *)file_dbp->internal;
	GET_META(file_dbp, hashp);
	getmeta = 1;
	modified = 0;

	/* This is the bucket page. */
	ret = memp_fget(mpf, &argp->pgno, 0, &pagep);
	if (ret != 0)
		if (!redo) {
			/*
			 * We are undoing and the page doesn't exist.  That
			 * is equivalent to having a pagelsn of 0, so we
			 * would not have to undo anything.  In this case,
			 * don't bother creating a page.
			 */
			ret = 0;
			goto donext;
		} else if ((ret = memp_fget(mpf, &argp->pgno,
		    DB_MPOOL_CREATE, &pagep)) != 0)
			goto out;

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->pagelsn);

	if (cmp_p == 0 && redo) {
		/* Need to redo update described. */
		memcpy(pagep, argp->page.data, argp->page.size);
		LSN(pagep) = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && !redo) {
		/* Need to undo update described. */
		P_INIT(pagep, hashp->hdr->pagesize, argp->pgno, PGNO_INVALID,
		    argp->next_pgno, 0, P_HASH);
		LSN(pagep) = argp->pagelsn;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;

	/* Now fix up the "next" page. */
donext:	ret = memp_fget(mpf, &argp->next_pgno, 0, &pagep);
	if (ret != 0)
		if (!redo) {
			/*
			 * We are undoing and the page doesn't exist.  That
			 * is equivalent to having a pagelsn of 0, so we
			 * would not have to undo anything.  In this case,
			 * don't bother creating a page.
			 */
			ret = 0;
			goto do_nn;
		} else if ((ret = memp_fget(mpf, &argp->next_pgno,
		    DB_MPOOL_CREATE, &pagep)) != 0)
			goto out;

	/* There is nothing to do in the REDO case; only UNDO. */

	cmp_n = log_compare(lsnp, &LSN(pagep));
	if (cmp_n == 0 && !redo) {
		/* Need to undo update described. */
		memcpy(pagep, argp->page.data, argp->page.size);
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;

	/* Now fix up the next's next page. */
do_nn:	if (argp->nnext_pgno == PGNO_INVALID) {
		*lsnp = argp->prev_lsn;
		goto out;
	}

	ret = memp_fget(mpf, &argp->nnext_pgno, 0, &pagep);
	if (ret != 0)
		if (!redo) {
			/*
			 * We are undoing and the page doesn't exist.  That
			 * is equivalent to having a pagelsn of 0, so we
			 * would not have to undo anything.  In this case,
			 * don't bother creating a page.
			 */
			ret = 0;
			*lsnp = argp->prev_lsn;
			goto out;
		} else if ((ret = memp_fget(mpf, &argp->nnext_pgno,
		    DB_MPOOL_CREATE, &pagep)) != 0)
			goto out;

	cmp_n = log_compare(lsnp, &LSN(pagep));
	cmp_p = log_compare(&LSN(pagep), &argp->nnextlsn);

	if (cmp_p == 0 && redo) {
		/* Need to redo update described. */
		PREV_PGNO(pagep) = argp->pgno;
		LSN(pagep) = *lsnp;
		modified = 1;
	} else if (cmp_n == 0 && !redo) {
		/* Need to undo update described. */
		PREV_PGNO(pagep) = argp->next_pgno;
		LSN(pagep) = argp->nnextlsn;
		modified = 1;
	}
	if ((ret = memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;

	*lsnp = argp->prev_lsn;

out:	if (getmeta)
		RELEASE_META(file_dbp, hashp);
	REC_CLOSE;
}
