/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_map.c	10.19 (Sleepycat) 5/3/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif

#ifdef HAVE_SHMGET
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "common_ext.h"

#ifdef HAVE_MMAP
static int __os_map __P((char *, int, size_t, int, int, int, void **));
#endif
#ifdef HAVE_SHMGET
static int __os_shmget __P((char *, REGINFO *));
#endif

/*
 * __db_mapanon_ok --
 *	Return if this OS can support anonymous memory regions.
 *
 * PUBLIC: int __db_mapanon_ok __P((int));
 */
int
__db_mapanon_ok(need_names)
	int need_names;
{
	int ret;

	ret = EINVAL;

	/*
	 * If we don't have spinlocks, we have to have a file descriptor
	 * for fcntl(2) locking, which implies using mmap(2) to map in a
	 * regular file.  Theoretically, we could probably find ways to
	 * get a file descriptor to lock other types of shared regions,
	 * but I don't see any reason to do so.
	 *
	 * If need_names is set, the application wants to share anonymous
	 * memory among multiple processes, so we have to have a way to
	 * name it.  This requires shmget(2), on UNIX systems.
	 */
#ifdef HAVE_SPINLOCKS
#ifdef HAVE_SHMGET
	ret = 0;
#endif
#ifdef HAVE_MMAP
#ifdef MAP_ANON
	if (!need_names)
		ret = 0;
#endif
#ifdef MAP_ANONYMOUS
	if (!need_names)
		ret = 0;
#endif
#else
	COMPQUIET(need_names, 0);
#endif /* HAVE_MMAP */
#endif /* HAVE_SPINLOCKS */

	return (ret);
}

/*
 * __db_mapinit --
 *	Return if shared regions need to be initialized.
 *
 * PUBLIC: int __db_mapinit __P((void));
 */
int
__db_mapinit()
{
	/*
	 * Historically, some systems required that all of the bytes of the
	 * region be written before it could be mmapped and accessed randomly.
	 * We have the option of setting REGION_INIT_NEEDED at configuration
	 * time if we're running on one of those systems.
	 */
#ifdef REGION_INIT_NEEDED
	return (1);
#else
	return (0);
#endif
}

/*
 * __db_mapregion --
 *	Attach to a shared memory region.
 *
 * PUBLIC: int __db_mapregion __P((char *, REGINFO *));
 */
int
__db_mapregion(path, infop)
	char *path;
	REGINFO *infop;
{
	int called, ret;

	called = 0;
	ret = EINVAL;

	/* If the user replaces the map call, call through their interface. */
	if (__db_jump.j_map != NULL) {
		F_SET(infop, REGION_HOLDINGSYS);
		return (__db_jump.j_map(path, infop->fd, infop->size,
		    1, F_ISSET(infop, REGION_ANONYMOUS), 0, &infop->addr));
	}

	if (F_ISSET(infop, REGION_ANONYMOUS)) {
		/*
		 * !!!
		 * If we're creating anonymous regions:
		 *
		 * If it's private, we use mmap(2).  The problem with using
		 * shmget(2) is that we may be creating a region of which the
		 * application isn't aware, and if the application crashes
		 * we'll have no way to remove the system resources for the
		 * region.
		 *
		 * If it's not private, we use the shmget(2) interface if it's
		 * available, because it allows us to name anonymous memory.
		 * If shmget(2) isn't available, use the mmap(2) calls.
		 *
		 * In the case of anonymous memory, using mmap(2) means the
		 * memory isn't named and only the single process and its
		 * threads can access the region.
		 */
#ifdef	HAVE_MMAP
#ifdef	MAP_ANON
#define	HAVE_MMAP_ANONYMOUS	1
#else
#ifdef	MAP_ANONYMOUS
#define	HAVE_MMAP_ANONYMOUS	1
#endif
#endif
#endif
#ifdef HAVE_MMAP_ANONYMOUS
		if (!called && F_ISSET(infop, REGION_PRIVATE)) {
			called = 1;
			ret = __os_map(path,
			    infop->fd, infop->size, 1, 1, 0, &infop->addr);
		}
#endif
#ifdef HAVE_SHMGET
		if (!called) {
			called = 1;
			ret = __os_shmget(path, infop);
		}
#endif
#ifdef HAVE_MMAP
		/*
		 * If we're trying to join an unnamed anonymous region, fail --
		 * that's not possible.
		 */
		if (!called) {
			called = 1;

			if (!F_ISSET(infop, REGION_CREATED)) {
				__db_err(infop->dbenv,
			    "cannot join region in unnamed anonymous memory");
				return (EINVAL);
			}

			ret = __os_map(path,
			    infop->fd, infop->size, 1, 1, 0, &infop->addr);
		}
#endif
	} else {
		/*
		 * !!!
		 * If we're creating normal regions, we use the mmap(2)
		 * interface if it's available because it's POSIX 1003.1
		 * standard and we trust it more than we do shmget(2).
		 */
#ifdef HAVE_MMAP
		if (!called) {
			called = 1;

			/* Mmap(2) regions that aren't anonymous can grow. */
			F_SET(infop, REGION_CANGROW);

			ret = __os_map(path,
			    infop->fd, infop->size, 1, 0, 0, &infop->addr);
		}
#endif
#ifdef HAVE_SHMGET
		if (!called) {
			called = 1;
			ret = __os_shmget(path, infop);
		}
#endif
	}
	return (ret);
}

