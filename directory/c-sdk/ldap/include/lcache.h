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

/* lcache.h - ldap persistent cache */
#ifndef _LCACHE_H
#define _LCACHE_H

#ifdef __cplusplus
extern "C" {
#endif

/* calling conventions used by library */
#ifndef LDAP_CALL
#if defined( _WINDOWS ) || defined( _WIN32 )
#define LDAP_C __cdecl
#ifndef _WIN32 
#define __stdcall _far _pascal
#define LDAP_CALLBACK _loadds
#else
#define LDAP_CALLBACK
#endif /* _WIN32 */
#define LDAP_PASCAL __stdcall
#define LDAP_CALL LDAP_PASCAL
#else /* _WINDOWS */
#define LDAP_C
#define LDAP_CALLBACK
#define LDAP_PASCAL
#define LDAP_CALL
#endif /* _WINDOWS */
#endif /* LDAP_CALL */

LDAP_API(int) LDAP_C lcache_init( LDAP *ld, void *arg );
LDAP_API(int) LDAP_C lcache_bind( LDAP *ld, int msgid, unsigned long tag,
	const char *dn, struct berval *cred, int method );
LDAP_API(int) LDAP_C lcache_unbind( LDAP *ld, int msgid, unsigned long tag );
LDAP_API(int) LDAP_C lcache_search( LDAP *ld, int msgid, unsigned long tag,
	const char *dn, int scope, const char *filter, char **attrs,
	int attrsonly );
LDAP_API(int) LDAP_C lcache_compare( LDAP *ld, int msgid, unsigned long tag,
	const char *dn, const char *attr, struct berval *val );
LDAP_API(int) LDAP_C lcache_add( LDAP *ld, int msgid, unsigned long tag,
	const char *dn, LDAPMod **entry );
LDAP_API(int) LDAP_C lcache_delete( LDAP *ld, int msgid, unsigned long tag,
	const char *dn );
LDAP_API(int) LDAP_C lcache_rename( LDAP *ld, int msgid, unsigned long tag,
	const char *dn, const char *newrdn, const char *newparent, 
	int deleteoldrdn );
LDAP_API(int) LDAP_C lcache_modify( LDAP *ld, int msgid, unsigned long tag,
	const char *dn, LDAPMod **mods );
LDAP_API(int) LDAP_C lcache_modrdn( LDAP *ld, int msgid, unsigned long tag,
	const char *dn, const char *newrdn, int deleteoldrdn );
LDAP_API(int) LDAP_C lcache_result( LDAP *ld, int msgid, int all,
	struct timeval *timeout, LDAPMessage **result );
LDAP_API(int) LDAP_C lcache_flush( LDAP *ld, char *dn, char *filter );

#ifdef __cplusplus
}
#endif

#endif /* _LCACHE_H */
