/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */
/*
** Netscape portable install command.
**
** Brendan Eich, 7/20/95
*/
#ifdef XP_OS2_VACPP
#include "getopt.c"
#include "dirent.c"
#endif
#include <stdio.h>  /* OSF/1 requires this before grp.h, so put it first */
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#ifndef XP_OS2_VACPP
#include <grp.h>
#include <pwd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef XP_OS2_VACPP
#include <unistd.h>
#include <utime.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include "pathsub.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifdef XP_OS2_VACPP
getopt(int nargc, char **nargv, char *ostr);
#include <dirent.h>
#include <direct.h>
#include <io.h>
#include <sys\utime.h>
#include <sys\types.h>
#endif

#ifdef SUNOS4
#include "sunos4.h"
#endif

#ifdef NEXTSTEP
#include <bsd/libc.h>
#endif

#ifdef __QNX__
#include <unix.h>
#endif

#ifdef NEED_S_ISLNK
#if !defined(S_ISLNK) && defined(S_IFLNK)
#define S_ISLNK(a)	(((a) & S_IFMT) == S_IFLNK)
#endif
#endif

#ifdef NEED_FCHMOD_PROTO
extern int fchmod(int fildes, mode_t mode);
#endif

#ifdef XP_OS2
/*Note: OS/2 has no concept of users or groups, or symbolic links...*/
#define lstat stat
/*looks reasonably safe based on OS/2's stat.h...*/
#define S_ISLNK(mode)   0 /*no way in hell on a file system that doesn't support it*/
#ifdef XP_OS2_VACPP
typedef unsigned short mode_t;
typedef unsigned short uid_t;
typedef unsigned short gid_t;
#define mkdir(path, mode) mkdir(path)
#define W_OK 1
#endif /* XP_OS2_VACPP */
#define touid(spam) 0
#define togid(spam) 0
#define access(spam, spam2) 0
#define chown(spam1, spam2, spam3) 0
#define lchown(spam1, spam2, spam3) 0
#define fchown(spam1, spam2, spam3) 0
#define readlink(spam1, spam2, spam3) -1
#define symlink(spam1, spam2) -1
#ifndef XP_OS2_VACPP
unsigned long DosSetFileSize(int, int);
#endif /* XP_OS2_VACPP */
#define ftruncate(spam1, spam2) (DosSetFileSize(spam1, spam2)?-1:0)
#endif /* XP_OS2 */

static void
usage(void)
{
    fprintf(stderr,
	"usage: %s [-C cwd] [-L linkprefix] [-m mode] [-o owner] [-g group]\n"
	"       %*s [-DdltR] file [file ...] directory\n",
	program, (int) strlen(program), "");
    exit(2);
}

static int
mkdirs(char *path, mode_t mode)
{
    char *cp;
    struct stat sb;
    int res;
    int l;

    /* strip trailing "/." */
    l = strlen(path);
    if(l > 1 && path[l - 1] == '.' && path[l - 2] == '/')
        path[l - 2] = 0;

    while (*path == '/' && path[1] == '/')
	path++;
    while ((cp = strrchr(path, '/')) && cp[1] == '\0')
	*cp = '\0';
    if (cp && cp != path) {
	*cp = '\0';
	if ((lstat(path, &sb) < 0 || !S_ISDIR(sb.st_mode)) &&
	    mkdirs(path, mode) < 0) {
	    return -1;
	}
	*cp = '/';
    }
    
    res = mkdir(path, mode);
    if ((res != 0) && (errno == EEXIST))
      return 0;
    else
      return res;
}

#ifndef XP_OS2
static uid_t
touid(char *owner)
{
    struct passwd *pw;
    uid_t uid;
    char *cp;

    pw = getpwnam(owner);
    if (pw)
	return pw->pw_uid;
    uid = strtol(owner, &cp, 0);
    if (uid == 0 && cp == owner)
	fail("cannot find uid for %s", owner);
    return uid;
}

static gid_t
togid(char *group)
{
    struct group *gr;
    gid_t gid;
    char *cp;

    gr = getgrnam(group);
    if (gr)
	return gr->gr_gid;
    gid = strtol(group, &cp, 0);
    if (gid == 0 && cp == group)
	fail("cannot find gid for %s", group);
    return gid;
}
#endif

