/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
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
static const char sccsid[] = "@(#)hash_dup.c	10.14 (Sleepycat) 5/7/98";
#endif /* not lint */

/*
 * PACKAGE:  hashing
 *
 * DESCRIPTION:
 *      Manipulation of duplicates for the hash package.
 *
 * ROUTINES:
 *
 * External
 *      __add_dup
 * Internal
 */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "hash.h"

static int __ham_check_move __P((HTAB *, HASH_CURSOR *, int32_t));
static int __ham_dup_convert __P((HTAB *, HASH_CURSOR *));
static int __ham_make_dup __P((const DBT *, DBT *d, void **, u_int32_t *));

/*
 * Called from hash_access to add a duplicate key. nval is the new
 * value that we want to add.  The flags correspond to the flag values
 * to cursor_put indicating where to add the new element.
 * There are 4 cases.
 * Case 1: The existing duplicate set already resides on a separate page.
 *	   We can use common code for this.
 * Case 2: The element is small enough to just be added to the existing set.
 * Case 3: The element is large enough to be a big item, so we're going to
 *	   have to push the set onto a new page.
 * Case 4: The element is large enough to push the duplicate set onto a
 *	   separate page.
 *
 * PUBLIC: int __ham_add_dup __P((HTAB *, HASH_CURSOR *, DBT *, u_int32_t));
 */
int
__ham_add_dup(hashp, hcp, nval, flags)
	HTAB *hashp;
	HASH_CURSOR *hcp;
	DBT *nval;
	u_int32_t flags;
{
	DBT pval, tmp_val;
	u_int32_t del_len, new_size;
	int ret;
	u_int8_t *hk;

	if (flags == DB_CURRENT && hcp->dpgno == PGNO_INVALID)
		del_len = hcp->dup_len;
	else
		del_len = 0;

	if ((ret = __ham_check_move(hashp, hcp,
	    (int32_t)DUP_SIZE(nval->size) - (int32_t)del_len)) != 0)
		return (ret);

	/*
	 * Check if resulting duplicate set is going to need to go
	 * onto a separate duplicate page.  If so, convert the
	 * duplicate set and add the new one.  After conversion,
	 * hcp->dndx is the first free ndx or the index of the
	 * current pointer into the duplicate set.
	 */
	hk = H_PAIRDATA(hcp->pagep, hcp->bndx);
	new_size = DUP_SIZE(nval->size) - del_len + LEN_HKEYDATA(hcp->pagep,
	    hashp->hdr->pagesize, H_DATAINDEX(hcp->bndx));

	/*
	 * We convert to off-page duplicates if the item is a big item,
	 * the addition of the new item will make the set large, or
	 * if there isn't enough room on this page to add the next item.
	 */
	if (HPAGE_PTYPE(hk) != H_OFFDUP &&
	    (HPAGE_PTYPE(hk) == H_OFFPAGE || ISBIG(hashp, new_size) ||
	    DUP_SIZE(nval->size) - del_len > P_FREESPACE(hcp->pagep))) {

		if ((ret = __ham_dup_convert(hashp, hcp)) != 0)
			return (ret);
		else
			hk = H_PAIRDATA(hcp->pagep, hcp->bndx);
	}

	/* There are two separate cases here: on page and off page. */
	if (HPAGE_PTYPE(hk) != H_OFFDUP) {
		if (HPAGE_PTYPE(hk) != H_DUPLICATE) {
			HPAGE_PTYPE(hk) = H_DUPLICATE;
			pval.flags = 0;
			pval.data = HKEYDATA_DATA(hk);
			pval.size = LEN_HDATA(hcp->pagep, hashp->hdr->pagesize,
			    hcp->bndx);
			if ((ret =
			    __ham_make_dup(&pval, &tmp_val, &hcp->big_data,
			    &hcp->big_datalen)) != 0 || (ret =
			    __ham_replpair(hashp, hcp, &tmp_val, 1)) != 0)
				return (ret);
		}

		/* Now make the new entry a duplicate. */
		if ((ret = __ham_make_dup(nval,
		    &tmp_val, &hcp->big_data, &hcp->big_datalen)) != 0)
			return (ret);

		tmp_val.dlen = 0;
		switch (flags) {			/* On page. */
		case DB_KEYFIRST:
			tmp_val.doff = 0;
			break;
		case DB_KEYLAST:
			tmp_val.doff = LEN_HDATA(hcp->pagep,
			    hashp->hdr->pagesize, hcp->bndx);
			break;
		case DB_CURRENT:
			tmp_val.doff = hcp->dup_off;
			tmp_val.dlen = DUP_SIZE(hcp->dup_len);
			break;
		case DB_BEFORE:
			tmp_val.doff = hcp->dup_off;
			break;
		case DB_AFTER:
			tmp_val.doff = hcp->dup_off + DUP_SIZE(hcp->dup_len);
			break;
		}
		/* Add the duplicate. */
		ret = __ham_replpair(hashp, hcp, &tmp_val, 0);
		if (ret == 0)
			ret = __ham_dirty_page(hashp, hcp->pagep);
		__ham_c_update(hcp, hcp->pgno, tmp_val.size, 1, 1);
		return (ret);
	}

	/* If we get here, then we're on duplicate pages. */
	if (hcp->dpgno == PGNO_INVALID) {
		memcpy(&hcp->dpgno, HOFFDUP_PGNO(hk), sizeof(db_pgno_t));
		hcp->dndx = 0;
	}

	switch (flags) {
	case DB_KEYFIRST:
		/*
		 * The only way that we are already on a dup page is
		 * if we just converted the on-page representation.
		 * In that case, we've only got one page of duplicates.
		 */
		if (hcp->dpagep == NULL && (ret =
		    __db_dend(hashp->dbp, hcp->dpgno, &hcp->dpagep)) != 0)
			return (ret);
		hcp->dndx = 0;
		break;
	case DB_KEYLAST:
		if (hcp->dpagep == NULL && (ret =
		    __db_dend(hashp->dbp, hcp->dpgno, &hcp->dpagep)) != 0)
			return (ret);
		hcp->dpgno = PGNO(hcp->dpagep);
		hcp->dndx = NUM_ENT(hcp->dpagep);
		break;
	case DB_CURRENT:
		if ((ret = __db_ditem(hashp->dbp, hcp->dpagep, hcp->dndx,
		    BKEYDATA_SIZE(GET_BKEYDATA(hcp->dpagep, hcp->dndx)->len)))
		    != 0)
			return (ret);
		break;
	case DB_BEFORE:	/* The default behavior is correct. */
		break;
	case DB_AFTER:
		hcp->dndx++;
		break;
	}

	ret = __db_dput(hashp->dbp,
	    nval, &hcp->dpagep, &hcp->dndx, __ham_overflow_page);
	hcp->pgno = PGNO(hcp->pagep);
	__ham_c_update(hcp, hcp->pgno, nval->size, 1, 1);
	return (ret);
}

