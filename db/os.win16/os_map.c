/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998
 *	Sleepycat Software.  All rights reserved.
 *
 * This code is derived from software contributed to Sleepycat Software by
 * Frederick G.M. Roeber of Netscape Communications Corp.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_map.c	10.6 (Sleepycat) 4/25/98";
#endif /* not lint */

#include "db_int.h"
#include "common_ext.h"

/*
 * DB version 2 uses memory-mapped files for two things: 1) faster access of
 * read-only databases, and 2) shared memory for process synchronization and
 * locking.  The code carefully does not mix the two uses.  The first-case
 * uses are actually written such that memory-mapping isn't really required
 * -- it's merely a convenience -- so we don't have to worry much about it.
 * In the second case, it's solely used as a shared memory mechanism, so that's
 * all we have to replace.
 *
 * The mechanism I use for shared memory on Win16 is actually fairly simple.
 * All memory in Win16 is shared, and a DLL can allocate memory and keep notes.
 * This implementation of Win16, at least, is entirely done as a DLL.  So I
 * merely have to allocate memory, remember the "filename" for that memory,
 * and issue small-integer segment IDs which index the DLL's list of these
 * shared-memory segments.  Subsequent opens are checked against the list of
 * already open segments.
 */

typedef struct {
	void *segment;			/* Segment address. */
	u_int32_t size;			/* Segment size. */
	char *name;			/* Segment name. */
} os_segdata_t;

static os_segdata_t *__os_segdata;	/* Segment table. */
static int __os_segdata_size;		/* Segment table size. */

#define	OS_SEGDATA_STARTING_SIZE 16
#define	OS_SEGDATA_INCREMENT	 16

static int __os_map __P((char *, REGINFO *));
static int __os_segdata_allocate __P((const char *, REGINFO *));
static int __os_segdata_find_byname __P((const char *, REGINFO *));
static int __os_segdata_new __P((int *));
static int __os_segdata_release __P((int));

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
	COMPQUIET(need_names, 0);

	/*
	 * All Win16 regions are in named anonymous shared memory,
	 * so the answer is always yes.
	 */
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
	/* Win16 regions do not need to be fully written. */
	return (0);
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
	/* If the user replaces the map call, call through their interface. */
	if (__db_jump.j_map != NULL)
		return (__db_jump.j_map(path, infop->fd, infop->size,
		    1, F_ISSET(infop, REGION_ANONYMOUS), 0, &infop->addr));

	/*
	 * XXX
	 * We could actually grow regions without a lot of difficulty, but
	 * since the maximum region is 64K, it's unclear to me it's worth
	 * the effort.
	 */
	return (__os_map(path, infop));
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

	/* There's no mapping to discard, we're done. */
	return (0);
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

	return (__os_segdata_release(infop->segid));
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

	/* We cannot map in regular files in Win16. */
	return (EINVAL);
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

	/* We cannot map in regular files in Win16. */
	return (EINVAL);
}

/*
 * __os_map --
 *	Create/find a shared region.
 */
static int
__os_map(path, infop)
	char *path;
	REGINFO *infop;
{
	int ret;

	/* Try to find an already existing segment. */
	if (__os_segdata_find_byname(path, infop) == 0)
		return (0);

	/*
	 * If we're trying to join the region and failing, assume
	 * that there was a reboot and the region no longer exists.
	 */
	if (!F_ISSET(infop, REGION_CREATED))
		return (EAGAIN);

	/* Create a new segment. */
	if ((ret = __os_segdata_allocate(path, infop)) != 0)
		return (ret);

	return (0);
}

/*
 * __os_segdata_init --
 *	Initialises the library's table of shared memory segments.  Called
 *	(once!) from the DLL main routine.
 *
 * PUBLIC: int __os_segdata_init __P((void));
 */
int
__os_segdata_init()
{
	if (__os_segdata != NULL)
		return (EEXIST);

	__os_segdata_size = OS_SEGDATA_STARTING_SIZE;
	if ((__os_segdata = (os_segdata_t *)
	    __db_calloc(__os_segdata_size, sizeof(os_segdata_t))) == NULL)
		return (ENOMEM);

	return (0);
}

/*
 * __os_segdata_destroy --
 *	Destroys the library's table of shared memory segments.  It also
 *	frees all linked data: the segments themselves, and their names.
 *	Called (once!) from the DLL shutdown routine.
 *
 * PUBLIC: int __os_segdata_destroy __P((void));
 */
