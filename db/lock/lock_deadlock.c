/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)lock_deadlock.c	10.32 (Sleepycat) 4/26/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_shash.h"
#include "lock.h"
#include "common_ext.h"

#define	ISSET_MAP(M, N)	(M[(N) / 32] & (1 << (N) % 32))

#define	CLEAR_MAP(M, N) {						\
	u_int32_t __i;							\
	for (__i = 0; __i < (N); __i++)					\
		M[__i] = 0;						\
}

#define	SET_MAP(M, B)	(M[(B) / 32] |= (1 << ((B) % 32)))
#define	CLR_MAP(M, B)	(M[(B) / 32] &= ~(1 << ((B) % 32)))

#define	OR_MAP(D, S, N)	{						\
	u_int32_t __i;							\
	for (__i = 0; __i < (N); __i++)					\
		D[__i] |= S[__i];					\
}
#define	BAD_KILLID	0xffffffff

typedef struct {
	int		valid;
	u_int32_t	id;
	DB_LOCK		last_lock;
	db_pgno_t	pgno;
} locker_info;

static int  __dd_abort __P((DB_ENV *, locker_info *));
static int  __dd_build
	__P((DB_ENV *, u_int32_t **, u_int32_t *, locker_info **));
static u_int32_t
	   *__dd_find __P((u_int32_t *, locker_info *, u_int32_t));

#ifdef DIAGNOSTIC
static void __dd_debug __P((DB_ENV *, locker_info *, u_int32_t *, u_int32_t));
#endif

int
lock_detect(lt, flags, atype)
	DB_LOCKTAB *lt;
	u_int32_t flags, atype;
{
	DB_ENV *dbenv;
	locker_info *idmap;
	u_int32_t *bitmap, *deadlock, i, killid, nentries, nlockers;
	int do_pass, ret;

	/* Validate arguments. */
	if ((ret =
	    __db_fchk(lt->dbenv, "lock_detect", flags, DB_LOCK_CONFLICT)) != 0)
		return (ret);

	/* Check if a detector run is necessary. */
	dbenv = lt->dbenv;
	if (LF_ISSET(DB_LOCK_CONFLICT)) {
		/* Make a pass every time a lock waits. */
		LOCK_LOCKREGION(lt);
		do_pass = dbenv->lk_info->region->need_dd != 0;
		UNLOCK_LOCKREGION(lt);

		if (!do_pass)
			return (0);
	}

	/* Build the waits-for bitmap. */
	if ((ret = __dd_build(dbenv, &bitmap, &nlockers, &idmap)) != 0)
		return (ret);

	if (nlockers == 0)
		return (0);
#ifdef DIAGNOSTIC
	if (dbenv->db_verbose != 0)
		__dd_debug(dbenv, idmap, bitmap, nlockers);
#endif
	/* Find a deadlock. */
	deadlock = __dd_find(bitmap, idmap, nlockers);
	nentries = ALIGN(nlockers, 32) / 32;
	killid = BAD_KILLID;
	if (deadlock != NULL) {
		/* Kill someone. */
		switch (atype) {
		case DB_LOCK_OLDEST:
			/*
			 * Find the first bit set in the current
			 * array and then look for a lower tid in
			 * the array.
			 */
			for (i = 0; i < nlockers; i++)
				if (ISSET_MAP(deadlock, i))
					killid = i;

			if (killid == BAD_KILLID) {
				__db_err(dbenv,
				    "warning: could not find locker to abort");
				break;
			}

			/*
			 * The oldest transaction has the lowest
			 * transaction id.
			 */
			for (i = killid + 1; i < nlockers; i++)
				if (ISSET_MAP(deadlock, i) &&
				    idmap[i].id < idmap[killid].id)
					killid = i;
			break;
		case DB_LOCK_DEFAULT:
		case DB_LOCK_RANDOM:
			/*
			 * We are trying to calculate the id of the
			 * locker whose entry is indicated by deadlock.
			 */
			killid = (deadlock - bitmap) / nentries;
			break;
		case DB_LOCK_YOUNGEST:
			/*
			 * Find the first bit set in the current
			 * array and then look for a lower tid in
			 * the array.
			 */
			for (i = 0; i < nlockers; i++)
				if (ISSET_MAP(deadlock, i))
					killid = i;

			if (killid == BAD_KILLID) {
				__db_err(dbenv,
				    "warning: could not find locker to abort");
				break;
			}
			/*
			 * The youngest transaction has the highest
			 * transaction id.
			 */
			for (i = killid + 1; i < nlockers; i++)
				if (ISSET_MAP(deadlock, i) &&
				    idmap[i].id > idmap[killid].id)
					killid = i;
			break;
		default:
			killid = BAD_KILLID;
			ret = EINVAL;
		}

		/* Kill the locker with lockid idmap[killid]. */
		if (dbenv->db_verbose != 0 && killid != BAD_KILLID)
			__db_err(dbenv, "Aborting locker %lx",
			    (u_long)idmap[killid].id);

		if (killid != BAD_KILLID &&
		    (ret = __dd_abort(dbenv, &idmap[killid])) != 0)
			__db_err(dbenv,
			    "warning: unable to abort locker %lx",
			    (u_long)idmap[killid].id);
	}
	__db_free(bitmap);
	__db_free(idmap);

	return (ret);
}