/*
 * Convert an on-page set of duplicates to an offpage set of duplicates.
 */
static int
__ham_dup_convert(hashp, hcp)
	HTAB *hashp;
	HASH_CURSOR *hcp;
{
	BOVERFLOW bo;
	DBT dbt;
	HOFFPAGE ho;
	db_indx_t dndx, len;
	int ret;
	u_int8_t *p, *pend;

	/*
	 * Create a new page for the duplicates.
	 */
	if ((ret =
	    __ham_overflow_page(hashp->dbp, P_DUPLICATE, &hcp->dpagep)) != 0)
		return (ret);
	hcp->dpagep->type = P_DUPLICATE;
	hcp->dpgno = PGNO(hcp->dpagep);

	/*
	 * Now put the duplicates onto the new page.
	 */
	dbt.flags = 0;
	switch (HPAGE_PTYPE(H_PAIRDATA(hcp->pagep, hcp->bndx))) {
	case H_KEYDATA:
		/* Simple case, one key on page; move it to dup page. */
		dndx = 0;
		dbt.size =
		    LEN_HDATA(hcp->pagep, hashp->hdr->pagesize, hcp->bndx);
		dbt.data = HKEYDATA_DATA(H_PAIRDATA(hcp->pagep, hcp->bndx));
		ret = __db_pitem(hashp->dbp, hcp->dpagep,
		    (u_int32_t)dndx, BKEYDATA_SIZE(dbt.size), NULL, &dbt);
		if (ret == 0)
			__ham_dirty_page(hashp, hcp->dpagep);
		break;
	case H_OFFPAGE:
		/* Simple case, one key on page; move it to dup page. */
		dndx = 0;
		memcpy(&ho,
		    P_ENTRY(hcp->pagep, H_DATAINDEX(hcp->bndx)), HOFFPAGE_SIZE);
		B_TSET(bo.type, ho.type, 0);
		bo.pgno = ho.pgno;
		bo.tlen = ho.tlen;
		dbt.size = BOVERFLOW_SIZE;
		dbt.data = &bo;

		ret = __db_pitem(hashp->dbp, hcp->dpagep,
		   (u_int32_t)dndx, dbt.size, &dbt, NULL);
		if (ret == 0)
			__ham_dirty_page(hashp, hcp->dpagep);
		break;
	case H_DUPLICATE:
		p = HKEYDATA_DATA(H_PAIRDATA(hcp->pagep, hcp->bndx));
		pend = p +
		    LEN_HDATA(hcp->pagep, hashp->hdr->pagesize, hcp->bndx);

		for (dndx = 0; p < pend; dndx++) {
			memcpy(&len, p, sizeof(db_indx_t));
			dbt.size = len;
			p += sizeof(db_indx_t);
			dbt.data = p;
			p += len + sizeof(db_indx_t);
			ret = __db_dput(hashp->dbp, &dbt,
			    &hcp->dpagep, &dndx, __ham_overflow_page);
			if (ret != 0)
				break;
		}
		break;
	default:
		ret = __db_pgfmt(hashp->dbp, (u_long)hcp->pgno);
	}
	if (ret == 0) {
		/*
		 * Now attach this to the source page in place of
		 * the old duplicate item.
		 */
		__ham_move_offpage(hashp, hcp->pagep,
		    (u_int32_t)H_DATAINDEX(hcp->bndx), hcp->dpgno);

		/* Can probably just do a "put" here. */
		ret = __ham_dirty_page(hashp, hcp->pagep);
	} else {
		(void)__ham_del_page(hashp->dbp, hcp->dpagep);
		hcp->dpagep = NULL;
	}
	return (ret);
}

