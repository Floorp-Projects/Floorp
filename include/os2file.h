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
    Stuff to fake unix file I/O on os2 boxes
    ------------------------------------------------------------------------*/

#ifndef OS2FILE_H
#define OS2FILE_H

#if defined(XP_OS2)
/* 32-bit stuff here */
#define INCL_DOSPROCESS
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_DOSFILEMGR
#define INCL_DOSMODULEMGR
#define INCL_PM


#define INCL_WIN
#define INCL_WINATOM
#define INCL_GPI
#define TID OS2TID   /* global rename in OS2 H's!               */
#include <os2.h>
#undef TID           /* and restore                             */


/*DAK these are empty.. #include <windef.h> */
/*DAK these are empty.. #include <winbase.h>*/
#include <stdlib.h>
#include <sys\types.h>
#include <sys\stat.h>
#include "dirent.h"

/*DSR020697 - now using dirent.h for DIR...*/

#define _ST_FSTYPSZ 16
typedef unsigned long mode_t;
typedef          long uid_t;
typedef          long gid_t;
#ifndef XP_OS2
   typedef          long off_t;
#endif
typedef unsigned long nlink_t;

typedef struct timestruc {
    time_t  tv_sec;         /* seconds */
    long    tv_nsec;        /* and nanoseconds */
} timestruc_t;

/*DSR020697 - now using dirent.h for dirent, S_ISDIR...*/

#endif /* XP_OS2 */

#define CONST const

#endif /* OS2FILE_H */
