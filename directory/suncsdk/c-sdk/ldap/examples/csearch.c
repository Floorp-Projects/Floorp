/*
 * Copyright 2005 Sun Microsystems, Inc. All Rights Reserved
 * Use of this product is subject to license terms.
 */

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
 * Enable the in-memory cache and then search the directory 10 times
 * for all people whose surname (last name) is "Jensen".  Since the
 * "sn" attribute is a caseignorestring (cis), case is not significant
 * when searching.
 *
 */

#include "examples.h"

LDAP		*ld;
LDAPMessage	*result, *e;
BerElement	*ber;
char		*a, *dn;
char		**vals;
int		i, j;
LDAPMemCache   *cacheHandle;
int            memCache;

int mySearch();

int
main( int argc, char **argv )
{
	/* get a handle to an LDAP connection */
	if ( (ld = ldap_init( MY_HOST, MY_PORT )) == NULL ) {
		perror( "ldap_init" );
		return( 1 );
	}

        memCache=ldap_memcache_init( 1800, 1024L * 1024, NULL, NULL, &cacheHandle );

        switch ( memCache )
        {
        case LDAP_SUCCESS :
          printf("LDAP_SUCCESS\n");
          printf("LDAP_SUCCESS = %d\n", LDAP_SUCCESS);
          printf("memCache = %d\n", memCache);
          break;
        case  LDAP_PARAM_ERROR:
          printf("LDAP_PARAM_ERROR\n");
          break;
        case  LDAP_NO_MEMORY:
          printf("LDAP_NO_MEMORY\n");
          break;
        case  LDAP_SIZELIMIT_EXCEEDED:
          printf("LDAP_SIZELIMITE_EXCEEDED\n");
          break;
        default :
          printf("Unknown Error = %d\n", memCache);
          break;
        };

	/* authenticate to the directory as nobody */
	if ( ldap_simple_bind_s( ld, NULL, NULL ) != LDAP_SUCCESS ) {
		ldap_perror( ld, "ldap_simple_bind_s" );
		ldap_unbind( ld );
		return( 1 );
	}

       ldap_memcache_set( ld, cacheHandle );


	for ( j = 0; j < 10; ++j ) {
#ifdef _WINDOWS
	    Sleep( 1000 );
#else
	    sleep( 1 );
#endif
	    mySearch();
	}

	ldap_memcache_destroy( cacheHandle );
	ldap_unbind( ld );

	return 0;
}

int mySearch()
{
	/* search for all entries with surname of Jensen */
	if ( ldap_search_s( ld, MY_SEARCHBASE, LDAP_SCOPE_SUBTREE,
		MY_FILTER, NULL, 0, &result ) != LDAP_SUCCESS ) {
		ldap_perror( ld, "ldap_search_s" );
	    if ( result == NULL ) {
		ldap_unbind( ld );
		return( 1 );
	    }
	}
	/* for each entry print out name + all attrs and values */
	for ( e = ldap_first_entry( ld, result ); e != NULL;
	    e = ldap_next_entry( ld, e ) ) {
		if ( (dn = ldap_get_dn( ld, e )) != NULL ) {
		    printf( "dn: %s\n", dn );
		    ldap_memfree( dn );
		}
		for ( a = ldap_first_attribute( ld, e, &ber );
		    a != NULL; a = ldap_next_attribute( ld, e, ber ) ) {
			if ((vals = ldap_get_values( ld, e, a)) != NULL ) {
				for ( i = 0; vals[i] != NULL; i++ ) {
				    printf( "%s: %s\n", a, vals[i] );
				}
				ldap_value_free( vals );
			}
			ldap_memfree( a );
		}
		if ( ber != NULL ) {
			ber_free( ber, 0 );
		}
		printf( "\n" );
	}
	ldap_msgfree( result );
	return( 0 );
}
