/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)log.c	10.54 (Sleepycat) 5/31/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "log.h"
#include "db_dispatch.h"
#include "txn_auto.h"
#include "common_ext.h"

static int __log_recover __P((DB_LOG *));

/*
 * log_open --
 *	Initialize and/or join a log.
 */
int
log_open(path, flags, mode, dbenv, lpp)
	const char *path;
	u_int32_t flags;
	int mode;
	DB_ENV *dbenv;
	DB_LOG **lpp;
{
	DB_LOG *dblp;
	LOG *lp;
	int ret;

	/* Validate arguments. */
#ifdef HAVE_SPINLOCKS
#define	OKFLAGS	(DB_CREATE | DB_THREAD)
#else
#define	OKFLAGS	(DB_CREATE)
#endif
	if ((ret = __db_fchk(dbenv, "log_open", flags, OKFLAGS)) != 0)
		return (ret);

	/* Create and initialize the DB_LOG structure. */
	if ((dblp = (DB_LOG *)__db_calloc(1, sizeof(DB_LOG))) == NULL)
		return (ENOMEM);

	if (path != NULL && (dblp->dir = __db_strdup(path)) == NULL) {
		ret = ENOMEM;
		goto err;
	}

	dblp->dbenv = dbenv;
	dblp->lfd = -1;
	ZERO_LSN(dblp->c_lsn);
	dblp->c_fd = -1;

	/*
	 * The log region isn't fixed size because we store the registered
	 * file names there.  Make it fairly large so that we don't have to
	 * grow it.
	 */
#define	DEF_LOG_SIZE	(30 * 1024)

	/* Map in the region. */
	dblp->reginfo.dbenv = dbenv;
	dblp->reginfo.appname = DB_APP_LOG;
	if (path == NULL)
		dblp->reginfo.path = NULL;
	else
		if ((dblp->reginfo.path = __db_strdup(path)) == NULL)
			goto err;
	dblp->reginfo.file = DB_DEFAULT_LOG_FILE;
	dblp->reginfo.mode = mode;
	dblp->reginfo.size = DEF_LOG_SIZE;
	dblp->reginfo.dbflags = flags;
	dblp->reginfo.flags = REGION_SIZEDEF;
	if ((ret = __db_rattach(&dblp->reginfo)) != 0)
		goto err;

	/*
	 * The LOG structure is first in the region, the rest of the region
	 * is free space.
	 */
	dblp->lp = dblp->reginfo.addr;
	dblp->addr = (u_int8_t *)dblp->lp + sizeof(LOG);

	/* Initialize a created region. */
	if (F_ISSET(&dblp->reginfo, REGION_CREATED)) {
		__db_shalloc_init(dblp->addr, DEF_LOG_SIZE - sizeof(LOG));

		/* Initialize the LOG structure. */
		lp = dblp->lp;
		lp->persist.lg_max = dbenv == NULL ? 0 : dbenv->lg_max;
		if (lp->persist.lg_max == 0)
			lp->persist.lg_max = DEFAULT_MAX;
		lp->persist.magic = DB_LOGMAGIC;
		lp->persist.version = DB_LOGVERSION;
		lp->persist.mode = mode;
		SH_TAILQ_INIT(&lp->fq);

		/* Initialize LOG LSNs. */
		lp->lsn.file = 1;
		lp->lsn.offset = 0;
	}

	/* Initialize thread information, mutex. */
	if (LF_ISSET(DB_THREAD)) {
		F_SET(dblp, DB_AM_THREAD);
		if ((ret = __db_shalloc(dblp->addr,
		    sizeof(db_mutex_t), MUTEX_ALIGNMENT, &dblp->mutexp)) != 0)
			goto err;
		(void)__db_mutex_init(dblp->mutexp, -1);
	}

	/*
	 * If doing recovery, try and recover any previous log files before
	 * releasing the lock.
	 */
	if (F_ISSET(&dblp->reginfo, REGION_CREATED) &&
	    (ret = __log_recover(dblp)) != 0)
		goto err;

	UNLOCK_LOGREGION(dblp);
	*lpp = dblp;
	return (0);

err:	if (dblp->reginfo.addr != NULL) {
		if (dblp->mutexp != NULL)
			__db_shalloc_free(dblp->addr, dblp->mutexp);

		UNLOCK_LOGREGION(dblp);
		(void)__db_rdetach(&dblp->reginfo);
		if (F_ISSET(&dblp->reginfo, REGION_CREATED))
			(void)log_unlink(path, 1, dbenv);
	}

	if (dblp->reginfo.path != NULL)
		FREES(dblp->reginfo.path);
	if (dblp->dir != NULL)
		FREES(dblp->dir);
	FREE(dblp, sizeof(*dblp));
	return (ret);
}

