/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)lock.c	10.52 (Sleepycat) 5/10/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_page.h"
#include "db_shash.h"
#include "lock.h"
#include "common_ext.h"
#include "db_am.h"

static void __lock_checklocker __P((DB_LOCKTAB *, struct __db_lock *, int));
static void __lock_freeobj __P((DB_LOCKTAB *, DB_LOCKOBJ *));
static int  __lock_get_internal __P((DB_LOCKTAB *, u_int32_t, u_int32_t,
    const DBT *, db_lockmode_t, struct __db_lock **));
static int  __lock_put_internal __P((DB_LOCKTAB *, struct __db_lock *, int));
static void __lock_remove_waiter
    __P((DB_LOCKTAB *, DB_LOCKOBJ *, struct __db_lock *, db_status_t));

int
lock_id(lt, idp)
	DB_LOCKTAB *lt;
	u_int32_t *idp;
{
	u_int32_t id;

	LOCK_LOCKREGION(lt);
	if (lt->region->id >= DB_LOCK_MAXID)
		lt->region->id = 0;
	id = ++lt->region->id;
	UNLOCK_LOCKREGION(lt);

	*idp = id;
	return (0);
}

int
lock_vec(lt, locker, flags, list, nlist, elistp)
	DB_LOCKTAB *lt;
	u_int32_t locker, flags;
	int nlist;
	DB_LOCKREQ *list, **elistp;
{
	struct __db_lock *lp;
	DB_LOCKOBJ *sh_obj, *sh_locker;
	int i, ret, run_dd;

	/* Validate arguments. */
	if ((ret =
	    __db_fchk(lt->dbenv, "lock_vec", flags, DB_LOCK_NOWAIT)) != 0)
		return (ret);

	LOCK_LOCKREGION(lt);

	if ((ret = __lock_validate_region(lt)) != 0) {
		UNLOCK_LOCKREGION(lt);
		return (ret);
	}

	ret = 0;
	for (i = 0; i < nlist && ret == 0; i++) {
		switch (list[i].op) {
		case DB_LOCK_GET:
			ret = __lock_get_internal(lt, locker, flags,
			    list[i].obj, list[i].mode, &lp);
			if (ret == 0) {
				list[i].lock = LOCK_TO_OFFSET(lt, lp);
				lt->region->nrequests++;
			}
			break;
		case DB_LOCK_PUT:
			lp = OFFSET_TO_LOCK(lt, list[i].lock);
			if (lp->holder != locker) {
				ret = DB_LOCK_NOTHELD;
				break;
			}
			list[i].mode = lp->mode;

			/* XXX Need to copy the object. ??? */
			ret = __lock_put_internal(lt, lp, 0);
			break;
		case DB_LOCK_PUT_ALL:
			/* Find the locker. */
			if ((ret = __lock_getobj(lt, locker,
			    NULL, DB_LOCK_LOCKER, &sh_locker)) != 0)
				break;

			for (lp = SH_LIST_FIRST(&sh_locker->heldby, __db_lock);
			    lp != NULL;
			    lp = SH_LIST_FIRST(&sh_locker->heldby, __db_lock)) {
				if ((ret = __lock_put_internal(lt, lp, 1)) != 0)
					break;
			}
			__lock_freeobj(lt, sh_locker);
			lt->region->nlockers--;
			break;
		case DB_LOCK_PUT_OBJ:

			/* Look up the object in the hash table. */
			HASHLOOKUP(lt->hashtab, __db_lockobj, links,
			    list[i].obj, sh_obj, lt->region->table_size,
			    __lock_ohash, __lock_cmp);
			if (sh_obj == NULL) {
				ret = EINVAL;
				break;
			}
			/*
			 * Release waiters first, because they won't cause
			 * anyone else to be awakened.  If we release the
			 * lockers first, all the waiters get awakened
			 * needlessly.
			 */
			for (lp = SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock);
			    lp != NULL;
			    lp = SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock)) {
				lt->region->nreleases += lp->refcount;
				__lock_remove_waiter(lt, sh_obj, lp,
				    DB_LSTAT_NOGRANT);
				__lock_checklocker(lt, lp, 1);
			}

			for (lp = SH_TAILQ_FIRST(&sh_obj->holders, __db_lock);
			    lp != NULL;
			    lp = SH_TAILQ_FIRST(&sh_obj->holders, __db_lock)) {

				lt->region->nreleases += lp->refcount;
				SH_LIST_REMOVE(lp, locker_links, __db_lock);
				SH_TAILQ_REMOVE(&sh_obj->holders, lp, links,
				    __db_lock);
				lp->status = DB_LSTAT_FREE;
				SH_TAILQ_INSERT_HEAD(&lt->region->free_locks,
				    lp, links, __db_lock);
			}

			/* Now free the object. */
			__lock_freeobj(lt, sh_obj);
			break;
#ifdef DEBUG
		case DB_LOCK_DUMP:
			/* Find the locker. */
			if ((ret = __lock_getobj(lt, locker,
			    NULL, DB_LOCK_LOCKER, &sh_locker)) != 0)
				break;

			for (lp = SH_LIST_FIRST(&sh_locker->heldby, __db_lock);
			    lp != NULL;
			    lp = SH_LIST_NEXT(lp, locker_links, __db_lock)) {
				__lock_printlock(lt, lp, 1);
				ret = EINVAL;
			}
			if (ret == 0) {
				__lock_freeobj(lt, sh_locker);
				lt->region->nlockers--;
			}
			break;
#endif
		default:
			ret = EINVAL;
			break;
		}
	}

	if (lt->region->need_dd && lt->region->detect != DB_LOCK_NORUN) {
		run_dd = 1;
		lt->region->need_dd = 0;
	} else
		run_dd = 0;

	UNLOCK_LOCKREGION(lt);

	if (ret == 0 && run_dd)
		lock_detect(lt, 0, lt->region->detect);

	if (elistp && ret != 0)
		*elistp = &list[i - 1];
	return (ret);
}

