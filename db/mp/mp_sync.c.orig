/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)mp_sync.c	10.25 (Sleepycat) 4/26/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_shash.h"
#include "mp.h"
#include "common_ext.h"

static int __bhcmp __P((const void *, const void *));
static int __memp_fsync __P((DB_MPOOLFILE *));

/*
 * memp_sync --
 *	Mpool sync function.
 */
int
memp_sync(dbmp, lsnp)
	DB_MPOOL *dbmp;
	DB_LSN *lsnp;
{
	BH *bhp, **bharray;
	DB_ENV *dbenv;
	MPOOL *mp;
	MPOOLFILE *mfp;
	int ar_cnt, cnt, nalloc, next, ret, wrote;

	dbenv = dbmp->dbenv;

	if (dbenv->lg_info == NULL) {
		__db_err(dbenv, "memp_sync: requires logging");
		return (EINVAL);
	}

	/*
	 * We try and write the buffers in page order so that the underlying
	 * filesystem doesn't have to seek and can write contiguous blocks,
	 * plus, we don't want to hold the region lock while we write the
	 * buffers.  Get memory to hold the buffer pointers.  Get a good-size
	 * block, too, because we realloc while holding the region lock if we
	 * run out.
	 */
	if ((bharray =
	    (BH **)__db_malloc((nalloc = 1024) * sizeof(BH *))) == NULL)
		return (ENOMEM);

	LOCKREGION(dbmp);

	/*
	 * If the application is asking about a previous call to memp_sync(),
	 * and we haven't found any buffers that the application holding the
	 * pin couldn't write, return yes or no based on the current count.
	 * Note, if the application is asking about a LSN *smaller* than one
	 * we've already handled or are currently handling, then we return a
	 * result based on the count for the larger LSN.
	 */
	mp = dbmp->mp;
	if (!F_ISSET(mp, MP_LSN_RETRY) && log_compare(lsnp, &mp->lsn) <= 0) {
		if (mp->lsn_cnt == 0) {
			*lsnp = mp->lsn;
			ret = 0;
		} else
			ret = DB_INCOMPLETE;
		goto done;
	}

	/* Else, it's a new checkpoint. */
	F_CLR(mp, MP_LSN_RETRY);

	/*
	 * Save the LSN.  We know that it's a new LSN or larger than the one
	 * for which we were already doing a checkpoint.  (BTW, I don't expect
	 * to see multiple LSN's from the same or multiple processes, but You
	 * Just Never Know.  Responding as if they all called with the largest
	 * of the LSNs specified makes everything work.)
	 *
	 * We don't currently use the LSN we save.  We could potentially save
	 * the last-written LSN in each buffer header and use it to determine
	 * what buffers need to be written.  The problem with this is that it's
	 * sizeof(LSN) more bytes of buffer header.  We currently write all the
	 * dirty buffers instead.
	 *
	 * Walk the list of shared memory segments clearing the count of
	 * buffers waiting to be written.
	 */
	mp->lsn = *lsnp;
	mp->lsn_cnt = 0;
	for (mfp = SH_TAILQ_FIRST(&dbmp->mp->mpfq, __mpoolfile);
	    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile))
		mfp->lsn_cnt = 0;

	/*
	 * Walk the list of buffers and mark all dirty buffers to be written
	 * and all pinned buffers to be potentially written (we can't know if
	 * we'll need to write them until the holding process returns them to
	 * the cache).  We do this in one pass while holding the region locked
	 * so that processes can't make new buffers dirty, causing us to never
	 * finish.  Since the application may have restarted the sync, clear
	 * any BH_WRITE flags that appear to be left over from previous calls.
	 *
	 * Keep a count of the total number of buffers we need to write in
	 * MPOOL->lsn_cnt, and for each file, in MPOOLFILE->lsn_count.
	 */
	ar_cnt = 0;
	for (bhp = SH_TAILQ_FIRST(&mp->bhq, __bh);
	    bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, q, __bh))
		if (F_ISSET(bhp, BH_DIRTY) || bhp->ref != 0) {
			F_SET(bhp, BH_WRITE);

			++mp->lsn_cnt;

			mfp = R_ADDR(dbmp, bhp->mf_offset);
			++mfp->lsn_cnt;

			/*
			 * If the buffer isn't in use, we should be able to
			 * write it immediately, so save a reference to it.
			 */
			if (bhp->ref == 0) {
				if (ar_cnt == nalloc) {
					nalloc *= 2;
					if ((bharray =
					    (BH **)__db_realloc(bharray,
					    nalloc * sizeof(BH *))) == NULL) {
						ret = ENOMEM;
						goto err;
					}
				}
				bharray[ar_cnt++] = bhp;
			}
		} else
			if (F_ISSET(bhp, BH_WRITE))
				F_CLR(bhp, BH_WRITE);

	/* If there no buffers we can write immediately, we're done. */
	if (ar_cnt == 0) {
		ret = mp->lsn_cnt ? DB_INCOMPLETE : 0;
		goto done;
	}

	/* Lock down the buffers and their contents. */
	for (cnt = 0; cnt < ar_cnt; ++cnt)
		++bharray[cnt]->ref;

	UNLOCKREGION(dbmp);

	/* Sort the buffers we're going to write. */
	qsort(bharray, ar_cnt, sizeof(BH *), __bhcmp);

	LOCKREGION(dbmp);

	/* Walk the array, writing buffers. */
	for (next = 0; next < ar_cnt; ++next) {
		/*
		 * It's possible for a thread to have gotten the buffer since
		 * we listed it for writing.  If the reference count is still
		 * 1, we're the only ones using the buffer, go ahead and write.
		 * If it's >1, then skip the buffer and assume that it will be
		 * written when it's returned to the cache.
		 */
		if (bharray[next]->ref > 1) {
			--bharray[next]->ref;
			continue;
		}

		/* Write the buffer. */
		mfp = R_ADDR(dbmp, bharray[next]->mf_offset);
		ret = __memp_bhwrite(dbmp, mfp, bharray[next], NULL, &wrote);

		/* Release the buffer. */
		--bharray[next]->ref;

		/* If there's an error, release the rest of the buffers. */
		if (ret != 0 || !wrote) {
			/*
			 * Any process syncing the shared memory buffer pool
			 * had better be able to write to any underlying file.
			 * Be understanding, but firm, on this point.
			 */
			if (ret == 0) {
				__db_err(dbenv, "%s: unable to flush page: %lu",
				    __memp_fns(dbmp, mfp),
				    (u_long)bharray[next]->pgno);
				ret = EPERM;
			}

			while (++next < ar_cnt)
				--bharray[next]->ref;
			goto err;
		}
	}
	ret = mp->lsn_cnt ? DB_INCOMPLETE : 0;

