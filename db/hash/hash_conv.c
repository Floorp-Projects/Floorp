/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)hash_conv.c	10.5 (Sleepycat) 4/10/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_swap.h"
#include "hash.h"

/*
 * __ham_pgin --
 *	Convert host-specific page layout from the host-independent format
 *	stored on disk.
 *
 * PUBLIC: int __ham_pgin __P((db_pgno_t, void *, DBT *));
 */
int
__ham_pgin(pg, pp, cookie)
	db_pgno_t pg;
	void *pp;
	DBT *cookie;
{
	DB_PGINFO *pginfo;
	u_int32_t tpgno;

	pginfo = (DB_PGINFO *)cookie->data;
	tpgno = PGNO((PAGE *)pp);
	if (pginfo->needswap)
		M_32_SWAP(tpgno);

	if (pg != PGNO_METADATA && pg != tpgno) {
		P_INIT(pp, pginfo->db_pagesize,
		    pg, PGNO_INVALID, PGNO_INVALID, 0, P_HASH);
		return (0);
	}

	if (!pginfo->needswap)
		return (0);
	return (pg == PGNO_METADATA ?
	    __ham_mswap(pp) : __db_pgin(pg, pginfo->db_pagesize, pp));
}

/*
 * __ham_pgout --
 *	Convert host-specific page layout to the host-independent format
 *	stored on disk.
 *
 * PUBLIC: int __ham_pgout __P((db_pgno_t, void *, DBT *));
 */
int
__ham_pgout(pg, pp, cookie)
	db_pgno_t pg;
	void *pp;
	DBT *cookie;
{
	DB_PGINFO *pginfo;

	pginfo = (DB_PGINFO *)cookie->data;
	if (!pginfo->needswap)
		return (0);
	return (pg == PGNO_METADATA ?
	    __ham_mswap(pp) : __db_pgout(pg, pginfo->db_pagesize, pp));
}

/*
 * __ham_mswap --
 *	Swap the bytes on the hash metadata page.
 *
 * PUBLIC: int __ham_mswap __P((void *));
 */
int
__ham_mswap(pg)
	void *pg;
{
	u_int8_t *p;
	int i;

	p = (u_int8_t *)pg;
	SWAP32(p);		/* lsn part 1 */
	SWAP32(p);		/* lsn part 2 */
	SWAP32(p);		/* pgno */
	SWAP32(p);		/* magic */
	SWAP32(p);		/* version */
	SWAP32(p);		/* pagesize */
	SWAP32(p);		/* ovfl_point */
	SWAP32(p);		/* last_freed */
	SWAP32(p);		/* max_bucket */
	SWAP32(p);		/* high_mask */
	SWAP32(p);		/* low_mask */
	SWAP32(p);		/* ffactor */
	SWAP32(p);		/* nelem */
	SWAP32(p);		/* h_charkey */
	SWAP32(p);		/* flags */
	for (i = 0; i < NCACHED; ++i)
		SWAP32(p);	/* spares */
	return (0);
}