int
__os_segdata_destroy()
{
	os_segdata_t *p;
	int i;

	if (__os_segdata == NULL)
		return (0);

	for (i = 0; i < __os_segdata_size; i++) {
		p = &__os_segdata[i];
		if (p->name != NULL) {
			FREES(p->name);
			p->name = NULL;
		}
		if (p->segment != NULL) {
			FREE(p->segment, p->size);
			p->segment = NULL;
		}
		p->size = 0;
	}

	FREE(__os_segdata, __os_segdata_size * sizeof(os_segdata_t));
	__os_segdata = NULL;
	__os_segdata_size = 0;

	return (0);
}

/*
 * __os_segdata_allocate --
 *	Creates a new segment of the specified size, optionally with the
 *	specified name.
 */
static int
__os_segdata_allocate(name, infop)
	const char *name;
	REGINFO *infop;
{
	os_segdata_t *p;
	int segid, ret;

	if ((ret = __os_segdata_new(&segid)) != 0)
		return (ret);

	p = &__os_segdata[segid];
	if ((p->segment = (void *)__db_calloc(infop->size, 1)) == NULL)
		return (ENOMEM);
	if ((p->name = __db_strdup(name)) == NULL) {
		FREE(p->segment, infop->size);
		p->segment = NULL;
		return (ENOMEM);
	}
	p->size = infop->size;

	infop->addr = p->segment;
	infop->segid = segid;

	return (0);
}

/*
 * __os_segdata_new --
 *	Finds a new segdata slot.  Does not initialise it, so the fd returned
 *	is only valid until you call this again.
 */
static int
__os_segdata_new(segidp)
	int *segidp;
{
	os_segdata_t *p, *newptr;
	int i, newsize;

	if (__os_segdata == NULL)
		return (EAGAIN);

	for (i = 0; i < __os_segdata_size; i++) {
		p = &__os_segdata[i];
		if (p->segment == NULL) {
			*segidp = i;
			return (0);
		}
	}

	/*
	 * This test is actually pedantic, since I can't malloc more than 64K,
	 * and the structure is more than two bytes big.  But I'm a pedant.
	 */
	if ((u_int)__os_segdata_size >=
	    (32768 / sizeof(os_segdata_t) - OS_SEGDATA_INCREMENT))
		return (ENOMEM);

	newsize = __os_segdata_size + OS_SEGDATA_INCREMENT;
	if ((newptr = (os_segdata_t *)
	    __db_realloc(__os_segdata, newsize * sizeof(os_segdata_t))) == NULL)
		return (ENOMEM);
	memset(&newptr[__os_segdata_size],
	    0, OS_SEGDATA_INCREMENT * sizeof(os_segdata_t));
	__os_segdata = newptr;
	__os_segdata_size = newsize;

	*segidp = __os_segdata_size;

	return (0);
}

/*
 * __os_segdata_find_byname --
 *	Finds a segment by its name.
 *
 * PUBLIC: __os_segdata_find_byname __P((const char *, REGINFO *));
 */
static int
__os_segdata_find_byname(name, infop)
	const char *name;
	REGINFO *infop;
{
	os_segdata_t *p;
	int i;

	if (__os_segdata == NULL)
		return (EAGAIN);

	if (name == NULL)
		return (ENOENT);

	for (i = 0; i < __os_segdata_size; i++) {
		p = &__os_segdata[i];
		if (p->name != NULL && strcmp(name, p->name) == 0) {
			infop->addr = p->segment;
			infop->segid = i;
			return (0);
		}
	}
	return (ENOENT);
}

/*
 * __os_segdata_release --
 *	Free a segdata entry.
 */
static int
__os_segdata_release(segid)
	int segid;
{
	os_segdata_t *p;

	if (__os_segdata == NULL)
		return (EAGAIN);

	if (segid < 0 || segid >= __os_segdata_size)
		return (EINVAL);

	p = &__os_segdata[segid];
	if (p->name != NULL) {
		FREES(p->name);
		p->name = NULL;
	}
	if (p->segment != NULL) {
		FREE(p->segment, p->size);
		p->segment = NULL;
	}
	p->size = 0;

	/* Any shrink-table logic could go here */

	return (0);
}