done:
	if (0) {
err:		/*
		 * On error, clear:
		 *	MPOOL->lsn_cnt (the total sync count)
		 *	MPOOLFILE->lsn_cnt (the per-file sync count)
		 *	BH_WRITE flag (the scheduled for writing flag)
		 */
		mp->lsn_cnt = 0;
		for (mfp = SH_TAILQ_FIRST(&dbmp->mp->mpfq, __mpoolfile);
		    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile))
			mfp->lsn_cnt = 0;
		for (bhp = SH_TAILQ_FIRST(&mp->bhq, __bh);
		    bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, q, __bh))
			F_CLR(bhp, BH_WRITE);
	}
	UNLOCKREGION(dbmp);
	__db_free(bharray);
	return (ret);
}

/*
 * memp_fsync --
 *	Mpool file sync function.
 */
int
memp_fsync(dbmfp)
	DB_MPOOLFILE *dbmfp;
{
	DB_MPOOL *dbmp;
	int is_tmp;

	dbmp = dbmfp->dbmp;

	/*
	 * If this handle doesn't have a file descriptor that's open for
	 * writing, or if the file is a temporary, there's no reason to
	 * proceed further.
	 */
	if (F_ISSET(dbmfp, MP_READONLY))
		return (0);

	LOCKREGION(dbmp);
	is_tmp = F_ISSET(dbmfp->mfp, MP_TEMP);
	UNLOCKREGION(dbmp);
	if (is_tmp)
		return (0);

	return (__memp_fsync(dbmfp));
}