int
lock_get(lt, locker, flags, obj, lock_mode, lock)
	DB_LOCKTAB *lt;
	u_int32_t locker, flags;
	const DBT *obj;
	db_lockmode_t lock_mode;
	DB_LOCK *lock;
{
	struct __db_lock *lockp;
	int ret;

	/* Validate arguments. */
	if ((ret =
	    __db_fchk(lt->dbenv, "lock_get", flags, DB_LOCK_NOWAIT)) != 0)
		return (ret);

	LOCK_LOCKREGION(lt);

	ret = __lock_validate_region(lt);
	if (ret == 0 && (ret = __lock_get_internal(lt,
	    locker, flags, obj, lock_mode, &lockp)) == 0) {
		*lock = LOCK_TO_OFFSET(lt, lockp);
		lt->region->nrequests++;
	}

	UNLOCK_LOCKREGION(lt);
	return (ret);
}

int
lock_put(lt, lock)
	DB_LOCKTAB *lt;
	DB_LOCK lock;
{
	struct __db_lock *lockp;
	int ret, run_dd;

	LOCK_LOCKREGION(lt);

	if ((ret = __lock_validate_region(lt)) != 0)
		return (ret);
	else {
		lockp = OFFSET_TO_LOCK(lt, lock);
		ret = __lock_put_internal(lt, lockp, 0);
	}

	__lock_checklocker(lt, lockp, 0);

	if (lt->region->need_dd && lt->region->detect != DB_LOCK_NORUN) {
		run_dd = 1;
		lt->region->need_dd = 0;
	} else
		run_dd = 0;

	UNLOCK_LOCKREGION(lt);

	if (ret == 0 && run_dd)
		lock_detect(lt, 0, lt->region->detect);

	return (ret);
}

