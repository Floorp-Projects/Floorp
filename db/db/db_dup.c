/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)db_dup.c	10.18 (Sleepycat) 5/31/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"
#include "db_am.h"

static int __db_addpage __P((DB *,
    PAGE **, db_indx_t *, int (*)(DB *, u_int32_t, PAGE **)));
static int __db_dsplit __P((DB *,
    PAGE **, db_indx_t *, u_int32_t, int (*)(DB *, u_int32_t, PAGE **)));

/*
 * __db_dput --
 *	Put a duplicate item onto a duplicate page at the given index.
 *
 * PUBLIC: int __db_dput __P((DB *,
 * PUBLIC:    DBT *, PAGE **, db_indx_t *, int (*)(DB *, u_int32_t, PAGE **)));
 */
int
__db_dput(dbp, dbt, pp, indxp, newfunc)
	DB *dbp;
	DBT *dbt;
	PAGE **pp;
	db_indx_t *indxp;
	int (*newfunc) __P((DB *, u_int32_t, PAGE **));
{
	BOVERFLOW bo;
	DBT *data_dbtp, hdr_dbt, *hdr_dbtp;
	PAGE *pagep;
	db_indx_t size, isize;
	db_pgno_t pgno;
	int ret;

	/*
	 * We need some access method independent threshold for when we put
	 * a duplicate item onto an overflow page.
	 */
	if (dbt->size > 0.25 * dbp->pgsize) {
		if ((ret = __db_poff(dbp, dbt, &pgno, newfunc)) != 0)
			return (ret);
		B_TSET(bo.type, B_OVERFLOW, 0);
		bo.tlen = dbt->size;
		bo.pgno = pgno;
		hdr_dbt.data = &bo;
		hdr_dbt.size = isize = BOVERFLOW_SIZE;
		hdr_dbtp = &hdr_dbt;
		size = BOVERFLOW_PSIZE;
		data_dbtp = NULL;
	} else {
		size = BKEYDATA_PSIZE(dbt->size);
		isize = BKEYDATA_SIZE(dbt->size);
		hdr_dbtp = NULL;
		data_dbtp = dbt;
	}

	pagep = *pp;
	if (size > P_FREESPACE(pagep)) {
		if (*indxp == NUM_ENT(*pp) && NEXT_PGNO(*pp) == PGNO_INVALID)
			ret = __db_addpage(dbp, pp, indxp, newfunc);
		else
			ret = __db_dsplit(dbp, pp, indxp, isize, newfunc);
		if (ret != 0)
			/* XXX: Pages not returned to free list. */
			return (ret);
		pagep = *pp;
	}

	/*
	 * Now, pagep references the page on which to insert and indx is the
	 * the location to insert.
	 */
	if ((ret = __db_pitem(dbp,
	    pagep, (u_int32_t)*indxp, isize, hdr_dbtp, data_dbtp)) != 0)
		return (ret);

	(void)memp_fset(dbp->mpf, pagep, DB_MPOOL_DIRTY);
	return (0);
}

/*
 * __db_drem --
 *	Remove a duplicate at the given index on the given page.
 *
 * PUBLIC: int __db_drem __P((DB *,
 * PUBLIC:    PAGE **, u_int32_t, int (*)(DB *, PAGE *)));
 */
