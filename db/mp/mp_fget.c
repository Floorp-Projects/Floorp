/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)mp_fget.c	10.48 (Sleepycat) 6/2/98";
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
 * memp_fget --
 *	Get a page from the file.
 */
int
memp_fget(dbmfp, pgnoaddr, flags, addrp)
	DB_MPOOLFILE *dbmfp;
	db_pgno_t *pgnoaddr;
	u_int32_t flags;
	void *addrp;
{
	BH *bhp;
	DB_MPOOL *dbmp;
	MPOOL *mp;
	MPOOLFILE *mfp;
	size_t bucket, mf_offset;
	u_int32_t st_hsearch;
	int b_incr, first, ret;

	dbmp = dbmfp->dbmp;
	mp = dbmp->mp;
	mfp = dbmfp->mfp;

	/*
	 * Validate arguments.
	 *
	 * !!!
	 * Don't test for DB_MPOOL_CREATE and DB_MPOOL_NEW flags for readonly
	 * files here, and create non-existent pages in readonly files if the
	 * flags are set, later.  The reason is that the hash access method
	 * wants to get empty pages that don't really exist in readonly files.
	 * The only alternative is for hash to write the last "bucket" all the
	 * time, which we don't want to do because one of our big goals in life
	 * is to keep database files small.  It's sleazy as hell, but we catch
	 * any attempt to actually write the file in memp_fput().
	 */
#define	OKFLAGS	(DB_MPOOL_CREATE | DB_MPOOL_LAST | DB_MPOOL_NEW)
	if (flags != 0) {
		if ((ret =
		    __db_fchk(dbmp->dbenv, "memp_fget", flags, OKFLAGS)) != 0)
			return (ret);

		switch (flags) {
		case DB_MPOOL_CREATE:
		case DB_MPOOL_LAST:
		case DB_MPOOL_NEW:
		case 0:
			break;
		default:
			return (__db_ferr(dbmp->dbenv, "memp_fget", 1));
		}
	}

#ifdef DIAGNOSTIC
	/*
	 * XXX
	 * We want to switch threads as often as possible.  Sleep every time
	 * we get a new page to make it more likely.
	 */
	if (DB_GLOBAL(db_pageyield) &&
	    (__db_yield == NULL || __db_yield() != 0))
		__db_sleep(0, 1);
#endif

	/* Initialize remaining local variables. */
	mf_offset = R_OFFSET(dbmp, mfp);
	bhp = NULL;
	st_hsearch = 0;
	b_incr = ret = 0;

	/* Determine the hash bucket where this page will live. */
	bucket = BUCKET(mp, mf_offset, *pgnoaddr);

	LOCKREGION(dbmp);

	/*
	 * Check for the last or last + 1 page requests.
	 *
	 * Examine and update the file's last_pgno value.  We don't care if
	 * the last_pgno value immediately changes due to another thread --
	 * at this instant in time, the value is correct.  We do increment the
	 * current last_pgno value if the thread is asking for a new page,
	 * however, to ensure that two threads creating pages don't get the
	 * same one.
	 */
	if (LF_ISSET(DB_MPOOL_LAST | DB_MPOOL_NEW)) {
		if (LF_ISSET(DB_MPOOL_NEW))
			++mfp->last_pgno;
		*pgnoaddr = mfp->last_pgno;
		bucket = BUCKET(mp, mf_offset, mfp->last_pgno);

		if (LF_ISSET(DB_MPOOL_NEW))
			goto alloc;
	}

	/*
	 * If mmap'ing the file and the page is not past the end of the file,
	 * just return a pointer.
	 *
	 * The page may be past the end of the file, so check the page number
	 * argument against the original length of the file.  If we previously
	 * returned pages past the original end of the file, last_pgno will
	 * have been updated to match the "new" end of the file, and checking
	 * against it would return pointers past the end of the mmap'd region.
	 *
	 * If another process has opened the file for writing since we mmap'd
	 * it, we will start playing the game by their rules, i.e. everything
	 * goes through the cache.  All pages previously returned will be safe,
	 * as long as the correct locking protocol was observed.
	 *
	 * XXX
	 * We don't discard the map because we don't know when all of the
	 * pages will have been discarded from the process' address space.
	 * It would be possible to do so by reference counting the open
	 * pages from the mmap, but it's unclear to me that it's worth it.
	 */
	if (dbmfp->addr != NULL && F_ISSET(mfp, MP_CAN_MMAP))
		if (*pgnoaddr > mfp->orig_last_pgno) {
			/*
			 * !!!
			 * See the comment above about non-existent pages and
			 * the hash access method.
			 */
			if (!LF_ISSET(DB_MPOOL_CREATE)) {
				__db_err(dbmp->dbenv,
				    "%s: page %lu doesn't exist",
				    __memp_fn(dbmfp), (u_long)*pgnoaddr);
				ret = EINVAL;
				goto err;
			}
		} else {
			*(void **)addrp =
			    R_ADDR(dbmfp, *pgnoaddr * mfp->stat.st_pagesize);
			++mp->stat.st_map;
			++mfp->stat.st_map;
			goto done;
		}

	/* Search the hash chain for the page. */
	for (bhp = SH_TAILQ_FIRST(&dbmp->htab[bucket], __bh);
	    bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, hq, __bh)) {
		++st_hsearch;
		if (bhp->pgno != *pgnoaddr || bhp->mf_offset != mf_offset)
			continue;

		/* Increment the reference count. */
		if (bhp->ref == UINT16_T_MAX) {
			__db_err(dbmp->dbenv,
			    "%s: page %lu: reference count overflow",
			    __memp_fn(dbmfp), (u_long)bhp->pgno);
			ret = EINVAL;
			goto err;
		}

		/*
		 * Increment the reference count.  We may discard the region
		 * lock as we evaluate and/or read the buffer, so we need to
		 * ensure that it doesn't move and that its contents remain
		 * unchanged.
		 */
		++bhp->ref;
		b_incr = 1;

		/*
	 	 * Any buffer we find might be trouble.
		 *
		 * BH_LOCKED --
		 * I/O is in progress.  Because we've incremented the buffer
		 * reference count, we know the buffer can't move.  Unlock
		 * the region lock, wait for the I/O to complete, and reacquire
		 * the region.
		 */
		for (first = 1; F_ISSET(bhp, BH_LOCKED); first = 0) {
			UNLOCKREGION(dbmp);

			/*
			 * Explicitly yield the processor if it's not the first
			 * pass through this loop -- if we don't, we might end
			 * up running to the end of our CPU quantum as we will
			 * simply be swapping between the two locks.
			 */
			if (!first && (__db_yield == NULL || __db_yield() != 0))
				__db_sleep(0, 1);

			LOCKBUFFER(dbmp, bhp);
			/* Wait for I/O to finish... */
			UNLOCKBUFFER(dbmp, bhp);
			LOCKREGION(dbmp);
		}

		/*
		 * BH_TRASH --
		 * The contents of the buffer are garbage.  Shouldn't happen,
		 * and this read is likely to fail, but might as well try.
		 */
		if (F_ISSET(bhp, BH_TRASH))
			goto reread;

		/*
		 * BH_CALLPGIN --
		 * The buffer was converted so it could be written, and the
		 * contents need to be converted again.
		 */
		if (F_ISSET(bhp, BH_CALLPGIN)) {
			if ((ret = __memp_pg(dbmfp, bhp, 1)) != 0)
				goto err;
			F_CLR(bhp, BH_CALLPGIN);
		}

		++mp->stat.st_cache_hit;
		++mfp->stat.st_cache_hit;
		*(void **)addrp = bhp->buf;
		goto done;
	}

