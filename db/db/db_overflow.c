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
static const char sccsid[] = "@(#)db_overflow.c	10.11 (Sleepycat) 5/7/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_am.h"

/*
 * Big key/data code.
 *
 * Big key and data entries are stored on linked lists of pages.  The initial
 * reference is a structure with the total length of the item and the page
 * number where it begins.  Each entry in the linked list contains a pointer
 * to the next page of data, and so on.
 */

/*
 * __db_goff --
 *	Get an offpage item.
 *
 * PUBLIC: int __db_goff __P((DB *, DBT *,
 * PUBLIC:     u_int32_t, db_pgno_t, void **, u_int32_t *));
 */
int
__db_goff(dbp, dbt, tlen, pgno, bpp, bpsz)
	DB *dbp;
	DBT *dbt;
	u_int32_t tlen;
	db_pgno_t pgno;
	void **bpp;
	u_int32_t *bpsz;
{
	PAGE *h;
	db_indx_t bytes;
	u_int32_t curoff, needed, start;
	u_int8_t *p, *src;
	int ret;

	/*
	 * Check if the buffer is big enough; if it is not and we are
	 * allowed to malloc space, then we'll malloc it.  If we are
	 * not (DB_DBT_USERMEM), then we'll set the dbt and return
	 * appropriately.
	 */
	if (F_ISSET(dbt, DB_DBT_PARTIAL)) {
		start = dbt->doff;
		needed = dbt->dlen;
	} else {
		start = 0;
		needed = tlen;
	}

	/*
	 * Allocate any necessary memory.
	 *
	 * XXX: Never allocate 0 bytes;
	 */
	if (F_ISSET(dbt, DB_DBT_USERMEM)) {
		if (needed > dbt->ulen) {
			dbt->size = needed;
			return (ENOMEM);
		}
	} else if (F_ISSET(dbt, DB_DBT_MALLOC)) {
		dbt->data = dbp->db_malloc == NULL ?
		    (void *)__db_malloc(needed + 1) :
		    (void *)dbp->db_malloc(needed + 1);
		if (dbt->data == NULL)
			return (ENOMEM);
	} else if (*bpsz == 0 || *bpsz < needed) {
		*bpp = (*bpp == NULL ?
		    (void *)__db_malloc(needed + 1) :
		    (void *)__db_realloc(*bpp, needed + 1));
		if (*bpp == NULL)
			return (ENOMEM);
		*bpsz = needed + 1;
		dbt->data = *bpp;
	} else
		dbt->data = *bpp;

	/*
	 * Step through the linked list of pages, copying the data on each
	 * one into the buffer.  Never copy more than the total data length.
	 */
	dbt->size = needed;
	for (curoff = 0, p = dbt->data; pgno != P_INVALID && needed > 0;) {
		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &h)) != 0) {
			(void)__db_pgerr(dbp, pgno);
			return (ret);
		}
		/* Check if we need any bytes from this page. */
		if (curoff + OV_LEN(h) >= start) {
			src = (u_int8_t *)h + P_OVERHEAD;
			bytes = OV_LEN(h);
			if (start > curoff) {
				src += start - curoff;
				bytes -= start - curoff;
			}
			if (bytes > needed)
				bytes = needed;
			memcpy(p, src, bytes);
			p += bytes;
			needed -= bytes;
		}
		curoff += OV_LEN(h);
		pgno = h->next_pgno;
		memp_fput(dbp->mpf, h, 0);
	}
	return (0);
}

/*
 * __db_poff --
 *	Put an offpage item.
 *
 * PUBLIC: int __db_poff __P((DB *, const DBT *, db_pgno_t *,
 * PUBLIC:     int (*)(DB *, u_int32_t, PAGE **)));
 */
int
__db_poff(dbp, dbt, pgnop, newfunc)
	DB *dbp;
	const DBT *dbt;
	db_pgno_t *pgnop;
	int (*newfunc) __P((DB *, u_int32_t, PAGE **));
{
	PAGE *pagep, *lastp;
	DB_LSN new_lsn, null_lsn;
	DBT tmp_dbt;
	db_indx_t pagespace;
	u_int32_t sz;
	u_int8_t *p;
	int ret;

	/*
	 * Allocate pages and copy the key/data item into them.  Calculate the
	 * number of bytes we get for pages we fill completely with a single
	 * item.
	 */
	pagespace = P_MAXSPACE(dbp->pgsize);

	lastp = NULL;
	for (p = dbt->data,
	    sz = dbt->size; sz > 0; p += pagespace, sz -= pagespace) {
		/*
		 * Reduce pagespace so we terminate the loop correctly and
		 * don't copy too much data.
		 */
		if (sz < pagespace)
			pagespace = sz;

		/*
		 * Allocate and initialize a new page and copy all or part of
		 * the item onto the page.  If sz is less than pagespace, we
		 * have a partial record.
		 */
		if ((ret = newfunc(dbp, P_OVERFLOW, &pagep)) != 0)
			return (ret);
		if (DB_LOGGING(dbp)) {
			tmp_dbt.data = p;
			tmp_dbt.size = pagespace;
			ZERO_LSN(null_lsn);
			if ((ret = __db_big_log(dbp->dbenv->lg_info, dbp->txn,
			    &new_lsn, 0, DB_ADD_BIG, dbp->log_fileid,
			    PGNO(pagep), lastp ? PGNO(lastp) : PGNO_INVALID,
			    PGNO_INVALID, &tmp_dbt, &LSN(pagep),
			    lastp == NULL ? &null_lsn : &LSN(lastp),
			    &null_lsn)) != 0)
				return (ret);

			/* Move lsn onto page. */
			if (lastp)
				LSN(lastp) = new_lsn;
			LSN(pagep) = new_lsn;
		}

		P_INIT(pagep, dbp->pgsize,
		    PGNO(pagep), PGNO_INVALID, PGNO_INVALID, 0, P_OVERFLOW);
		OV_LEN(pagep) = pagespace;
		OV_REF(pagep) = 1;
		memcpy((u_int8_t *)pagep + P_OVERHEAD, p, pagespace);

		/*
		 * If this is the first entry, update the user's info.
		 * Otherwise, update the entry on the last page filled
		 * in and release that page.
		 */
		if (lastp == NULL)
			*pgnop = PGNO(pagep);
		else {
			lastp->next_pgno = PGNO(pagep);
			pagep->prev_pgno = PGNO(lastp);
			(void)memp_fput(dbp->mpf, lastp, DB_MPOOL_DIRTY);
		}
		lastp = pagep;
	}
	(void)memp_fput(dbp->mpf, lastp, DB_MPOOL_DIRTY);
	return (0);
}

