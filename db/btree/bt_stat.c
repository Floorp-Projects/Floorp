/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)bt_stat.c	10.17 (Sleepycat) 4/26/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"

static void __bam_add_rstat __P((DB_BTREE_LSTAT *, DB_BTREE_STAT *));

/*
 * __bam_stat --
 *	Gather/print the btree statistics
 *
 * PUBLIC: int __bam_stat __P((DB *, void *, void *(*)(size_t), u_int32_t));
 */
int
__bam_stat(argdbp, spp, db_malloc, flags)
	DB *argdbp;
	void *spp;
	void *(*db_malloc) __P((size_t));
	u_int32_t flags;
{
	BTMETA *meta;
	BTREE *t;
	DB *dbp;
	DB_BTREE_STAT *sp;
	DB_LOCK lock;
	PAGE *h;
	db_pgno_t lastpgno, pgno;
	int ret;

	DEBUG_LWRITE(argdbp, NULL, "bam_stat", NULL, NULL, flags);

	/* Check for invalid flags. */
	if ((ret = __db_statchk(argdbp, flags)) != 0)
		return (ret);

	if (spp == NULL)
		return (0);

	GETHANDLE(argdbp, NULL, &dbp, ret);
	t = dbp->internal;

	/* Allocate and clear the structure. */
	if ((sp = db_malloc == NULL ?
	    (DB_BTREE_STAT *)__db_malloc(sizeof(*sp)) :
	    (DB_BTREE_STAT *)db_malloc(sizeof(*sp))) == NULL) {
		ret = ENOMEM;
		goto err;
	}
	memset(sp, 0, sizeof(*sp));

	/* If the app just wants the record count, make it fast. */
	if (LF_ISSET(DB_RECORDCOUNT)) {
		pgno = PGNO_ROOT;
		if ((ret = __bam_lget(dbp, 0, pgno, DB_LOCK_READ, &lock)) != 0)
			goto err;
		if ((ret = __bam_pget(dbp, (PAGE **)&h, &pgno, 0)) != 0)
			goto err;

		sp->bt_nrecs = RE_NREC(h);

		(void)memp_fput(dbp->mpf, h, 0);
		(void)__BT_LPUT(dbp, lock);
		goto done;
	}

	/* Get the meta-data page. */
	pgno = PGNO_METADATA;
	if ((ret = __bam_lget(dbp, 0, pgno, DB_LOCK_READ, &lock)) != 0)
		goto err;
	if ((ret = __bam_pget(dbp, (PAGE **)&meta, &pgno, 0)) != 0)
		goto err;

	/* Translate the metadata flags. */
	if (F_ISSET(meta, BTM_DUP))
		sp->bt_flags |= DB_DUP;
	if (F_ISSET(meta, BTM_FIXEDLEN))
		sp->bt_flags |= DB_FIXEDLEN;
	if (F_ISSET(meta, BTM_RECNUM))
		sp->bt_flags |= DB_RECNUM;
	if (F_ISSET(meta, BTM_RENUMBER))
		sp->bt_flags |= DB_RENUMBER;

	/* Get the remaining metadata fields. */
	sp->bt_minkey = meta->minkey;
	sp->bt_maxkey = meta->maxkey;
	sp->bt_re_len = meta->re_len;
	sp->bt_re_pad = meta->re_pad;
	sp->bt_magic = meta->magic;
	sp->bt_version = meta->version;

	/* Get the page size from the DB. */
	sp->bt_pagesize = dbp->pgsize;

	/* Initialize counters with the meta-data page information. */
	__bam_add_rstat(&meta->stat, sp);

	/*
	 * Add in the local information from this handle.
	 *
	 * !!!
	 * This is a bit odd, but it gets us closer to the truth.
	 */
	__bam_add_rstat(&t->lstat, sp);

	/* Walk the free list, counting pages. */
	for (sp->bt_free = 0, pgno = meta->free; pgno != PGNO_INVALID;) {
		++sp->bt_free;

		if ((ret = __bam_pget(dbp, &h, &pgno, 0)) != 0) {
			(void)memp_fput(dbp->mpf, meta, 0);
			(void)__BT_TLPUT(dbp, lock);
			goto err;
		}
		pgno = h->next_pgno;
		(void)memp_fput(dbp->mpf, h, 0);
	}

	/* Discard the meta-data page. */
	(void)memp_fput(dbp->mpf, meta, 0);
	(void)__BT_TLPUT(dbp, lock);

	/* Determine the last page of the database. */
	if ((ret = memp_fget(dbp->mpf, &lastpgno, DB_MPOOL_LAST, &h)) != 0)
		goto err;
	(void)memp_fput(dbp->mpf, h, 0);

	/* Get the root page. */
	pgno = PGNO_ROOT;
	if ((ret = __bam_lget(dbp, 0, PGNO_ROOT, DB_LOCK_READ, &lock)) != 0)
		goto err;
	if ((ret = __bam_pget(dbp, &h, &pgno, 0)) != 0) {
		(void)__BT_LPUT(dbp, lock);
		goto err;
	}

	/* Get the levels from the root page. */
	sp->bt_levels = h->level;

	/* Walk the page list, counting things. */
	for (;;) {
		switch (TYPE(h)) {
		case P_INVALID:
			break;
		case P_IBTREE:
		case P_IRECNO:
			++sp->bt_int_pg;
			sp->bt_int_pgfree += HOFFSET(h) - LOFFSET(h);
			break;
		case P_LBTREE:
			++sp->bt_leaf_pg;
			sp->bt_leaf_pgfree += HOFFSET(h) - LOFFSET(h);
			sp->bt_nrecs += NUM_ENT(h) / P_INDX;
			break;
		case P_LRECNO:
			++sp->bt_leaf_pg;
			sp->bt_leaf_pgfree += HOFFSET(h) - LOFFSET(h);
			sp->bt_nrecs += NUM_ENT(h);
			break;
		case P_DUPLICATE:
			++sp->bt_dup_pg;
			/* XXX MARGO: sp->bt_dup_pgfree; */
			break;
		case P_OVERFLOW:
			++sp->bt_over_pg;
			/* XXX MARGO: sp->bt_over_pgfree; */
			break;
		default:
			(void)memp_fput(dbp->mpf, h, 0);
			(void)__BT_LPUT(dbp, lock);
			return (__db_pgfmt(dbp, pgno));
		}

		(void)memp_fput(dbp->mpf, h, 0);
		(void)__BT_LPUT(dbp, lock);

		if (++pgno > lastpgno)
			break;
		if (__bam_lget(dbp, 0, pgno, DB_LOCK_READ, &lock))
			break;
		if (memp_fget(dbp->mpf, &pgno, 0, &h) != 0) {
			(void)__BT_LPUT(dbp, lock);
			break;
		}
	}

done:	*(DB_BTREE_STAT **)spp = sp;
	ret = 0;

err:	PUTHANDLE(dbp);
	return (ret);
}

