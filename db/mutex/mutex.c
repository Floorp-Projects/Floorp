/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)mutex.c	10.48 (Sleepycat) 5/23/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"

#ifdef HAVE_SPINLOCKS

#ifdef HAVE_FUNC_AIX
#define	TSL_INIT(x)
#define	TSL_SET(x)	(!_check_lock(x, 0, 1))
#define	TSL_UNSET(x)	_clear_lock(x, 0)
#endif

#ifdef HAVE_ASSEM_MC68020_GCC
#include "68020.gcc"
#endif

#if defined(HAVE_FUNC_MSEM)
/*
 * XXX
 * Should we not use MSEM_IF_NOWAIT and let the system block for us?
 * I've no idea if this will block all threads in the process or not.
 */
#define	TSL_INIT(x)	(msem_init(x, MSEM_UNLOCKED) == NULL)
#define	TSL_INIT_ERROR	1
#define	TSL_SET(x)	(!msem_lock(x, MSEM_IF_NOWAIT))
#define	TSL_UNSET(x)	msem_unlock(x, 0)
#endif

#ifdef HAVE_FUNC_RELIANT
#define	TSL_INIT(x)	initspin(x, 1)
#define	TSL_SET(x)	(cspinlock(x) == 0)
#define	TSL_UNSET(x)	spinunlock(x)
#endif

#ifdef HAVE_FUNC_SGI
#define	TSL_INIT(x)	(init_lock(x) != 0)
#define	TSL_INIT_ERROR	1
#define	TSL_SET(x)	(!acquire_lock(x))
#define	TSL_UNSET(x)	release_lock(x)
#endif

#ifdef HAVE_FUNC_SOLARIS
/*
 * Semaphore calls don't work on Solaris 5.5.
 *
 * #define	TSL_INIT(x)	(sema_init(x, 1, USYNC_PROCESS, NULL) != 0)
 * #define	TSL_INIT_ERROR	1
 * #define	TSL_SET(x)	(sema_wait(x) == 0)
 * #define	TSL_UNSET(x)	sema_post(x)
 */
#define	TSL_INIT(x)
#define	TSL_SET(x)	(_lock_try(x))
#define	TSL_UNSET(x)	_lock_clear(x)
#endif

#ifdef HAVE_ASSEM_PARISC_GCC
#include "parisc.gcc"
#endif

#ifdef HAVE_ASSEM_SCO_CC
#include "sco.cc"
#endif

#ifdef HAVE_ASSEM_SPARC_GCC
#include "sparc.gcc"
#endif

#ifdef HAVE_ASSEM_UTS4_CC
#define TSL_INIT(x)
#define TSL_SET(x)	(!uts_lock(x, 1))
#define TSL_UNSET(x)	(*(x) = 0)
#endif

#ifdef HAVE_ASSEM_X86_GCC
#include "x86.gcc"
#endif

#ifdef WIN16
/* Win16 spinlocks are simple because we cannot possibly be preempted. */
#define	TSL_INIT(tsl)
#define	TSL_SET(tsl)	(*(tsl) = 1)
#define	TSL_UNSET(tsl)	(*(tsl) = 0)
#endif

#if defined(_WIN32)
/*
 * XXX
 * DBDB this needs to be byte-aligned!!
 */
#define	TSL_INIT(tsl)
#define	TSL_SET(tsl)	(!InterlockedExchange((PLONG)tsl, 1))
#define	TSL_UNSET(tsl)	(*(tsl) = 0)
#endif

#endif /* HAVE_SPINLOCKS */

/*
 * __db_mutex_init --
 *	Initialize a DB mutex structure.
 *
 * PUBLIC: int __db_mutex_init __P((db_mutex_t *, u_int32_t));
 */
int
__db_mutex_init(mp, off)
	db_mutex_t *mp;
	u_int32_t off;
{
#ifdef DIAGNOSTIC
	if ((ALIGNTYPE)mp & (MUTEX_ALIGNMENT - 1)) {
		(void)fprintf(stderr,
		    "MUTEX ERROR: mutex NOT %d-byte aligned!\n",
		    MUTEX_ALIGNMENT);
		abort();
	}
#endif
	memset(mp, 0, sizeof(db_mutex_t));

#ifdef HAVE_SPINLOCKS
	COMPQUIET(off, 0);

#ifdef TSL_INIT_ERROR
	if (TSL_INIT(&mp->tsl_resource))
		return (errno);
#else
	TSL_INIT(&mp->tsl_resource);
#endif
	mp->spins = __os_spin();
#else
	mp->off = off;
#endif
	return (0);
}

