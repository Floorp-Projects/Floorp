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
static const char sccsid[] = "@(#)bt_compare.c	10.9 (Sleepycat) 5/6/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"

/*
 * __bam_cmp --
 *	Compare a key to a given record.
 *
 * PUBLIC: int __bam_cmp __P((DB *, const DBT *, EPG *));
 */
int
__bam_cmp(dbp, k1, e)
	DB *dbp;
	const DBT *k1;
	EPG *e;
{
	BINTERNAL *bi;
	BKEYDATA *bk;
	BOVERFLOW *bo;
	BTREE *t;
	DBT k2;
	PAGE *h;

	t = dbp->internal;

	/*
	 * Returns:
	 *	< 0 if k1 is < record
	 *	= 0 if k1 is = record
	 *	> 0 if k1 is > record
	 *
	 * The left-most key on internal pages, at any level of the tree, is
	 * guaranteed, by the following code, to be less than any user key.
	 * This saves us from having to update the leftmost key on an internal
	 * page when the user inserts a new key in the tree smaller than
	 * anything we've yet seen.
	 */
	h = e->page;
	if (e->indx == 0 &&
	    h->prev_pgno == PGNO_INVALID && TYPE(h) != P_LBTREE)
		return (1);

	bo = NULL;
	if (TYPE(h) == P_LBTREE) {
		bk = GET_BKEYDATA(h, e->indx);
		if (B_TYPE(bk->type) == B_OVERFLOW)
			bo = (BOVERFLOW *)bk;
		else {
			k2.data = bk->data;
			k2.size = bk->len;
		}
	} else {
		bi = GET_BINTERNAL(h, e->indx);
		if (B_TYPE(bi->type) == B_OVERFLOW)
			bo = (BOVERFLOW *)(bi->data);
		else {
			k2.data = bi->data;
			k2.size = bi->len;
		}
	}

	/*
	 * XXX
	 * We ignore system errors; the only recoverable one is ENOMEM, and we
	 * don't want to require that comparison routines handle random errors.
	 * We don't want to return a valid comparison, either, so we stop.
	 */
	if (bo != NULL) {
		/*
		 * If using the default comparison routine, use __db_moff(),
		 * which compares the overflow key a page at a time.
		 */
		if (t->bt_compare == __bam_defcmp)
			return (__db_moff(dbp, k1, bo->pgno));

		/*
		 * Otherwise, we need a contiguous record so we can hand it
		 * to the user's routine.
		 */
		memset(&k2, 0, sizeof(k2));
		if (__db_goff(dbp, &k2, bo->tlen,
		    bo->pgno, &t->bt_rdata.data, &t->bt_rdata.ulen) != 0) {
			(void)__db_panic(dbp);
			return (0);
		}
	}

	/*
	 * XXX
	 * Note, we have not cleared the k2 DBT in this path.  This should
	 * be okay, because the user's comparison routine had better not be
	 * looking at any fields other than the data/size.  We don't clear
	 * it because we go through this path a lot and it's expensive.
	 */
	return ((*t->bt_compare)(k1, &k2));
}

/*
 * __bam_defcmp --
 *	Default comparison routine.
 *
 * PUBLIC: int __bam_defcmp __P((const DBT *, const DBT *));
 */
int
__bam_defcmp(a, b)
	const DBT *a, *b;
{
	size_t len;
	u_int8_t *p1, *p2;

	/*
	 * Returns:
	 *	< 0 if a is < b
	 *	= 0 if a is = b
	 *	> 0 if a is > b
	 *
	 * XXX
	 * If a size_t doesn't fit into a long, or if the difference between
	 * any two characters doesn't fit into an int, this routine can lose.
	 * What we need is a signed integral type that's guaranteed to be at
	 * least as large as a size_t, and there is no such thing.
	 */
	len = a->size > b->size ? b->size : a->size;
	for (p1 = a->data, p2 = b->data; len--; ++p1, ++p2)
		if (*p1 != *p2)
			return ((long)*p1 - (long)*p2);
	return ((long)a->size - (long)b->size);
}

/*
 * __bam_defpfx --
 *	Default prefix routine.
 *
 * PUBLIC: size_t __bam_defpfx __P((const DBT *, const DBT *));
 */
size_t
__bam_defpfx(a, b)
	const DBT *a, *b;
{
	size_t cnt, len;
	u_int8_t *p1, *p2;

	cnt = 1;
	len = a->size > b->size ? b->size : a->size;
	for (p1 = a->data, p2 = b->data; len--; ++p1, ++p2, ++cnt)
		if (*p1 != *p2)
			return (cnt);

	/*
	 * We know that a->size must be <= b->size, or they wouldn't be
	 * in this order.
	 */
	return (a->size < b->size ? a->size + 1 : a->size);
}