/*
 * __log_recover --
 *	Recover a log.
 */
static int
__log_recover(dblp)
	DB_LOG *dblp;
{
	DBT dbt;
	DB_LSN lsn;
	LOG *lp;
	u_int32_t chk;
	int cnt, found_checkpoint, ret;

	lp = dblp->lp;

	/*
	 * Find a log file.  If none exist, we simply return, leaving
	 * everything initialized to a new log.
	 */
	if ((ret = __log_find(dblp, 0, &cnt)) != 0)
		return (ret);
	if (cnt == 0)
		return (0);

	/*
	 * We have the last useful log file and we've loaded any persistent
	 * information.  Pretend that the log is larger than it can possibly
	 * be, and read the last file, looking for the last checkpoint and
	 * the log's end.
	 */
	lp->lsn.file = cnt + 1;
	lp->lsn.offset = 0;
	lsn.file = cnt;
	lsn.offset = 0;

	/* Set the cursor.  Shouldn't fail, leave error messages on. */
	memset(&dbt, 0, sizeof(dbt));
	if ((ret = __log_get(dblp, &lsn, &dbt, DB_SET, 0)) != 0)
		return (ret);

	/*
	 * Read to the end of the file, saving checkpoints.  This will fail
	 * at some point, so turn off error messages.
	 */
	found_checkpoint = 0;
	while (__log_get(dblp, &lsn, &dbt, DB_NEXT, 1) == 0) {
		if (dbt.size < sizeof(u_int32_t))
			continue;
		memcpy(&chk, dbt.data, sizeof(u_int32_t));
		if (chk == DB_txn_ckp) {
			lp->chkpt_lsn = lsn;
			found_checkpoint = 1;
		}
	}

	/*
	 * We know where the end of the log is.  Since that record is on disk,
	 * it's also the last-synced LSN.
	 */
	lp->lsn = lsn;
	lp->lsn.offset += dblp->c_len;
	lp->s_lsn = lp->lsn;

	/* Set up the current buffer information, too. */
	lp->len = dblp->c_len;
	lp->b_off = 0;
	lp->w_off = lp->lsn.offset;

	/*
	 * It's possible that we didn't find a checkpoint because there wasn't
	 * one in the last log file.  Start searching.
	 */
	while (!found_checkpoint && cnt > 1) {
		lsn.file = --cnt;
		lsn.offset = 0;

		/* Set the cursor.  Shouldn't fail, leave error messages on. */
		if ((ret = __log_get(dblp, &lsn, &dbt, DB_SET, 0)) != 0)
			return (ret);

		/*
		 * Read to the end of the file, saving checkpoints.  Shouldn't
		 * fail, leave error messages on.
		 */
		while (__log_get(dblp, &lsn, &dbt, DB_NEXT, 0) == 0) {
			if (dbt.size < sizeof(u_int32_t))
				continue;
			memcpy(&chk, dbt.data, sizeof(u_int32_t));
			if (chk == DB_txn_ckp) {
				lp->chkpt_lsn = lsn;
				found_checkpoint = 1;
			}
		}
	}

	/* If we never find a checkpoint, that's okay, just 0 it out. */
	if (!found_checkpoint)
		ZERO_LSN(lp->chkpt_lsn);

	__db_err(dblp->dbenv,
	    "Recovering the log: last valid LSN: file: %lu offset %lu",
	    (u_long)lp->lsn.file, (u_long)lp->lsn.offset);

	return (0);
}

