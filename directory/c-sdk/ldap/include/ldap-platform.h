/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 * 
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */

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
