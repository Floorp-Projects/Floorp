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

/*
 * The contents of this file used to be part of prosdep.h,
 * a private NSPR header file of the NSPR module.  The macros
 * defined in this file are the only things the Mozilla
 * client needs from prosdep.h, so I extracted them and put
 * them in this new file.
 *   -- Wan-Teh Chang <wtc@netscape.com>, 18 September 1998
 */

#ifndef _XP_PATH_H
#define _XP_PATH_H

#if defined(XP_UNIX) || defined(XP_MAC) || defined(XP_BEOS)
#define PR_DIRECTORY_SEPARATOR '/'
#define PR_DIRECTORY_SEPARATOR_STR "/"
#define PR_PATH_SEPARATOR ':'
#define PR_PATH_SEPARATOR_STR ":"
#elif defined(XP_PC) /* Win32, Win16, and OS/2 */
#define PR_DIRECTORY_SEPARATOR '\\'
#define PR_DIRECTORY_SEPARATOR_STR "\\"
#define PR_PATH_SEPARATOR ';'
#define PR_PATH_SEPARATOR_STR ";"
#else
#error "Unknown platform"
#endif

#ifdef XP_MAC
#define PATH_SEPARATOR_STR ":"
#define DIRECTORY_SEPARATOR_STR "/"
#define MAXPATHLEN 512
#endif

#endif /* _XP_PATH_H */