static int
__lock_put_internal(lt, lockp, do_all)
	DB_LOCKTAB *lt;
	struct __db_lock *lockp;
	int do_all;
{
	struct __db_lock *lp_w, *lp_h, *next_waiter;
	DB_LOCKOBJ *sh_obj;
	int state_changed;

	if (lockp->refcount == 0 || (lockp->status != DB_LSTAT_HELD &&
	    lockp->status != DB_LSTAT_WAITING) || lockp->obj == 0) {
		__db_err(lt->dbenv, "lock_put: invalid lock %lu",
		    (u_long)((u_int8_t *)lockp - (u_int8_t *)lt->region));
		return (EINVAL);
	}

	if (do_all)
		lt->region->nreleases += lockp->refcount;
	else
		lt->region->nreleases++;
	if (do_all == 0 && lockp->refcount > 1) {
		lockp->refcount--;
		return (0);
	}

	/* Get the object associated with this lock. */
	sh_obj = (DB_LOCKOBJ *)((u_int8_t *)lockp + lockp->obj);

	/* Remove lock from locker list. */
	SH_LIST_REMOVE(lockp, locker_links, __db_lock);

	/* Remove this lock from its holders/waitlist. */
	if (lockp->status != DB_LSTAT_HELD)
		__lock_remove_waiter(lt, sh_obj, lockp, DB_LSTAT_FREE);
	else
		SH_TAILQ_REMOVE(&sh_obj->holders, lockp, links, __db_lock);

	/*
	 * We need to do lock promotion.  We also need to determine if
	 * we're going to need to run the deadlock detector again.  If
	 * we release locks, and there are waiters, but no one gets promoted,
	 * then we haven't fundamentally changed the lockmgr state, so
	 * we may still have a deadlock and we have to run again.  However,
	 * if there were no waiters, or we actually promoted someone, then
	 * we are OK and we don't have to run it immediately.
	 */
	for (lp_w = SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock),
	    state_changed = lp_w == NULL;
	    lp_w != NULL;
	    lp_w = next_waiter) {
		next_waiter = SH_TAILQ_NEXT(lp_w, links, __db_lock);
		for (lp_h = SH_TAILQ_FIRST(&sh_obj->holders, __db_lock);
		    lp_h != NULL;
		    lp_h = SH_TAILQ_NEXT(lp_h, links, __db_lock)) {
			if (CONFLICTS(lt, lp_h->mode, lp_w->mode) &&
			    lp_h->holder != lp_w->holder)
				break;
		}
		if (lp_h != NULL)	/* Found a conflict. */
			break;

		/* No conflict, promote the waiting lock. */
		SH_TAILQ_REMOVE(&sh_obj->waiters, lp_w, links, __db_lock);
		lp_w->status = DB_LSTAT_PENDING;
		SH_TAILQ_INSERT_TAIL(&sh_obj->holders, lp_w, links);

		/* Wake up waiter. */
		(void)__db_mutex_unlock(&lp_w->mutex, lt->reginfo.fd);
		state_changed = 1;
	}

	/* Check if object should be reclaimed. */
	if (SH_TAILQ_FIRST(&sh_obj->holders, __db_lock) == NULL) {
		HASHREMOVE_EL(lt->hashtab, __db_lockobj,
		    links, sh_obj, lt->region->table_size, __lock_lhash);
		if (sh_obj->lockobj.size > sizeof(sh_obj->objdata))
			__db_shalloc_free(lt->mem,
			    SH_DBT_PTR(&sh_obj->lockobj));
		SH_TAILQ_INSERT_HEAD(&lt->region->free_objs, sh_obj, links,
		    __db_lockobj);
		state_changed = 1;
	}

	/* Free lock. */
	lockp->status = DB_LSTAT_FREE;
	SH_TAILQ_INSERT_HEAD(&lt->region->free_locks, lockp, links, __db_lock);

	/*
	 * If we did not promote anyone; we need to run the deadlock
	 * detector again.
	 */
	if (state_changed == 0)
		lt->region->need_dd = 1;

	return (0);
}

