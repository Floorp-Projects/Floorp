/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)mp_region.c	10.30 (Sleepycat) 5/31/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_shash.h"
#include "mp.h"
#include "common_ext.h"

/*
 * __memp_ralloc --
 *	Allocate some space in the mpool region.
 *
 * PUBLIC: int __memp_ralloc __P((DB_MPOOL *, size_t, size_t *, void *));
 */
int
__memp_ralloc(dbmp, len, offsetp, retp)
	DB_MPOOL *dbmp;
	size_t len, *offsetp;
	void *retp;
{
	BH *bhp, *nbhp;
	MPOOL *mp;
	MPOOLFILE *mfp;
	size_t fsize, total;
	int nomore, restart, ret, wrote;
	void *p;

	mp = dbmp->mp;

	nomore = 0;
alloc:	if ((ret = __db_shalloc(dbmp->addr, len, MUTEX_ALIGNMENT, &p)) == 0) {
		if (offsetp != NULL)
			*offsetp = R_OFFSET(dbmp, p);
		*(void **)retp = p;
		return (0);
	}
	if (nomore) {
		__db_err(dbmp->dbenv, "%s", strerror(ret));
		return (ret);
	}

	/* Look for a buffer on the free list that's the right size. */
	for (bhp =
	    SH_TAILQ_FIRST(&mp->bhfq, __bh); bhp != NULL; bhp = nbhp) {
		nbhp = SH_TAILQ_NEXT(bhp, q, __bh);

		if (__db_shsizeof(bhp) == len) {
			SH_TAILQ_REMOVE(&mp->bhfq, bhp, q, __bh);
			if (offsetp != NULL)
				*offsetp = R_OFFSET(dbmp, bhp);
			*(void **)retp = bhp;
			return (0);
		}
	}

	/* Discard from the free list until we've freed enough memory. */
	total = 0;
	for (bhp =
	    SH_TAILQ_FIRST(&mp->bhfq, __bh); bhp != NULL; bhp = nbhp) {
		nbhp = SH_TAILQ_NEXT(bhp, q, __bh);

		SH_TAILQ_REMOVE(&mp->bhfq, bhp, q, __bh);
		__db_shalloc_free(dbmp->addr, bhp);
		--mp->stat.st_page_clean;

		/*
		 * Retry as soon as we've freed up sufficient space.  If we
		 * will have to coalesce memory to satisfy the request, don't
		 * try until it's likely (possible?) that we'll succeed.
		 */
		total += fsize = __db_shsizeof(bhp);
		if (fsize >= len || total >= 3 * len)
			goto alloc;
	}

retry:	/* Find a buffer we can flush; pure LRU. */
	total = 0;
	for (bhp =
	    SH_TAILQ_FIRST(&mp->bhq, __bh); bhp != NULL; bhp = nbhp) {
		nbhp = SH_TAILQ_NEXT(bhp, q, __bh);

		/* Ignore pinned or locked (I/O in progress) buffers. */
		if (bhp->ref != 0 || F_ISSET(bhp, BH_LOCKED))
			continue;

		/* Find the associated MPOOLFILE. */
		mfp = R_ADDR(dbmp, bhp->mf_offset);

		/*
		 * Write the page if it's dirty.
		 *
		 * If we wrote the page, fall through and free the buffer.  We
		 * don't have to rewalk the list to acquire the buffer because
		 * it was never available for any other process to modify it.
		 * If we didn't write the page, but we discarded and reacquired
		 * the region lock, restart the buffer list walk.  If we neither
		 * wrote the buffer nor discarded the region lock, continue down
		 * the buffer list.
		 */
		if (F_ISSET(bhp, BH_DIRTY)) {
			if ((ret = __memp_bhwrite(dbmp,
			    mfp, bhp, &restart, &wrote)) != 0)
				return (ret);

			/*
			 * It's possible that another process wants this buffer
			 * and incremented the ref count while we were writing
			 * it.
			 */
			if (bhp->ref != 0)
				goto retry;

			if (wrote)
				++mp->stat.st_rw_evict;
			else {
				if (restart)
					goto retry;
				continue;
			}
		} else
			++mp->stat.st_ro_evict;

		/*
		 * Check to see if the buffer is the size we're looking for.
		 * If it is, simply reuse it.
		 */
		total += fsize = __db_shsizeof(bhp);
		if (fsize == len) {
			__memp_bhfree(dbmp, mfp, bhp, 0);

			if (offsetp != NULL)
				*offsetp = R_OFFSET(dbmp, bhp);
			*(void **)retp = bhp;
			return (0);
		}

		/* Free the buffer. */
		__memp_bhfree(dbmp, mfp, bhp, 1);

		/*
		 * Retry as soon as we've freed up sufficient space.  If we
		 * have to coalesce of memory to satisfy the request, don't
		 * try until it's likely (possible?) that we'll succeed.
		 */
		if (fsize >= len || total >= 3 * len)
			goto alloc;

		/* Restart the walk if we discarded the region lock. */
		if (restart)
			goto retry;
	}
	nomore = 1;
	goto alloc;
}