static int
__ham_make_dup(notdup, duplicate, bufp, sizep)
	const DBT *notdup;
	DBT *duplicate;
	void **bufp;
	u_int32_t *sizep;
{
	db_indx_t tsize, item_size;
	int ret;
	u_int8_t *p;

	item_size = (db_indx_t)notdup->size;
	tsize = DUP_SIZE(item_size);
	if ((ret = __ham_init_dbt(duplicate, tsize, bufp, sizep)) != 0)
		return (ret);

	duplicate->dlen = 0;
	duplicate->flags = notdup->flags;
	F_SET(duplicate, DB_DBT_PARTIAL);

	p = duplicate->data;
	memcpy(p, &item_size, sizeof(db_indx_t));
	p += sizeof(db_indx_t);
	memcpy(p, notdup->data, notdup->size);
	p += notdup->size;
	memcpy(p, &item_size, sizeof(db_indx_t));

	duplicate->doff = 0;
	duplicate->dlen = notdup->size;

	return (0);
}

static int
__ham_check_move(hashp, hcp, add_len)
	HTAB *hashp;
	HASH_CURSOR *hcp;
	int32_t add_len;
{
	DBT k, d;
	DB_LSN new_lsn;
	PAGE *next_pagep;
	db_pgno_t next_pgno;
	u_int32_t new_datalen, old_len, rectype;
	u_int8_t *hk;
	int ret;

	/*
	 * Check if we can do whatever we need to on this page.  If not,
	 * then we'll have to move the current element to a new page.
	 */
	hk = H_PAIRDATA(hcp->pagep, hcp->bndx);

	/*
	 * If the item is already off page duplicates or an offpage item,
	 * then we know we can do whatever we need to do in-place
	 */
	if (HPAGE_PTYPE(hk) == H_OFFDUP || HPAGE_PTYPE(hk) == H_OFFPAGE)
		return (0);

	old_len =
	    LEN_HITEM(hcp->pagep, hashp->hdr->pagesize, H_DATAINDEX(hcp->bndx));
	new_datalen = old_len - HKEYDATA_SIZE(0) + add_len;

	/*
	 * We need to add a new page under two conditions:
	 * 1. The addition makes the total data length cross the BIG
	 *    threshold and the OFFDUP structure won't fit on this page.
	 * 2. The addition does not make the total data cross the
	 *    threshold, but the new data won't fit on the page.
	 * If neither of these is true, then we can return.
	 */
	if (ISBIG(hashp, new_datalen) && (old_len > HOFFDUP_SIZE ||
	    HOFFDUP_SIZE - old_len <= P_FREESPACE(hcp->pagep)))
		return (0);

	if (!ISBIG(hashp, new_datalen) &&
	    add_len <= (int32_t)P_FREESPACE(hcp->pagep))
		return (0);

	/*
	 * If we get here, then we need to move the item to a new page.
	 * Check if there are more pages in the chain.
	 */

	new_datalen = ISBIG(hashp, new_datalen) ?
	    HOFFDUP_SIZE : HKEYDATA_SIZE(new_datalen);

	next_pagep = NULL;
	for (next_pgno = NEXT_PGNO(hcp->pagep); next_pgno != PGNO_INVALID;
	    next_pgno = NEXT_PGNO(next_pagep)) {
		if (next_pagep != NULL &&
		    (ret = __ham_put_page(hashp->dbp, next_pagep, 0)) != 0)
			return (ret);

		if ((ret =
		    __ham_get_page(hashp->dbp, next_pgno, &next_pagep)) != 0)
			return (ret);

		if (P_FREESPACE(next_pagep) >= new_datalen)
			break;
	}

	/* No more pages, add one. */
	if (next_pagep == NULL &&
	    (ret = __ham_add_ovflpage(hashp, hcp->pagep, 0, &next_pagep)) != 0)
		return (ret);

	/* Add new page at the end of the chain. */
	if (P_FREESPACE(next_pagep) < new_datalen &&
	    (ret = __ham_add_ovflpage(hashp, next_pagep, 1, &next_pagep)) != 0)
		return (ret);

	/* Copy the item to the new page. */
	if (DB_LOGGING(hashp->dbp)) {
		rectype = PUTPAIR;
		k.flags = 0;
		d.flags = 0;
		if (HPAGE_PTYPE(
		    H_PAIRKEY(hcp->pagep, hcp->bndx)) == H_OFFPAGE) {
			rectype |= PAIR_KEYMASK;
			k.data = H_PAIRKEY(hcp->pagep, hcp->bndx);
			k.size = HOFFPAGE_SIZE;
		} else {
			k.data =
			    HKEYDATA_DATA(H_PAIRKEY(hcp->pagep, hcp->bndx));
			k.size = LEN_HKEY(hcp->pagep,
			    hashp->hdr->pagesize, hcp->bndx);
		}

		if (HPAGE_PTYPE(hk) == H_OFFPAGE) {
			rectype |= PAIR_DATAMASK;
			d.data = H_PAIRDATA(hcp->pagep, hcp->bndx);
			d.size = HOFFPAGE_SIZE;
		} else {
			d.data =
			    HKEYDATA_DATA(H_PAIRDATA(hcp->pagep, hcp->bndx));
			d.size = LEN_HDATA(hcp->pagep,
			    hashp->hdr->pagesize, hcp->bndx);
		}


		if ((ret = __ham_insdel_log(hashp->dbp->dbenv->lg_info,
		    (DB_TXN *)hashp->dbp->txn, &new_lsn, 0, rectype,
		    hashp->dbp->log_fileid, PGNO(next_pagep),
		    (u_int32_t)H_NUMPAIRS(next_pagep), &LSN(next_pagep),
		    &k, &d)) != 0)
			return (ret);

		/* Move lsn onto page. */
		LSN(next_pagep) = new_lsn;	/* Structure assignment. */
	}

	__ham_copy_item(hashp, hcp->pagep, H_KEYINDEX(hcp->bndx), next_pagep);
	__ham_copy_item(hashp, hcp->pagep, H_DATAINDEX(hcp->bndx), next_pagep);

	/* Now delete the pair from the current page. */
	ret = __ham_del_pair(hashp, hcp, 0);

	(void)__ham_put_page(hashp->dbp, hcp->pagep, 1);
	hcp->pagep = next_pagep;
	hcp->pgno = PGNO(hcp->pagep);
	hcp->bndx = H_NUMPAIRS(hcp->pagep) - 1;
	F_SET(hcp, H_EXPAND);
	return (ret);
}