static int
__lock_get_internal(lt, locker, flags, obj, lock_mode, lockp)
	DB_LOCKTAB *lt;
	u_int32_t locker, flags;
	const DBT *obj;
	db_lockmode_t lock_mode;
	struct __db_lock **lockp;
{
	struct __db_lock *newl, *lp;
	DB_LOCKOBJ *sh_obj, *sh_locker;
	DB_LOCKREGION *lrp;
	size_t newl_off;
	int ihold, ret;

	ret = 0;
	/*
	 * Check that lock mode is valid.
	 */

	lrp = lt->region;
	if ((u_int32_t)lock_mode >= lrp->nmodes) {
		__db_err(lt->dbenv,
		    "lock_get: invalid lock mode %lu\n", (u_long)lock_mode);
		return (EINVAL);
	}

	/* Allocate a new lock.  Optimize for the common case of a grant. */
	if ((newl = SH_TAILQ_FIRST(&lrp->free_locks, __db_lock)) == NULL) {
		if ((ret = __lock_grow_region(lt, DB_LOCK_LOCK, 0)) != 0)
			return (ret);
		lrp = lt->region;
		newl = SH_TAILQ_FIRST(&lrp->free_locks, __db_lock);
	}
	newl_off = LOCK_TO_OFFSET(lt, newl);

	/* Optimize for common case of granting a lock. */
	SH_TAILQ_REMOVE(&lrp->free_locks, newl, links, __db_lock);

	newl->mode = lock_mode;
	newl->status = DB_LSTAT_HELD;
	newl->holder = locker;
	newl->refcount = 1;

	if ((ret = __lock_getobj(lt, 0, obj, DB_LOCK_OBJTYPE, &sh_obj)) != 0)
		return (ret);

	lrp = lt->region;			/* getobj might have grown */
	newl = OFFSET_TO_LOCK(lt, newl_off);

	/* Now make new lock point to object */
	newl->obj = SH_PTR_TO_OFF(newl, sh_obj);

	/*
	 * Now we have a lock and an object and we need to see if we should
	 * grant the lock.  We use a FIFO ordering so we can only grant a
	 * new lock if it does not conflict with anyone on the holders list
	 * OR anyone on the waiters list.  The reason that we don't grant if
	 * there's a conflict is that this can lead to starvation (a writer
	 * waiting on a popularly read item will never be granted).  The
	 * downside of this is that a waiting reader can prevent an upgrade
	 * from reader to writer, which is not uncommon.
	 *
	 * There is one exception to the no-conflict rule.  If a lock is held
	 * by the requesting locker AND the new lock does not conflict with
	 * any other holders, then we grant the lock.  The most common place
	 * this happens is when the holder has a WRITE lock and a READ lock
	 * request comes in for the same locker.  If we do not grant the read
	 * lock, then we guarantee deadlock.
	 *
	 * In case of conflict, we put the new lock on the end of the waiters
	 * list.
	 */
	ihold = 0;
	for (lp = SH_TAILQ_FIRST(&sh_obj->holders, __db_lock);
	    lp != NULL;
	    lp = SH_TAILQ_NEXT(lp, links, __db_lock)) {
		if (locker == lp->holder) {
			if (lp->mode == lock_mode &&
			    lp->status == DB_LSTAT_HELD) {
				/* Lock is held, just inc the ref count. */
				lp->refcount++;
				SH_TAILQ_INSERT_HEAD(&lrp->free_locks,
				    newl, links, __db_lock);
				*lockp = lp;
				return (0);
			} else
				ihold = 1;
		} else if (CONFLICTS(lt, lp->mode, lock_mode))
			break;
    	}

