/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef jsosdep_h___
#define jsosdep_h___
/*
 * OS (and machine, and compiler XXX) dependent information.
 */

#ifdef MOZILLA_CLIENT
#include "platform.h"
#endif

#ifdef XP_PC

#ifdef _WIN32
#define JS_HAVE_LONG_LONG
#else
#undef JS_HAVE_LONG_LONG
#endif
#endif /* XP_PC */

#ifdef XP_MAC

JS_BEGIN_EXTERN_C

#include <stddef.h>

extern void* reallocSmaller(void* block, size_t newSize);

extern char* strdup(const char* str);

JS_END_EXTERN_C

#endif /* XP_MAC */

#ifdef XP_UNIX

/*
 * Get OS specific header information.
 */
#if defined(AIXV3) || defined(AIX)
#define JS_HAVE_LONG_LONG

#elif defined(BSDI)
#define JS_HAVE_LONG_LONG

#elif defined(HPUX)
#undef JS_HAVE_LONG_LONG

#elif defined(IRIX)
#define JS_HAVE_LONG_LONG

#elif defined(linux)
#define JS_HAVE_LONG_LONG

#elif defined(OSF1)
#define JS_HAVE_LONG_LONG

#elif defined(SCO)
#undef JS_HAVE_LONG_LONG

#elif defined(SOLARIS)
#define JS_HAVE_LONG_LONG

#elif defined(SUNOS4)
#undef JS_HAVE_LONG_LONG

/*
** Missing function prototypes
*/

extern void *sbrk(int);

#elif defined(UNIXWARE)
#undef JS_HAVE_LONG_LONG
#endif

#endif /* XP_UNIX */

#endif /* jsosdep_h___ */
