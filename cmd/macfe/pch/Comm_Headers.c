/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Comm_Headers.c
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define macintosh			// macintosh is defined for GUSI
#define XP_MAC 1

// we have to do this here because ConditionalMacros.h will be included from
// within OpenTptInternet.h and will stupidly define these to 1 if they
// have not been previously defined. The new PowerPlant (CWPro1) requires that
// this be set to 0. (pinkerton)
#define OLDROUTINENAMES 0
#ifndef OLDROUTINELOCATIONS
	#define OLDROUTINELOCATIONS	0
#endif

// OpenTransport.h has changed to not include the error messages we need from
// it unless this is defined. Why? dunnno...(pinkerton)
#define OTUNIXERRORS 1

#ifndef DEBUG
	#define NDEBUG
#endif

// #include <osl.h> for some reason, including this messes up PP compiles, we get 
// "already defined" errors

// Unix headers
#include <unistd.h>
#ifdef __MATH__
#errror scream
#endif
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <setjmp.h>
#ifdef __MATH__
#errror scream
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
// #include <yvals.h>
#include <OpenTptInternet.h>		// include this first for errno's

#include <PP_MacHeaders.c>			// Toolbox headers
									// Action Classes
#include <Sound.h>			// Toolbox headers

// no more compat.h
#ifndef	MAX
	#define	MAX(_a,_b)	((_a)<(_b)?(_b):(_a))
#endif

#ifndef	MIN
	#define	MIN(_a,_b)	((_a)<(_b)?(_a):(_b))
#endif
