/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1995
 *	The President and Fellows of Harvard University.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jeremy Rassen.
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
static const char sccsid[] = "@(#)hash_debug.c	10.6 (Sleepycat) 5/7/98";
#endif /* not lint */

#ifdef DEBUG
/*
 * PACKAGE:  hashing
 *
 * DESCRIPTION:
 *	Debug routines.
 *
 * ROUTINES:
 *
 * External
 *	__dump_bucket
 */
#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "hash.h"

/*
 * __ham_dump_bucket --
 *
 * PUBLIC: #ifdef DEBUG
 * PUBLIC: void __ham_dump_bucket __P((HTAB *, u_int32_t));
 * PUBLIC: #endif
 */
void
__ham_dump_bucket(hashp, bucket)
	HTAB *hashp;
	u_int32_t bucket;
{
	PAGE *p;
	db_pgno_t pgno;

	for (pgno = BUCKET_TO_PAGE(hashp, bucket); pgno != PGNO_INVALID;) {
		if (memp_fget(hashp->dbp->mpf, &pgno, 0, &p) != 0)
			break;
		(void)__db_prpage(p, 1);
		pgno = p->next_pgno;
		(void)memp_fput(hashp->dbp->mpf, p, 0);
	}
}
#endif /* DEBUG */
