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

#ifndef XPSTRING_H
#define XPSTRING_H

#include <string.h>

#define XP_STRCASECMP  strcasecomp
#define XP_STRNCASECMP strncasecomp
#define XP_STRCASESTR strcasestr
#define XP_STRNCHR strnchr

#define XP_TO_UPPER(i)  xp_toupper(i)	
#define XP_TO_LOWER(i) 	xp_tolower(i)

#ifdef XP_UNIX /* because I can't verify yet.. */
#ifdef DEBUG
	XP_BEGIN_PROTOS
		extern char * NOT_NULL (const char *x);
	XP_END_PROTOS
#else
#	define NOT_NULL(X)	X
#endif

#define XP_STRLEN(s)             	strlen(NOT_NULL(s))
#define XP_STRCMP(a, b)           	strcmp(NOT_NULL(a), NOT_NULL(b))
#define XP_STRNCMP(a, b, n)       strncmp(NOT_NULL(a), NOT_NULL(b), (n))
#define XP_STRCPY(d, s)           	strcpy(NOT_NULL(d), NOT_NULL(s))
#define XP_STRCHR                 	strchr
#define XP_STRRCHR                	strrchr
#define XP_STRTOK                 	strtok
#define XP_STRCAT                 	strcat
#define XP_STRNCAT                	strncat
#define XP_STRSTR                 	strstr
#define XP_STRTOUL                	strtoul

/* 
	XP_FILENAMECMP compares two filenames, treating case differences
   	appropriately for this OS. 
   */

#ifdef XP_WIN 
#define 	XP_FILENAMECMP	 	stricmp	
#else
#define 	XP_FILENAMECMP		XP_STRCMP
#endif


#if !defined(XP_WIN) && !defined(XP_OS2)
	/* strdup is not an ANSI function */
	XP_BEGIN_PROTOS
	extern char * strdup (const char * s);
	XP_END_PROTOS
#endif


#define XP_STRDUP(s)              strdup((s))
#define XP_MEMCPY(d, s, n)        memcpy((d), (s), (n))

/* NOTE: XP_MEMMOVE gurantees that overlaps will be properly handled */
#if defined( __sun) && !defined(__svr4__)
#define XP_MEMMOVE(Dest,Src,Len)  bcopy((Src),(Dest),(Len))
#else
#define XP_MEMMOVE(Dest,Src,Len)  memmove((Dest),(Src),(Len))
#endif /* __sun */

#define XP_MEMSET                 	memset
#define XP_SPRINTF                	sprintf
#define XP_SAFE_SPRINTF		PR_snprintf
#define XP_MEMCMP                 	memcmp
#define XP_VSPRINTF               	vsprintf

#define XP_IS_SPACE(VAL)                \
    (((((intn)(VAL)) & 0x7f) == ((intn)(VAL))) && isspace((intn)(VAL)) )

#define XP_IS_CNTRL(i) 	((((unsigned int) (i)) > 0x7f) ? (int) 0 : iscntrl(i))
#define XP_IS_DIGIT(i) 	((((unsigned int) (i)) > 0x7f) ? (int) 0 : isdigit(i))

#ifdef XP_WIN 
#define XP_IS_ALPHA(VAL)         (isascii((int)(VAL)) && isalpha((int)(VAL)))
#else
#define XP_IS_ALPHA(VAL) 	((((unsigned int) (VAL)) > 0x7f) ? FALSE : isalpha((int)(VAL)))
#endif

#define XP_ATOI(PTR)                    (atoi((PTR)))
#define XP_BZERO(a,b)	           memset(a,0,b)

/* NOTE: XP_BCOPY gurantees that overlaps will be properly handled */
#ifdef XP_WIN16

	XP_BEGIN_PROTOS
	extern void WIN16_bcopy(char *, char *, unsigned long);
	XP_END_PROTOS

	#define XP_BCOPY(PTR_FROM, PTR_TO, LEN) \
	        		(WIN16_bcopy((char *) (PTR_FROM), (char *)(PTR_TO), (LEN)))

#else

	#define XP_BCOPY(Src,Dest,Len)  XP_MEMMOVE((Dest),(Src),(Len))

#endif /* XP_WIN16 */


