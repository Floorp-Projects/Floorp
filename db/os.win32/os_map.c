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

#include "db_int.h"

/* Return if we're Windows/95 or Windows/NT. */
#define	IS_WNT	((GetVersion() & 0x80000000) == 0)

static int __os_map __P((char *, int, size_t, int, int, int, int, void **));
static int __os_unique_name __P((char *, int, char *));

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
	/*
	 * XXX
	 * Windows/95 supports anonymous regions backed by the paging file.
	 *
	 * On Windows/NT, if you call CreateFileMapping with a file descriptor,
	 * you get proper sharing, but without a file descriptor (i.e. request
	 * to use a paging file), changes are not seen by any other processes
	 * that open that piece of named memory.
	 */
	if (IS_WNT && need_names)
		return (ENXIO);

	return (0);
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
	 * XXX
	 * Windows/95 leaves file contents uninitialized, bless its heart.
	 *
	 * Windows/NT initializes them.
	 */
	return (!IS_WNT);
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
	int ret;

	/* If the user replaces the map call, call through their interface. */
	if (__db_jump.j_map != NULL)
		return (__db_jump.j_map(path, infop->fd, infop->size,
			1, F_ISSET(infop, REGION_ANONYMOUS), 0, &infop->addr));

	/* CreateFileMapping regions that aren't anonymous can grow. */
	if (!F_ISSET(infop, REGION_ANONYMOUS))
		F_SET(infop, REGION_CANGROW);

	if ((ret = __os_map(path, infop->fd, infop->size,
	    1, F_ISSET(infop, REGION_ANONYMOUS),
	    0, F_ISSET(infop, REGION_CREATED), &infop->addr)) != 0)
		return (ret);

	return (0);
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
	if (__db_jump.j_unmap != NULL)
		return (__db_jump.j_unmap(infop->addr, infop->size));

	return (!UnmapViewOfFile(infop->addr) ? errno : 0);
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
	COMPQUIET(infop, NULL);

	if (__db_jump.j_runlink != NULL)
		return (__db_jump.j_runlink(name));

	return (0);
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

	return (__os_map(path, fd, len, 0, 0, is_rdonly, 0, addr));
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

	return (!UnmapViewOfFile(addr) ? errno : 0);
}

/*
 * __os_unique_name --
 *	Create a unique identifying name from a pathname (may be absolute or
 *	relative) and/or a file descriptor.
 *
 *	The name returned must be unique (different files map to different
 *	names), and repeatable (same files, map to same names).  It's not so
 *	easy to do by name.  Should handle not only:
 *
 *		foo.bar  ==  ./foo.bar  ==  c:/whatever_path/foo.bar
 *
 *	but also understand that:
 *
 *		foo.bar  ==  Foo.Bar	(FAT file system)
 *		foo.bar  !=  Foo.Bar	(NTFS)
 *
 *	The best solution is to use the identifying number in the file
 *	information structure (similar to UNIX inode #).
 */
static int
__os_unique_name(orig_path, fd, result_path)
	char *orig_path, *result_path;
	int fd;
{
	BY_HANDLE_FILE_INFORMATION fileinfo;

	if (!GetFileInformationByHandle(
	    (HANDLE)_get_osfhandle(fd), &fileinfo))
		return (errno);
	(void)sprintf(result_path, "%ld.%ld.%ld",
	    fileinfo.dwVolumeSerialNumber,
	    fileinfo.nFileIndexHigh, fileinfo.nFileIndexLow);
	return (0);
}

/*
 * __os_map --
 *	The mmap(2) function for Windows.
 */
static int
__os_map(path, fd, len, is_region, is_anonymous, is_rdonly, is_create, addr)
	char *path;
	int fd, is_region, is_anonymous, is_rdonly, is_create;
	size_t len;
	void **addr;
{
	HANDLE hMemory;
	RLAYOUT *rlp;
	int ret;
	void *pMemory;
	char shmem_name[MAXPATHLEN];

	ret = 0;

	if (is_region && is_anonymous) {
		/* Get a matching name in the paging file namespace */
		(void)strcpy(shmem_name, "__db_shmem.");
		if ((ret = __os_unique_name(path, fd,
		    &shmem_name[strlen(shmem_name)])) != 0)
			return (ret);
	}

	/*
	 * XXX
	 * DB: We have not implemented copy-on-write here.
	 *
	 * XXX
	 * DB: This code will fail if the library is ever compiled on a 64-bit
	 * machine.
	 */
	hMemory = CreateFileMapping(
		is_region && is_anonymous ?
		    (HANDLE)0xFFFFFFFF : (HANDLE)_get_osfhandle(fd),
		0,
		is_rdonly ? PAGE_READONLY : PAGE_READWRITE,
		0, len,
		is_region && is_anonymous ? shmem_name : NULL);

	if (hMemory == NULL)
		return (errno);

	pMemory = MapViewOfFile(hMemory,
	    (is_rdonly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS), 0, 0, len);

	CloseHandle(hMemory);

	if (is_region) {
		/*
		 * Windows/95 zeroes anonymous memory regions at last close.
		 * This means that the backing file can exist and reference
		 * the region, but the region itself is no longer initialized.
		 * We handle this by returning EAGAIN to the caller, who will
		 * attempt to remove the backing file and start all over.
		 */
		rlp = (RLAYOUT *)pMemory;
		if (rlp->valid == 0 && !is_create) {
			(void)UnmapViewOfFile(pMemory);
			pMemory = NULL;
			ret = EAGAIN;
		}
	}

	*addr = pMemory;
	return (ret);
}