#define	MS(n)		((n) * 1000)	/* Milliseconds to micro-seconds. */
#define	SECOND		(MS(1000))	/* A second's worth of micro-seconds. */

/*
 * __db_mutex_lock
 *	Lock on a mutex, logically blocking if necessary.
 *
 * PUBLIC: int __db_mutex_lock __P((db_mutex_t *, int));
 */
int
__db_mutex_lock(mp, fd)
	db_mutex_t *mp;
	int fd;
{
	u_long usecs;
#ifdef HAVE_SPINLOCKS
	int nspins;
#else
	struct flock k_lock;
	pid_t mypid;
	int locked;
#endif

	if (!DB_GLOBAL(db_mutexlocks))
		return (0);

#ifdef HAVE_SPINLOCKS
	COMPQUIET(fd, 0);

	for (usecs = MS(10);;) {
		/* Try and acquire the uncontested resource lock for N spins. */
		for (nspins = mp->spins; nspins > 0; --nspins)
			if (TSL_SET(&mp->tsl_resource)) {
#ifdef DIAGNOSTIC
				if (mp->pid != 0) {
					(void)fprintf(stderr,
		    "MUTEX ERROR: __db_mutex_lock: lock currently locked\n");
					abort();
				}
				mp->pid = getpid();
#endif
				if (usecs == MS(10))
					++mp->mutex_set_nowait;
				else
					++mp->mutex_set_wait;
				return (0);
			}

		/* Yield the processor; wait 10ms initially, up to 1 second. */
		if (__db_yield == NULL || __db_yield() != 0) {
			(void)__db_sleep(0, usecs);
			if ((usecs <<= 1) > SECOND)
				usecs = SECOND;
		}
	}
	/* NOTREACHED */

#else /* !HAVE_SPINLOCKS */

	/* Initialize the lock. */
	k_lock.l_whence = SEEK_SET;
	k_lock.l_start = mp->off;
	k_lock.l_len = 1;

	for (locked = 0, mypid = getpid();;) {
		/*
		 * Wait for the lock to become available; wait 10ms initially,
		 * up to 1 second.
		 */
		for (usecs = MS(10); mp->pid != 0;)
			if (__db_yield == NULL || __db_yield() != 0) {
				(void)__db_sleep(0, usecs);
				if ((usecs <<= 1) > SECOND)
					usecs = SECOND;
			}

		/* Acquire an exclusive kernel lock. */
		k_lock.l_type = F_WRLCK;
		if (fcntl(fd, F_SETLKW, &k_lock))
			return (errno);

		/* If the resource tsl is still available, it's ours. */
		if (mp->pid == 0) {
			locked = 1;
			mp->pid = mypid;
		}

		/* Release the kernel lock. */
		k_lock.l_type = F_UNLCK;
		if (fcntl(fd, F_SETLK, &k_lock))
			return (errno);

		/*
		 * If we got the resource tsl we're done.
		 *
		 * !!!
		 * We can't check to see if the lock is ours, because we may
		 * be trying to block ourselves in the lock manager, and so
		 * the holder of the lock that's preventing us from getting
		 * the lock may be us!  (Seriously.)
		 */
		if (locked)
			break;
	}
	return (0);
#endif /* !HAVE_SPINLOCKS */
}

/*
 * __db_mutex_unlock --
 *	Release a lock.
 *
 * PUBLIC: int __db_mutex_unlock __P((db_mutex_t *, int));
 */
int
__db_mutex_unlock(mp, fd)
	db_mutex_t *mp;
	int fd;
{
	if (!DB_GLOBAL(db_mutexlocks))
		return (0);

#ifdef DIAGNOSTIC
	if (mp->pid == 0) {
		(void)fprintf(stderr,
	    "MUTEX ERROR: __db_mutex_unlock: lock already unlocked\n");
		abort();
	}
#endif

#ifdef HAVE_SPINLOCKS
	COMPQUIET(fd, 0);

#ifdef DIAGNOSTIC
	mp->pid = 0;
#endif

	/* Release the resource tsl. */
	TSL_UNSET(&mp->tsl_resource);
#else
	/*
	 * Release the resource tsl.  We don't have to acquire any locks
	 * because processes trying to acquire the lock are checking for
	 * a pid of 0, not a specific value.
	 */
	mp->pid = 0;
#endif
	return (0);
}
