/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)lock_region.c	10.15 (Sleepycat) 6/2/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_shash.h"
#include "lock.h"
#include "common_ext.h"

static u_int32_t __lock_count_locks __P((DB_LOCKREGION *));
static u_int32_t __lock_count_objs __P((DB_LOCKREGION *));
static void	 __lock_dump_locker __P((DB_LOCKTAB *, DB_LOCKOBJ *, FILE *));
static void	 __lock_dump_object __P((DB_LOCKTAB *, DB_LOCKOBJ *, FILE *));
static char	*__lock_dump_status __P((db_status_t));
static void	 __lock_reset_region __P((DB_LOCKTAB *));
static int	 __lock_tabinit __P((DB_ENV *, DB_LOCKREGION *));

int
lock_open(path, flags, mode, dbenv, ltp)
	const char *path;
	u_int32_t flags;
	int mode;
	DB_ENV *dbenv;
	DB_LOCKTAB **ltp;
{
	DB_LOCKTAB *lt;
	u_int32_t lock_modes, maxlocks, regflags;
	int ret;

	/* Validate arguments. */
#ifdef HAVE_SPINLOCKS
#define	OKFLAGS	(DB_CREATE | DB_THREAD)
#else
#define	OKFLAGS	(DB_CREATE)
#endif
	if ((ret = __db_fchk(dbenv, "lock_open", flags, OKFLAGS)) != 0)
		return (ret);

	/* Create the lock table structure. */
	if ((lt = (DB_LOCKTAB *)__db_calloc(1, sizeof(DB_LOCKTAB))) == NULL) {
		__db_err(dbenv, "%s", strerror(ENOMEM));
		return (ENOMEM);
	}
	lt->dbenv = dbenv;

	/* Grab the values that we need to compute the region size. */
	lock_modes = DB_LOCK_RW_N;
	maxlocks = DB_LOCK_DEFAULT_N;
	regflags = REGION_SIZEDEF;
	if (dbenv != NULL) {
		if (dbenv->lk_modes != 0) {
			lock_modes = dbenv->lk_modes;
			regflags = 0;
		}
		if (dbenv->lk_max != 0) {
			maxlocks = dbenv->lk_max;
			regflags = 0;
		}
	}

	/* Join/create the lock region. */
	lt->reginfo.dbenv = dbenv;
	lt->reginfo.appname = DB_APP_NONE;
	if (path == NULL)
		lt->reginfo.path = NULL;
	else
		if ((lt->reginfo.path = (char *)__db_strdup(path)) == NULL)
			goto err;
	lt->reginfo.file = DB_DEFAULT_LOCK_FILE;
	lt->reginfo.mode = mode;
	lt->reginfo.size =
	    LOCK_REGION_SIZE(lock_modes, maxlocks, __db_tablesize(maxlocks));
	lt->reginfo.dbflags = flags;
	lt->reginfo.addr = NULL;
	lt->reginfo.fd = -1;
	lt->reginfo.flags = regflags;

	if ((ret = __db_rattach(&lt->reginfo)) != 0)
		goto err;

	/* Now set up the pointer to the region. */
	lt->region = lt->reginfo.addr;

	/* Initialize the region if we created it. */
	if (F_ISSET(&lt->reginfo, REGION_CREATED)) {
		lt->region->maxlocks = maxlocks;
		lt->region->nmodes = lock_modes;
		if ((ret = __lock_tabinit(dbenv, lt->region)) != 0)
			goto err;
	} else {
		/* Check for an unexpected region. */
		if (lt->region->magic != DB_LOCKMAGIC) {
			__db_err(dbenv,
			    "lock_open: %s: bad magic number", path);
			ret = EINVAL;
			goto err;
		}
	}

	/* Check for automatic deadlock detection. */
	if (dbenv != NULL && dbenv->lk_detect != DB_LOCK_NORUN) {
		if (lt->region->detect != DB_LOCK_NORUN &&
		    dbenv->lk_detect != DB_LOCK_DEFAULT &&
		    lt->region->detect != dbenv->lk_detect) {
			__db_err(dbenv,
		    "lock_open: incompatible deadlock detector mode");
			ret = EINVAL;
			goto err;
		}
		if (lt->region->detect == DB_LOCK_NORUN)
			lt->region->detect = dbenv->lk_detect;
	}

	/* Set up remaining pointers into region. */
	lt->conflicts = (u_int8_t *)lt->region + sizeof(DB_LOCKREGION);
	lt->hashtab =
	    (DB_HASHTAB *)((u_int8_t *)lt->region + lt->region->hash_off);
	lt->mem = (void *)((u_int8_t *)lt->region + lt->region->mem_off);

	UNLOCK_LOCKREGION(lt);
	*ltp = lt;
	return (0);

err:	if (lt->reginfo.addr != NULL) {
		UNLOCK_LOCKREGION(lt);
		(void)__db_rdetach(&lt->reginfo);
		if (F_ISSET(&lt->reginfo, REGION_CREATED))
			(void)lock_unlink(path, 1, dbenv);
	}

	if (lt->reginfo.path != NULL)
		FREES(lt->reginfo.path);
	FREE(lt, sizeof(*lt));
	return (ret);
}