/*
 * __log_find --
 *	Try to find a log file.  If find_first is set, valp will contain
 * the number of the first log file, else it will contain the number of
 * the last log file.
 *
 * PUBLIC: int __log_find __P((DB_LOG *, int, int *));
 */
int
__log_find(dblp, find_first, valp)
	DB_LOG *dblp;
	int find_first, *valp;
{
	int cnt, fcnt, logval, ret;
	const char *dir;
	char **names, *p, *q;

	*valp = 0;

	/* Find the directory name. */
	if ((ret = __log_name(dblp, 1, &p)) != 0)
		return (ret);
	if ((q = __db_rpath(p)) == NULL)
		dir = PATH_DOT;
	else {
		*q = '\0';
		dir = p;
	}

	/* Get the list of file names. */
	ret = __db_dirlist(dir, &names, &fcnt);
	FREES(p);
	if (ret != 0) {
		__db_err(dblp->dbenv, "%s: %s", dir, strerror(ret));
		return (ret);
	}

	/*
	 * Search for a valid log file name, return a value of 0 on
	 * failure.
	 */
	for (cnt = fcnt, logval = 0; --cnt >= 0;)
		if (strncmp(names[cnt], "log.", sizeof("log.") - 1) == 0) {
			logval = atoi(names[cnt] + 4);
			if (logval != 0 &&
			    __log_valid(dblp, dblp->lp, logval) == 0)
				break;
		}

	/* Discard the list. */
	__db_dirfree(names, fcnt);

	/* We have a valid log file, find either the first or last one. */
	if (find_first) {
		for (; logval > 0; --logval)
			if (__log_valid(dblp, dblp->lp, logval - 1) != 0)
				break;
	} else
		for (; logval < MAXLFNAME; ++logval)
			if (__log_valid(dblp, dblp->lp, logval + 1) != 0)
				break;
	*valp = logval;

	return (0);
}

/*
 * log_valid --
 *	Validate a log file.
 *
 * PUBLIC: int __log_valid __P((DB_LOG *, LOG *, int));
 */
int
__log_valid(dblp, lp, cnt)
	DB_LOG *dblp;
	LOG *lp;
	int cnt;
{
	LOGP persist;
	ssize_t nw;
	int fd, ret;
	char *p;

	if ((ret = __log_name(dblp, cnt, &p)) != 0)
		return (ret);

	fd = -1;
	if ((ret = __db_open(p,
	    DB_RDONLY | DB_SEQUENTIAL,
	    DB_RDONLY | DB_SEQUENTIAL, 0, &fd)) != 0 ||
	    (ret = __db_seek(fd, 0, 0, sizeof(HDR), 0, SEEK_SET)) != 0 ||
	    (ret = __db_read(fd, &persist, sizeof(LOGP), &nw)) != 0 ||
	    nw != sizeof(LOGP)) {
		if (ret == 0)
			ret = EIO;
		if (fd != -1) {
			(void)__db_close(fd);
			__db_err(dblp->dbenv,
			    "Ignoring log file: %s: %s", p, strerror(ret));
		}
		goto err;
	}
	(void)__db_close(fd);

	if (persist.magic != DB_LOGMAGIC) {
		__db_err(dblp->dbenv,
		    "Ignoring log file: %s: magic number %lx, not %lx",
		    p, (u_long)persist.magic, (u_long)DB_LOGMAGIC);
		ret = EINVAL;
		goto err;
	}
	if (persist.version < DB_LOGOLDVER || persist.version > DB_LOGVERSION) {
		__db_err(dblp->dbenv,
		    "Ignoring log file: %s: unsupported log version %lu",
		    p, (u_long)persist.version);
		ret = EINVAL;
		goto err;
	}

	if (lp != NULL) {
		lp->persist.lg_max = persist.lg_max;
		lp->persist.mode = persist.mode;
	}
	ret = 0;

err:	FREES(p);
	return (ret);
}