/*
 * __bam_add_mstat --
 *	Add the local statistics to the meta-data page statistics.
 *
 * PUBLIC: void __bam_add_mstat __P((DB_BTREE_LSTAT *, DB_BTREE_LSTAT *));
 */
void
__bam_add_mstat(from, to)
	DB_BTREE_LSTAT *from;
	DB_BTREE_LSTAT *to;
{
	to->bt_freed += from->bt_freed;
	to->bt_pfxsaved += from->bt_pfxsaved;
	to->bt_split += from->bt_split;
	to->bt_rootsplit += from->bt_rootsplit;
	to->bt_fastsplit += from->bt_fastsplit;
	to->bt_added += from->bt_added;
	to->bt_deleted += from->bt_deleted;
	to->bt_get += from->bt_get;
	to->bt_cache_hit += from->bt_cache_hit;
	to->bt_cache_miss += from->bt_cache_miss;
}

/*
 * __bam_add_rstat --
 *	Add the local statistics to the returned statistics.
 */
static void
__bam_add_rstat(from, to)
	DB_BTREE_LSTAT *from;
	DB_BTREE_STAT *to;
{
	to->bt_freed += from->bt_freed;
	to->bt_pfxsaved += from->bt_pfxsaved;
	to->bt_split += from->bt_split;
	to->bt_rootsplit += from->bt_rootsplit;
	to->bt_fastsplit += from->bt_fastsplit;
	to->bt_added += from->bt_added;
	to->bt_deleted += from->bt_deleted;
	to->bt_get += from->bt_get;
	to->bt_cache_hit += from->bt_cache_hit;
	to->bt_cache_miss += from->bt_cache_miss;
}
