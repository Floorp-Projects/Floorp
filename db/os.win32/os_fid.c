/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_fid.c	10.13 (Sleepycat) 4/17/98";
#endif /* not lint */

#include "db_int.h"

/*
 * __db_fileid --
 *	Return a unique identifier for a file.
 */
int
__db_fileid(dbenv, fname, timestamp, fidp)
	DB_ENV *dbenv;
	const char *fname;
	int timestamp;
	u_int8_t *fidp;
{
	size_t i;
	time_t now;
	u_int8_t *p;

	/*
	 * The documentation for GetFileInformationByHandle() states that the
	 * inode-type numbers are not constant between processes.  Actually,
	 * they are, they're the NTFS MFT indexes.  So, this works on NTFS,
	 * but perhaps not on other platforms, and perhaps not over a network.
	 * Can't think of a better solution right now.
	 */
	int fd = 0;
	HANDLE fh = 0;
	BY_HANDLE_FILE_INFORMATION fi;
	BOOL retval = FALSE;

	/* Clear the buffer. */
	memset(fidp, 0, DB_FILE_ID_LEN);

	/* first we open the file, because we're not given a handle to it */
	fd = __os_open(fname,_O_RDONLY,_S_IREAD);
	if (-1 == fd) {
		/* If we can't open it, we're in trouble */
		return (errno);
	}

	/* File open, get its info */
	fh = (HANDLE)_get_osfhandle(fd);
	if ((HANDLE)(-1) != fh) {
		retval = GetFileInformationByHandle(fh,&fi);
	}
	__os_close(fd);

	/*
	 * We want the three 32-bit words which tell us the volume ID and
	 * the file ID.  We make a crude attempt to copy the bytes over to
	 * the callers buffer.
	 *
	 * DBDB: really we should ensure that the bytes get packed the same
	 * way on all compilers, platforms etc.
	 */
	if ( ((HANDLE)(-1) != fh) && (TRUE == retval) ) {
		memcpy(fidp, &fi.nFileIndexLow, sizeof(u_int32_t));
		fidp += sizeof(u_int32_t);
		memcpy(fidp, &fi.nFileIndexHigh, sizeof(u_int32_t));
		fidp += sizeof(u_int32_t);
		memcpy(fidp, &fi.dwVolumeSerialNumber, sizeof(u_int32_t));
		fidp += sizeof(u_int32_t);
	}

	if (timestamp) {
		(void)time(&now);
		for (p = (u_int8_t *)&now +
		    sizeof(now), i = 0; i < sizeof(now); ++i)
			*fidp++ = *--p;
	}
	return (0);
}
