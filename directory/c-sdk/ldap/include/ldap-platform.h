/*
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

/* ldap-platform.h - platform transparency */

#ifndef _LDAP_PLATFORM_H
#define _LDAP_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined (WIN32) || defined (_WIN32) || defined( _CONSOLE ) 
#include <windows.h>
#  if defined( _WINDOWS )
#  include <winsock.h>
#  endif
#elif defined(macintosh)
#ifndef LDAP_TYPE_TIMEVAL_DEFINED
#include <utime.h>
#endif
#ifndef LDAP_TYPE_SOCKET_DEFINED	/* API extension */
#include "macsocket.h"
#endif
#else /* everything else, e.g., Unix */
#ifndef LDAP_TYPE_TIMEVAL_DEFINED
#include <sys/time.h>
#endif
#ifndef LDAP_TYPE_SOCKET_DEFINED	/* API extension */
#include <sys/types.h>
#include <sys/socket.h>
#endif
#endif

#ifdef _AIX
#include <sys/select.h>
#endif /* _AIX */

#ifdef XP_OS2
#include <sys/select.h>
#endif
#ifdef XP_OS2_EMX
/*
 * EMX-specific tweaks:
 *    o Use stricmp instead of strcmpi.
 */
#define strcmpi stricmp
#endif

/*
 * LDAP_API macro definition:
 */
#ifndef LDAP_API
#if defined( _WINDOWS ) || defined( _WIN32 )
#define LDAP_API(rt) rt
#else /* _WINDOWS */
#define LDAP_API(rt) rt
#endif /* _WINDOWS */
#endif /* LDAP_API */

#ifdef __cplusplus
}
#endif
#endif /* _LDAP_PLATFORM_H */
