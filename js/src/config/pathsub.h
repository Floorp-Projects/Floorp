/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef pathsub_h___
#define pathsub_h___
/*
** Pathname subroutines.
**
** Brendan Eich, 8/29/95
*/
#include <limits.h>
#include <sys/types.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

/*
 * Just prevent stupidity
 */
#undef NAME_MAX
#define NAME_MAX 256

extern char *program;

extern void fail(char *format, ...);
extern char *getcomponent(char *path, char *name);
extern char *ino2name(ino_t ino);
extern void *xmalloc(size_t size);
extern char *xstrdup(char *s);
extern char *xbasename(char *path);
extern void xchdir(char *dir);

/* Relate absolute pathnames from and to returning the result in outpath. */
extern int relatepaths(char *from, char *to, char *outpath);

/* XXX changes current working directory -- caveat emptor */
extern void reversepath(char *inpath, char *name, int len, char *outpath);

#endif /* pathsub_h___ */