/*
 * ========================================================================
 * Utilities
 */
static int
__dd_build(dbenv, bmp, nlockers, idmap)
	DB_ENV *dbenv;
	u_int32_t **bmp, *nlockers;
	locker_info **idmap;
{
	struct __db_lock *lp;
	DB_LOCKTAB *lt;
	DB_LOCKOBJ *op, *lo, *lockerp;
	u_int8_t *pptr;
	locker_info *id_array;
	u_int32_t *bitmap, count, *entryp, i, id, nentries, *tmpmap;
	int is_first;

	lt = dbenv->lk_info;

	/*
	 * We'll check how many lockers there are, add a few more in for
	 * good measure and then allocate all the structures.  Then we'll
	 * verify that we have enough room when we go back in and get the
	 * mutex the second time.
	 */
	LOCK_LOCKREGION(lt);
retry:	count = lt->region->nlockers;
	lt->region->need_dd = 0;
	UNLOCK_LOCKREGION(lt);

	if (count == 0) {
		*nlockers = 0;
		return (0);
	}

	if (dbenv->db_verbose)
		__db_err(dbenv, "%lu lockers", (u_long)count);

	count += 10;
	nentries = ALIGN(count, 32) / 32;
	/*
	 * Allocate enough space for a count by count bitmap matrix.
	 *
	 * XXX
	 * We can probably save the malloc's between iterations just
	 * reallocing if necessary because count grew by too much.
	 */
	if ((bitmap = (u_int32_t *)__db_calloc((size_t)count,
	    sizeof(u_int32_t) * nentries)) == NULL) {
		__db_err(dbenv, "%s", strerror(ENOMEM));
		return (ENOMEM);
	}

	if ((tmpmap =
	    (u_int32_t *)__db_calloc(sizeof(u_int32_t), nentries)) == NULL) {
		__db_err(dbenv, "%s", strerror(ENOMEM));
		__db_free(bitmap);
		return (ENOMEM);
	}

	if ((id_array = (locker_info *)__db_calloc((size_t)count,
	    sizeof(locker_info))) == NULL) {
		__db_err(dbenv, "%s", strerror(ENOMEM));
		__db_free(bitmap);
		__db_free(tmpmap);
		return (ENOMEM);
	}

	/*
	 * Now go back in and actually fill in the matrix.
	 */
	LOCK_LOCKREGION(lt);
	if (lt->region->nlockers > count) {
		__db_free(bitmap);
		__db_free(tmpmap);
		__db_free(id_array);
		goto retry;
	}

	/*
	 * First we go through and assign each locker a deadlock detector id.
	 * Note that we fill in the idmap in the next loop since that's the
	 * only place where we conveniently have both the deadlock id and the
	 * actual locker.
	 */
	for (id = 0, i = 0; i < lt->region->table_size; i++)
		for (op = SH_TAILQ_FIRST(&lt->hashtab[i], __db_lockobj);
		    op != NULL; op = SH_TAILQ_NEXT(op, links, __db_lockobj))
			if (op->type == DB_LOCK_LOCKER)
				op->dd_id = id++;
	/*
	 * We go through the hash table and find each object.  For each object,
	 * we traverse the waiters list and add an entry in the waitsfor matrix
	 * for each waiter/holder combination.
	 */
	for (i = 0; i < lt->region->table_size; i++) {
		for (op = SH_TAILQ_FIRST(&lt->hashtab[i], __db_lockobj);
		    op != NULL; op = SH_TAILQ_NEXT(op, links, __db_lockobj)) {
			if (op->type != DB_LOCK_OBJTYPE)
				continue;
			CLEAR_MAP(tmpmap, nentries);

			/*
			 * First we go through and create a bit map that
			 * represents all the holders of this object.
			 */
			for (lp = SH_TAILQ_FIRST(&op->holders, __db_lock);
			    lp != NULL;
			    lp = SH_TAILQ_NEXT(lp, links, __db_lock)) {
				if (__lock_getobj(lt, lp->holder,
				    NULL, DB_LOCK_LOCKER, &lockerp) != 0) {
					__db_err(dbenv,
					    "warning unable to find object");
					continue;
				}
				id_array[lockerp->dd_id].id = lp->holder;
				id_array[lockerp->dd_id].valid = 1;

				/*
				 * If the holder has already been aborted, then
				 * we should ignore it for now.
				 */
				if (lp->status == DB_LSTAT_HELD)
					SET_MAP(tmpmap, lockerp->dd_id);
			}

			/*
			 * Next, for each waiter, we set its row in the matrix
			 * equal to the map of holders we set up above.
			 */
			for (is_first = 1,
			    lp = SH_TAILQ_FIRST(&op->waiters, __db_lock);
			    lp != NULL;
			    is_first = 0,
			    lp = SH_TAILQ_NEXT(lp, links, __db_lock)) {
				if (__lock_getobj(lt, lp->holder,
				    NULL, DB_LOCK_LOCKER, &lockerp) != 0) {
					__db_err(dbenv,
					    "warning unable to find object");
					continue;
				}
				id_array[lockerp->dd_id].id = lp->holder;
				id_array[lockerp->dd_id].valid = 1;

				/*
				 * If the transaction is pending abortion, then
				 * ignore it on this iteration.
				 */
				if (lp->status != DB_LSTAT_WAITING)
					continue;

				entryp = bitmap + (nentries * lockerp->dd_id);
				OR_MAP(entryp, tmpmap, nentries);
				/*
				 * If this is the first waiter on the queue,
				 * then we remove the waitsfor relationship
				 * with oneself.  However, if it's anywhere
				 * else on the queue, then we have to keep
				 * it and we have an automatic deadlock.
				 */
				if (is_first)
					CLR_MAP(entryp, lockerp->dd_id);
			}
		}
	}

	/* Now for each locker; record its last lock. */
	for (id = 0; id < count; id++) {
		if (!id_array[id].valid)
			continue;
		if (__lock_getobj(lt,
		    id_array[id].id, NULL, DB_LOCK_LOCKER, &lockerp) != 0) {
			__db_err(dbenv,
			    "No locks for locker %lu", (u_long)id_array[id].id);
			continue;
		}
		lp = SH_LIST_FIRST(&lockerp->heldby, __db_lock);
		if (lp != NULL) {
			id_array[id].last_lock = LOCK_TO_OFFSET(lt, lp);
			lo = (DB_LOCKOBJ *)((u_int8_t *)lp + lp->obj);
			pptr = SH_DBT_PTR(&lo->lockobj);
			if (lo->lockobj.size >= sizeof(db_pgno_t))
				memcpy(&id_array[id].pgno, pptr,
				    sizeof(db_pgno_t));
			else
				id_array[id].pgno = 0;
		}
	}

	/* Pass complete, reset the deadlock detector bit. */
	lt->region->need_dd = 0;
	UNLOCK_LOCKREGION(lt);

	/*
	 * Now we can release everything except the bitmap matrix that we
	 * created.
	 */
	*nlockers = id;
	*idmap = id_array;
	*bmp = bitmap;
	__db_free(tmpmap);
	return (0);
}

