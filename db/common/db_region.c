/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)db_region.c	10.46 (Sleepycat) 5/26/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "common_ext.h"

static int __db_growregion __P((REGINFO *, size_t));

/*
 * __db_rattach --
 *	Optionally create and attach to a shared memory region.
 *
 * PUBLIC: int __db_rattach __P((REGINFO *));
 */
int
__db_rattach(infop)
	REGINFO *infop;
{
	RLAYOUT *rlp, rl;
	size_t grow_region, size;
	ssize_t nr, nw;
	u_int32_t flags, mbytes, bytes;
	u_int8_t *p;
	int malloc_possible, ret, retry_cnt;

	grow_region = 0;
	malloc_possible = 1;
	ret = retry_cnt = 0;

	/* Round off the requested size to the next page boundary. */
	DB_ROUNDOFF(infop->size);

	/* Some architectures have hard limits on the maximum region size. */
#ifdef DB_REGIONSIZE_MAX
	if (infop->size > DB_REGIONSIZE_MAX) {
		__db_err(infop->dbenv, "__db_rattach: cache size too large");
		return (EINVAL);
	}
#endif

	/* Intialize the return information in the REGINFO structure. */
loop:	infop->addr = NULL;
	infop->fd = -1;
	infop->segid = INVALID_SEGID;
	if (infop->name != NULL) {
		FREES(infop->name);
		infop->name = NULL;
	}
	F_CLR(infop, REGION_CANGROW | REGION_CREATED);

#ifndef HAVE_SPINLOCKS
	/*
	 * XXX
	 * Lacking spinlocks, we must have a file descriptor for fcntl(2)
	 * locking, which implies using mmap(2) to map in a regular file.
	 * (Theoretically, we could probably get a file descriptor to lock
	 * other types of shared regions, but I don't see any reason to
	 * bother.)
	 */
	malloc_possible = 0;
#endif

#ifdef __hppa
	/*
	 * XXX
	 * HP-UX won't permit mutexes to live in anything but shared memory.
	 * Instantiate a shared region file on that architecture, regardless.
	 */
	malloc_possible = 0;
#endif
	/*
	 * If a region is truly private, malloc the memory.  That's faster
	 * than either anonymous memory or a shared file.
	 */
	if (malloc_possible && F_ISSET(infop, REGION_PRIVATE)) {
		if ((infop->addr = __db_malloc(infop->size)) == NULL)
			return (ENOMEM);

		/*
		 * It's sometimes significantly faster to page-fault in all
		 * of the region's pages before we run the application, as
		 * we can see fairly nasty side-effects when we page-fault
		 * while holding various locks, i.e., the lock takes a long
		 * time, and other threads convoy behind the lock holder.
		 */
		if (DB_GLOBAL(db_region_init))
			for (p = infop->addr;
			    p < (u_int8_t *)infop->addr + infop->size;
			    p += DB_VMPAGESIZE)
				p[0] = '\0';

		F_SET(infop, REGION_CREATED | REGION_MALLOC);
		goto region_init;
	}

	/*
	 * Get the name of the region (creating the file if a temporary file
	 * is being used).  The dbenv contains the current DB environment,
	 * including naming information.  The path argument may be a file or
	 * a directory.  If path is a directory, it must exist and file is the
	 * file name to be created inside the directory.  If path is a file,
	 * then file must be NULL.
	 */
	if ((ret = __db_appname(infop->dbenv, infop->appname, infop->path,
	    infop->file, infop->dbflags, &infop->fd, &infop->name)) != 0)
		return (ret);
	if (infop->fd != -1)
		F_SET(infop, REGION_CREATED);

	/*
	 * Try to create the file, if we have authority.  We have to make sure
	 * that multiple threads/processes attempting to simultaneously create
	 * the region are properly ordered, so we open it using DB_CREATE and
	 * DB_EXCL, so two attempts to create the region will return failure in
	 * one.
	 */
	if (infop->fd == -1 && infop->dbflags & DB_CREATE) {
		flags = infop->dbflags;
		LF_SET(DB_EXCL);
		if ((ret = __db_open(infop->name,
		    flags, flags, infop->mode, &infop->fd)) == 0)
			F_SET(infop, REGION_CREATED);
		else
			if (ret != EEXIST)
				goto errmsg;
	}

	/* If we couldn't create the file, try and open it. */
	if (infop->fd == -1) {
		flags = infop->dbflags;
		LF_CLR(DB_CREATE | DB_EXCL);
		if ((ret = __db_open(infop->name,
		    flags, flags, infop->mode, &infop->fd)) != 0)
			goto errmsg;
	}

	/*
	 * There are three cases we support:
	 *    1. Named anonymous memory (shmget(2)).
	 *    2. Unnamed anonymous memory (mmap(2): MAP_ANON/MAP_ANONYMOUS).
	 *    3. Memory backed by a regular file (mmap(2)).
	 *
	 * We instantiate a backing file in all cases, which contains at least
	 * the RLAYOUT structure, and in case #4, contains the actual region.
	 * This is necessary for a couple of reasons:
	 *
	 * First, the mpool region uses temporary files to name regions, and
	 * since you may have multiple regions in the same directory, we need
	 * a filesystem name to ensure that they don't collide.
	 *
	 * Second, applications are allowed to forcibly remove regions, even
	 * if they don't know anything about them other than the name.  If a
	 * region is backed by anonymous memory, there has to be some way for
	 * the application to find out that information, and, in some cases,
	 * determine ID information for the anonymous memory.
	 */
	if (F_ISSET(infop, REGION_CREATED)) {
		/*
		 * If we're using anonymous memory to back this region, set
		 * the flag.
		 */
		if (DB_GLOBAL(db_region_anon))
			F_SET(infop, REGION_ANONYMOUS);

		/*
		 * If we're using a regular file to back a region we created,
		 * grow it to the specified size.
		 */
		if (!DB_GLOBAL(db_region_anon) &&
		    (ret = __db_growregion(infop, infop->size)) != 0)
			goto err;
	} else {
		/*
		 * If we're joining a region, figure out what it looks like.
		 *
		 * XXX
		 * We have to figure out if the file is a regular file backing
		 * a region that we want to map into our address space, or a
		 * file with the information we need to find a shared anonymous
		 * region that we want to map into our address space.
		 *
		 * All this noise is because some systems don't have a coherent
		 * VM and buffer cache, and worse, if you mix operations on the
		 * VM and buffer cache, half the time you hang the system.
		 *
		 * There are two possibilities.  If the file is the size of an
		 * RLAYOUT structure, then we know that the real region is in
		 * shared memory, because otherwise it would be bigger.  (As
		 * the RLAYOUT structure size is smaller than a disk sector,
		 * the only way it can be this size is if deliberately written
		 * that way.)  In which case, retrieve the information we need
		 * from the RLAYOUT structure and use it to acquire the shared
		 * memory.
		 *
		 * If the structure is larger than an RLAYOUT structure, then
		 * the file is backing the shared memory region, and we use
		 * the current size of the file without reading any information
		 * from the file itself so that we don't confuse the VM.
		 *
		 * And yes, this makes me want to take somebody and kill them,
		 * but I can't think of any other solution.
		 */
		if ((ret = __db_ioinfo(infop->name,
		    infop->fd, &mbytes, &bytes, NULL)) != 0)
			goto errmsg;
		size = mbytes * MEGABYTE + bytes;

		if (size <= sizeof(RLAYOUT)) {
			/*
			 * If the size is too small, the read fails or the
			 * valid flag is incorrect, assume it's because the
			 * RLAYOUT information hasn't been written out yet,
			 * and retry.
			 */
			if (size < sizeof(RLAYOUT))
				goto retry;
			if ((ret =
			    __db_read(infop->fd, &rl, sizeof(rl), &nr)) != 0)
				goto retry;
			if (rl.valid != DB_REGIONMAGIC)
				goto retry;

			/* Copy the size, memory id and characteristics. */
			size = rl.size;
			infop->segid = rl.segid;
			if (F_ISSET(&rl, REGION_ANONYMOUS))
				F_SET(infop, REGION_ANONYMOUS);
		}

		/*
		 * If the region is larger than we think, that's okay, use the
		 * current size.  If it's smaller than we think, and we were
		 * just using the default size, that's okay, use the current
		 * size.  If it's smaller than we think and we really care,
		 * save the size and we'll catch that further down -- we can't
		 * correct it here because we have to have a lock to grow the
		 * region.
		 */
		if (infop->size > size && !F_ISSET(infop, REGION_SIZEDEF))
			grow_region = infop->size;
		infop->size = size;
	}

	/*
	 * Map the region into our address space.  If we're creating it, the
	 * underlying routines will make it the right size.
	 *
	 * There are at least two cases where we can "reasonably" fail when
	 * we attempt to map in the region.  On Windows/95, closing the last
	 * reference to a region causes it to be zeroed out.  On UNIX, when
	 * using the shmget(2) interfaces, the region will no longer exist
	 * if the system was rebooted.  In these cases, the underlying map call
	 * returns EAGAIN, and we *remove* our file and try again.  There are
	 * obvious races in doing this, but it should eventually settle down
	 * to a winner and then things should proceed normally.
	 */
	if ((ret = __db_mapregion(infop->name, infop)) != 0)
		if (ret == EAGAIN) {
			/*
			 * Pretend we created the region even if we didn't so
			 * that our error processing unlinks it.
			 */
			F_SET(infop, REGION_CREATED);
			ret = 0;
			goto retry;
		} else
			goto err;

region_init:
	/*
	 * Initialize the common region information.
	 *
	 * !!!
	 * We have to order the region creates so that two processes don't try
	 * to simultaneously create the region.  This is handled by using the
	 * DB_CREATE and DB_EXCL flags when we create the "backing" region file.
	 *
	 * We also have to order region joins so that processes joining regions
	 * never see inconsistent data.  We'd like to play permissions games
	 * with the backing file, but we can't because WNT filesystems won't
	 * open a file mode 0.
	 */
	rlp = (RLAYOUT *)infop->addr;
	if (F_ISSET(infop, REGION_CREATED)) {
		/*
		 * The process creating the region acquires a lock before it
		 * sets the valid flag.  Any processes joining the region will
		 * check the valid flag before acquiring the lock.
		 *
		 * Check the return of __db_mutex_init() and __db_mutex_lock(),
		 * even though we don't usually check elsewhere.  This is the
		 * first lock we initialize and acquire, and we have to know if
		 * it fails.  (It CAN fail, e.g., SunOS, when using fcntl(2)
		 * for locking, with an in-memory filesystem specified as the
		 * database home.)
		 */
		if ((ret = __db_mutex_init(&rlp->lock,
		    MUTEX_LOCK_OFFSET(rlp, &rlp->lock))) != 0 ||
		    (ret = __db_mutex_lock(&rlp->lock, infop->fd)) != 0)
			goto err;

		/* Initialize the remaining region information. */
		rlp->refcnt = 1;
		rlp->size = infop->size;
		db_version(&rlp->majver, &rlp->minver, &rlp->patch);
		rlp->segid = infop->segid;
		rlp->flags = 0;
		if (F_ISSET(infop, REGION_ANONYMOUS))
			F_SET(rlp, REGION_ANONYMOUS);

		/*
		 * Fill in the valid field last -- use a magic number, memory
		 * may not be zero-filled, and we want to minimize the chance
		 * for collision.
		 */
		rlp->valid = DB_REGIONMAGIC;

		/*
		 * If the region is anonymous, write the RLAYOUT information
		 * into the backing file so that future region join and unlink
		 * calls can find it.
		 *
		 * XXX
		 * We MUST do the seek before we do the write.  On Win95, while
		 * closing the last reference to an anonymous shared region
		 * doesn't discard the region, it does zero it out.  So, the
		 * REGION_CREATED may be set, but the file may have already
		 * been written and the file descriptor may be at the end of
		 * the file.
		 */
		if (F_ISSET(infop, REGION_ANONYMOUS)) {
			if ((ret = __db_seek(infop->fd, 0, 0, 0, 0, 0)) != 0)
				goto err;
			if ((ret =
			    __db_write(infop->fd, rlp, sizeof(*rlp), &nw)) != 0)
				goto err;
		}
	} else {
		/*
		 * Check the valid flag to ensure the region is initialized.
		 * If the valid flag has not been set, the mutex may not have
		 * been initialized, and an attempt to get it could lead to
		 * random behavior.
		 */
		if (rlp->valid != DB_REGIONMAGIC)
			goto retry;

		/* Get the region lock. */
		(void)__db_mutex_lock(&rlp->lock, infop->fd);

		/*
		 * We now own the region.  There are a couple of things that
		 * may have gone wrong, however.
		 *
		 * Problem #1: while we were waiting for the lock, the region
		 * was deleted.  Detected by re-checking the valid flag, since
		 * it's cleared by the delete region routines.
		 */
		if (rlp->valid != DB_REGIONMAGIC) {
			(void)__db_mutex_unlock(&rlp->lock, infop->fd);
			goto retry;
		}

		/*
		 * Problem #2: We want a bigger region than has previously been
		 * created.  Detected by checking if the region is smaller than
		 * our caller requested.  If it is, we grow the region, (which
		 * does the detach and re-attach for us).
		 */
		if (grow_region != 0 &&
		    (ret = __db_rgrow(infop, grow_region)) != 0) {
			(void)__db_mutex_unlock(&rlp->lock, infop->fd);
			goto err;
		}

		/*
		 * Problem #3: when we checked the size of the file, it was
		 * still growing as part of creation.  Detected by the fact
		 * that infop->size isn't the same size as the region.
		 */
		if (infop->size != rlp->size) {
			(void)__db_mutex_unlock(&rlp->lock, infop->fd);
			goto retry;
		}

		/* Increment the reference count. */
		++rlp->refcnt;
	}

	/* Return the region in a locked condition. */

	if (0) {
errmsg:		__db_err(infop->dbenv, "%s: %s", infop->name, strerror(ret));

err:
retry:		/* Discard the region. */
		if (infop->addr != NULL) {
			(void)__db_unmapregion(infop);
			infop->addr = NULL;
		}

		/* Discard the backing file. */
		if (infop->fd != -1) {
			(void)__db_close(infop->fd);
			infop->fd = -1;

			if (F_ISSET(infop, REGION_CREATED))
				(void)__db_unlink(infop->name);
		}

		/* Discard the name. */
		if (infop->name != NULL) {
			FREES(infop->name);
			infop->name = NULL;
		}

		/*
		 * If we had a temporary error, wait a few seconds and
		 * try again.
		 */
		if (ret == 0) {
			if (++retry_cnt <= 3) {
				__db_sleep(retry_cnt * 2, 0);
				goto loop;
			}
			ret = EAGAIN;
		}
	}

	/*
	 * XXX
	 * HP-UX won't permit mutexes to live in anything but shared memory.
	 * Instantiate a shared region file on that architecture, regardless.
	 *
	 * XXX
	 * There's a problem in cleaning this up on application exit, or on
	 * application failure.  If an application opens a database without
	 * an environment, we create a temporary backing mpool region for it.
	 * That region is marked REGION_PRIVATE, but as HP-UX won't permit
	 * mutexes to live in anything but shared memory, we instantiate a
	 * real file plus a memory region of some form.  If the application
	 * crashes, the necessary information to delete the backing file and
	 * any system region (e.g., the shmget(2) segment ID) is no longer
	 * available.  We can't completely fix the problem, but we try.
	 *
	 * The underlying UNIX __db_mapregion() code preferentially uses the
	 * mmap(2) interface with the MAP_ANON/MAP_ANONYMOUS flags for regions
	 * that are marked REGION_PRIVATE.  This means that we normally aren't
	 * holding any system resources when we get here, in which case we can
	 * delete the backing file.  This results in a short race, from the
	 * __db_open() call above to here.
	 *
	 * If, for some reason, we are holding system resources when we get
	 * here, we don't have any choice -- we can't delete the backing file
	 * because we may need it to detach from the resources.  Set the
	 * REGION_LASTDETACH flag, so that we do all necessary cleanup when
	 * the application closes the region.
	 */
	if (F_ISSET(infop, REGION_PRIVATE) && !F_ISSET(infop, REGION_MALLOC))
		if (F_ISSET(infop, REGION_HOLDINGSYS))
			F_SET(infop, REGION_LASTDETACH);
		else {
			F_SET(infop, REGION_REMOVED);
			F_CLR(infop, REGION_CANGROW);

			(void)__db_close(infop->fd);
			(void)__db_unlink(infop->name);
		}

	return (ret);
}

