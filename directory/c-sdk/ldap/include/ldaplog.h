/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 * 
 * The contents of this file are subject to the Mozilla Public License Version 
 * 1.1 (the "License"); you may not use this file except in compliance with 
 * the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */

#ifndef _LDAPLOG_H
#define _LDAPLOG_H

#ifdef __cplusplus
extern "C" {
#endif

#define LDAP_DEBUG_TRACE	0x00001
#define LDAP_DEBUG_PACKETS	0x00002
#define LDAP_DEBUG_ARGS		0x00004
#define LDAP_DEBUG_CONNS	0x00008
#define LDAP_DEBUG_BER		0x00010
#define LDAP_DEBUG_FILTER	0x00020
#define LDAP_DEBUG_CONFIG	0x00040
#define LDAP_DEBUG_ACL		0x00080
#define LDAP_DEBUG_STATS	0x00100
#define LDAP_DEBUG_STATS2	0x00200
#define LDAP_DEBUG_SHELL	0x00400
#define LDAP_DEBUG_PARSE	0x00800
#define LDAP_DEBUG_HOUSE        0x01000
#define LDAP_DEBUG_REPL         0x02000
#define LDAP_DEBUG_ANY          0x04000
#define LDAP_DEBUG_CACHE        0x08000
#define LDAP_DEBUG_PLUGIN	0x10000

/* debugging stuff */
/* Disable by default */
#define LDAPDebug( level, fmt, arg1, arg2, arg3 )

#ifdef LDAP_DEBUG
#  undef LDAPDebug

/* SLAPD_LOGGING should not be on for WINSOCK (16-bit Windows) */
#  if defined(SLAPD_LOGGING)
#    ifdef _WIN32
       extern int	*module_ldap_debug;
#      define LDAPDebug( level, fmt, arg1, arg2, arg3 )	\
       { \
		if ( *module_ldap_debug & level ) { \
		        slapd_log_error_proc( NULL, fmt, arg1, arg2, arg3 ); \
	    } \
       }
#    else /* _WIN32 */
       extern int	ldap_debug;
#      define LDAPDebug( level, fmt, arg1, arg2, arg3 )	\
       { \
		if ( ldap_debug & level ) { \
		        slapd_log_error_proc( NULL, fmt, arg1, arg2, arg3 ); \
	    } \
       }
#    endif /* Win32 */
#  else /* no SLAPD_LOGGING */
     extern void ber_err_print( char * );
     extern int	ldap_debug;
#    define LDAPDebug( level, fmt, arg1, arg2, arg3 ) \
		if ( ldap_debug & level ) { \
			char msg[1024]; \
			snprintf( msg, sizeof(msg), fmt, arg1, arg2, arg3 ); \
			msg[sizeof(msg)-1] = '\0'; \
			ber_err_print( msg ); \
		}
#  endif /* SLAPD_LOGGING */
#endif /* LDAP_DEBUG */

#ifdef __cplusplus
}
#endif

#endif /* _LDAP_H */
