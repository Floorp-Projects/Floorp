/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)mp_fput.c	10.22 (Sleepycat) 4/26/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_shash.h"
#include "mp.h"
#include "common_ext.h"

/*
 * memp_fput --
 *	Mpool file put function.
 */
int
memp_fput(dbmfp, pgaddr, flags)
	DB_MPOOLFILE *dbmfp;
	void *pgaddr;
	u_int32_t flags;
{
	BH *bhp;
	DB_MPOOL *dbmp;
	MPOOL *mp;
	int wrote, ret;

	dbmp = dbmfp->dbmp;
	mp = dbmp->mp;

	/* Validate arguments. */
	if (flags) {
		if ((ret = __db_fchk(dbmp->dbenv, "memp_fput", flags,
		    DB_MPOOL_CLEAN | DB_MPOOL_DIRTY | DB_MPOOL_DISCARD)) != 0)
			return (ret);
		if ((ret = __db_fcchk(dbmp->dbenv, "memp_fput",
		    flags, DB_MPOOL_CLEAN, DB_MPOOL_DIRTY)) != 0)
			return (ret);

		if (LF_ISSET(DB_MPOOL_DIRTY) && F_ISSET(dbmfp, MP_READONLY)) {
			__db_err(dbmp->dbenv,
			    "%s: dirty flag set for readonly file page",
			    __memp_fn(dbmfp));
			return (EACCES);
		}
	}

	/* Decrement the pinned reference count. */
	LOCKHANDLE(dbmp, dbmfp->mutexp);
	if (dbmfp->pinref == 0)
		__db_err(dbmp->dbenv,
		    "%s: put: more blocks returned than retrieved",
		    __memp_fn(dbmfp));
	else
		--dbmfp->pinref;
	UNLOCKHANDLE(dbmp, dbmfp->mutexp);

	/*
	 * If we're mapping the file, there's nothing to do.  Because we can
	 * stop mapping the file at any time, we have to check on each buffer
	 * to see if the address we gave the application was part of the map
	 * region.
	 */
	if (dbmfp->addr != NULL && pgaddr >= dbmfp->addr &&
	    (u_int8_t *)pgaddr <= (u_int8_t *)dbmfp->addr + dbmfp->len)
		return (0);

	/* Convert the page address to a buffer header. */
	bhp = (BH *)((u_int8_t *)pgaddr - SSZA(BH, buf));

	LOCKREGION(dbmp);

	/* Set/clear the page bits. */
	if (LF_ISSET(DB_MPOOL_CLEAN) && F_ISSET(bhp, BH_DIRTY)) {
		++mp->stat.st_page_clean;
		--mp->stat.st_page_dirty;
		F_CLR(bhp, BH_DIRTY);
	}
	if (LF_ISSET(DB_MPOOL_DIRTY) && !F_ISSET(bhp, BH_DIRTY)) {
		--mp->stat.st_page_clean;
		++mp->stat.st_page_dirty;
		F_SET(bhp, BH_DIRTY);
	}
	if (LF_ISSET(DB_MPOOL_DISCARD))
		F_SET(bhp, BH_DISCARD);

	/*
	 * Check for a reference count going to zero.  This can happen if the
	 * application returns a page twice.
	 */
	if (bhp->ref == 0) {
		__db_err(dbmp->dbenv, "%s: page %lu: unpinned page returned",
		    __memp_fn(dbmfp), (u_long)bhp->pgno);
		UNLOCKREGION(dbmp);
		return (EINVAL);
	}

	/*
	 * If more than one reference to the page, we're done.  Ignore the
	 * discard flags (for now) and leave it at its position in the LRU
	 * chain.  The rest gets done at last reference close.
	 */
	if (--bhp->ref > 0) {
		UNLOCKREGION(dbmp);
		return (0);
	}

	/*
	 * If this buffer is scheduled for writing because of a checkpoint, we
	 * need to write it (if we marked it dirty), or update the checkpoint
	 * counters (if we didn't mark it dirty).  If we try to write it and
	 * can't, that's not necessarily an error, but set a flag so that the
	 * next time the memp_sync function runs we try writing it there, as
	 * the checkpoint application better be able to write all of the files.
	 */
	if (F_ISSET(bhp, BH_WRITE))
		if (F_ISSET(bhp, BH_DIRTY)) {
			if (__memp_bhwrite(dbmp,
			    dbmfp->mfp, bhp, NULL, &wrote) != 0 || !wrote)
				F_SET(mp, MP_LSN_RETRY);
		} else {
			F_CLR(bhp, BH_WRITE);

			--dbmfp->mfp->lsn_cnt;
			--mp->lsn_cnt;
		}

	/* Move the buffer to the head/tail of the LRU chain. */
	SH_TAILQ_REMOVE(&mp->bhq, bhp, q, __bh);
	if (F_ISSET(bhp, BH_DISCARD))
		SH_TAILQ_INSERT_HEAD(&mp->bhq, bhp, q, __bh);
	else
		SH_TAILQ_INSERT_TAIL(&mp->bhq, bhp, q);


	UNLOCKREGION(dbmp);
	return (0);
}