/*
 * __db_rdetach --
 *	De-attach from a shared memory region.
 *
 * PUBLIC: int __db_rdetach __P((REGINFO *));
 */
int
__db_rdetach(infop)
	REGINFO *infop;
{
	RLAYOUT *rlp;
	int detach, ret, t_ret;

	ret = 0;

	/*
	 * If the region was removed when it was created, no further action
	 * is required.
	 */
	if (F_ISSET(infop, REGION_REMOVED))
		goto done;
	/*
	 * If the region was created in memory returned by malloc, the only
	 * action required is freeing the memory.
	 */
	if (F_ISSET(infop, REGION_MALLOC)) {
		__db_free(infop->addr);
		goto done;
	}

	/* Otherwise, attach to the region and optionally delete it. */
	rlp = infop->addr;

	/* Get the lock. */
	(void)__db_mutex_lock(&rlp->lock, infop->fd);

	/* Decrement the reference count. */
	if (rlp->refcnt == 0)
		__db_err(infop->dbenv,
		    "region rdetach: reference count went to zero!");
	else
		--rlp->refcnt;

	/*
	 * If we're going to remove the region, clear the valid flag so
	 * that any region join that's blocked waiting for us will know
	 * what happened.
	 */
	detach = 0;
	if (F_ISSET(infop, REGION_LASTDETACH))
		if (rlp->refcnt == 0) {
			detach = 1;
			rlp->valid = 0;
		} else
			ret = EBUSY;

	/* Release the lock. */
	(void)__db_mutex_unlock(&rlp->lock, infop->fd);

	/* Close the backing file descriptor. */
	(void)__db_close(infop->fd);
	infop->fd = -1;

	/* Discard our mapping of the region. */
	if ((t_ret = __db_unmapregion(infop)) != 0 && ret == 0)
		ret = t_ret;

	/* Discard the region itself. */
	if (detach) {
		if ((t_ret =
		    __db_unlinkregion(infop->name, infop) != 0) && ret == 0)
			ret = t_ret;
		if ((t_ret = __db_unlink(infop->name) != 0) && ret == 0)
			ret = t_ret;
	}

done:	/* Discard the name. */
	if (infop->name != NULL) {
		FREES(infop->name);
		infop->name = NULL;
	}

	return (ret);
}

