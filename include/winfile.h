/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* ---------------------------------------------------------------------------
    Stuff to fake unix file I/O on windows boxes
    ------------------------------------------------------------------------*/

#ifndef WINFILE_H
#define WINFILE_H

#ifdef _WINDOWS
#if defined(XP_WIN32) || defined(_WIN32)
/* 32-bit stuff here */
#include <windows.h>
#include <stdlib.h>
#include <sys\types.h>
#include <sys\stat.h>

typedef struct DIR_Struct {
    void            * directoryPtr;
    WIN32_FIND_DATA   data;
} DIR;

#define _ST_FSTYPSZ 16

#ifndef __BORLANDC__
 typedef unsigned long mode_t;
 typedef          long uid_t;
 typedef          long gid_t;
 typedef          long off_t;
 typedef unsigned long nlink_t;
#endif 

typedef struct timestruc {
    time_t  tv_sec;         /* seconds */
    long    tv_nsec;        /* and nanoseconds */
} timestruc_t;


struct dirent {                                 /* data from readdir() */
        ino_t           d_ino;                  /* inode number of entry */
        off_t           d_off;                  /* offset of disk direntory entry */
        unsigned short  d_reclen;               /* length of this record */
        char            d_name[_MAX_FNAME];     /* name of file */
};

#ifndef __BORLANDC__
#define S_ISDIR(s)  ((s) & _S_IFDIR)
#endif

#else /* _WIN32 */
/* 16-bit windows stuff */

#include <sys\types.h>
#include <sys\stat.h>
#include <dos.h>



/*	Getting cocky to support multiple file systems */
typedef struct	dirStruct_tag	{
	struct _find_t	file_data;
	char			c_checkdrive;
} dirStruct;

typedef struct DIR_Struct {
    void            * directoryPtr;
    dirStruct         data;
} DIR;

#define _ST_FSTYPSZ 16
typedef unsigned long mode_t;
typedef          long uid_t;
typedef          long gid_t;
typedef          long off_t;
typedef unsigned long nlink_t;

typedef struct timestruc {
    time_t  tv_sec;         /* seconds */
    long    tv_nsec;        /* and nanoseconds */
} timestruc_t;

struct dirent {                             /* data from readdir() */
        ino_t           d_ino;              /* inode number of entry */
        off_t           d_off;              /* offset of disk direntory entry */
        unsigned short  d_reclen;           /* length of this record */
#ifdef XP_WIN32
        char            d_name[_MAX_FNAME]; /* name of file */
#else
        char            d_name[20]; /* name of file */
#endif
};

#define S_ISDIR(s)  ((s) & _S_IFDIR)

#endif /* 16-bit windows */

#define CONST const

#endif /* _WINDOWS */

#endif /* WINFILE_H */
