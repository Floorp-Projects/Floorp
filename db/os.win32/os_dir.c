/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_dir.c	10.15 (Sleepycat) 4/10/98";
#endif /* not lint */

#include "db_int.h"
#include "common_ext.h"

/*
 * __os_dirlist --
 *	Return a list of the files in a directory.
 */
int
__os_dirlist(dir, namesp, cntp)
	const char *dir;
	char ***namesp;
	int *cntp;
{
	struct _finddata_t fdata;
	long dirhandle;
	int arraysz, cnt, finished;
	char **names, filespec[MAXPATHLEN];

	(void)snprintf(filespec, sizeof(filespec), "%s/*", dir);
	if ((dirhandle = _findfirst(filespec, &fdata)) == -1)
		return (errno);

	names = NULL;
	finished = 0;
	for (arraysz = cnt = 0; finished != 1; ++cnt) {
		if (cnt >= arraysz) {
			arraysz += 100;
			names = (char **)(names == NULL ?
			    __db_malloc(arraysz * sizeof(names[0])) :
			    __db_realloc(names, arraysz * sizeof(names[0])));
			if (names == NULL)
				goto nomem;
		}
		if ((names[cnt] = (char *)__db_strdup(fdata.name)) == NULL)
			goto nomem;
		if (_findnext(dirhandle,&fdata) != 0)
			finished = 1;
	}
	_findclose(dirhandle);

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