/*
 * __lock_tabinit --
 *	Initialize the lock region.
 */
static int
__lock_tabinit(dbenv, lrp)
	DB_ENV *dbenv;
	DB_LOCKREGION *lrp;
{
	struct __db_lock *lp;
	struct lock_header *tq_head;
	struct obj_header *obj_head;
	DB_LOCKOBJ *op;
	u_int32_t i, nelements;
	const u_int8_t *conflicts;
	u_int8_t *curaddr;

	conflicts = dbenv == NULL || dbenv->lk_conflicts == NULL ?
	    db_rw_conflicts : dbenv->lk_conflicts;

	lrp->table_size = __db_tablesize(lrp->maxlocks);
	lrp->magic = DB_LOCKMAGIC;
	lrp->version = DB_LOCKVERSION;
	lrp->id = 0;
	/*
	 * These fields (lrp->maxlocks, lrp->nmodes) are initialized
	 * in the caller, since we had to grab those values to size
	 * the region.
	 */
	lrp->need_dd = 0;
	lrp->detect = DB_LOCK_NORUN;
	lrp->numobjs = lrp->maxlocks;
	lrp->nlockers = 0;
	lrp->mem_bytes = ALIGN(STRING_SIZE(lrp->maxlocks), sizeof(size_t));
	lrp->increment = lrp->hdr.size / 2;
	lrp->nconflicts = 0;
	lrp->nrequests = 0;
	lrp->nreleases = 0;
	lrp->ndeadlocks = 0;

	/*
	 * As we write the region, we've got to maintain the alignment
	 * for the structures that follow each chunk.  This information
	 * ends up being encapsulated both in here as well as in the
	 * lock.h file for the XXX_SIZE macros.
	 */
	/* Initialize conflict matrix. */
	curaddr = (u_int8_t *)lrp + sizeof(DB_LOCKREGION);
	memcpy(curaddr, conflicts, lrp->nmodes * lrp->nmodes);
	curaddr += lrp->nmodes * lrp->nmodes;

	/*
	 * Initialize hash table.
	 */
	curaddr = (u_int8_t *)ALIGNP(curaddr, LOCK_HASH_ALIGN);
	lrp->hash_off = curaddr - (u_int8_t *)lrp;
	nelements = lrp->table_size;
	__db_hashinit(curaddr, nelements);
	curaddr += nelements * sizeof(DB_HASHTAB);

	/*
	 * Initialize locks onto a free list. Since locks contains mutexes,
	 * we need to make sure that each lock is aligned on a MUTEX_ALIGNMENT
	 * boundary.
	 */
	curaddr = (u_int8_t *)ALIGNP(curaddr, MUTEX_ALIGNMENT);
	tq_head = &lrp->free_locks;
	SH_TAILQ_INIT(tq_head);

	for (i = 0; i++ < lrp->maxlocks;
	    curaddr += ALIGN(sizeof(struct __db_lock), MUTEX_ALIGNMENT)) {
		lp = (struct __db_lock *)curaddr;
		lp->status = DB_LSTAT_FREE;
		SH_TAILQ_INSERT_HEAD(tq_head, lp, links, __db_lock);
	}

	/* Initialize objects onto a free list.  */
	obj_head = &lrp->free_objs;
	SH_TAILQ_INIT(obj_head);

	for (i = 0; i++ < lrp->maxlocks; curaddr += sizeof(DB_LOCKOBJ)) {
		op = (DB_LOCKOBJ *)curaddr;
		SH_TAILQ_INSERT_HEAD(obj_head, op, links, __db_lockobj);
	}

	/*
	 * Initialize the string space; as for all shared memory allocation
	 * regions, this requires size_t alignment, since we store the
	 * lengths of malloc'd areas in the area.
	 */
	curaddr = (u_int8_t *)ALIGNP(curaddr, sizeof(size_t));
	lrp->mem_off = curaddr - (u_int8_t *)lrp;
	__db_shalloc_init(curaddr, lrp->mem_bytes);
	return (0);
}

