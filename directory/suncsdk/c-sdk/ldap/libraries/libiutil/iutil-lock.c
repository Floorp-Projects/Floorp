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
 * Public interface for libiutil the innosoft migration library
 *
 */

#ifdef _WINDOWS

#include "ldap.h"

#define LDAP_MUTEX_T    HANDLE

int
pthread_mutex_init( LDAP_MUTEX_T *mp, void *attr)
{
        if ( (*mp = CreateMutex(NULL, FALSE, NULL)) == NULL )
                return( 1 );
        else
                return( 0 );
}

static void *
pthread_mutex_alloc( void )
{
        LDAP_MUTEX_T *mutexp;

        if ( (mutexp = malloc( sizeof(LDAP_MUTEX_T) )) != NULL ) {
                pthread_mutex_init( mutexp, NULL );
        }
        return( mutexp );
}

int
pthread_mutex_destroy( LDAP_MUTEX_T *mp )
{
        if ( !(CloseHandle(*mp)) )
                return( 1 );
        else
                return( 0 );
}

static void
pthread_mutex_free( void *mutexp )
{
        pthread_mutex_destroy( (LDAP_MUTEX_T *) mutexp );
        free( mutexp );
}

int
pthread_mutex_lock( LDAP_MUTEX_T *mp )
{
        if ( (WaitForSingleObject(*mp, INFINITE) != WAIT_OBJECT_0) )
                return( 1 );
        else
                return( 0 );
}


int
pthread_mutex_unlock( LDAP_MUTEX_T *mp )
{
        if ( !(ReleaseMutex(*mp)) )
                return( 1 );
        else
                return( 0 );
}
#endif /* WINDOWS */
