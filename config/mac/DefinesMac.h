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

#ifndef DefinesMac_h_
#define DefinesMac_h_

/* 
	This is a common prefix file, included for both projects like
	NSStdLib and Mozilla.
*/

/* enable to start building for Carbon */
#define TARGET_CARBON 0
#if TARGET_CARBON
#define PP_Target_Carbon 1
#endif

/* Some build-wide Mac-related defines */
#define macintosh			/* macintosh is defined for GUSI */
#define XP_MAC 		1

/* We have to do this here because ConditionalMacros.h will be included from
 * within OpenTptInternet.h and will stupidly define these to 1 if they
 * have not been previously defined. The new PowerPlant (CWPro1) requires that
 * this be set to 0. (pinkerton)
 */
#define OLDROUTINENAMES 0
#ifndef OLDROUTINELOCATIONS
#define OLDROUTINELOCATIONS	0
#endif

/* OpenTransport.h has changed to not include the error messages we need from
 * it unless this is defined. Why? dunnno...(pinkerton)
 */
#define OTUNIXERRORS 1

#ifdef DEBUG
#define DEVELOPER_DEBUG 1
#else
#define NDEBUG
#endif

/* Some other random defines */
#define _NO_FAST_STRING_INLINES_ 1
#define _PR_NO_PREEMPT 1
///#define HAVE_BOOLEAN 1			// used by JPEG lib

#endif /* DefinesMac_h_ */
