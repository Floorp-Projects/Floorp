/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*-----------------------------------------------------------------------------
    xp_core.h
    Cross-Platform Core Types
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
    Platform-specific defines

        XP_WIN              XP_IS_WIN       XP_WIN_ARG(X)
        XP_MAC              XP_IS_MAC         XP_MAC_ARG(X)
        XP_UNIX             XP_IS_UNIX        XP_UNIX_ARG(X)
        XP_CPLUSPLUS        XP_IS_CPLUSPLUS
        XP_OS2              XP_IS_OS2          XP_OS2_ARG(X)  IBM-LTB added these
        defined iff         always defined    defined to nothing
        on that platform    as 0 or 1         or X

        Also Bool, Int32, Int16, Int, Uint32, Uint16, Uint, and nil
        And TRUE, FALSE, ON, OFF, YES, NO
-----------------------------------------------------------------------------*/

#include "prtypes.h"	/* for intn, etc. */
#include "prlog.h"

#ifndef _XP_Core_
#define _XP_Core_

/* which system are we on, get the base macro defined */

#if defined(macintosh) || defined(__MWERKS__) || defined(applec)
#ifndef macintosh
#define macintosh 1
#endif
#endif

#if defined(__unix) || defined(unix) || defined(UNIX) || defined(XP_UNIX)
#ifndef unix
#define unix 1
#endif
#endif

#if !defined(macintosh) && !defined(_WINDOWS) && !defined(unix)
  /* #error xp library can't determine system type */
#endif

/* flush out all the system macros */

#ifdef macintosh
# ifndef XP_MAC
# define XP_MAC 1
# endif
# define XP_IS_MAC 1
# define XP_MAC_ARG(x) x
#else
# define XP_IS_MAC 0
# define XP_MAC_ARG(x)
#endif

#ifdef _WINDOWS
# ifndef XP_WIN
# define XP_WIN
# endif
# define XP_IS_WIN 1
# define XP_WIN_ARG(x) x
#if defined(_WIN32) || defined(WIN32)
# ifndef XP_WIN32
# define XP_WIN32
# endif
#else
# ifndef XP_WIN16
# define XP_WIN16
# endif
#endif
#else
# define XP_IS_WIN 0
# define XP_WIN_ARG(x)
#endif

#ifdef unix
# ifndef XP_UNIX
# define XP_UNIX
# endif
# define XP_IS_UNIX 1
# define XP_UNIX_ARG(x) x
#else
# define XP_IS_UNIX 0
# define XP_UNIX_ARG(x)
#endif

/*  IBM-LTB  Setup system macros for OS/2   */
#if defined (__OS2__)
# ifndef XP_OS2
# define XP_OS2
# endif
# define XP_IS_OS2 1
# define XP_OS2_ARG(x) x
#else
# define XP_IS_OS2 0
# define XP_OS2_ARG(x)
#endif

/* what language do we have? */

#if defined(__cplusplus)
# define XP_CPLUSPLUS
# define XP_IS_CPLUSPLUS 1
#else
# define XP_IS_CPLUSPLUS 0
#endif

#if defined(DEBUG) || !defined(XP_CPLUSPLUS)
#define XP_REQUIRES_FUNCTIONS
#endif

/*
	language macros
	
	If C++ code includes a prototype for a function *compiled* in C, it needs to be
	wrapped in extern "C" for the linking to work properly. On the Mac, all code is
	being compiled in C++ so this isn't necessary, and Unix compiles it all in C. So
	only Windows actually will make use of the defined macros.
*/

#if defined(XP_CPLUSPLUS)
# define XP_BEGIN_PROTOS extern "C" {
# define XP_END_PROTOS }
#else
# define XP_BEGIN_PROTOS
# define XP_END_PROTOS
#endif

/* simple common types */

#ifdef XP_MAC
#include <Types.h>

	#if __option(bool)
		typedef bool BOOL;
		typedef bool Bool;
		typedef bool XP_Bool;
	#else
		typedef char BOOL;
	    typedef char Bool;
	    typedef char XP_Bool;
    #endif

#elif defined(XP_WIN)
    typedef int Bool;
    typedef int XP_Bool;
#elif defined(XP_OS2) && !defined(RC_INVOKED)
    typedef unsigned long Bool;
    typedef unsigned long XP_Bool;
#else
    /*  XP_UNIX: X11/Xlib.h "define"s Bool to be int. This is really lame
     *  (that's what typedef is for losers). So.. in lieu of a #undef Bool
     *  here (Xlib still needs ints for Bool-typed parameters) people have
     *  been #undef-ing Bool before including this file.
     *  Can we just #undef Bool here? <mailto:mcafee> (help from djw, converse)
     */
#ifdef Bool
#undef Bool
#endif
    typedef char Bool;
    typedef char XP_Bool;
#endif

/* this should just go away, as nspr has it. */
#if !defined(XP_WIN) && !defined(XP_UNIX) && !defined(XP_OS2) && !defined(XP_MAC)
typedef int (*FARPROC)();
#endif

#if defined(XP_WIN)
#ifndef BOOL
#define BOOL Bool
#endif
#define MIN(a, b)     min((a), (b))
#endif

#if defined(XP_OS2_VACPP)
#ifndef MIN
#define MIN(a, b)     min((a), (b))
#endif
#endif

#if (defined(XP_UNIX) || defined(XP_MAC)) && !defined(MIN)
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef nil
#define nil 0
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef TRACEMSG
#ifdef DEBUG
#define TRACEMSG(msg)  do { if(MKLib_trace_flag)  XP_Trace msg; } while (0)
#else
#define TRACEMSG(msg)
#endif /* DEBUG */
#endif /* TRACEMSG */

/* removed #ifdef for hpux defined in /usr/include/model.h */
#ifndef XP_MAC
#ifndef _INT16
#define _INT16
#endif
#ifndef _INT32
#define _INT32
#endif
#define _UINT16
#define _UINT32
#endif

/* function classifications */

#define PUBLIC
#define MODULE_PRIVATE

#if defined(XP_UNIX) && defined(PRIVATE)
#undef PRIVATE
#endif
#define PRIVATE static

/* common typedefs */
typedef struct _XP_List XP_List;

/* standard system headers */

#if !defined(RC_INVOKED)
#include <assert.h>
#include <ctype.h>
#ifdef __sgi
# include <float.h>
# include <sys/bsd_types.h>
#endif
#ifdef XP_UNIX
#include <stdarg.h>
#endif
#include <limits.h>
#include <locale.h>
#if defined(XP_WIN) && defined(XP_CPLUSPLUS) && defined(_MSC_VER) && _MSC_VER >= 1020
/* math.h under MSVC 4.2 needs C++ linkage when C++. */
extern "C++"    {
#include <math.h>
}
#elif (defined(__hpux) || defined(SCO)) && defined(__cplusplus)
extern "C++"    {
#include <math.h>
}
#else
#include <math.h>
#endif
#ifdef XP_OS2
/*DSR021097 - on OS/2 we have conflicts over jmp_buf & HW_THREADS */
#ifdef SW_THREADS
#include <setjmp.h>
#endif
#else /*!XP_OS2*/
#include <setjmp.h>
#endif
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#endif

#endif /* _XP_Core_ */