int
__db_drem(dbp, pp, indx, freefunc)
	DB *dbp;
	PAGE **pp;
	u_int32_t indx;
	int (*freefunc) __P((DB *, PAGE *));
{
	PAGE *pagep;
	int ret;

	pagep = *pp;

	/* Check if we are freeing a big item. */
	if (B_TYPE(GET_BKEYDATA(pagep, indx)->type) == B_OVERFLOW) {
		if ((ret = __db_doff(dbp,
		    GET_BOVERFLOW(pagep, indx)->pgno, freefunc)) != 0)
			return (ret);
		ret = __db_ditem(dbp, pagep, indx, BOVERFLOW_SIZE);
	} else
		ret = __db_ditem(dbp, pagep, indx,
		    BKEYDATA_SIZE(GET_BKEYDATA(pagep, indx)->len));
	if (ret != 0)
		return (ret);

	if (NUM_ENT(pagep) == 0) {
		/*
		 * If the page is emptied, then the page is freed and the pp
		 * parameter is set to reference the next, locked page in the
		 * duplicate chain, if one exists.  If there was no such page,
		 * then it is set to NULL.
		 *
		 * !!!
		 * __db_relink will set the dirty bit for us.
		 */
		if ((ret = __db_relink(dbp, pagep, pp, 0)) != 0)
			return (ret);
		if ((ret = freefunc(dbp, pagep)) != 0)
			return (ret);
	} else
		(void)memp_fset(dbp->mpf, pagep, DB_MPOOL_DIRTY);

	return (0);
}

/*
 * __db_dend --
 *	Find the last page in a set of offpage duplicates.
 *
 * PUBLIC: int __db_dend __P((DB *, db_pgno_t, PAGE **));
 */
int
__db_dend(dbp, pgno, pagep)
	DB *dbp;
	db_pgno_t pgno;
	PAGE **pagep;
{
	PAGE *h;
	int ret;

	/*
	 * This implements DB_KEYLAST.  The last page is returned in pp; pgno
	 * should be the page number of the first page of the duplicate chain.
	 */
	for (;;) {
		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &h)) != 0) {
			(void)__db_pgerr(dbp, pgno);
			return (ret);
		}
		if ((pgno = NEXT_PGNO(h)) == PGNO_INVALID)
			break;
		(void)memp_fput(dbp->mpf, h, 0);
	}

	*pagep = h;
	return (0);
}

/*
 * __db_dsplit --
 *	Split a page of duplicates, calculating the split point based
 *	on an element of size "size" being added at "*indxp".
 *	On entry hp contains a pointer to the page-pointer of the original
 *	page.  On exit, it returns a pointer to the page containing "*indxp"
 *	and "indxp" has been modified to reflect the index on the new page
 *	where the element should be added.  The function returns with
 *	the page on which the insert should happen, not yet put.
 */