/*
 * __db_runlink --
 *	Remove a region.
 *
 * PUBLIC: int __db_runlink __P((REGINFO *, int));
 */
int
__db_runlink(infop, force)
	REGINFO *infop;
	int force;
{
	RLAYOUT rl, *rlp;
	size_t size;
	ssize_t nr;
	u_int32_t mbytes, bytes;
	int fd, ret, t_ret;
	char *name;

	/*
	 * XXX
	 * We assume that we've created a new REGINFO structure for this
	 * call, not used one that was already initialized.  Regardless,
	 * if anyone is planning to use it after we're done, they're going
	 * to be sorely disappointed.
	 *
	 * If force isn't set, we attach to the region, set a flag to delete
	 * the region on last close, and let the region delete code do the
	 * work.
	 */
	if (!force) {
		if ((ret = __db_rattach(infop)) != 0)
			return (ret);

		rlp = (RLAYOUT *)infop->addr;
		(void)__db_mutex_unlock(&rlp->lock, infop->fd);

		F_SET(infop, REGION_LASTDETACH);

		return (__db_rdetach(infop));
	}

	/*
	 * Otherwise, we don't want to attach to the region.  We may have been
	 * called to clean up if a process died leaving a region locked and/or
	 * corrupted, which could cause the attach to hang.
	 */
	if ((ret = __db_appname(infop->dbenv, infop->appname,
	    infop->path, infop->file, infop->dbflags, NULL, &name)) != 0)
		return (ret);

	/*
	 * An underlying file is created for all regions other than private
	 * (REGION_PRIVATE) ones, regardless of whether or not it's used to
	 * back the region.  If that file doesn't exist, we're done.
	 */
	if (__db_exists(name, NULL) != 0) {
		FREES(name);
		return (0);
	}

	/*
	 * See the comments in __db_rattach -- figure out if this is a regular
	 * file backing a region or if it's a regular file with information
	 * about a region.
	 */
	if ((ret = __db_open(name, DB_RDONLY, DB_RDONLY, 0, &fd)) != 0)
		goto errmsg;
	if ((ret = __db_ioinfo(name, fd, &mbytes, &bytes, NULL)) != 0)
		goto errmsg;
	size = mbytes * MEGABYTE + bytes;

	if (size <= sizeof(RLAYOUT)) {
		if ((ret = __db_read(fd, &rl, sizeof(rl), &nr)) != 0)
			goto errmsg;
		if (rl.valid != DB_REGIONMAGIC) {
			__db_err(infop->dbenv,
			    "%s: illegal region magic number", name);
			ret = EINVAL;
			goto err;
		}

		/* Set the size, memory id and characteristics. */
		infop->size = rl.size;
		infop->segid = rl.segid;
		if (F_ISSET(&rl, REGION_ANONYMOUS))
			F_SET(infop, REGION_ANONYMOUS);
	} else {
		infop->size = size;
		infop->segid = INVALID_SEGID;
	}

	/* Remove the underlying region. */
	ret = __db_unlinkregion(name, infop);

	/*
	 * Unlink the backing file.  Close the open file descriptor first,
	 * because some architectures (e.g., Win32) won't unlink a file if
	 * open file descriptors remain.
	 */
	(void)__db_close(fd);
	if ((t_ret = __db_unlink(name)) != 0 && ret == 0)
		ret = t_ret;

	if (0) {
errmsg:		__db_err(infop->dbenv, "%s: %s", name, strerror(ret));
err:		(void)__db_close(fd);
	}

	FREES(name);
	return (ret);
}

