/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 
/*
 Definitions for all routines normally found in string.h, but not there in the Metrowerks
 ANSII headers.
*/

#ifndef __MACTYPES__
#include <MacTypes.h>
#endif

#ifndef __QUICKDRAW__
#include <Quickdraw.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern char *strdup(const char *source);

extern int strcasecmp(const char*, const char*);

extern int strncasecmp(const char*, const char*, int length);

extern void InitializeMacToolbox(void); // also calls InitializeSIOUX(false) if DEBUG.

#if DEBUG
extern void InitializeSIOUX(unsigned char isStandAlone);
extern Boolean IsSIOUXWindow(WindowPtr inWindow);
#endif		/* DEBUG */

#ifdef __cplusplus
}
#endif