static int
__db_dsplit(dbp, hp, indxp, size, newfunc)
	DB *dbp;
	PAGE **hp;
	db_indx_t *indxp;
	u_int32_t size;
	int (*newfunc) __P((DB *, u_int32_t, PAGE **));
{
	PAGE *h, *np, *tp;
	BKEYDATA *bk;
	DBT page_dbt;
	db_indx_t halfbytes, i, indx, lastsum, nindex, oindex, s, sum;
	int did_indx, ret;

	h = *hp;
	indx = *indxp;

	/* Create a temporary page to do compaction onto. */
	if ((tp = (PAGE *)__db_malloc(dbp->pgsize)) == NULL)
		return (ENOMEM);
#ifdef DIAGNOSTIC
	memset(tp, 0xff, dbp->pgsize);
#endif
	/* Create new page for the split. */
	if ((ret = newfunc(dbp, P_DUPLICATE, &np)) != 0) {
		FREE(tp, dbp->pgsize);
		return (ret);
	}

	P_INIT(np, dbp->pgsize, PGNO(np), PGNO(h), NEXT_PGNO(h), 0,
	    P_DUPLICATE);
	P_INIT(tp, dbp->pgsize, PGNO(h), PREV_PGNO(h), PGNO(np), 0,
	    P_DUPLICATE);

	/* Figure out the split point */
	halfbytes = (dbp->pgsize - HOFFSET(h)) / 2;
	did_indx = 0;
	for (sum = 0, lastsum = 0, i = 0; i < NUM_ENT(h); i++) {
		if (i == indx) {
			sum += size;
			did_indx = 1;
			if (lastsum < halfbytes && sum >= halfbytes) {
				/* We've crossed the halfway point. */
				if ((db_indx_t)(halfbytes - lastsum) <
				    (db_indx_t)(sum - halfbytes)) {
					*hp = np;
					*indxp = 0;
					i--;
				} else
					*indxp = i;
				break;
			}
			*indxp = i;
			lastsum = sum;
		}
		if (B_TYPE(GET_BKEYDATA(h, i)->type) == B_KEYDATA)
			sum += BKEYDATA_SIZE(GET_BKEYDATA(h, i)->len);
		else
			sum += BOVERFLOW_SIZE;

		if (lastsum < halfbytes && sum >= halfbytes) {
			/* We've crossed the halfway point. */
			if ((db_indx_t)(halfbytes - lastsum) <
			    (db_indx_t)(sum - halfbytes))
				i--;
			break;
		}
	}

	/*
	 * Check if we have set the return values of the index pointer and
	 * page pointer.
	 */
	if (!did_indx) {
		*hp = np;
		*indxp = indx - i - 1;
	}

	if (DB_LOGGING(dbp)) {
		page_dbt.size = dbp->pgsize;
		page_dbt.data = h;
		if ((ret = __db_split_log(dbp->dbenv->lg_info,
		    dbp->txn, &LSN(h), 0, DB_SPLITOLD, dbp->log_fileid,
		    PGNO(h), &page_dbt, &LSN(h))) != 0) {
			FREE(tp, dbp->pgsize);
			return (ret);
		}
		LSN(tp) = LSN(h);
	}

	/*
	 * If it's a btree, adjust the cursors.
	 *
	 * i is the index of the last element to stay on the page.
	 */
	if (dbp->type == DB_BTREE || dbp->type == DB_RECNO)
		__bam_ca_split(dbp, PGNO(h), PGNO(h), PGNO(np), i + 1, 0);

	for (nindex = 0, oindex = i + 1; oindex < NUM_ENT(h); oindex++) {
		bk = GET_BKEYDATA(h, oindex);
		if (B_TYPE(bk->type) == B_KEYDATA)
			s = BKEYDATA_SIZE(bk->len);
		else
			s = BOVERFLOW_SIZE;

		np->inp[nindex++] = HOFFSET(np) -= s;
		memcpy((u_int8_t *)np + HOFFSET(np), bk, s);
		NUM_ENT(np)++;
	}

	/*
	 * Now do data compaction by copying the remaining stuff onto the
	 * temporary page and then copying it back to the real page.
	 */
	for (nindex = 0, oindex = 0; oindex <= i; oindex++) {
		bk = GET_BKEYDATA(h, oindex);
		if (B_TYPE(bk->type) == B_KEYDATA)
			s = BKEYDATA_SIZE(bk->len);
		else
			s = BOVERFLOW_SIZE;

		tp->inp[nindex++] = HOFFSET(tp) -= s;
		memcpy((u_int8_t *)tp + HOFFSET(tp), bk, s);
		NUM_ENT(tp)++;
	}

	/*
	 * This page (the temporary) should be only half full, so we do two
	 * memcpy's, one for the top of the page and one for the bottom of
	 * the page.  This way we avoid copying the middle which should be
	 * about half a page.
	 */
	memcpy(h, tp, LOFFSET(tp));
	memcpy((u_int8_t *)h + HOFFSET(tp),
	    (u_int8_t *)tp + HOFFSET(tp), dbp->pgsize - HOFFSET(tp));
	FREE(tp, dbp->pgsize);

	if (DB_LOGGING(dbp)) {
		page_dbt.size = dbp->pgsize;
		page_dbt.data = h;
		if ((ret = __db_split_log(dbp->dbenv->lg_info,
		    dbp->txn, &LSN(h), 0, DB_SPLITNEW, dbp->log_fileid,
		    PGNO(h), &page_dbt, &LSN(h))) != 0)
			return (ret);

		page_dbt.size = dbp->pgsize;
		page_dbt.data = np;
		if ((ret = __db_split_log(dbp->dbenv->lg_info,
		    dbp->txn, &LSN(np), 0, DB_SPLITNEW, dbp->log_fileid,
		    PGNO(np),  &page_dbt, &LSN(np))) != 0)
			return (ret);
	}

	/*
	 * Figure out if the location we're interested in is on the new
	 * page, and if so, reset the callers' pointer.  Push the other
	 * page back to the store.
	 */
	if (*hp == h)
		ret = memp_fput(dbp->mpf, np, DB_MPOOL_DIRTY);
	else
		ret = memp_fput(dbp->mpf, h, DB_MPOOL_DIRTY);

	return (ret);
}

