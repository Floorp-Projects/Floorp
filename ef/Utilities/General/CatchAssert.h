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

#include "Exports.h"

#ifndef XP_PC
#include <assert.h>
#else

/* Define __cdeck for non MS compilers. */
#if	( !defined(_MSC_VER) && !defined(__cdecl) )
#   define __cdecl
#endif


/* Use the catchAssert instead of the regular assert. */
#undef	assert

/*  If the build target is optimized, don't include assert. */
#ifdef NDEBUG

#   define assert(exp)	((void)0)

#else

/* 
 * Define an assert function that doesn't have a dialog pop-up, 
 * which would require user intervention. 
 *
 */
NS_EXTERN void __cdecl catchAssert(void *, void *, unsigned);

#   define assert(exp) (void)( (exp) || (catchAssert(#exp, __FILE__, __LINE__), 0) )

#   endif	/* NDEBUG */

#endif //XP_MAC