/*
 * __mp_xxx_fd --
 *	Return a file descriptor for DB 1.85 compatibility locking.
 *
 * PUBLIC: int __mp_xxx_fd __P((DB_MPOOLFILE *, int *));
 */
int
__mp_xxx_fd(dbmfp, fdp)
	DB_MPOOLFILE *dbmfp;
	int *fdp;
{
	int ret;

	/*
	 * This is a truly spectacular layering violation, intended ONLY to
	 * support compatibility for the DB 1.85 DB->fd call.
	 *
	 * Sync the database file to disk, creating the file as necessary.
	 *
	 * We skip the MP_READONLY and MP_TEMP tests done by memp_fsync(3).
	 * The MP_READONLY test isn't interesting because we will either
	 * already have a file descriptor (we opened the database file for
	 * reading) or we aren't readonly (we created the database which
	 * requires write privileges).  The MP_TEMP test isn't interesting
	 * because we want to write to the backing file regardless so that
	 * we get a file descriptor to return.
	 */
	ret = dbmfp->fd == -1 ? __memp_fsync(dbmfp) : 0;

	return ((*fdp = dbmfp->fd) == -1 ? ENOENT : ret);
}

/*
 * __memp_fsync --
 *	Mpool file internal sync function.
 */
static int
__memp_fsync(dbmfp)
	DB_MPOOLFILE *dbmfp;
{
	BH *bhp, **bharray;
	DB_MPOOL *dbmp;
	size_t mf_offset;
	int ar_cnt, cnt, nalloc, next, pincnt, ret, wrote;

	ret = 0;
	dbmp = dbmfp->dbmp;
	mf_offset = R_OFFSET(dbmp, dbmfp->mfp);

	/*
	 * We try and write the buffers in page order so that the underlying
	 * filesystem doesn't have to seek and can write contiguous blocks,
	 * plus, we don't want to hold the region lock while we write the
	 * buffers.  Get memory to hold the buffer pointers.  Get a good-size
	 * block, too, because we realloc while holding the region lock if we
	 * run out.
	 */
	nalloc = 1024;
	if ((bharray =
	    (BH **)__db_malloc((size_t)nalloc * sizeof(BH *))) == NULL)
		return (ENOMEM);

	LOCKREGION(dbmp);

	/*
	 * Walk the LRU list of buffer headers, and get a list of buffers to
	 * write for this MPOOLFILE.
	 */
	ar_cnt = pincnt = 0;
	for (bhp = SH_TAILQ_FIRST(&dbmp->mp->bhq, __bh);
	    bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, q, __bh)) {
		if (!F_ISSET(bhp, BH_DIRTY) || bhp->mf_offset != mf_offset)
			continue;
		if (bhp->ref != 0 || F_ISSET(bhp, BH_LOCKED)) {
			++pincnt;
			continue;
		}

		if (ar_cnt == nalloc) {
			nalloc *= 2;
			if ((bharray = (BH **)__db_realloc(bharray,
			    nalloc * sizeof(BH *))) == NULL) {
				ret = ENOMEM;
				goto err;
			}
		}

		bharray[ar_cnt++] = bhp;
	}

	/* Lock down the buffers and their contents. */
	for (cnt = 0; cnt < ar_cnt; ++cnt)
		++bharray[cnt]->ref;

	UNLOCKREGION(dbmp);

	/* Sort the buffers we're going to write. */
	qsort(bharray, ar_cnt, sizeof(BH *), __bhcmp);

	LOCKREGION(dbmp);

	/* Walk the array, writing buffers. */
	for (next = 0; next < ar_cnt; ++next) {
		/*
		 * It's possible for a thread to have gotten the buffer since
		 * we listed it for writing.  If the reference count is still
		 * 1, we're the only ones using the buffer, go ahead and write.
		 * If it's >1, then skip the buffer and assume that it will be
		 * written when it's returned to the cache.
		 */
		if (bharray[next]->ref > 1) {
			++pincnt;

			--bharray[next]->ref;
			continue;
		}

		/* Write the buffer. */
		ret = __memp_pgwrite(dbmfp, bharray[next], NULL, &wrote);

		/* Release the buffer. */
		--bharray[next]->ref;

		/* If there's an error, release the rest of the buffers. */
		if (ret != 0) {
			while (++next < ar_cnt)
				--bharray[next]->ref;
			goto err;
		}
		if (!wrote)
			++pincnt;
	}