/*
 * __db_ditem --
 *	Remove an item from a page.
 *
 * PUBLIC:  int __db_ditem __P((DB *, PAGE *, u_int32_t, u_int32_t));
 */
int
__db_ditem(dbp, pagep, indx, nbytes)
	DB *dbp;
	PAGE *pagep;
	u_int32_t indx, nbytes;
{
	DBT ldbt;
	db_indx_t cnt, offset;
	int ret;
	u_int8_t *from;

	if (DB_LOGGING(dbp)) {
		ldbt.data = P_ENTRY(pagep, indx);
		ldbt.size = nbytes;
		if ((ret = __db_addrem_log(dbp->dbenv->lg_info, dbp->txn,
		    &LSN(pagep), 0, DB_REM_DUP, dbp->log_fileid, PGNO(pagep),
		    (u_int32_t)indx, nbytes, &ldbt, NULL, &LSN(pagep))) != 0)
			return (ret);
	}

	/*
	 * If there's only a single item on the page, we don't have to
	 * work hard.
	 */
	if (NUM_ENT(pagep) == 1) {
		NUM_ENT(pagep) = 0;
		HOFFSET(pagep) = dbp->pgsize;
		return (0);
	}

	/*
	 * Pack the remaining key/data items at the end of the page.  Use
	 * memmove(3), the regions may overlap.
	 */
	from = (u_int8_t *)pagep + HOFFSET(pagep);
	memmove(from + nbytes, from, pagep->inp[indx] - HOFFSET(pagep));
	HOFFSET(pagep) += nbytes;

	/* Adjust the indices' offsets. */
	offset = pagep->inp[indx];
	for (cnt = 0; cnt < NUM_ENT(pagep); ++cnt)
		if (pagep->inp[cnt] < offset)
			pagep->inp[cnt] += nbytes;

	/* Shift the indices down. */
	--NUM_ENT(pagep);
	if (indx != NUM_ENT(pagep))
		memmove(&pagep->inp[indx], &pagep->inp[indx + 1],
		    sizeof(db_indx_t) * (NUM_ENT(pagep) - indx));

	/* If it's a btree, adjust the cursors. */
	if (dbp->type == DB_BTREE || dbp->type == DB_RECNO)
		__bam_ca_di(dbp, PGNO(pagep), indx, -1);

	return (0);
}

/*
 * __db_pitem --
 *	Put an item on a page.
 *
 * PUBLIC: int __db_pitem
 * PUBLIC:     __P((DB *, PAGE *, u_int32_t, u_int32_t, DBT *, DBT *));
 */
