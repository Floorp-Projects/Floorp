/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef DefinesMac_h_
#define DefinesMac_h_

/* 
	This is a common prefix file, included for both projects like
	NSStdLib and Mozilla.
*/

/* enable to start building for Carbon */
#if TARGET_CARBON
#define PP_Target_Carbon 1
#define OLDP2C 1
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
