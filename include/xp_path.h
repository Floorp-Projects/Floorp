/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