int
__db_pitem(dbp, pagep, indx, nbytes, hdr, data)
	DB *dbp;
	PAGE *pagep;
	u_int32_t indx;
	u_int32_t nbytes;
	DBT *hdr, *data;
{
	BKEYDATA bk;
	DBT thdr;
	int ret;
	u_int8_t *p;

	/*
	 * Put a single item onto a page.  The logic figuring out where to
	 * insert and whether it fits is handled in the caller.  All we do
	 * here is manage the page shuffling.  We cheat a little bit in that
	 * we don't want to copy the dbt on a normal put twice.  If hdr is
	 * NULL, we create a BKEYDATA structure on the page, otherwise, just
	 * copy the caller's information onto the page.
	 *
	 * This routine is also used to put entries onto the page where the
	 * entry is pre-built, e.g., during recovery.  In this case, the hdr
	 * will point to the entry, and the data argument will be NULL.
	 *
	 * !!!
	 * There's a tremendous potential for off-by-one errors here, since
	 * the passed in header sizes must be adjusted for the structure's
	 * placeholder for the trailing variable-length data field.
	 */
	if (DB_LOGGING(dbp))
		if ((ret = __db_addrem_log(dbp->dbenv->lg_info, dbp->txn,
		    &LSN(pagep), 0, DB_ADD_DUP, dbp->log_fileid, PGNO(pagep),
		    (u_int32_t)indx, nbytes, hdr, data, &LSN(pagep))) != 0)
			return (ret);

	if (hdr == NULL) {
		B_TSET(bk.type, B_KEYDATA, 0);
		bk.len = data == NULL ? 0 : data->size;

		thdr.data = &bk;
		thdr.size = SSZA(BKEYDATA, data);
		hdr = &thdr;
	}

	/* Adjust the index table, then put the item on the page. */
	if (indx != NUM_ENT(pagep))
		memmove(&pagep->inp[indx + 1], &pagep->inp[indx],
		    sizeof(db_indx_t) * (NUM_ENT(pagep) - indx));
	HOFFSET(pagep) -= nbytes;
	pagep->inp[indx] = HOFFSET(pagep);
	++NUM_ENT(pagep);

	p = P_ENTRY(pagep, indx);
	memcpy(p, hdr->data, hdr->size);
	if (data != NULL)
		memcpy(p + hdr->size, data->data, data->size);

	/* If it's a btree, adjust the cursors. */
	if (dbp->type == DB_BTREE || dbp->type == DB_RECNO)
		__bam_ca_di(dbp, PGNO(pagep), indx, 1);

	return (0);
}

/*
 * __db_relink --
 *	Relink around a deleted page.
 *
 * PUBLIC: int __db_relink __P((DB *, PAGE *, PAGE **, int));
 */
int
__db_relink(dbp, pagep, new_next, needlock)
	DB *dbp;
	PAGE *pagep, **new_next;
	int needlock;
{
	PAGE *np, *pp;
	DB_LOCK npl, ppl;
	DB_LSN *nlsnp, *plsnp;
	int ret;

	ret = 0;
	np = pp = NULL;
	npl = ppl = LOCK_INVALID;
	nlsnp = plsnp = NULL;

	/* Retrieve and lock the two pages. */
	if (pagep->next_pgno != PGNO_INVALID) {
		if (needlock && (ret = __bam_lget(dbp,
		    0, pagep->next_pgno, DB_LOCK_WRITE, &npl)) != 0)
			goto err;
		if ((ret = memp_fget(dbp->mpf,
		    &pagep->next_pgno, 0, &np)) != 0) {
			(void)__db_pgerr(dbp, pagep->next_pgno);
			goto err;
		}
		nlsnp = &np->lsn;
	}
	if (pagep->prev_pgno != PGNO_INVALID) {
		if (needlock && (ret = __bam_lget(dbp,
		    0, pagep->prev_pgno, DB_LOCK_WRITE, &ppl)) != 0)
			goto err;
		if ((ret = memp_fget(dbp->mpf,
		    &pagep->prev_pgno, 0, &pp)) != 0) {
			(void)__db_pgerr(dbp, pagep->next_pgno);
			goto err;
		}
		plsnp = &pp->lsn;
	}

	/* Log the change. */
	if (DB_LOGGING(dbp)) {
		if ((ret = __db_relink_log(dbp->dbenv->lg_info, dbp->txn,
		    &pagep->lsn, 0, dbp->log_fileid, pagep->pgno, &pagep->lsn,
		    pagep->prev_pgno, plsnp, pagep->next_pgno, nlsnp)) != 0)
			goto err;
		if (np != NULL)
			np->lsn = pagep->lsn;
		if (pp != NULL)
			pp->lsn = pagep->lsn;
	}

	/*
	 * Modify and release the two pages.
	 *
	 * !!!
	 * The parameter new_next gets set to the page following the page we
	 * are removing.  If there is no following page, then new_next gets
	 * set to NULL.
	 */
	if (np != NULL) {
		np->prev_pgno = pagep->prev_pgno;
		if (new_next == NULL)
			ret = memp_fput(dbp->mpf, np, DB_MPOOL_DIRTY);
		else {
			*new_next = np;
			ret = memp_fset(dbp->mpf, np, DB_MPOOL_DIRTY);
		}
		if (ret != 0)
			goto err;
		if (needlock)
			(void)__bam_lput(dbp, npl);
	} else if (new_next != NULL)
		*new_next = NULL;

	if (pp != NULL) {
		pp->next_pgno = pagep->next_pgno;
		if ((ret = memp_fput(dbp->mpf, pp, DB_MPOOL_DIRTY)) != 0)
			goto err;
		if (needlock)
			(void)__bam_lput(dbp, ppl);
	}
	return (0);

err:	if (np != NULL)
		(void)memp_fput(dbp->mpf, np, 0);
	if (needlock && npl != LOCK_INVALID)
		(void)__bam_lput(dbp, npl);
	if (pp != NULL)
		(void)memp_fput(dbp->mpf, pp, 0);
	if (needlock && ppl != LOCK_INVALID)
		(void)__bam_lput(dbp, ppl);
	return (ret);
}

