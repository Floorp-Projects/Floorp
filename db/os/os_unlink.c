/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_unlink.c	10.5 (Sleepycat) 4/10/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <unistd.h>
#endif

#include "db_int.h"

/*
 * __db_unlink --
 *	Remove a file.
 *
 * PUBLIC: int __db_unlink __P((const char *));
 */
int
__db_unlink(path)
	const char *path;
{
	return (__os_unlink(path) == -1 ? errno : 0);
}
