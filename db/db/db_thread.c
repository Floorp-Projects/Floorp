/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)db_thread.c	8.15 (Sleepycat) 4/26/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_am.h"

static int __db_getlockid __P((DB *, DB *));

/*
 * __db_gethandle --
 *	Called by db access method routines when the DB_THREAD flag is set.
 *	This routine returns a handle, either an existing handle from the
 *	chain of handles, or creating one if necessary.
 *
 * PUBLIC: int __db_gethandle __P((DB *, int (*)(DB *, DB *), DB **));
 */
int
__db_gethandle(dbp, am_func, dbpp)
	DB *dbp, **dbpp;
	int (*am_func) __P((DB *, DB *));
{
	DB *ret_dbp;
	int ret, t_ret;

	if ((ret = __db_mutex_lock((db_mutex_t *)dbp->mutexp, -1)) != 0)
		return (ret);

	if ((ret_dbp = LIST_FIRST(&dbp->handleq)) != NULL)
		/* Simply take one off the list. */
		LIST_REMOVE(ret_dbp, links);
	else {
		/* Allocate a new handle. */
		if ((ret_dbp = (DB *)__db_malloc(sizeof(*dbp))) == NULL) {
			ret = ENOMEM;
			goto err;
		}
		memcpy(ret_dbp, dbp, sizeof(*dbp));
		ret_dbp->internal = NULL;
		TAILQ_INIT(&ret_dbp->curs_queue);

		/* Set the locker, the lock structure and the lock DBT. */
		if ((ret = __db_getlockid(dbp, ret_dbp)) != 0)
			goto err;

		/* Finally, call the access method specific dup function. */
		if ((ret = am_func(dbp, ret_dbp)) != 0)
			goto err;
	}

	*dbpp = ret_dbp;

	if (0) {
err:		if (ret_dbp != NULL)
			FREE(ret_dbp, sizeof(*ret_dbp));
	}
	if ((t_ret =
	    __db_mutex_unlock((db_mutex_t *)dbp->mutexp, -1)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __db_puthandle --
 *	Return a DB handle to the pool for later use.
 *
 * PUBLIC: int __db_puthandle __P((DB *));
 */
int
__db_puthandle(dbp)
	DB *dbp;
{
	DB *master;
	int ret;

	master = dbp->master;
	if ((ret = __db_mutex_lock((db_mutex_t *)master->mutexp, -1)) != 0)
		return (ret);

	LIST_INSERT_HEAD(&master->handleq, dbp, links);

	return (__db_mutex_unlock((db_mutex_t *)master->mutexp, -1));
}

/*
 * __db_getlockid --
 *	Create a new locker ID and copy the file lock information from
 *	the old DB into the new one.
 */
static int
__db_getlockid(dbp, new_dbp)
	DB *dbp, *new_dbp;
{
	int ret;

	if (F_ISSET(dbp, DB_AM_LOCKING)) {
		if ((ret = lock_id(dbp->dbenv->lk_info, &new_dbp->locker)) != 0)
			return (ret);
		memcpy(new_dbp->lock.fileid, dbp->lock.fileid, DB_FILE_ID_LEN);
		new_dbp->lock_dbt.size = sizeof(new_dbp->lock);
		new_dbp->lock_dbt.data = &new_dbp->lock;
	}
	return (0);
}