/*
 * __db_ddup --
 *	Delete an offpage chain of duplicates.
 *
 * PUBLIC: int __db_ddup __P((DB *, db_pgno_t, int (*)(DB *, PAGE *)));
 */
int
__db_ddup(dbp, pgno, freefunc)
	DB *dbp;
	db_pgno_t pgno;
	int (*freefunc) __P((DB *, PAGE *));
{
	PAGE *pagep;
	DBT tmp_dbt;
	int ret;

	do {
		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &pagep)) != 0) {
			(void)__db_pgerr(dbp, pgno);
			return (ret);
		}

		if (DB_LOGGING(dbp)) {
			tmp_dbt.data = pagep;
			tmp_dbt.size = dbp->pgsize;
			if ((ret = __db_split_log(dbp->dbenv->lg_info, dbp->txn,
			    &LSN(pagep), 0, DB_SPLITOLD, dbp->log_fileid,
			    PGNO(pagep), &tmp_dbt, &LSN(pagep))) != 0)
				return (ret);
		}
		pgno = pagep->next_pgno;
		if ((ret = freefunc(dbp, pagep)) != 0)
			return (ret);
	} while (pgno != PGNO_INVALID);

	return (0);
}

/*
 * __db_addpage --
 *	Create a new page and link it onto the next_pgno field of the
 *	current page.
 */
static int
__db_addpage(dbp, hp, indxp, newfunc)
	DB *dbp;
	PAGE **hp;
	db_indx_t *indxp;
	int (*newfunc) __P((DB *, u_int32_t, PAGE **));
{
	PAGE *newpage;
	int ret;

	if ((ret = newfunc(dbp, P_DUPLICATE, &newpage)) != 0)
		return (ret);

	if (DB_LOGGING(dbp)) {
		if ((ret = __db_addpage_log(dbp->dbenv->lg_info,
		    dbp->txn, &LSN(*hp), 0, dbp->log_fileid,
		    PGNO(*hp), &LSN(*hp), PGNO(newpage), &LSN(newpage))) != 0) {
			return (ret);
		}
		LSN(newpage) = LSN(*hp);
	}

	PREV_PGNO(newpage) = PGNO(*hp);
	NEXT_PGNO(*hp) = PGNO(newpage);

	if ((ret = memp_fput(dbp->mpf, *hp, DB_MPOOL_DIRTY)) != 0)
		return (ret);
	*hp = newpage;
	*indxp = 0;
	return (0);
}