/*
 * __db_unmapregion --
 *	Detach from the shared memory region.
 *
 * PUBLIC: int __db_unmapregion __P((REGINFO *));
 */
int
__db_unmapregion(infop)
	REGINFO *infop;
{
	int called, ret;

	called = 0;
	ret = EINVAL;

	if (__db_jump.j_unmap != NULL)
		return (__db_jump.j_unmap(infop->addr, infop->size));

#ifdef HAVE_SHMGET
	if (infop->segid != INVALID_SEGID) {
		called = 1;
		ret = shmdt(infop->addr) ? errno : 0;
	}
#endif
#ifdef HAVE_MMAP
	if (!called) {
		called = 1;
		ret = munmap(infop->addr, infop->size) ? errno : 0;
	}
#endif
	return (ret);
}

/*
 * __db_unlinkregion --
 *	Remove the shared memory region.
 *
 * PUBLIC: int __db_unlinkregion __P((char *, REGINFO *));
 */
int
__db_unlinkregion(name, infop)
	char *name;
	REGINFO *infop;
{
	int called, ret;

	called = 0;
	ret = EINVAL;

	if (__db_jump.j_runlink != NULL)
		return (__db_jump.j_runlink(name));

#ifdef HAVE_SHMGET
	if (infop->segid != INVALID_SEGID) {
		called = 1;
		ret = shmctl(infop->segid, IPC_RMID, NULL) ? errno : 0;
	}
#else
	COMPQUIET(infop, NULL);
#endif
#ifdef HAVE_MMAP
	if (!called) {
		called = 1;
		ret = 0;
	}
#endif
	return (ret);
}

/*
 * __db_mapfile --
 *	Map in a shared memory file.
 *
 * PUBLIC: int __db_mapfile __P((char *, int, size_t, int, void **));
 */
int
__db_mapfile(path, fd, len, is_rdonly, addr)
	char *path;
	int fd, is_rdonly;
	size_t len;
	void **addr;
{
	if (__db_jump.j_map != NULL)
		return (__db_jump.j_map(path, fd, len, 0, 0, is_rdonly, addr));

#ifdef HAVE_MMAP
	return (__os_map(path, fd, len, 0, 0, is_rdonly, addr));
#else
	return (EINVAL);
#endif
}

/*
 * __db_unmapfile --
 *	Unmap the shared memory file.
 *
 * PUBLIC: int __db_unmapfile __P((void *, size_t));
 */
int
__db_unmapfile(addr, len)
	void *addr;
	size_t len;
{
	if (__db_jump.j_unmap != NULL)
		return (__db_jump.j_unmap(addr, len));

#ifdef HAVE_MMAP
	return (munmap(addr, len) ? errno : 0);
#else
	return (EINVAL);
#endif
}