	if (lp == NULL && !ihold)
		for (lp = SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock);
		    lp != NULL;
		    lp = SH_TAILQ_NEXT(lp, links, __db_lock)) {
			if (CONFLICTS(lt, lp->mode, lock_mode) &&
			    locker != lp->holder)
				break;
		}
	if (lp == NULL)
		SH_TAILQ_INSERT_TAIL(&sh_obj->holders, newl, links);
	else if (!(flags & DB_LOCK_NOWAIT))
		SH_TAILQ_INSERT_TAIL(&sh_obj->waiters, newl, links);
	else {
		/* Free the lock and return an error. */
		newl->status = DB_LSTAT_FREE;
		SH_TAILQ_INSERT_HEAD(&lrp->free_locks, newl, links, __db_lock);
		return (DB_LOCK_NOTGRANTED);
	}

	/*
	 * This is really a blocker for the process, so initialize it
	 * set.  That way the current process will block when it tries
	 * to get it and the waking process will release it.
	 */
	(void)__db_mutex_init(&newl->mutex,
	    MUTEX_LOCK_OFFSET(lt->region, &newl->mutex));
	(void)__db_mutex_lock(&newl->mutex, lt->reginfo.fd);

	/*
	 * Now, insert the lock onto its locker's list.
	 */
	if ((ret =
	    __lock_getobj(lt, locker, NULL, DB_LOCK_LOCKER, &sh_locker)) != 0)
		return (ret);

	lrp = lt->region;
	SH_LIST_INSERT_HEAD(&sh_locker->heldby, newl, locker_links, __db_lock);

	if (lp != NULL) {
		newl->status = DB_LSTAT_WAITING;
		lrp->nconflicts++;
		/*
		 * We are about to wait; must release the region mutex.
		 * Then, when we wakeup, we need to reacquire the region
		 * mutex before continuing.
		 */
		if (lrp->detect == DB_LOCK_NORUN)
			lt->region->need_dd = 1;
		UNLOCK_LOCKREGION(lt);

		/*
		 * We are about to wait; before waiting, see if the deadlock
		 * detector should be run.
		 */
		if (lrp->detect != DB_LOCK_NORUN)
			ret = lock_detect(lt, 0, lrp->detect);

		(void)__db_mutex_lock(&newl->mutex, lt->reginfo.fd);

		LOCK_LOCKREGION(lt);
		if (newl->status != DB_LSTAT_PENDING) {
			/* Return to free list. */
			__lock_checklocker(lt, newl, 0);
			SH_TAILQ_INSERT_HEAD(&lrp->free_locks, newl, links,
			    __db_lock);
			switch (newl->status) {
				case DB_LSTAT_ABORTED:
					ret = DB_LOCK_DEADLOCK;
					break;
				case DB_LSTAT_NOGRANT:
					ret = DB_LOCK_NOTGRANTED;
					break;
				default:
					ret = EINVAL;
					break;
			}
			newl->status = DB_LSTAT_FREE;
			newl = NULL;
		} else
			newl->status = DB_LSTAT_HELD;
	}

	*lockp = newl;
	return (ret);
}

/*
 * __lock_is_locked --
 *
 * PUBLIC: int __lock_is_locked
 * PUBLIC:    __P((DB_LOCKTAB *, u_int32_t, DBT *, db_lockmode_t));
 */
int
__lock_is_locked(lt, locker, dbt, mode)
	DB_LOCKTAB *lt;
	u_int32_t locker;
	DBT *dbt;
	db_lockmode_t mode;
{
	struct __db_lock *lp;
	DB_LOCKOBJ *sh_obj;
	DB_LOCKREGION *lrp;

	lrp = lt->region;

	/* Look up the object in the hash table. */
	HASHLOOKUP(lt->hashtab, __db_lockobj, links,
	    dbt, sh_obj, lrp->table_size, __lock_ohash, __lock_cmp);
	if (sh_obj == NULL)
		return (0);

	for (lp = SH_TAILQ_FIRST(&sh_obj->holders, __db_lock);
	    lp != NULL;
	    lp = SH_TAILQ_FIRST(&sh_obj->holders, __db_lock)) {
		if (lp->holder == locker && lp->mode == mode)
			return (1);
	}

	return (0);
}