int
lock_close(lt)
	DB_LOCKTAB *lt;
{
	int ret;

	if ((ret = __db_rdetach(&lt->reginfo)) != 0)
		return (ret);

	if (lt->reginfo.path != NULL)
		FREES(lt->reginfo.path);
	FREE(lt, sizeof(*lt));

	return (0);
}

int
lock_unlink(path, force, dbenv)
	const char *path;
	int force;
	DB_ENV *dbenv;
{
	REGINFO reginfo;
	int ret;

	memset(&reginfo, 0, sizeof(reginfo));
	reginfo.dbenv = dbenv;
	reginfo.appname = DB_APP_NONE;
	if (path != NULL && (reginfo.path = (char *)__db_strdup(path)) == NULL)
		return (ENOMEM);
	reginfo.file = DB_DEFAULT_LOCK_FILE;
	ret = __db_runlink(&reginfo, force);
	if (reginfo.path != NULL)
		FREES(reginfo.path);
	return (ret);
}

/*
 * __lock_validate_region --
 *	Called at every interface to verify if the region has changed size,
 *	and if so, to remap the region in and reset the process' pointers.
 *
 * PUBLIC: int __lock_validate_region __P((DB_LOCKTAB *));
 */
int
__lock_validate_region(lt)
	DB_LOCKTAB *lt;
{
	int ret;

	if (lt->reginfo.size == lt->region->hdr.size)
		return (0);

	/* Detach/reattach the region. */
	if ((ret = __db_rreattach(&lt->reginfo, lt->region->hdr.size)) != 0)
		return (ret);

	/* Reset region information. */
	lt->region = lt->reginfo.addr;
	__lock_reset_region(lt);

	return (0);
}

/*
 * __lock_grow_region --
 *	We have run out of space; time to grow the region.
 *
 * PUBLIC: int __lock_grow_region __P((DB_LOCKTAB *, int, size_t));
 */
