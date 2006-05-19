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