err:	UNLOCKREGION(dbmp);

	__db_free(bharray);

	/*
	 * Sync the underlying file as the last thing we do, so that the OS
	 * has maximal opportunity to flush buffers before we request it.
	 *
	 * XXX:
	 * Don't lock the region around the sync, fsync(2) has no atomicity
	 * issues.
	 */
	if (ret == 0)
		return (pincnt == 0 ? __db_fsync(dbmfp->fd) : DB_INCOMPLETE);
	return (ret);
}

/*
 * memp_trickle --
 *	Keep a specified percentage of the buffers clean.
 */
int
memp_trickle(dbmp, pct, nwrotep)
	DB_MPOOL *dbmp;
	int pct, *nwrotep;
{
	BH *bhp;
	MPOOL *mp;
	MPOOLFILE *mfp;
	u_long total;
	int ret, wrote;

	mp = dbmp->mp;
	if (nwrotep != NULL)
		*nwrotep = 0;

	if (pct < 1 || pct > 100)
		return (EINVAL);

	LOCKREGION(dbmp);

	/*
	 * If there are sufficient clean buffers, or no buffers or no dirty
	 * buffers, we're done.
	 *
	 * XXX
	 * Using st_page_clean and st_page_dirty is our only choice at the
	 * moment, but it's not as correct as we might like in the presence
	 * of pools with more than one buffer size, as a free 512-byte buffer
	 * isn't the same as a free 8K buffer.
	 */
loop:	total = mp->stat.st_page_clean + mp->stat.st_page_dirty;
	if (total == 0 || mp->stat.st_page_dirty == 0 ||
	    (mp->stat.st_page_clean * 100) / total >= (u_long)pct) {
		UNLOCKREGION(dbmp);
		return (0);
	}

	/* Loop until we write a buffer. */
	for (bhp = SH_TAILQ_FIRST(&mp->bhq, __bh);
	    bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, q, __bh)) {
		if (bhp->ref != 0 ||
		    !F_ISSET(bhp, BH_DIRTY) || F_ISSET(bhp, BH_LOCKED))
			continue;

		mfp = R_ADDR(dbmp, bhp->mf_offset);

		/*
		 * We can't write to temporary files -- see the comment in
		 * mp_bh.c:__memp_bhwrite().
		 */
		if (F_ISSET(mfp, MP_TEMP))
			continue;

		if ((ret = __memp_bhwrite(dbmp, mfp, bhp, NULL, &wrote)) != 0)
			goto err;

		/*
		 * Any process syncing the shared memory buffer pool had better
		 * be able to write to any underlying file.  Be understanding,
		 * but firm, on this point.
		 */
		if (!wrote) {
			__db_err(dbmp->dbenv, "%s: unable to flush page: %lu",
			    __memp_fns(dbmp, mfp), (u_long)bhp->pgno);
			ret = EPERM;
			goto err;
		}

		++mp->stat.st_page_trickle;
		if (nwrotep != NULL)
			++*nwrotep;
		goto loop;
	}

	/* No more buffers to write. */
	return (0);

err:	UNLOCKREGION(dbmp);
	return (ret);
}

static int
__bhcmp(p1, p2)
	const void *p1, *p2;
{
	BH *bhp1, *bhp2;

	bhp1 = *(BH * const *)p1;
	bhp2 = *(BH * const *)p2;

	/* Sort by file (shared memory pool offset). */
	if (bhp1->mf_offset < bhp2->mf_offset)
		return (-1);
	if (bhp1->mf_offset > bhp2->mf_offset)
		return (1);

	/* Sort by page in file. */
	return (bhp1->pgno < bhp2->pgno ? -1 : 1);
}
