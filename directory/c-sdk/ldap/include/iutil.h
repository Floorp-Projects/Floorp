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

/*
 * Interface for libiutil the innosoft migration library
 *
 */

#ifndef _IUTIL_H
#define _IUTIL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* from iutil-lock.c */

#ifdef _WINDOWS
#define LDAP_MUTEX_T HANDLE

extern		char *ldap_strdup();
extern		unsigned char *ldap_utf8_nextchar();
extern		char **ldap_explode_ava();
extern		int ldap_utf8_toupper();

int		pthread_mutex_init( LDAP_MUTEX_T *mp, void *attr);
static void  *	pthread_mutex_alloc( void );
int		pthread_mutex_destroy( LDAP_MUTEX_T *mp );
static void	pthread_mutex_free( void *mutexp );
int		pthread_mutex_lock( LDAP_MUTEX_T *mp );
int		pthread_mutex_unlock( LDAP_MUTEX_T *mp );

#endif /* _WINDOWS */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _IUTIL_H */
