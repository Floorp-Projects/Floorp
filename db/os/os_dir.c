/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_dir.c	10.15 (Sleepycat) 4/26/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <errno.h>
#endif

#include "db_int.h"

/*
 * __os_dirlist --
 *	Return a list of the files in a directory.
 *
 * PUBLIC: int __os_dirlist __P((const char *, char ***, int *));
 */
int
__os_dirlist(dir, namesp, cntp)
	const char *dir;
	char ***namesp;
	int *cntp;
{
	struct dirent *dp;
	DIR *dirp;
	int arraysz, cnt;
	char **names;

	if ((dirp = opendir(dir)) == NULL)
		return (errno);
	names = NULL;
	for (arraysz = cnt = 0; (dp = readdir(dirp)) != NULL; ++cnt) {
		if (cnt >= arraysz) {
			arraysz += 100;
			names = (char **)(names == NULL ?
			    __db_malloc(arraysz * sizeof(names[0])) :
			    __db_realloc(names, arraysz * sizeof(names[0])));
			if (names == NULL)
				goto nomem;
		}
		if ((names[cnt] = (char *)__db_strdup(dp->d_name)) == NULL)
			goto nomem;
	}
	(void)closedir(dirp);

	*namesp = names;
	*cntp = cnt;
	return (0);

nomem:	if (names != NULL)
		__os_dirfree(names, cnt);
	return (ENOMEM);
}

/*
 * __os_dirfree --
 *	Free the list of files.
 *
 * PUBLIC: void __os_dirfree __P((char **, int));
 */
void
__os_dirfree(names, cnt)
	char **names;
	int cnt;
{
	while (cnt > 0)
		__db_free(names[--cnt]);
	__db_free(names);
}