static u_int32_t *
__dd_find(bmp, idmap, nlockers)
	u_int32_t *bmp, nlockers;
	locker_info *idmap;
{
	u_int32_t i, j, nentries, *mymap, *tmpmap;

	/*
	 * For each locker, OR in the bits from the lockers on which that
	 * locker is waiting.
	 */
	nentries = ALIGN(nlockers, 32) / 32;
	for (mymap = bmp, i = 0; i < nlockers; i++, mymap += nentries) {
		if (!idmap[i].valid)
			continue;
		for (j = 0; j < nlockers; j++) {
			if (ISSET_MAP(mymap, j)) {
				/* Find the map for this bit. */
				tmpmap = bmp + (nentries * j);
				OR_MAP(mymap, tmpmap, nentries);
				if (ISSET_MAP(mymap, i))
					return (mymap);
			}
		}
	}
	return (NULL);
}

static int
__dd_abort(dbenv, info)
	DB_ENV *dbenv;
	locker_info *info;
{
	struct __db_lock *lockp;
	DB_LOCKTAB *lt;
	DB_LOCKOBJ *lockerp, *sh_obj;
	int ret;

	lt = dbenv->lk_info;
	LOCK_LOCKREGION(lt);

	/* Find the locker's last lock. */
	if ((ret =
	    __lock_getobj(lt, info->id, NULL, DB_LOCK_LOCKER, &lockerp)) != 0)
		goto out;

	lockp = SH_LIST_FIRST(&lockerp->heldby, __db_lock);
	if (LOCK_TO_OFFSET(lt, lockp) != info->last_lock ||
	    lockp == NULL || lockp->status != DB_LSTAT_WAITING)
		goto out;

	/* Abort lock, take it off list, and wake up this lock. */
	lockp->status = DB_LSTAT_ABORTED;
	lt->region->ndeadlocks++;
	SH_LIST_REMOVE(lockp, locker_links, __db_lock);
	sh_obj = (DB_LOCKOBJ *)((u_int8_t *)lockp + lockp->obj);
	SH_TAILQ_REMOVE(&sh_obj->waiters, lockp, links, __db_lock);
        (void)__db_mutex_unlock(&lockp->mutex, lt->reginfo.fd);

	ret = 0;

out:	UNLOCK_LOCKREGION(lt);
	return (ret);
}