alloc:	/* Allocate new buffer header and data space. */
	if ((ret = __memp_ralloc(dbmp, sizeof(BH) -
	    sizeof(u_int8_t) + mfp->stat.st_pagesize, NULL, &bhp)) != 0)
		goto err;

#ifdef DIAGNOSTIC
	if ((ALIGNTYPE)bhp->buf & (sizeof(size_t) - 1)) {
		__db_err(dbmp->dbenv,
		    "Internal error: BH data NOT size_t aligned.");
		ret = EINVAL;
		goto err;
	}
#endif
	/* Initialize the BH fields. */
	memset(bhp, 0, sizeof(BH));
	LOCKINIT(dbmp, &bhp->mutex);
	bhp->ref = 1;
	bhp->pgno = *pgnoaddr;
	bhp->mf_offset = mf_offset;

	/*
	 * Prepend the bucket header to the head of the appropriate MPOOL
	 * bucket hash list.  Append the bucket header to the tail of the
	 * MPOOL LRU chain.
	 */
	SH_TAILQ_INSERT_HEAD(&dbmp->htab[bucket], bhp, hq, __bh);
	SH_TAILQ_INSERT_TAIL(&mp->bhq, bhp, q);

	/*
	 * If we created the page, zero it out and continue.
	 *
	 * !!!
	 * Note: DB_MPOOL_NEW specifically doesn't call the pgin function.
	 * If DB_MPOOL_CREATE is used, then the application's pgin function
	 * has to be able to handle pages of 0's -- if it uses DB_MPOOL_NEW,
	 * it can detect all of its page creates, and not bother.
	 *
	 * Otherwise, read the page into memory, optionally creating it if
	 * DB_MPOOL_CREATE is set.
	 */
	if (LF_ISSET(DB_MPOOL_NEW)) {
		if (mfp->clear_len == 0)
			memset(bhp->buf, 0, mfp->stat.st_pagesize);
		else {
			memset(bhp->buf, 0, mfp->clear_len);
#ifdef DIAGNOSTIC
			memset(bhp->buf + mfp->clear_len, 0xff,
			    mfp->stat.st_pagesize - mfp->clear_len);
#endif
		}

		++mp->stat.st_page_create;
		++mfp->stat.st_page_create;
	} else {
		/*
		 * It's possible for the read function to fail, which means
		 * that we fail as well.  Note, the __memp_pgread() function
		 * discards the region lock, so the buffer must be pinned
		 * down so that it cannot move and its contents are unchanged.
		 */
reread:		if ((ret = __memp_pgread(dbmfp,
		    bhp, LF_ISSET(DB_MPOOL_CREATE))) != 0) {
			/*
			 * !!!
			 * Discard the buffer unless another thread is waiting
			 * on our I/O to complete.  Regardless, the header has
			 * the BH_TRASH flag set.
			 */
			if (bhp->ref == 1)
				__memp_bhfree(dbmp, mfp, bhp, 1);
			goto err;
		}

		++mp->stat.st_cache_miss;
		++mfp->stat.st_cache_miss;
	}

	/*
	 * If we're returning a page after our current notion of the last-page,
	 * update our information.  Note, there's no way to un-instantiate this
	 * page, it's going to exist whether it's returned to us dirty or not.
	 */
	if (bhp->pgno > mfp->last_pgno)
		mfp->last_pgno = bhp->pgno;

	++mp->stat.st_page_clean;
	*(void **)addrp = bhp->buf;

done:	/* Update the chain search statistics. */
	if (st_hsearch) {
		++mp->stat.st_hash_searches;
		if (st_hsearch > mp->stat.st_hash_longest)
			mp->stat.st_hash_longest = st_hsearch;
		mp->stat.st_hash_examined += st_hsearch;
	}

	UNLOCKREGION(dbmp);

	LOCKHANDLE(dbmp, dbmfp->mutexp);
	++dbmfp->pinref;
	UNLOCKHANDLE(dbmp, dbmfp->mutexp);

	return (0);

err:	/* Discard our reference. */
	if (b_incr)
		--bhp->ref;
	UNLOCKREGION(dbmp);

	*(void **)addrp = NULL;
	return (ret);
}