int
__lock_grow_region(lt, which, howmuch)
	DB_LOCKTAB *lt;
	int which;
	size_t howmuch;
{
	struct __db_lock *newl;
	struct lock_header *lock_head;
	struct obj_header *obj_head;
	DB_LOCKOBJ *op;
	DB_LOCKREGION *lrp;
	float lock_ratio, obj_ratio;
	size_t incr, oldsize, used, usedmem;
	u_int32_t i, newlocks, newmem, newobjs, usedlocks, usedobjs;
	u_int8_t *curaddr;
	int ret;

	lrp = lt->region;
	oldsize = lrp->hdr.size;
	incr = lrp->increment;

	/* Figure out how much of each sort of space we have. */
	usedmem = lrp->mem_bytes - __db_shalloc_count(lt->mem);
	usedobjs = lrp->numobjs - __lock_count_objs(lrp);
	usedlocks = lrp->maxlocks - __lock_count_locks(lrp);

	/*
	 * Figure out what fraction of the used space belongs to each
	 * different type of "thing" in the region.  Then partition the
	 * new space up according to this ratio.
	 */
	used = usedmem +
	    usedlocks * ALIGN(sizeof(struct __db_lock), MUTEX_ALIGNMENT) +
	    usedobjs * sizeof(DB_LOCKOBJ);

	lock_ratio = usedlocks *
	    ALIGN(sizeof(struct __db_lock), MUTEX_ALIGNMENT) / (float)used;
	obj_ratio = usedobjs * sizeof(DB_LOCKOBJ) / (float)used;

	newlocks = (u_int32_t)(lock_ratio *
	    incr / ALIGN(sizeof(struct __db_lock), MUTEX_ALIGNMENT));
	newobjs = (u_int32_t)(obj_ratio * incr / sizeof(DB_LOCKOBJ));
	newmem = incr -
	    (newobjs * sizeof(DB_LOCKOBJ) +
	    newlocks * ALIGN(sizeof(struct __db_lock), MUTEX_ALIGNMENT));

	/*
	 * Make sure we allocate enough memory for the object being
	 * requested.
	 */
	switch (which) {
	case DB_LOCK_LOCK:
		if (newlocks == 0) {
			newlocks = 10;
			incr += newlocks * sizeof(struct __db_lock);
		}
		break;
	case DB_LOCK_OBJ:
		if (newobjs == 0) {
			newobjs = 10;
			incr += newobjs * sizeof(DB_LOCKOBJ);
		}
		break;
	case DB_LOCK_MEM:
		if (newmem < howmuch * 2) {
			incr += howmuch * 2 - newmem;
			newmem = howmuch * 2;
		}
		break;
	}

	newmem += ALIGN(incr, sizeof(size_t)) - incr;
	incr = ALIGN(incr, sizeof(size_t));

	/*
	 * Since we are going to be allocating locks at the beginning of the
	 * new chunk, we need to make sure that the chunk is MUTEX_ALIGNMENT
	 * aligned.  We did not guarantee this when we created the region, so
	 * we may need to pad the old region by extra bytes to ensure this
	 * alignment.
	 */
	incr += ALIGN(oldsize, MUTEX_ALIGNMENT) - oldsize;

	__db_err(lt->dbenv,
	    "Growing lock region: %lu locks %lu objs %lu bytes",
	    (u_long)newlocks, (u_long)newobjs, (u_long)newmem);

	if ((ret = __db_rgrow(&lt->reginfo, oldsize + incr)) != 0)
		return (ret);
	lt->region = lt->reginfo.addr;
	__lock_reset_region(lt);

	/* Update region parameters. */
	lrp = lt->region;
	lrp->increment = incr << 1;
	lrp->maxlocks += newlocks;
	lrp->numobjs += newobjs;
	lrp->mem_bytes += newmem;

	curaddr = (u_int8_t *)lrp + oldsize;
	curaddr = (u_int8_t *)ALIGNP(curaddr, MUTEX_ALIGNMENT);

	/* Put new locks onto the free list. */
	lock_head = &lrp->free_locks;
	for (i = 0; i++ < newlocks;
	    curaddr += ALIGN(sizeof(struct __db_lock), MUTEX_ALIGNMENT)) {
		newl = (struct __db_lock *)curaddr;
		SH_TAILQ_INSERT_HEAD(lock_head, newl, links, __db_lock);
	}

	/* Put new objects onto the free list.  */
	obj_head = &lrp->free_objs;
	for (i = 0; i++ < newobjs; curaddr += sizeof(DB_LOCKOBJ)) {
		op = (DB_LOCKOBJ *)curaddr;
		SH_TAILQ_INSERT_HEAD(obj_head, op, links, __db_lockobj);
	}

	*((size_t *)curaddr) = newmem - sizeof(size_t);
	curaddr += sizeof(size_t);
	__db_shalloc_free(lt->mem, curaddr);

	return (0);
}

static void
__lock_reset_region(lt)
	DB_LOCKTAB *lt;
{
	lt->conflicts = (u_int8_t *)lt->region + sizeof(DB_LOCKREGION);
	lt->hashtab =
	    (DB_HASHTAB *)((u_int8_t *)lt->region + lt->region->hash_off);
	lt->mem = (void *)((u_int8_t *)lt->region + lt->region->mem_off);
}

/*
 * lock_stat --
 *	Return LOCK statistics.
 */
int
lock_stat(lt, gspp, db_malloc)
	DB_LOCKTAB *lt;
	DB_LOCK_STAT **gspp;
	void *(*db_malloc) __P((size_t));
{
	DB_LOCKREGION *rp;

	*gspp = NULL;

	if ((*gspp = db_malloc == NULL ?
	    (DB_LOCK_STAT *)__db_malloc(sizeof(**gspp)) :
	    (DB_LOCK_STAT *)db_malloc(sizeof(**gspp))) == NULL)
		return (ENOMEM);

	/* Copy out the global statistics. */
	LOCK_LOCKREGION(lt);

	rp = lt->region;
	(*gspp)->st_magic = rp->magic;
	(*gspp)->st_version = rp->version;
	(*gspp)->st_maxlocks = rp->maxlocks;
	(*gspp)->st_nmodes = rp->nmodes;
	(*gspp)->st_numobjs = rp->numobjs;
	(*gspp)->st_nlockers = rp->nlockers;
	(*gspp)->st_nconflicts = rp->nconflicts;
	(*gspp)->st_nrequests = rp->nrequests;
	(*gspp)->st_nreleases = rp->nreleases;
	(*gspp)->st_ndeadlocks = rp->ndeadlocks;
	(*gspp)->st_region_nowait = rp->hdr.lock.mutex_set_nowait;
	(*gspp)->st_region_wait = rp->hdr.lock.mutex_set_wait;
	(*gspp)->st_refcnt = rp->hdr.refcnt;
	(*gspp)->st_regsize = rp->hdr.size;

	UNLOCK_LOCKREGION(lt);

	return (0);
}

