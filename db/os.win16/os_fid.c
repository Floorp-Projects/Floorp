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
static const char sccsid[] = "@(#)os_fid.c	10.3 (Sleepycat) 4/10/98";
#endif /* not lint */

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
	time_t now;
	size_t i, len;
	u_int8_t *p;

	COMPQUIET(dbenv, NULL);

	/*
	 * Windows/16 has no unique identifier, like the inode number on
	 * UNIX.  We use the name plus the timestamp, and hope that it's
	 * sufficient.
	 */
	len = DB_FILE_ID_LEN;
	if (timestamp) {
		(void)time(&now);
		p = (u_int8_t *)&now;
		for (i = 0; i < sizeof(time_t); ++i)
			*fidp++ = *p++;
		len -= sizeof(time_t);
	}

	/*
	 * XXX
	 * We're appending the full path name here -- is that right?  If
	 * the drive is mounted elsewhere next time, won't this be wrong.
	 * Maybe only append up to the path separator?
	 */
	for (p = (u_int8_t *)fname; *p != '\0'; ++p);

	for (; len > 0 && --p >= (u_int8_t *)fname; --len)
		*fidp++ = *p;

	return (0);
}
