/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)mp_open.c	10.23 (Sleepycat) 5/3/98";
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
 * memp_open --
 *	Initialize and/or join a memory pool.
 */
int
memp_open(path, flags, mode, dbenv, retp)
	const char *path;
	u_int32_t flags;
	int mode;
	DB_ENV *dbenv;
	DB_MPOOL **retp;
{
	DB_MPOOL *dbmp;
	size_t cachesize;
	int is_private, ret;

	/* Validate arguments. */
#ifdef HAVE_SPINLOCKS
#define	OKFLAGS	(DB_CREATE | DB_MPOOL_PRIVATE | DB_NOMMAP | DB_THREAD)
#else
#define	OKFLAGS	(DB_CREATE | DB_MPOOL_PRIVATE | DB_NOMMAP)
#endif
	if ((ret = __db_fchk(dbenv, "memp_open", flags, OKFLAGS)) != 0)
		return (ret);

	/* Extract fields from DB_ENV structure. */
	cachesize = dbenv == NULL ? 0 : dbenv->mp_size;

	/* Create and initialize the DB_MPOOL structure. */
	if ((dbmp = (DB_MPOOL *)__db_calloc(1, sizeof(DB_MPOOL))) == NULL)
		return (ENOMEM);
	LIST_INIT(&dbmp->dbregq);
	TAILQ_INIT(&dbmp->dbmfq);

	dbmp->dbenv = dbenv;

	/* Decide if it's possible for anyone else to access the pool. */
	is_private =
	    (dbenv == NULL && path == NULL) || LF_ISSET(DB_MPOOL_PRIVATE);

	/*
	 * Map in the region.  We do locking regardless, as portions of it are
	 * implemented in common code (if we put the region in a file, that is).
	 */
	F_SET(dbmp, MP_LOCKREGION);
	if ((ret = __memp_ropen(dbmp,
	    path, cachesize, mode, is_private, LF_ISSET(DB_CREATE))) != 0)
		goto err;
	F_CLR(dbmp, MP_LOCKREGION);

	/*
	 * If there's concurrent access, then we have to lock the region.
	 * If it's threaded, then we have to lock both the handles and the
	 * region, and we need to allocate a mutex for that purpose.
	 */
	if (!is_private)
		F_SET(dbmp, MP_LOCKREGION);
	if (LF_ISSET(DB_THREAD)) {
		F_SET(dbmp, MP_LOCKHANDLE | MP_LOCKREGION);
		LOCKREGION(dbmp);
		ret = __memp_ralloc(dbmp,
		    sizeof(db_mutex_t), NULL, &dbmp->mutexp);
		UNLOCKREGION(dbmp);
		if (ret != 0) {
			(void)memp_close(dbmp);
			goto err;
		}
		LOCKINIT(dbmp, dbmp->mutexp);
	}

	*retp = dbmp;
	return (0);

err:	if (dbmp != NULL)
		FREE(dbmp, sizeof(DB_MPOOL));
	return (ret);
}

/*
 * memp_close --
 *	Close a memory pool.
 */
int
memp_close(dbmp)
	DB_MPOOL *dbmp;
{
	DB_MPOOLFILE *dbmfp;
	DB_MPREG *mpreg;
	int ret, t_ret;

	ret = 0;

	/* Discard DB_MPREGs. */
	while ((mpreg = LIST_FIRST(&dbmp->dbregq)) != NULL) {
		LIST_REMOVE(mpreg, q);
		FREE(mpreg, sizeof(DB_MPREG));
	}

	/* Discard DB_MPOOLFILEs. */
	while ((dbmfp = TAILQ_FIRST(&dbmp->dbmfq)) != NULL)
		if ((t_ret = memp_fclose(dbmfp)) != 0 && ret == 0)
			ret = t_ret;

	/* Discard thread mutex. */
	if (F_ISSET(dbmp, MP_LOCKHANDLE)) {
		LOCKREGION(dbmp);
		__db_shalloc_free(dbmp->addr, dbmp->mutexp);
		UNLOCKREGION(dbmp);
	}

	/* Close the region. */
	if ((t_ret = __db_rdetach(&dbmp->reginfo)) != 0 && ret == 0)
		ret = t_ret;

	if (dbmp->reginfo.path != NULL)
		FREES(dbmp->reginfo.path);
	FREE(dbmp, sizeof(DB_MPOOL));

	return (ret);
}

/*
 * memp_unlink --
 *	Exit a memory pool.
 */
int
memp_unlink(path, force, dbenv)
	const char *path;
	int force;
	DB_ENV *dbenv;
{
	REGINFO reginfo;
	int ret;

	memset(&reginfo, 0, sizeof(reginfo));
	reginfo.dbenv = dbenv;
	reginfo.appname = DB_APP_NONE;
	if (path != NULL && (reginfo.path = __db_strdup(path)) == NULL)
		return (ENOMEM);
	reginfo.file = DB_DEFAULT_MPOOL_FILE;
	ret = __db_runlink(&reginfo, force);
	if (reginfo.path != NULL)
		FREES(reginfo.path);
	return (ret);
}

/*
 * memp_register --
 *	Register a file type's pgin, pgout routines.
 */
int
memp_register(dbmp, ftype, pgin, pgout)
	DB_MPOOL *dbmp;
	int ftype;
	int (*pgin) __P((db_pgno_t, void *, DBT *));
	int (*pgout) __P((db_pgno_t, void *, DBT *));
{
	DB_MPREG *mpr;

	if ((mpr = (DB_MPREG *)__db_malloc(sizeof(DB_MPREG))) == NULL)
		return (ENOMEM);

	mpr->ftype = ftype;
	mpr->pgin = pgin;
	mpr->pgout = pgout;

	/*
	 * Insert at the head.  Because we do a linear walk, we'll find
	 * the most recent registry in the case of multiple entries, so
	 * we don't have to check for multiple registries.
	 */
	LOCKHANDLE(dbmp, dbmp->mutexp);
	LIST_INSERT_HEAD(&dbmp->dbregq, mpr, q);
	UNLOCKHANDLE(dbmp, dbmp->mutexp);

	return (0);
}