#ifdef HAVE_MMAP
/*
 * __os_map --
 *	Call the mmap(2) function.
 */
static int
__os_map(path, fd, len, is_region, is_anonymous, is_rdonly, addr)
	char *path;
	int fd, is_region, is_anonymous, is_rdonly;
	size_t len;
	void **addr;
{
	void *p;
	int flags, prot;

	COMPQUIET(path, NULL);

	/*
	 * If it's read-only, it's private, and if it's not, it's shared.
	 * Don't bother with an additional parameter.
	 */
	flags = is_rdonly ? MAP_PRIVATE : MAP_SHARED;

	if (is_region && is_anonymous) {
		/*
		 * BSD derived systems use MAP_ANON; Digital Unix and HP/UX
		 * use MAP_ANONYMOUS.
		 */
#ifdef MAP_ANON
		flags |= MAP_ANON;
#endif
#ifdef MAP_ANONYMOUS
		flags |= MAP_ANONYMOUS;
#endif
		fd = -1;
	}
#ifdef MAP_FILE
	if (!is_region || !is_anonymous) {
		/*
		 * Historically, MAP_FILE was required for mapping regular
		 * files, even though it was the default.  Some systems have
		 * it, some don't, some that have it set it to 0.
		 */
		flags |= MAP_FILE;
	}
#endif

	/*
	 * I know of no systems that implement the flag to tell the system
	 * that the region contains semaphores, but it's not an unreasonable
	 * thing to do, and has been part of the design since forever.  I
	 * don't think anyone will object, but don't set it for read-only
	 * files, it doesn't make sense.
	 */
#ifdef MAP_HASSEMAPHORE
	if (!is_rdonly)
		flags |= MAP_HASSEMAPHORE;
#endif

	prot = PROT_READ | (is_rdonly ? 0 : PROT_WRITE);

	/* MAP_FAILED was not defined in early mmap implementations. */
#ifndef MAP_FAILED
#define	MAP_FAILED	-1
#endif
	if ((p =
	    mmap(NULL, len, prot, flags, fd, (off_t)0)) == (void *)MAP_FAILED)
		return (errno);

	*addr = p;
	return (0);
}
#endif

#ifdef HAVE_SHMGET
/*
 * __os_shmget --
 *	Call the shmget(2) family of functions.
 */
static int
__os_shmget(path, infop)
	REGINFO *infop;
	char *path;
{
	key_t key;
	int shmflg;

	if (F_ISSET(infop, REGION_CREATED)) {
		/*
		 * The return key from ftok(3) is not guaranteed to be unique.
		 * The nice thing about the shmget(2) interface is that it
		 * allows you to name anonymous pieces of memory.  The evil
		 * thing about it is that the name space is separate from the
		 * filesystem.
		 */
#ifdef __hp3000s900
		{char mpe_path[MAXPATHLEN];
		/*
		 * MPE ftok() is broken as of 5.5pp4.  If the file path does
		 * not start with '/' or '.', then ftok() tries to interpret
		 * the file path in MPE syntax instead of POSIX HFS syntax.
		 * The workaround is to prepend "./" to these paths.  See HP
		 * SR 5003416081 for details.
		 */
		if (*path != '/' && *path != '.') {
			if (strlen(path) + strlen("./") + 1 > sizeof(mpe_path))
				return (ENAMETOOLONG);
			mpe_path[0] = '.';
			mpe_path[1] = '/';
			(void)strcpy(mpe_path + 2, path);
			path = mpe_path;
		}
		}
#endif
		if ((key = ftok(path, 1)) == (key_t)-1)
			return (errno);

		shmflg = IPC_CREAT | 0600;
		if ((infop->segid = shmget(key, infop->size, shmflg)) == -1)
			return (errno);
	}

	if ((infop->addr = shmat(infop->segid, NULL, 0)) == (void *)-1) {
		/*
		 * If we're trying to join the region and failing, assume
		 * that there was a reboot and the region no longer exists.
		 */
		if (!F_ISSET(infop, REGION_CREATED))
			errno = EAGAIN;
		return (errno);
	}

	F_SET(infop, REGION_HOLDINGSYS);
	return (0);
}
#endif
