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
 * Use ldap_compare() to compare values agains values contained in entry
 * ENTRYDN (defined in examples.h)
 * We test to see if (1) the value "person" is one of the values in the
 * objectclass attribute (it is), and if (2) the value "xyzzy" is in the
 * objectlass attribute (it isn't, or at least, it shouldn't be).
 *
 */

#include "examples.h"

int
main( int main, char **argv )
{
    LDAP	*ld;
    int		rc;

    /* get a handle to an LDAP connection */
    if ( (ld = ldap_init( MY_HOST, MY_PORT )) == NULL ) {
	perror( "ldap_init" );
	return( 1 );
    }

    /* authenticate to the directory as nobody */
    if ( ldap_simple_bind_s( ld, NULL, NULL ) != LDAP_SUCCESS ) {
	ldap_perror( ld, "ldap_simple_bind_s" );
	return( 1 );
    }

    /* compare the value "person" against the objectclass attribute */
    rc = ldap_compare_s( ld, ENTRYDN, "objectclass", "person" );
    switch ( rc ) {
    case LDAP_COMPARE_TRUE:
	printf( "The value \"person\" is contained in the objectclass "
		"attribute.\n" );
	break;
    case LDAP_COMPARE_FALSE:
	printf( "The value \"person\" is not contained in the objectclass "
		"attribute.\n" );
	break;
    default:
	ldap_perror( ld, "ldap_compare_s" );
    }

    /* compare the value "xyzzy" against the objectclass attribute */
    rc = ldap_compare_s( ld, ENTRYDN, "objectclass", "xyzzy" );
    switch ( rc ) {
    case LDAP_COMPARE_TRUE:
	printf( "The value \"xyzzy\" is contained in the objectclass "
		"attribute.\n" );
	break;
    case LDAP_COMPARE_FALSE:
	printf( "The value \"xyzzy\" is not contained in the objectclass "
		"attribute.\n" );
	break;
    default:
	ldap_perror( ld, "ldap_compare_s" );
    }

    ldap_unbind( ld );
    return( 0 );
}
