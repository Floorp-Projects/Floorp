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

#ifndef _GUI_H_
#define _GUI_H_

/* These are defined in libnet/mkhttp.c.

  XP_AppName      The name of the client program - usually "Netscape", but
                  possibly something else for bundled versions, like the
                  MCI client.

  XP_AppCodeName  The name sent at the HTTP vendor ID string; regardless of 
                  the value of XP_AppName, this must be "Mozilla" or 
                  everything will break.

  XP_AppVersion   The version number of the client as a string.  This is the
                  string sent along with the vendor ID string, so it should be
                  of the form "1.1N (Windows)" or "1.1N (X11; SunOS 4.1.3)".

  XP_AppLanguage  The language of the navigator client.  Usually a two-letter
                  code (EN, FR) but could be a 5-letter code for translations
                  where a sub-language designation is appropriate (FR_CA)

  XP_AppPlatform  The compiled flavor of the navigator (as opposed to what 
                  it may actually be running on).
 */

XP_BEGIN_PROTOS
#if defined(XP_WIN) || defined(XP_OS2)
extern char *XP_AppName, *XP_AppCodeName, *XP_AppVersion;
extern char *XP_AppLanguage, *XP_AppPlatform;
#else
extern const char *XP_AppName, *XP_AppCodeName, *XP_AppVersion;
extern const char *XP_AppLanguage, *XP_AppPlatform;
#endif
XP_END_PROTOS

/* this define is needed for error message efficiency
 *
 * please don't comment it out for UNIX - LJM
 */
/* this is constant across languages - do NOT localize it */
#define XP_CANONICAL_CLIENT_NAME "Netscape"

/* name of the program */
/* XP_LOCAL_CLIENT_NAME was never used consistently: use XP_AppName instead. */

#endif /* _GUI_H_ */