/*
 * __lock_printlock --
 *
 * PUBLIC: void __lock_printlock __P((DB_LOCKTAB *, struct __db_lock *, int));
 */
void
__lock_printlock(lt, lp, ispgno)
	DB_LOCKTAB *lt;
	struct __db_lock *lp;
	int ispgno;
{
	DB_LOCKOBJ *lockobj;
	db_pgno_t pgno;
	size_t obj;
	u_int8_t *ptr;
	const char *mode, *status;

	switch (lp->mode) {
	case DB_LOCK_IREAD:
		mode = "IREAD";
		break;
	case DB_LOCK_IWR:
		mode = "IWR";
		break;
	case DB_LOCK_IWRITE:
		mode = "IWRITE";
		break;
	case DB_LOCK_NG:
		mode = "NG";
		break;
	case DB_LOCK_READ:
		mode = "READ";
		break;
	case DB_LOCK_WRITE:
		mode = "WRITE";
		break;
	default:
		mode = "UNKNOWN";
		break;
	}
	switch (lp->status) {
	case DB_LSTAT_ABORTED:
		status = "ABORT";
		break;
	case DB_LSTAT_ERR:
		status = "ERROR";
		break;
	case DB_LSTAT_FREE:
		status = "FREE";
		break;
	case DB_LSTAT_HELD:
		status = "HELD";
		break;
	case DB_LSTAT_NOGRANT:
		status = "NONE";
		break;
	case DB_LSTAT_WAITING:
		status = "WAIT";
		break;
	case DB_LSTAT_PENDING:
		status = "PENDING";
		break;
	default:
		status = "UNKNOWN";
		break;
	}
	printf("\t%lx\t%s\t%lu\t%s\t",
	    (u_long)lp->holder, mode, (u_long)lp->refcount, status);

	lockobj = (DB_LOCKOBJ *)((u_int8_t *)lp + lp->obj);
	ptr = SH_DBT_PTR(&lockobj->lockobj);
	if (ispgno) {
		/* Assume this is a DBT lock. */
		memcpy(&pgno, ptr, sizeof(db_pgno_t));
		printf("page %lu\n", (u_long)pgno);
	} else {
		obj = (u_int8_t *)lp + lp->obj - (u_int8_t *)lt->region;
		printf("0x%lx ", (u_long)obj);
		__db_pr(ptr, lockobj->lockobj.size);
		printf("\n");
	}
}

/*
 * PUBLIC: int __lock_getobj  __P((DB_LOCKTAB *,
 * PUBLIC:     u_int32_t, const DBT *, u_int32_t type, DB_LOCKOBJ **));
 */