/*
 * __db_rgrow --
 *	Extend a region.
 *
 * PUBLIC: int __db_rgrow __P((REGINFO *, size_t));
 */
int
__db_rgrow(infop, new_size)
	REGINFO *infop;
	size_t new_size;
{
	RLAYOUT *rlp;
	size_t increment;
	int ret;

	/*
	 * !!!
	 * This routine MUST be called with the region already locked.
	 */

	/* The underlying routines have flagged if this region can grow. */
	if (!F_ISSET(infop, REGION_CANGROW))
		return (EINVAL);

	/*
	 * Round off the requested size to the next page boundary, and
	 * determine the additional space required.
	 */
	rlp = (RLAYOUT *)infop->addr;
	DB_ROUNDOFF(new_size);
	increment = new_size - rlp->size;

	if ((ret = __db_growregion(infop, increment)) != 0)
		return (ret);

	/* Update the on-disk region size. */
	rlp->size = new_size;

	/* Detach from and reattach to the region. */
	return (__db_rreattach(infop, new_size));
}

/*
 * __db_growregion --
 *	Grow a shared memory region.
 */
static int
__db_growregion(infop, increment)
	REGINFO *infop;
	size_t increment;
{
	db_pgno_t pages;
	size_t i;
	ssize_t nr, nw;
	u_int32_t relative;
	int ret;
	char buf[DB_VMPAGESIZE];

	/* Seek to the end of the region. */
	if ((ret = __db_seek(infop->fd, 0, 0, 0, 0, SEEK_END)) != 0)
		goto err;

	/* Write nuls to the new bytes. */
	memset(buf, 0, sizeof(buf));

	/*
	 * Some systems require that all of the bytes of the region be
	 * written before it can be mapped and accessed randomly, and
	 * other systems don't zero out the pages.
	 */
	if (__db_mapinit())
		/* Extend the region by writing each new page. */
		for (i = 0; i < increment; i += DB_VMPAGESIZE) {
			if ((ret =
			    __db_write(infop->fd, buf, sizeof(buf), &nw)) != 0)
				goto err;
			if (nw != sizeof(buf))
				goto eio;
		}
	else {
		/*
		 * Extend the region by writing the last page.  If the region
		 * is >4Gb, increment may be larger than the maximum possible
		 * seek "relative" argument, as it's an unsigned 32-bit value.
		 * Break the offset into pages of 1MB each so that we don't
		 * overflow (2^20 + 2^32 is bigger than any memory I expect
		 * to see for awhile).
		 */
		pages = (increment - DB_VMPAGESIZE) / MEGABYTE;
		relative = (increment - DB_VMPAGESIZE) % MEGABYTE;
		if ((ret = __db_seek(infop->fd,
		    MEGABYTE, pages, relative, 0, SEEK_CUR)) != 0)
			goto err;
		if ((ret = __db_write(infop->fd, buf, sizeof(buf), &nw)) != 0)
			goto err;
		if (nw != sizeof(buf))
			goto eio;

		/*
		 * It's sometimes significantly faster to page-fault in all
		 * of the region's pages before we run the application, as
		 * we can see fairly nasty side-effects when we page-fault
		 * while holding various locks, i.e., the lock takes a long
		 * time, and other threads convoy behind the lock holder.
		 */
		if (DB_GLOBAL(db_region_init)) {
			pages = increment / MEGABYTE;
			relative = increment % MEGABYTE;
			if ((ret = __db_seek(infop->fd,
			    MEGABYTE, pages, relative, 1, SEEK_END)) != 0)
				goto err;

			/* Read a byte from each page. */
			for (i = 0; i < increment; i += DB_VMPAGESIZE) {
				if ((ret =
				    __db_read(infop->fd, buf, 1, &nr)) != 0)
					goto err;
				if (nr != 1)
					goto eio;
				if ((ret = __db_seek(infop->fd,
				    0, 0, DB_VMPAGESIZE - 1, 0, SEEK_CUR)) != 0)
					goto err;
			}
		}
	}
	return (0);

eio:	ret = EIO;
err:	__db_err(infop->dbenv, "region grow: %s", strerror(ret));
	return (ret);
}

