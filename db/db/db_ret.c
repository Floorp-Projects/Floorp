/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)db_ret.c	10.13 (Sleepycat) 5/7/98";
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

/*
 * __db_ret --
 *	Build return DBT.
 *
 * PUBLIC: int __db_ret __P((DB *,
 * PUBLIC:    PAGE *, u_int32_t, DBT *, void **, u_int32_t *));
 */
int
__db_ret(dbp, h, indx, dbt, memp, memsize)
	DB *dbp;
	PAGE *h;
	u_int32_t indx;
	DBT *dbt;
	void **memp;
	u_int32_t *memsize;
{
	BKEYDATA *bk;
	HOFFPAGE ho;
	BOVERFLOW *bo;
	u_int32_t len;
	u_int8_t *hk;
	void *data;

	switch (TYPE(h)) {
	case P_HASH:
		hk = P_ENTRY(h, indx);
		if (HPAGE_PTYPE(hk) == H_OFFPAGE) {
			memcpy(&ho, hk, sizeof(HOFFPAGE));
			return (__db_goff(dbp, dbt,
			    ho.tlen, ho.pgno, memp, memsize));
		}
		len = LEN_HKEYDATA(h, dbp->pgsize, indx);
		data = HKEYDATA_DATA(hk);
		break;
	case P_DUPLICATE:
	case P_LBTREE:
	case P_LRECNO:
		bk = GET_BKEYDATA(h, indx);
		if (B_TYPE(bk->type) == B_OVERFLOW) {
			bo = (BOVERFLOW *)bk;
			return (__db_goff(dbp, dbt,
			    bo->tlen, bo->pgno, memp, memsize));
		}
		len = bk->len;
		data = bk->data;
		break;
	default:
		return (__db_pgfmt(dbp, h->pgno));
	}

	return (__db_retcopy(dbt, data, len, memp, memsize,
	    F_ISSET(dbt, DB_DBT_INTERNAL) ? NULL : dbp->db_malloc));
}

/*
 * __db_retcopy --
 *	Copy the returned data into the user's DBT, handling special flags.
 *
 * PUBLIC: int __db_retcopy __P((DBT *,
 * PUBLIC:    void *, u_int32_t, void **, u_int32_t *, void *(*)(size_t)));
 */
int
__db_retcopy(dbt, data, len, memp, memsize, db_malloc)
	DBT *dbt;
	void *data;
	u_int32_t len;
	void **memp;
	u_int32_t *memsize;
	void *(*db_malloc) __P((size_t));
{
	/* If returning a partial record, reset the length. */
	if (F_ISSET(dbt, DB_DBT_PARTIAL)) {
		data = (u_int8_t *)data + dbt->doff;
		if (len > dbt->doff) {
			len -= dbt->doff;
			if (len > dbt->dlen)
				len = dbt->dlen;
		} else
			len = 0;
	}

	/*
	 * Return the length of the returned record in the DBT size field.
	 * This satisfies the requirement that if we're using user memory
	 * and insufficient memory was provided, return the amount necessary
	 * in the size field.
	 */
	dbt->size = len;

	/*
	 * Allocate memory to be owned by the application: DB_DBT_MALLOC.
	 *
	 * !!!
	 * We always allocate memory, even if we're copying out 0 bytes. This
	 * guarantees consistency, i.e., the application can always free memory
	 * without concern as to how many bytes of the record were requested.
	 *
	 * XXX
	 * Never allocate 0 bytes, it's known to make malloc/realloc unhappy.
	 *
	 * Use the memory specified by the application: DB_DBT_USERMEM.
	 *
	 * !!!
	 * If the length we're going to copy is 0, the application-supplied
	 * memory pointer is allowed to be NULL.
	 */
	if (F_ISSET(dbt, DB_DBT_MALLOC)) {
		dbt->data = db_malloc == NULL ?
		    (void *)__db_malloc(len) :
		    (void *)db_malloc(len + 1);
		if (dbt->data == NULL)
			return (ENOMEM);
	} else if (F_ISSET(dbt, DB_DBT_USERMEM)) {
		if (len != 0 && (dbt->data == NULL || dbt->ulen < len))
			return (ENOMEM);
	} else if (memp == NULL || memsize == NULL) {
		return (EINVAL);
	} else {
		if (len != 0 && (*memsize == 0 || *memsize < len)) {
			*memp = *memp == NULL ?
			    (void *)__db_malloc(len) :
			    (void *)__db_realloc(*memp, len);
			if (*memp == NULL) {
				*memsize = 0;
				return (ENOMEM);
			}
			*memsize = len;
		}
		dbt->data = *memp;
	}

	if (len != 0)
		memcpy(dbt->data, data, len);
	return (0);
}
