/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_oflags.c	10.6 (Sleepycat) 4/19/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#endif

#include "db_int.h"

/*
 * __db_oflags --
 *	Convert open(2) flags to DB flags.
 *
 * PUBLIC: u_int32_t __db_oflags __P((int));
 */
u_int32_t
__db_oflags(oflags)
	int oflags;
{
	u_int32_t dbflags;

	/*
	 * XXX
	 * Convert POSIX 1003.1 open(2) flags to DB flags.  Not an exact
	 * science as most POSIX implementations don't have a flag value
	 * for O_RDONLY, it's simply the lack of a write flag.
	 */
	dbflags = 0;
	if (oflags & O_CREAT)
		dbflags |= DB_CREATE;
	if (!(oflags & (O_RDWR | O_WRONLY)) || oflags & O_RDONLY)
		dbflags |= DB_RDONLY;
	if (oflags & O_TRUNC)
		dbflags |= DB_TRUNCATE;
	return (dbflags);
}

/*
 * __db_omode --
 *	Convert a permission string to the correct open(2) flags.
 *
 * PUBLIC: int __db_omode __P((const char *));
 */
int
__db_omode(perm)
	const char *perm;
{
	int mode;

#ifndef	S_IRUSR
#if defined(_WIN32) || defined(WIN16)
#define	S_IRUSR	S_IREAD		/* R for owner */
#define	S_IWUSR	S_IWRITE	/* W for owner */
#define	S_IRGRP	0		/* R for group */
#define	S_IWGRP	0		/* W for group */
#define	S_IROTH	0		/* R for other */
#define	S_IWOTH	0		/* W for other */
#else
#define	S_IRUSR	0000400		/* R for owner */
#define	S_IWUSR	0000200		/* W for owner */
#define	S_IRGRP	0000040		/* R for group */
#define	S_IWGRP	0000020		/* W for group */
#define	S_IROTH	0000004		/* R for other */
#define	S_IWOTH	0000002		/* W for other */
#endif /* _WIN32 || WIN16 */
#endif
	mode = 0;
	if (perm[0] == 'r')
		mode |= S_IRUSR;
	if (perm[1] == 'w')
		mode |= S_IWUSR;
	if (perm[2] == 'r')
		mode |= S_IRGRP;
	if (perm[3] == 'w')
		mode |= S_IWGRP;
	if (perm[4] == 'r')
		mode |= S_IROTH;
	if (perm[5] == 'w')
		mode |= S_IWOTH;
	return (mode);
}
