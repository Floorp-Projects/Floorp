/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This file should be the first Mcom file included
 *
 * All cross-platform definitions, regardless of project, should be
 *  contained in this file or its includes
 */

#ifndef _MCOM_H_
#define _MCOM_H_

#include "platform.h"
#include "xp_core.h"
#include "xp_mem.h"
#include "xp_debug.h"
#include "xp_str.h"
#include "xp_list.h"


/* platform-specific types */

/* include header files needed for prototypes/etc */

#include "xp_file.h"

XP_BEGIN_PROTOS

/* XXX where should this kind of junk go? */
unsigned char *XP_WordWrap(int charset, unsigned char *string, int maxColumn,
                 int checkQuoting);

XP_END_PROTOS

/* --------------------------------------------------------------------- */
/*
    Define the hooks for cross-platform string + memory functions

*/

#ifdef DEBUG
	XP_BEGIN_PROTOS
		extern char * NOT_NULL (const char *x);
	XP_END_PROTOS
#else
#	define NOT_NULL(X)	X
#endif

#include <string.h>
#define XP_STRLEN(s)              strlen(NOT_NULL(s))
#define XP_STRCMP(a, b)           strcmp(NOT_NULL(a), NOT_NULL(b))
#define XP_STRNCMP(a, b, n)       strncmp(NOT_NULL(a), NOT_NULL(b), (n))
#define XP_STRCPY(d, s)           strcpy(NOT_NULL(d), NOT_NULL(s))
#define XP_STRCHR                 strchr
#define XP_STRRCHR                strrchr
#define XP_STRTOK                 strtok
#define XP_STRCAT                 strcat
#define XP_STRNCAT                strncat
#define XP_STRSTR                 strstr
#define XP_STRTOUL                strtoul


/* XP_FILENAMECMP compares two filenames, treating case differences
   appropriately for this OS. */

#if defined(XP_WIN) || defined(XP_OS2)
#define XP_FILENAMECMP	 	stricmp	
#else
#define XP_FILENAMECMP		XP_STRCMP
#endif


#if !defined(XP_WIN) && !defined(XP_OS2) && !(defined(__GLIBC__) && __GLIBC__ >= 2) && !defined(AIXV3)
/* strdup is not an ANSI function */
XP_BEGIN_PROTOS
extern char * strdup (const char * s);
XP_END_PROTOS
#endif

#ifndef __QNX__
#include <memory.h>
#else
XP_BEGIN_PROTOS
extern long random(void);	/* QNX doesn't provide a prototype, so do this until they fix it. */
XP_END_PROTOS
#endif

#define XP_STRDUP(s)              strdup((s))
#define XP_MEMCPY(d, s, n)        memcpy((d), (s), (n))

/* NOTE: XP_MEMMOVE gurantees that overlaps will be properly handled */
#ifdef SUNOS4
#define XP_MEMMOVE(Dest,Src,Len)  bcopy((Src),(Dest),(Len))
#else
#define XP_MEMMOVE(Dest,Src,Len)  memmove((Dest),(Src),(Len))
#endif /* SUNOS4 */

#define XP_MEMSET                 memset
#define XP_SPRINTF                sprintf

/* should I really include this here or what? */
#ifdef XP_MAC
#include "prprf.h"  
#else
#include "prprf.h"  
#endif
#define XP_SAFE_SPRINTF			  PR_snprintf
#define XP_MEMCMP                 memcmp

#define XP_VSPRINTF               vsprintf

#define XP_IS_SPACE(VAL)                \
    (((((intn)(VAL)) & 0x7f) == ((intn)(VAL))) && isspace((intn)(VAL)) )

#define XP_IS_CNTRL(i) 	((((unsigned int) (i)) > 0x7f) ? (int) 0 : iscntrl(i))
#define XP_IS_DIGIT(i) ((((unsigned int) (i)) > 0x7f) ? (int) 0 : isdigit(i))

#if defined(XP_WIN) || defined(XP_OS2)
#define XP_IS_ALPHA(VAL)                (isascii((int)(VAL)) && isalpha((int)(VAL)))
#else
#define XP_IS_ALPHA(VAL) ((((unsigned int) (VAL)) > 0x7f) ? FALSE : isalpha((int)(VAL)))
#endif

#define XP_ATOI(PTR)                    (atoi((PTR)))

/* NOTE: XP_BCOPY gurantees that overlaps will be properly handled */
#ifdef XP_WIN16

XP_BEGIN_PROTOS
extern void WIN16_bcopy(char *, char *, unsigned long);
XP_END_PROTOS

#define XP_BCOPY(PTR_FROM, PTR_TO, LEN) \
        		(WIN16_bcopy((char *) (PTR_FROM), (char *)(PTR_TO), (LEN)))
#else
#define XP_BCOPY(Src,Dest,Len)  XP_MEMMOVE((Dest),(Src),(Len))
#endif

#define XP_BZERO(a,b)             memset(a,0,b)

#if !defined(XP_RANDOM) || !defined(XP_SRANDOM)   /* defined in both xp_mcom.h and xp_str.h */
#ifdef HAVE_RANDOM
#define XP_RANDOM		random
#define XP_SRANDOM(seed)	srandom((seed))
#else
#define XP_RANDOM 		rand
#define XP_SRANDOM(seed)	srand((seed))
#endif
#endif

#ifdef XP_MAC
XP_BEGIN_PROTOS

extern time_t GetTimeMac();
extern time_t Mactime(time_t *timer);
extern struct tm *Macgmtime(const time_t *timer);
extern time_t Macmktime (struct tm *timeptr);
extern char * Macctime(const time_t *);
extern struct tm *Maclocaltime(const time_t *);

XP_END_PROTOS

#define XP_TIME()	GetTimeMac()
#define time(t)		Mactime(t)
#define gmtime(t)	Macgmtime(t)
#define mktime(t)	Macmktime(t)
#define ctime(t)	Macctime(t)
#define localtime(t)	Maclocaltime(t)
#define UNIXMINUSMACTIME 2082844800UL
#else
#define XP_TIME()	time(0)
#endif
#endif /* _MCOM_H_ */