/*
 * __db_rreattach --
 *	Detach from and reattach to a region.
 *
 * PUBLIC: int __db_rreattach __P((REGINFO *, size_t));
 */
int
__db_rreattach(infop, new_size)
	REGINFO *infop;
	size_t new_size;
{
	int ret;

#ifdef DIAGNOSTIC
	if (infop->name == NULL) {
		__db_err(infop->dbenv, "__db_rreattach: name was NULL");
		return (EINVAL);
	}
#endif
	/*
	 * If we're growing an already mapped region, we have to unmap it
	 * and get it back.  We have it locked, so nobody else can get in,
	 * which makes it fairly straight-forward to do, as everybody else
	 * is going to block while we do the unmap/remap.  NB: if we fail
	 * to get it back, the pooch is genuinely screwed, because we can
	 * never release the lock we're holding.
	 *
	 * Detach from the region.  We have to do this first so architectures
	 * that don't permit a file to be mapped into different places in the
	 * address space simultaneously, e.g., HP's PaRisc, will work.
	 */
	if ((ret = __db_unmapregion(infop)) != 0)
		return (ret);

	/* Update the caller's REGINFO size to the new map size. */
	infop->size = new_size;

	/* Attach to the region. */
	ret = __db_mapregion(infop->name, infop);

	return (ret);
}