/*
 * log_close --
 *	Close a log.
 */
int
log_close(dblp)
	DB_LOG *dblp;
{
	int ret, t_ret;

	/* Discard the per-thread pointer. */
	if (dblp->mutexp != NULL) {
		LOCK_LOGREGION(dblp);
		__db_shalloc_free(dblp->addr, dblp->mutexp);
		UNLOCK_LOGREGION(dblp);
	}

	/* Close the region. */
	ret = __db_rdetach(&dblp->reginfo);

	/* Close open files, release allocated memory. */
	if (dblp->lfd != -1 && (t_ret = __db_close(dblp->lfd)) != 0 && ret == 0)
		ret = t_ret;
	if (dblp->c_dbt.data != NULL)
		FREE(dblp->c_dbt.data, dblp->c_dbt.ulen);
	if (dblp->c_fd != -1 &&
	    (t_ret = __db_close(dblp->c_fd)) != 0 && ret == 0)
		ret = t_ret;
	if (dblp->dbentry != NULL)
		FREE(dblp->dbentry, (dblp->dbentry_cnt * sizeof(DB_ENTRY)));
	if (dblp->dir != NULL)
		FREES(dblp->dir);

	if (dblp->reginfo.path != NULL)
		FREES(dblp->reginfo.path);
	FREE(dblp, sizeof(*dblp));

	return (ret);
}

/*
 * log_unlink --
 *	Exit a log.
 */
int
log_unlink(path, force, dbenv)
	const char *path;
	int force;
	DB_ENV *dbenv;
{
	REGINFO reginfo;
	int ret;

	memset(&reginfo, 0, sizeof(reginfo));
	reginfo.dbenv = dbenv;
	reginfo.appname = DB_APP_LOG;
	if (path != NULL && (reginfo.path = __db_strdup(path)) == NULL)
		return (ENOMEM);
	reginfo.file = DB_DEFAULT_LOG_FILE;
	ret = __db_runlink(&reginfo, force);
	if (reginfo.path != NULL)
		FREES(reginfo.path);
	return (ret);
}

/*
 * log_stat --
 *	Return LOG statistics.
 */
int
log_stat(dblp, gspp, db_malloc)
	DB_LOG *dblp;
	DB_LOG_STAT **gspp;
	void *(*db_malloc) __P((size_t));
{
	LOG *lp;

	*gspp = NULL;
	lp = dblp->lp;

	if ((*gspp = db_malloc == NULL ?
	    (DB_LOG_STAT *)__db_malloc(sizeof(**gspp)) :
	    (DB_LOG_STAT *)db_malloc(sizeof(**gspp))) == NULL)
		return (ENOMEM);

	/* Copy out the global statistics. */
	LOCK_LOGREGION(dblp);
	**gspp = lp->stat;

	(*gspp)->st_magic = lp->persist.magic;
	(*gspp)->st_version = lp->persist.version;
	(*gspp)->st_mode = lp->persist.mode;
	(*gspp)->st_lg_max = lp->persist.lg_max;

	(*gspp)->st_region_nowait = lp->rlayout.lock.mutex_set_nowait;
	(*gspp)->st_region_wait = lp->rlayout.lock.mutex_set_wait;

	(*gspp)->st_cur_file = lp->lsn.file;
	(*gspp)->st_cur_offset = lp->lsn.offset;

	(*gspp)->st_refcnt = lp->rlayout.refcnt;
	(*gspp)->st_regsize = lp->rlayout.size;

	UNLOCK_LOGREGION(dblp);

	return (0);
}