#ifdef DIAGNOSTIC
static void
__dd_debug(dbenv, idmap, bitmap, nlockers)
	DB_ENV *dbenv;
	locker_info *idmap;
	u_int32_t *bitmap, nlockers;
{
	u_int32_t i, j, *mymap, nentries;
	char *msgbuf;

	__db_err(dbenv, "Waitsfor array");
	__db_err(dbenv, "waiter\twaiting on");
	/*
	 * Allocate space to print 10 bytes per item waited on.
	 */
	if ((msgbuf = (char *)__db_malloc((nlockers + 1) * 10 + 64)) == NULL) {
		__db_err(dbenv, "%s", strerror(ENOMEM));
		return;
	}

	nentries = ALIGN(nlockers, 32) / 32;
	for (mymap = bitmap, i = 0; i < nlockers; i++, mymap += nentries) {
		if (!idmap[i].valid)
			continue;
		sprintf(msgbuf,					/* Waiter. */
		    "%lx/%lu:\t", (u_long)idmap[i].id, (u_long)idmap[i].pgno);
		for (j = 0; j < nlockers; j++)
			if (ISSET_MAP(mymap, j))
				sprintf(msgbuf, "%s %lx", msgbuf,
				    (u_long)idmap[j].id);
		(void)sprintf(msgbuf,
		    "%s %lu", msgbuf, (u_long)idmap[i].last_lock);
		__db_err(dbenv, msgbuf);
	}

	__db_free(msgbuf);
}
#endif