/*
 * __memp_ropen --
 *	Attach to, and optionally create, the mpool region.
 *
 * PUBLIC: int __memp_ropen
 * PUBLIC:    __P((DB_MPOOL *, const char *, size_t, int, int, u_int32_t));
 */
int
__memp_ropen(dbmp, path, cachesize, mode, is_private, flags)
	DB_MPOOL *dbmp;
	const char *path;
	size_t cachesize;
	int mode, is_private;
	u_int32_t flags;
{
	MPOOL *mp;
	size_t rlen;
	int defcache, ret;

	/*
	 * Unlike other DB subsystems, mpool can't simply grow the region
	 * because it returns pointers into the region to its clients.  To
	 * "grow" the region, we'd have to allocate a new region and then
	 * store a region number in the structures that reference regional
	 * objects.  It's reasonable that we fail regardless, as clients
	 * shouldn't have every page in the region pinned, so the only
	 * "failure" mode should be a performance penalty because we don't
	 * find a page in the cache that we'd like to have found.
	 *
	 * Up the user's cachesize by 25% to account for our overhead.
	 */
	defcache = 0;
	if (cachesize < DB_CACHESIZE_MIN)
		if (cachesize == 0) {
			defcache = 1;
			cachesize = DB_CACHESIZE_DEF;
		} else
			cachesize = DB_CACHESIZE_MIN;
	rlen = cachesize + cachesize / 4;

	/*
	 * Map in the region.
	 *
	 * If it's a private mpool, use malloc, it's a lot faster than
	 * instantiating a region.
	 */
	dbmp->reginfo.dbenv = dbmp->dbenv;
	dbmp->reginfo.appname = DB_APP_NONE;
	if (path == NULL)
		dbmp->reginfo.path = NULL;
	else
		if ((dbmp->reginfo.path = __db_strdup(path)) == NULL)
			return (ENOMEM);
	dbmp->reginfo.file = DB_DEFAULT_MPOOL_FILE;
	dbmp->reginfo.mode = mode;
	dbmp->reginfo.size = rlen;
	dbmp->reginfo.dbflags = flags;
	dbmp->reginfo.flags = 0;
	if (defcache)
		F_SET(&dbmp->reginfo, REGION_SIZEDEF);

	/*
	 * If we're creating a temporary region, don't use any standard
	 * naming.
	 */
	if (is_private) {
		dbmp->reginfo.appname = DB_APP_TMP;
		dbmp->reginfo.file = NULL;
		F_SET(&dbmp->reginfo, REGION_PRIVATE);
	}

	if ((ret = __db_rattach(&dbmp->reginfo)) != 0) {
		if (dbmp->reginfo.path != NULL)
			FREES(dbmp->reginfo.path);
		return (ret);
	}

	/*
	 * The MPOOL structure is first in the region, the rest of the region
	 * is free space.
	 */
	dbmp->mp = dbmp->reginfo.addr;
	dbmp->addr = (u_int8_t *)dbmp->mp + sizeof(MPOOL);

	/* Initialize a created region. */
	if (F_ISSET(&dbmp->reginfo, REGION_CREATED)) {
		mp = dbmp->mp;
		SH_TAILQ_INIT(&mp->bhq);
		SH_TAILQ_INIT(&mp->bhfq);
		SH_TAILQ_INIT(&mp->mpfq);

		__db_shalloc_init(dbmp->addr, rlen - sizeof(MPOOL));

		/*
		 * Assume we want to keep the hash chains with under 10 pages
		 * on each chain.  We don't know the pagesize in advance, and
		 * it may differ for different files.  Use a pagesize of 1K for
		 * the calculation -- we walk these chains a lot, they should
		 * be short.
		 */
		mp->htab_buckets =
		    __db_tablesize((cachesize / (1 * 1024)) / 10);

		/* Allocate hash table space and initialize it. */
		if ((ret = __db_shalloc(dbmp->addr,
		    mp->htab_buckets * sizeof(DB_HASHTAB),
		    0, &dbmp->htab)) != 0)
			goto err;
		__db_hashinit(dbmp->htab, mp->htab_buckets);
		mp->htab = R_OFFSET(dbmp, dbmp->htab);

		ZERO_LSN(mp->lsn);
		mp->lsn_cnt = 0;

		memset(&mp->stat, 0, sizeof(mp->stat));
		mp->stat.st_cachesize = cachesize;

		mp->flags = 0;
	}

	/* Get the local hash table address. */
	dbmp->htab = R_ADDR(dbmp, dbmp->mp->htab);

	UNLOCKREGION(dbmp);
	return (0);

err:	UNLOCKREGION(dbmp);
	(void)__db_rdetach(&dbmp->reginfo);
	if (F_ISSET(&dbmp->reginfo, REGION_CREATED))
		(void)memp_unlink(path, 1, dbmp->dbenv);

	if (dbmp->reginfo.path != NULL)
		FREES(dbmp->reginfo.path);
	return (ret);
}
