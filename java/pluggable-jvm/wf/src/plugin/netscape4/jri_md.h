/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is The Waterfall Java Plugin Module
 * 
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 *
 * $Id: jri_md.h,v 1.1 2001/05/10 18:12:36 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

/* -*- Mode: C; tab-width: 4; -*- */
/*******************************************************************************
 * Java Runtime Interface - Machine Dependent Types
 * Copyright (c) 1996 Netscape Communications Corporation. All rights reserved.
 ******************************************************************************/
 
#ifndef JRI_MD_H
#define JRI_MD_H

#include <assert.h>
#include "jni.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * WHAT'S UP WITH THIS FILE?
 * 
 * This is where we define the mystical JRI_PUBLIC_API macro that works on all
 * platforms. If you're running with Visual C++, Symantec C, or Borland's 
 * development environment on the PC, you're all set. Or if you're on the Mac
 * with Metrowerks, Symantec or MPW with SC you're ok too. For UNIX it shouldn't
 * matter.
 *
 * On UNIX though you probably care about a couple of other symbols though:
 *	IS_LITTLE_ENDIAN must be defined for little-endian systems
 *	HAVE_LONG_LONG must be defined on systems that have 'long long' integers
 *	HAVE_ALIGNED_LONGLONGS must be defined if long-longs must be 8 byte aligned
 *	HAVE_ALIGNED_DOUBLES must be defined if doubles must be 8 byte aligned
 *	IS_64 must be defined on 64-bit machines (like Dec Alpha)
 ******************************************************************************/

/* DLL Entry modifiers... */

/* PC */
#if defined(XP_PC) || defined(_WINDOWS) || defined(WIN32) || defined(_WIN32)
#	include <windows.h>
#	if defined(_MSC_VER)
#		if defined(WIN32) || defined(_WIN32)
#			define JRI_PUBLIC_API(ResultType)	_declspec(dllexport) ResultType
#			define JRI_CALLBACK
#		else /* !_WIN32 */
#		    if defined(_WINDLL)
#			define JRI_PUBLIC_API(ResultType)	ResultType __cdecl __export __loadds 
#			define JRI_CALLBACK			__loadds
#		    else /* !WINDLL */
#			define JRI_PUBLIC_API(ResultType)	ResultType __cdecl __export
#			define JRI_CALLBACK			__export
#                   endif /* !WINDLL */
#		endif /* !_WIN32 */
#	elif defined(__BORLANDC__)
#		if defined(WIN32) || defined(_WIN32)
#			define JRI_PUBLIC_API(ResultType)	__export ResultType
#			define JRI_CALLBACK
#		else /* !_WIN32 */
#			define JRI_PUBLIC_API(ResultType)	ResultType _cdecl _export _loadds 
#			define JRI_CALLBACK					_loadds
#		endif
#	else
#		error Unsupported PC development environment.	
#	endif
#	ifndef IS_LITTLE_ENDIAN
#		define IS_LITTLE_ENDIAN
#	endif

/* Mac */
#elif macintosh || Macintosh || THINK_C
#	if defined(__MWERKS__)				/* Metrowerks */
#		if !__option(enumsalwaysint)
#			error You need to define 'Enums Always Int' for your project.
#		endif
#		if defined(GENERATING68K) && !GENERATINGCFM 
#			if !__option(fourbyteints) 
#				error You need to define 'Struct Alignment: 68k' for your project.
#			endif
#		endif /* !GENERATINGCFM */
#	elif defined(__SC__)				/* Symantec */
#		error What are the Symantec defines? (warren@netscape.com)
#	elif macintosh && applec			/* MPW */
#		error Please upgrade to the latest MPW compiler (SC).
#	else
#		error Unsupported Mac development environment.
#	endif
#	define JRI_PUBLIC_API(ResultType)		ResultType
#	define JRI_CALLBACK

/* Unix or else */
#else
#	define JRI_PUBLIC_API(ResultType)		ResultType
#	define JRI_CALLBACK
#endif

#ifndef FAR		/* for non-Win16 */
#define FAR
#endif

/******************************************************************************/

/* Java Scalar Types */

typedef unsigned char	jbool;
#ifdef IS_64 /* XXX ok for alpha, but not right on all 64-bit architectures */
typedef unsigned int	juint;
typedef int				jint;
#else
typedef unsigned long	juint;
#endif


/******************************************************************************/
/*
** JDK Stuff -- This stuff is still needed while we're using the JDK
** dynamic linking strategy to call native methods.
*/

typedef union JRI_JDK_stack_item {
    /* Non pointer items */
    jint           i;
    jfloat         f;
    jint           o;
    /* Pointer items */
    void          *h;
    void          *p;
    unsigned char *addr;
#ifdef IS_64
    double         d;
    long           l;		/* == 64bits! */
#endif
} JRI_JDK_stack_item;

typedef union JRI_JDK_Java8Str {
    jint x[2];
    jdouble d;
    jlong l;
    void *p;
    float f;
} JRI_JDK_Java8;

#ifdef HAVE_ALIGNED_LONGLONGS
#define JRI_GET_INT64(_t,_addr) ( ((_t).x[0] = ((jint*)(_addr))[0]), \
                              ((_t).x[1] = ((jint*)(_addr))[1]),      \
                              (_t).l )
#define JRI_SET_INT64(_t, _addr, _v) ( (_t).l = (_v),                \
                                   ((jint*)(_addr))[0] = (_t).x[0], \
                                   ((jint*)(_addr))[1] = (_t).x[1] )
#else
#define JRI_GET_INT64(_t,_addr) (*(jlong*)(_addr))
#define JRI_SET_INT64(_t, _addr, _v) (*(jlong*)(_addr) = (_v))
#endif

/* If double's must be aligned on doubleword boundaries then define this */
#ifdef HAVE_ALIGNED_DOUBLES
#define JRI_GET_DOUBLE(_t,_addr) ( ((_t).x[0] = ((jint*)(_addr))[0]), \
                               ((_t).x[1] = ((jint*)(_addr))[1]),      \
                               (_t).d )
#define JRI_SET_DOUBLE(_t, _addr, _v) ( (_t).d = (_v),                \
                                    ((jint*)(_addr))[0] = (_t).x[0], \
                                    ((jint*)(_addr))[1] = (_t).x[1] )
#else
#define JRI_GET_DOUBLE(_t,_addr) (*(jdouble*)(_addr))
#define JRI_SET_DOUBLE(_t, _addr, _v) (*(jdouble*)(_addr) = (_v))
#endif

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif /* JRI_MD_H */
/******************************************************************************/
