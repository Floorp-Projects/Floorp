/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_rw.c	10.7 (Sleepycat) 4/10/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <unistd.h>
#endif

#include "db_int.h"

/*
 * __db_read --
 *	Read from a file handle.
 *
 * PUBLIC: int __db_read __P((int, void *, size_t, ssize_t *));
 */
int
__db_read(fd, addr, len, nrp)
	int fd;
	void *addr;
	size_t len;
	ssize_t *nrp;
{
	size_t offset;
	ssize_t nr;
	u_int8_t *taddr;

	for (taddr = addr,
	    offset = 0; offset < len; taddr += nr, offset += nr) {
		if ((nr = __os_read(fd, taddr, len - offset)) < 0)
			return (errno);
		if (nr == 0)
			break;
	}
	*nrp = taddr - (u_int8_t *)addr;
	return (0);
}

/*
 * __db_write --
 *	Write to a file handle.
 *
 * PUBLIC: int __db_write __P((int, void *, size_t, ssize_t *));
 */
int
__db_write(fd, addr, len, nwp)
	int fd;
	void *addr;
	size_t len;
	ssize_t *nwp;
{
	size_t offset;
	ssize_t nw;
	u_int8_t *taddr;

	for (taddr = addr,
	    offset = 0; offset < len; taddr += nw, offset += nw)
		if ((nw = __os_write(fd, taddr, len - offset)) < 0)
			return (errno);
	*nwp = len;
	return (0);
}
