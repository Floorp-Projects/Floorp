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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/BasicDefines.h>									*/
/* Description:	Xfe widgets basic defines.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeBasicDefines_h_						/* start BasicDefines.h	*/
#define _XfeBasicDefines_h_

/*----------------------------------------------------------------------*/
/*																		*/
/* Include the main Xt header for the various types (Widget, etc).		*/
/*																		*/
/*----------------------------------------------------------------------*/
#include <X11/Intrinsic.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Macros to protect C code from the evils of C++.						*/
/*																		*/
/*----------------------------------------------------------------------*/
#ifdef __cplusplus

#ifndef XFE_BEGIN_CPLUSPLUS_PROTECTION
#define XFE_BEGIN_CPLUSPLUS_PROTECTION extern "C" {
#endif

#ifndef XFE_END_CPLUSPLUS_PROTECTION
#define XFE_END_CPLUSPLUS_PROTECTION }
#endif

#else

#define XFE_BEGIN_CPLUSPLUS_PROTECTION
#define XFE_END_CPLUSPLUS_PROTECTION

#endif

#endif											/* end BasciDefines.h	*/
