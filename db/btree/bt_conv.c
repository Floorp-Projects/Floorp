/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)bt_conv.c	10.6 (Sleepycat) 4/10/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_swap.h"
#include "btree.h"

/*
 * __bam_pgin --
 *	Convert host-specific page layout from the host-independent format
 *	stored on disk.
 *
 * PUBLIC: int __bam_pgin __P((db_pgno_t, void *, DBT *));
 */
int
__bam_pgin(pg, pp, cookie)
	db_pgno_t pg;
	void *pp;
	DBT *cookie;
{
	DB_PGINFO *pginfo;

	pginfo = (DB_PGINFO *)cookie->data;
	if (!pginfo->needswap)
		return (0);
	return (pg == PGNO_METADATA ?
	    __bam_mswap(pp) : __db_pgin(pg, pginfo->db_pagesize, pp));
}

/*
 * __bam_pgout --
 *	Convert host-specific page layout to the host-independent format
 *	stored on disk.
 *
 * PUBLIC: int __bam_pgout __P((db_pgno_t, void *, DBT *));
 */
int
__bam_pgout(pg, pp, cookie)
	db_pgno_t pg;
	void *pp;
	DBT *cookie;
{
	DB_PGINFO *pginfo;

	pginfo = (DB_PGINFO *)cookie->data;
	if (!pginfo->needswap)
		return (0);
	return (pg == PGNO_METADATA ?
	    __bam_mswap(pp) : __db_pgout(pg, pginfo->db_pagesize, pp));
}

/*
 * __bam_mswap --
 *	Swap the bytes on the btree metadata page.
 *
 * PUBLIC: int __bam_mswap __P((PAGE *));
 */
int
__bam_mswap(pg)
	PAGE *pg;
{
	u_int8_t *p;

	p = (u_int8_t *)pg;

	/* Swap the meta-data information. */
	SWAP32(p);		/* lsn.file */
	SWAP32(p);		/* lsn.offset */
	SWAP32(p);		/* pgno */
	SWAP32(p);		/* magic */
	SWAP32(p);		/* version */
	SWAP32(p);		/* pagesize */
	SWAP32(p);		/* maxkey */
	SWAP32(p);		/* minkey */
	SWAP32(p);		/* free */
	SWAP32(p);		/* flags */

	/* Swap the statistics. */
	p = (u_int8_t *)&((BTMETA *)pg)->stat;
	SWAP32(p);		/* bt_freed */
	SWAP32(p);		/* bt_pfxsaved */
	SWAP32(p);		/* bt_split */
	SWAP32(p);		/* bt_rootsplit */
	SWAP32(p);		/* bt_fastsplit */
	SWAP32(p);		/* bt_added */
	SWAP32(p);		/* bt_deleted */
	SWAP32(p);		/* bt_get */
	SWAP32(p);		/* bt_cache_hit */
	SWAP32(p);		/* bt_cache_miss */

	return (0);
}