static u_int32_t
__lock_count_locks(lrp)
	DB_LOCKREGION *lrp;
{
	struct __db_lock *newl;
	u_int32_t count;

	count = 0;
	for (newl = SH_TAILQ_FIRST(&lrp->free_locks, __db_lock);
	    newl != NULL;
	    newl = SH_TAILQ_NEXT(newl, links, __db_lock))
		count++;

	return (count);
}

static u_int32_t
__lock_count_objs(lrp)
	DB_LOCKREGION *lrp;
{
	DB_LOCKOBJ *obj;
	u_int32_t count;

	count = 0;
	for (obj = SH_TAILQ_FIRST(&lrp->free_objs, __db_lockobj);
	    obj != NULL;
	    obj = SH_TAILQ_NEXT(obj, links, __db_lockobj))
		count++;

	return (count);
}

#define	LOCK_DUMP_CONF		0x001		/* Conflict matrix. */
#define	LOCK_DUMP_FREE		0x002		/* Display lock free list. */
#define	LOCK_DUMP_LOCKERS	0x004		/* Display lockers. */
#define	LOCK_DUMP_MEM		0x008		/* Display region memory. */
#define	LOCK_DUMP_OBJECTS	0x010		/* Display objects. */
#define	LOCK_DUMP_ALL		0x01f		/* Display all. */

/*
 * __lock_dump_region --
 *
 * PUBLIC: void __lock_dump_region __P((DB_LOCKTAB *, char *, FILE *));
 */
void
__lock_dump_region(lt, area, fp)
	DB_LOCKTAB *lt;
	char *area;
	FILE *fp;
{
	struct __db_lock *lp;
	DB_LOCKOBJ *op;
	DB_LOCKREGION *lrp;
	u_int32_t flags, i, j;
	int label;

	/* Make it easy to call from the debugger. */
	if (fp == NULL)
		fp = stderr;

	for (flags = 0; *area != '\0'; ++area)
		switch (*area) {
		case 'A':
			LF_SET(LOCK_DUMP_ALL);
			break;
		case 'c':
			LF_SET(LOCK_DUMP_CONF);
			break;
		case 'f':
			LF_SET(LOCK_DUMP_FREE);
			break;
		case 'l':
			LF_SET(LOCK_DUMP_LOCKERS);
			break;
		case 'm':
			LF_SET(LOCK_DUMP_MEM);
			break;
		case 'o':
			LF_SET(LOCK_DUMP_OBJECTS);
			break;
		}

	lrp = lt->region;

	fprintf(fp, "%s\nLock region parameters\n", DB_LINE);
	fprintf(fp, "%s: %lu, %s: %lu, %s: %lu, %s: %lu\n%s: %lu, %s: %lu\n",
	    "table size", (u_long)lrp->table_size,
	    "hash_off", (u_long)lrp->hash_off,
	    "increment", (u_long)lrp->increment,
	    "mem_off", (u_long)lrp->mem_off,
	    "mem_bytes", (u_long)lrp->mem_bytes,
	    "need_dd", (u_long)lrp->need_dd);

	if (LF_ISSET(LOCK_DUMP_CONF)) {
		fprintf(fp, "\n%s\nConflict matrix\n", DB_LINE);
		for (i = 0; i < lrp->nmodes; i++) {
			for (j = 0; j < lrp->nmodes; j++)
				fprintf(fp, "%lu\t",
				    (u_long)lt->conflicts[i * lrp->nmodes + j]);
			fprintf(fp, "\n");
		}
	}

	if (LF_ISSET(LOCK_DUMP_LOCKERS | LOCK_DUMP_OBJECTS)) {
		fprintf(fp, "%s\nLock hash buckets\n", DB_LINE);
		for (i = 0; i < lrp->table_size; i++) {
			label = 1;
			for (op = SH_TAILQ_FIRST(&lt->hashtab[i], __db_lockobj);
			    op != NULL;
			    op = SH_TAILQ_NEXT(op, links, __db_lockobj)) {
				if (LF_ISSET(LOCK_DUMP_LOCKERS) &&
				    op->type == DB_LOCK_LOCKER) {
					if (label) {
						fprintf(fp,
						    "Bucket %lu:\n", (u_long)i);
						label = 0;
					}
					__lock_dump_locker(lt, op, fp);
				}
				if (LF_ISSET(LOCK_DUMP_OBJECTS) &&
				    op->type == DB_LOCK_OBJTYPE) {
					if (label) {
						fprintf(fp,
						    "Bucket %lu:\n", (u_long)i);
						label = 0;
					}
					__lock_dump_object(lt, op, fp);
				}
			}
		}
	}

	if (LF_ISSET(LOCK_DUMP_FREE)) {
		fprintf(fp, "%s\nLock free list\n", DB_LINE);
		for (lp = SH_TAILQ_FIRST(&lrp->free_locks, __db_lock);
		    lp != NULL;
		    lp = SH_TAILQ_NEXT(lp, links, __db_lock))
			fprintf(fp, "0x%x: %lu\t%lu\t%s\t0x%x\n", (u_int)lp,
			    (u_long)lp->holder, (u_long)lp->mode,
			    __lock_dump_status(lp->status), (u_int)lp->obj);

		fprintf(fp, "%s\nObject free list\n", DB_LINE);
		for (op = SH_TAILQ_FIRST(&lrp->free_objs, __db_lockobj);
		    op != NULL;
		    op = SH_TAILQ_NEXT(op, links, __db_lockobj))
			fprintf(fp, "0x%x\n", (u_int)op);
	}

	if (LF_ISSET(LOCK_DUMP_MEM))
		__db_shalloc_dump(lt->mem, fp);
}

