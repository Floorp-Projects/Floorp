/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_fid.c	10.11 (Sleepycat) 4/26/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <string.h>
#include <time.h>
#endif

#include "db_int.h"
#include "common_ext.h"

/*
 * __db_fileid --
 *	Return a unique identifier for a file.
 *
 * PUBLIC: int __db_fileid __P((DB_ENV *, const char *, int, u_int8_t *));
 */
int
__db_fileid(dbenv, fname, timestamp, fidp)
	DB_ENV *dbenv;
	const char *fname;
	int timestamp;
	u_int8_t *fidp;
{
	struct stat sb;
	size_t i;
	time_t now;
	u_int8_t *p;

	/* Clear the buffer. */
	memset(fidp, 0, DB_FILE_ID_LEN);

	/* Check for the unthinkable. */
	if (sizeof(sb.st_ino) +
	    sizeof(sb.st_dev) + sizeof(time_t) > DB_FILE_ID_LEN)
		return (EINVAL);

	/* On UNIX, use a dev/inode pair. */
	if (stat(fname, &sb)) {
		__db_err(dbenv, "%s: %s", fname, strerror(errno));
		return (errno);
	}

	/*
	 * Use the inode first and in reverse order, hopefully putting the
	 * distinguishing information early in the string.
	 */
	for (p = (u_int8_t *)&sb.st_ino +
	    sizeof(sb.st_ino), i = 0; i < sizeof(sb.st_ino); ++i)
		*fidp++ = *--p;
	for (p = (u_int8_t *)&sb.st_dev +
	    sizeof(sb.st_dev), i = 0; i < sizeof(sb.st_dev); ++i)
		*fidp++ = *--p;

	if (timestamp) {
		(void)time(&now);
		for (p = (u_int8_t *)&now +
		    sizeof(now), i = 0; i < sizeof(now); ++i)
			*fidp++ = *--p;
	}
	return (0);
}