int
__lock_getobj(lt, locker, dbt, type, objp)
	DB_LOCKTAB *lt;
	u_int32_t locker, type;
	const DBT *dbt;
	DB_LOCKOBJ **objp;
{
	DB_LOCKREGION *lrp;
	DB_LOCKOBJ *sh_obj;
	u_int32_t obj_size;
	int ret;
	void *p, *src;

	lrp = lt->region;

	/* Look up the object in the hash table. */
	if (type == DB_LOCK_OBJTYPE) {
		HASHLOOKUP(lt->hashtab, __db_lockobj, links, dbt, sh_obj,
		    lrp->table_size, __lock_ohash, __lock_cmp);
		obj_size = dbt->size;
	} else {
		HASHLOOKUP(lt->hashtab, __db_lockobj, links, locker,
		    sh_obj, lrp->table_size, __lock_locker_hash,
		    __lock_locker_cmp);
		obj_size = sizeof(locker);
	}

	/*
	 * If we found the object, then we can just return it.  If
	 * we didn't find the object, then we need to create it.
	 */
	if (sh_obj == NULL) {
		/* Create new object and then insert it into hash table. */
		if ((sh_obj =
		    SH_TAILQ_FIRST(&lrp->free_objs, __db_lockobj)) == NULL) {
			if ((ret = __lock_grow_region(lt, DB_LOCK_OBJ, 0)) != 0)
				return (ret);
			lrp = lt->region;
			sh_obj = SH_TAILQ_FIRST(&lrp->free_objs, __db_lockobj);
		}

		/*
		 * If we can fit this object in the structure, do so instead
		 * of shalloc-ing space for it.
		 */
		if (obj_size <= sizeof(sh_obj->objdata))
			p = sh_obj->objdata;
		else
			if ((ret =
			    __db_shalloc(lt->mem, obj_size, 0, &p)) != 0) {
				if ((ret = __lock_grow_region(lt,
				    DB_LOCK_MEM, obj_size)) != 0)
					return (ret);
				lrp = lt->region;
				/* Reacquire the head of the list. */
				sh_obj = SH_TAILQ_FIRST(&lrp->free_objs,
				    __db_lockobj);
				(void)__db_shalloc(lt->mem, obj_size, 0, &p);
			}

		src = type == DB_LOCK_OBJTYPE ? dbt->data : (void *)&locker;
		memcpy(p, src, obj_size);

		sh_obj->type = type;
		SH_TAILQ_REMOVE(&lrp->free_objs, sh_obj, links, __db_lockobj);

		SH_TAILQ_INIT(&sh_obj->waiters);
		if (type == DB_LOCK_LOCKER)
			SH_LIST_INIT(&sh_obj->heldby);
		else
			SH_TAILQ_INIT(&sh_obj->holders);
		sh_obj->lockobj.size = obj_size;
		sh_obj->lockobj.off = SH_PTR_TO_OFF(&sh_obj->lockobj, p);

		HASHINSERT(lt->hashtab,
		    __db_lockobj, links, sh_obj, lrp->table_size, __lock_lhash);

		if (type == DB_LOCK_LOCKER)
			lrp->nlockers++;
	}

	*objp = sh_obj;
	return (0);
}

/*
 * Any lock on the waitlist has a process waiting for it.  Therefore, we
 * can't return the lock to the freelist immediately.  Instead, we can
 * remove the lock from the list of waiters, set the status field of the
 * lock, and then let the process waking up return the lock to the
 * free list.
 */
static void
__lock_remove_waiter(lt, sh_obj, lockp, status)
	DB_LOCKTAB *lt;
	DB_LOCKOBJ *sh_obj;
	struct __db_lock *lockp;
	db_status_t status;
{
	SH_TAILQ_REMOVE(&sh_obj->waiters, lockp, links, __db_lock);
	lockp->status = status;

	/* Wake whoever is waiting on this lock. */
	(void)__db_mutex_unlock(&lockp->mutex, lt->reginfo.fd);
}

static void
__lock_checklocker(lt, lockp, do_remove)
	DB_LOCKTAB *lt;
	struct __db_lock *lockp;
	int do_remove;
{
	DB_LOCKOBJ *sh_locker;

	if (do_remove)
		SH_LIST_REMOVE(lockp, locker_links, __db_lock);

	/* if the locker list is NULL, free up the object. */
	if (__lock_getobj(lt, lockp->holder, NULL, DB_LOCK_LOCKER, &sh_locker)
	    == 0 && SH_LIST_FIRST(&sh_locker->heldby, __db_lock) == NULL) {
		__lock_freeobj(lt, sh_locker);
		    lt->region->nlockers--;
	}
}

static void
__lock_freeobj(lt, obj)
	DB_LOCKTAB *lt;
	DB_LOCKOBJ *obj;
{
	HASHREMOVE_EL(lt->hashtab,
	    __db_lockobj, links, obj, lt->region->table_size, __lock_lhash);
	if (obj->lockobj.size > sizeof(obj->objdata))
		__db_shalloc_free(lt->mem, SH_DBT_PTR(&obj->lockobj));
	SH_TAILQ_INSERT_HEAD(&lt->region->free_objs, obj, links, __db_lockobj);
}