int
main(int argc, char **argv)
{
    int onlydir, dodir, dolink, dorelsymlink, dotimes, opt, len, lplen, tdlen, bnlen, exists, fromfd, tofd, cc, wc;
    mode_t mode = 0755;
    char *linkprefix, *owner, *group, *cp, *cwd, *todir, *toname, *name, *base, *linkname, *bp, buf[BUFSIZ];
    uid_t uid;
    gid_t gid;
    struct stat sb, tosb, fromsb;
    struct utimbuf utb;

    program = argv[0];
    cwd = linkname = linkprefix = owner = group = 0;
    onlydir = dodir = dolink = dorelsymlink = dotimes = lplen = 0;

    while ((opt = getopt(argc, argv, "C:DdlL:Rm:o:g:t")) != EOF) {
	switch (opt) {
	  case 'C':
	    cwd = optarg;
	    break;
	  case 'D':
	    onlydir = 1;
	    break;
	  case 'd':
	    dodir = 1;
	    break;
	  case 'l':
	    dolink = 1;
	    break;
	  case 'L':
	    linkprefix = optarg;
	    lplen = strlen(linkprefix);
	    dolink = 1;
	    break;
     case 'R':
#ifdef XP_OS2
 /* treat like -t since no symbolic links on OS2 */
	    dotimes = 1;
#else
	    dolink = dorelsymlink = 1;
#endif
	    break;
	  case 'm':
	    mode = strtoul(optarg, &cp, 8);
	    if (mode == 0 && cp == optarg)
		usage();
	    break;
	  case 'o':
	    owner = optarg;
	    break;
	  case 'g':
	    group = optarg;
	    break;
	  case 't':
	    dotimes = 1;
	    break;
	  default:
	    usage();
	}
    }

    argc -= optind;
    argv += optind;
    if (argc < 2 - onlydir)
	usage();

    todir = argv[argc-1];
    if ((stat(todir, &sb) < 0 || !S_ISDIR(sb.st_mode)) &&
	mkdirs(todir, 0777) < 0) {
	fail("cannot make directory %s", todir);
    }
    if (onlydir)
	return 0;

    if (!cwd) {
#ifndef NEEDS_GETCWD
#ifndef GETCWD_CANT_MALLOC
	cwd = getcwd(0, PATH_MAX);
#else
	cwd = malloc(PATH_MAX + 1);
	cwd = getcwd(cwd, PATH_MAX);
#endif
#else
	cwd = malloc(PATH_MAX + 1);
	cwd = getwd(cwd);
#endif
    }

    xchdir(todir);
#ifndef NEEDS_GETCWD
#ifndef GETCWD_CANT_MALLOC
    todir = getcwd(0, PATH_MAX);
#else
    todir = malloc(PATH_MAX + 1);
    todir = getcwd(todir, PATH_MAX);
#endif
#else
    todir = malloc(PATH_MAX + 1);
    todir = getwd(todir);
#endif
    tdlen = strlen(todir);
    xchdir(cwd);
    tdlen = strlen(todir);

    uid = owner ? touid(owner) : (uid_t)(-1);
    gid = group ? togid(group) : (gid_t)(-1);

    while (--argc > 0) {
	name = *argv++;
	len = strlen(name);
	base = xbasename(name);
	bnlen = strlen(base);
	toname = xmalloc((unsigned int)(tdlen + 1 + bnlen + 1));
	sprintf(toname, "%s/%s", todir, base);
	exists = (lstat(toname, &tosb) == 0);

	if (dodir) {
	    /* -d means create a directory, always */
	    if (exists && !S_ISDIR(tosb.st_mode)) {
		(void) unlink(toname);
		exists = 0;
	    }
	    if (!exists && mkdir(toname, mode) < 0)
		fail("cannot make directory %s", toname);
	    if ((owner || group) && chown(toname, uid, gid) < 0)
		fail("cannot change owner of %s", toname);
	} else if (dolink) {
            if (access(name, R_OK) != 0) {
                fail("cannot access %s", name);
            }
	    if (*name == '/') {
		/* source is absolute pathname, link to it directly */
		linkname = 0;
	    } else {
		if (linkprefix) {
		    /* -L implies -l and prefixes names with a $cwd arg. */
		    len += lplen + 1;
		    linkname = xmalloc((unsigned int)(len + 1));
		    sprintf(linkname, "%s/%s", linkprefix, name);
		} else if (dorelsymlink) {
		    /* Symlink the relative path from todir to source name. */
		    linkname = xmalloc(PATH_MAX);

		    if (*todir == '/') {
			/* todir is absolute: skip over common prefix. */
			lplen = relatepaths(todir, cwd, linkname);
			strcpy(linkname + lplen, name);
		    } else {
			/* todir is named by a relative path: reverse it. */
			reversepath(todir, name, len, linkname);
			xchdir(cwd);
		    }

		    len = strlen(linkname);
		}
		name = linkname;
	    }

	    /* Check for a pre-existing symlink with identical content. */
#ifdef XP_OS2_VACPP
#pragma info(nocnd)
#endif
	    if ((exists && (!S_ISLNK(tosb.st_mode) ||
						readlink(toname, buf, sizeof buf) != len ||
						strncmp(buf, name, (unsigned int)len) != 0)) || 
			((stat(name, &fromsb) == 0) && 
			 (fromsb.st_mtime > tosb.st_mtime))) {
#ifdef XP_OS2_VACPP
#pragma info(restore)
#endif
		(void) (S_ISDIR(tosb.st_mode) ? rmdir : unlink)(toname);
		exists = 0;
	    }
	    if (!exists && symlink(name, toname) < 0)
		fail("cannot make symbolic link %s", toname);
#ifdef HAVE_LCHOWN
	    if ((owner || group) && lchown(toname, uid, gid) < 0)
		fail("cannot change owner of %s", toname);
#endif

	    if (linkname) {
		free(linkname);
		linkname = 0;
	    }
	} else {
	    /* Copy from name to toname, which might be the same file. */
#ifdef XP_OS2_FIX
	    fromfd = open(name, O_RDONLY | O_BINARY);
#else
	    fromfd = open(name, O_RDONLY);
#endif
	    if (fromfd < 0 || fstat(fromfd, &sb) < 0)
		fail("cannot access %s", name);
	    if (exists && (!S_ISREG(tosb.st_mode) || access(toname, W_OK) < 0))
		(void) (S_ISDIR(tosb.st_mode) ? rmdir : unlink)(toname);
#ifdef XP_OS2
	    chmod(toname, S_IREAD | S_IWRITE);
#endif
#ifdef XP_OS2_FIX
	    tofd = open(toname, O_CREAT | O_WRONLY | O_BINARY, 0666);
#else
	    tofd = open(toname, O_CREAT | O_WRONLY, 0666);
#endif
	    if (tofd < 0)
		fail("cannot create %s", toname);

	    bp = buf;
	    while ((cc = read(fromfd, bp, sizeof buf)) > 0) {
		while ((wc = write(tofd, bp, (unsigned int)cc)) > 0) {
		    if ((cc -= wc) == 0)
			break;
		    bp += wc;
		}
		if (wc < 0)
		    fail("cannot write to %s", toname);
	    }
	    if (cc < 0)
		fail("cannot read from %s", name);

	    if (ftruncate(tofd, sb.st_size) < 0)
		fail("cannot truncate %s", toname);
#ifndef XP_OS2
	    if (dotimes) {
		utb.actime = sb.st_atime;
		utb.modtime = sb.st_mtime;
		if (utime(toname, &utb) < 0)
		    fail("cannot set times of %s", toname);
	    }
#ifdef HAVE_FCHMOD
	    if (fchmod(tofd, mode) < 0)
#else
	    if (chmod(toname, mode) < 0)
#endif
		fail("cannot change mode of %s", toname);
#endif
	    if ((owner || group) && fchown(tofd, uid, gid) < 0)
		fail("cannot change owner of %s", toname);

	    /* Must check for delayed (NFS) write errors on close. */
	    if (close(tofd) < 0)
		fail("cannot write to %s", toname);
	    close(fromfd);
#ifdef XP_OS2
	    if (dotimes) {
		utb.actime = sb.st_atime;
		utb.modtime = sb.st_mtime;
		if (utime(toname, &utb) < 0)
		    fail("cannot set times of %s", toname);
	    }
	    if (chmod(toname, (mode & (S_IREAD | S_IWRITE))) < 0)
		fail("cannot change mode of %s", toname);
#endif
	}

	free(toname);
    }

    free(cwd);
    free(todir);
    return 0;
}