/*
 * Replace an onpage set of duplicates with the OFFDUP structure that
 * references the duplicate page.
 * XXX This is really just a special case of __onpage_replace; we should
 * probably combine them.
 * PUBLIC: void __ham_move_offpage __P((HTAB *, PAGE *, u_int32_t, db_pgno_t));
 */
void
__ham_move_offpage(hashp, pagep, ndx, pgno)
	HTAB *hashp;
	PAGE *pagep;
	u_int32_t ndx;
	db_pgno_t pgno;
{
	DBT new_dbt;
	DBT old_dbt;
	HOFFDUP od;
	db_indx_t i;
	int32_t shrink;
	u_int8_t *src;

	od.type = H_OFFDUP;
	od.pgno = pgno;

	if (DB_LOGGING(hashp->dbp)) {
		new_dbt.data = &od;
		new_dbt.size = HOFFDUP_SIZE;
		old_dbt.data = P_ENTRY(pagep, ndx);
		old_dbt.size = LEN_HITEM(pagep, hashp->hdr->pagesize, ndx);
		(void)__ham_replace_log(hashp->dbp->dbenv->lg_info,
		    (DB_TXN *)hashp->dbp->txn, &LSN(pagep), 0,
		    hashp->dbp->log_fileid, PGNO(pagep), (u_int32_t)ndx,
		    &LSN(pagep), -1, &old_dbt, &new_dbt, 0);
	}

	shrink =
	    LEN_HITEM(pagep, hashp->hdr->pagesize, ndx) - HOFFDUP_SIZE;

	if (shrink != 0) {
		/* Copy data. */
		src = (u_int8_t *)(pagep) + HOFFSET(pagep);
		memmove(src + shrink, src, pagep->inp[ndx] - HOFFSET(pagep));
		HOFFSET(pagep) += shrink;

		/* Update index table. */
		for (i = ndx; i < NUM_ENT(pagep); i++)
			pagep->inp[i] += shrink;
	}

	/* Now copy the offdup entry onto the page. */
	memcpy(P_ENTRY(pagep, ndx), &od, HOFFDUP_SIZE);
}