static void
__lock_dump_locker(lt, op, fp)
	DB_LOCKTAB *lt;
	DB_LOCKOBJ *op;
	FILE *fp;
{
	struct __db_lock *lp;
	u_int32_t locker;
	void *ptr;

	ptr = SH_DBT_PTR(&op->lockobj);
	memcpy(&locker, ptr, sizeof(u_int32_t));
	fprintf(fp, "L %lx", (u_long)locker);

	lp = SH_LIST_FIRST(&op->heldby, __db_lock);
	if (lp == NULL) {
		fprintf(fp, "\n");
		return;
	}
	for (; lp != NULL; lp = SH_LIST_NEXT(lp, locker_links, __db_lock))
		__lock_printlock(lt, lp, 0);
}

static void
__lock_dump_object(lt, op, fp)
	DB_LOCKTAB *lt;
	DB_LOCKOBJ *op;
	FILE *fp;
{
	struct __db_lock *lp;
	u_int32_t j;
	u_int8_t *ptr;
	u_int ch;

	ptr = SH_DBT_PTR(&op->lockobj);
	for (j = 0; j < op->lockobj.size; ptr++, j++) {
		ch = *ptr;
		fprintf(fp, isprint(ch) ? "%c" : "\\%o", ch);
	}
	fprintf(fp, "\n");

	fprintf(fp, "H:");
	for (lp =
	    SH_TAILQ_FIRST(&op->holders, __db_lock);
	    lp != NULL;
	    lp = SH_TAILQ_NEXT(lp, links, __db_lock))
		__lock_printlock(lt, lp, 0);
	lp = SH_TAILQ_FIRST(&op->waiters, __db_lock);
	if (lp != NULL) {
		fprintf(fp, "\nW:");
		for (; lp != NULL; lp = SH_TAILQ_NEXT(lp, links, __db_lock))
			__lock_printlock(lt, lp, 0);
	}
}

static char *
__lock_dump_status(status)
	db_status_t status;
{
	switch (status) {
	case DB_LSTAT_ABORTED:
		return ("aborted");
	case DB_LSTAT_ERR:
		return ("err");
	case DB_LSTAT_FREE:
		return ("free");
	case DB_LSTAT_HELD:
		return ("held");
	case DB_LSTAT_NOGRANT:
		return ("nogrant");
	case DB_LSTAT_PENDING:
		return ("pending");
	case DB_LSTAT_WAITING:
		return ("waiting");
	}
	return ("unknown status");
}