/*
 * __db_ovref --
 *	Increment/decrement the reference count on an overflow page.
 *
 * PUBLIC: int __db_ovref __P((DB *, db_pgno_t, int32_t));
 */
int
__db_ovref(dbp, pgno, adjust)
	DB *dbp;
	db_pgno_t pgno;
	int32_t adjust;
{
	PAGE *h;
	int ret;

	if ((ret = memp_fget(dbp->mpf, &pgno, 0, &h)) != 0) {
		(void)__db_pgerr(dbp, pgno);
		return (ret);
	}

	if (DB_LOGGING(dbp))
		if ((ret = __db_ovref_log(dbp->dbenv->lg_info, dbp->txn,
		    &LSN(h), 0, dbp->log_fileid, h->pgno, adjust,
		    &LSN(h))) != 0)
			return (ret);
	OV_REF(h) += adjust;

	(void)memp_fput(dbp->mpf, h, DB_MPOOL_DIRTY);
	return (0);
}

/*
 * __db_doff --
 *	Delete an offpage chain of overflow pages.
 *
 * PUBLIC: int __db_doff __P((DB *, db_pgno_t, int (*)(DB *, PAGE *)));
 */
int
__db_doff(dbp, pgno, freefunc)
	DB *dbp;
	db_pgno_t pgno;
	int (*freefunc) __P((DB *, PAGE *));
{
	PAGE *pagep;
	DB_LSN null_lsn;
	DBT tmp_dbt;
	int ret;

	do {
		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &pagep)) != 0) {
			(void)__db_pgerr(dbp, pgno);
			return (ret);
		}

		/*
		 * If it's an overflow page and it's referenced by more than
		 * one key/data item, decrement the reference count and return.
		 */
		if (TYPE(pagep) == P_OVERFLOW && OV_REF(pagep) > 1) {
			(void)memp_fput(dbp->mpf, pagep, 0);
			return (__db_ovref(dbp, pgno, -1));
		}

		if (DB_LOGGING(dbp)) {
			tmp_dbt.data = (u_int8_t *)pagep + P_OVERHEAD;
			tmp_dbt.size = OV_LEN(pagep);
			ZERO_LSN(null_lsn);
			if ((ret = __db_big_log(dbp->dbenv->lg_info, dbp->txn,
			    &LSN(pagep), 0, DB_REM_BIG, dbp->log_fileid,
			    PGNO(pagep), PREV_PGNO(pagep), NEXT_PGNO(pagep),
			    &tmp_dbt, &LSN(pagep), &null_lsn, &null_lsn)) != 0)
				return (ret);
		}
		pgno = pagep->next_pgno;
		if ((ret = freefunc(dbp, pagep)) != 0)
			return (ret);
	} while (pgno != PGNO_INVALID);

	return (0);
}

/*
 * __db_moff --
 *	Match on overflow pages.
 *
 * Given a starting page number and a key, return <0, 0, >0 to indicate if the
 * key on the page is less than, equal to or greater than the key specified.
 *
 * PUBLIC: int __db_moff __P((DB *, const DBT *, db_pgno_t));
 */
int
__db_moff(dbp, dbt, pgno)
	DB *dbp;
	const DBT *dbt;
	db_pgno_t pgno;
{
	PAGE *pagep;
	u_int32_t cmp_bytes, key_left;
	u_int8_t *p1, *p2;
	int ret;

	/* While there are both keys to compare. */
	for (ret = 0, p1 = dbt->data,
	    key_left = dbt->size; key_left > 0 && pgno != PGNO_INVALID;) {
		if (memp_fget(dbp->mpf, &pgno, 0, &pagep) != 0) {
			(void)__db_pgerr(dbp, pgno);
			return (0);	/* No system error return. */
		}

		cmp_bytes = OV_LEN(pagep) < key_left ? OV_LEN(pagep) : key_left;
		key_left -= cmp_bytes;
		for (p2 =
		    (u_int8_t *)pagep + P_OVERHEAD; cmp_bytes-- > 0; ++p1, ++p2)
			if (*p1 != *p2) {
				ret = (long)*p1 - (long)*p2;
				break;
			}
		pgno = NEXT_PGNO(pagep);
		(void)memp_fput(dbp->mpf, pagep, 0);
		if (ret != 0)
			return (ret);
	}
	if (key_left > 0)		/* DBT is longer than page key. */
		return (-1);
	if (pgno != PGNO_INVALID)	/* DBT is shorter than page key. */
		return (1);
	return (0);
}