/*
	Random stuff in a random place
*/
#if defined (XP_WIN) || defined (XP_OS2)
#define XP_RANDOM 		rand
#define XP_SRANDOM(seed)	srand((seed))
#endif
#ifdef XP_MAC
#define XP_RANDOM 		rand
#define XP_SRANDOM(seed)	srand((seed))
#endif
#ifdef XP_UNIX
#if !defined(XP_RANDOM) || !defined(XP_SRANDOM)   /* defined in both xp_mcom.h and xp_str.h */
#if defined(UNIXWARE) || defined(_INCLUDE_HPUX_SOURCE) || (defined(__sun) && defined(__svr4__)) || defined(SNI) || defined(NCR)
#define XP_RANDOM		rand
#define XP_SRANDOM(seed)	srand((seed))
#else
#define XP_RANDOM		random
#define XP_SRANDOM(seed)	srandom((seed))
#endif
#endif
#endif

#endif /* XP_UNIX -- ifdef'd because I can't yet verify */
XP_BEGIN_PROTOS

/*
** Some basic string things
*/

/*
** Concatenate s1 to s2. If s1 is NULL then a copy of s2 is
** returned. Otherwise, s1 is realloc'd and s2 is concatenated, with the
** new value of s1 being returned.
*/
extern char *XP_AppendStr(char *s1, const char *s2);


/*
** Concatenate a bunch of strings. The variable length argument list must
** be terminated with a NULL. This result is a DS_Alloc'd string.
*/
extern char *XP_Cat(char *s1, ...);

/* Fast table driven tolower and toupper functions.  
 ** 
 ** Only works on first 128 ascii 
 */
int xp_tolower(int c);
int xp_toupper(int c);

/*
 * Case-insensitive string comparison
 */
extern int strcasecomp  (const char *a, const char *b);
extern int strncasecomp (const char *a, const char *b, int n);

/* find a char within a specified length string 
 */
extern char * strnchr (const char * str, const char single, int32 len);

/* find a substring within a string with a case insensitive search
 */
extern char * strcasestr (const char * str, const char * substr);

/* find a substring within a specified length string with a case 
 * insensitive search
 */
extern char * strncasestr (const char * str, const char * substr, int32 len);

/* thread safe version of strtok.  placeHolder contains the remainder of the 
   string being tokenized and is used as the starting point of the next search */
   
/* example:  (1st call)    token = XP_STRTOK_R(tokenString, "<token>", &placeHolder);
             (2nd call)    token = XP_STRTOK_R(nil, "<token>", &placeHolder);*/
extern char * XP_STRTOK_R(char *s1, const char *s2, char **placeHolder);

/*
 * Malloc'd string manipulation
 *
 * notice that they are dereferenced by the define!
 */
#define StrAllocCopy(dest, src) NET_SACopy (&(dest), src)
#define StrAllocCat(dest, src)  NET_SACat  (&(dest), src)
extern char * NET_SACopy (char **dest, const char *src);
extern char * NET_SACat  (char **dest, const char *src);

/*
 * Malloc'd block manipulation
 *
 * Lengths are necessary here :(
 *
 * notice that they are dereferenced by the define!
 */
#define BlockAllocCopy(dest, src, src_length) NET_BACopy((char**)&(dest), src, src_length)
#define BlockAllocCat(dest, dest_length, src, src_length)  NET_BACat(&(dest), dest_length, src, src_length)
extern char * NET_BACopy (char **dest, const char *src, size_t src_length);
extern char * NET_BACat  (char **dest, size_t dest_length, const char *src, size_t src_length);

extern char * XP_StripLine (char *s);

/* Match = 0, NoMatch = 1, Abort = -1 */
/* Based loosely on sections of wildmat.c by Rich Salz */
extern int xp_RegExpSearch(char *str, char *exp);

/* 
 * These are "safe" versions of the runtime library routines. The RTL
 * versions do not null-terminate dest IFF strlen(src) >= destLength.
 * These versions always null-terminate, which is why they're safe.
 */
extern char *XP_STRNCAT_SAFE (char *dest, const char *src, size_t len);
extern char *XP_STRNCPY_SAFE (char *dest, const char *src, size_t len);

XP_END_PROTOS

#endif /* XPSTRING_H */
