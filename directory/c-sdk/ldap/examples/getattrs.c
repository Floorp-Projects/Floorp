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
 * Retrieve several attributes of a particular entry.
 */

#include "examples.h"


int
main( int argc, char **argv )
{
    LDAP		*ld;
    LDAPMessage		*result, *e;
    char		**vals, *attrs[ 5 ];
    int			i;

    /* get a handle to an LDAP connection */
    if ( (ld = ldap_init( MY_HOST, MY_PORT )) == NULL ) {
	perror( "ldap_init" );
	return( 1 );
    }

    attrs[ 0 ] = "cn";			/* Get canonical name(s) (full name) */
    attrs[ 1 ] = "sn";			/* Get surname(s) (last name) */
    attrs[ 2 ] = "mail";		/* Get email address(es) */
    attrs[ 3 ] = "telephonenumber";	/* Get telephone number(s) */
    attrs[ 4 ] = NULL;

    if ( ldap_search_s( ld, ENTRYDN, LDAP_SCOPE_BASE,
	    "(objectclass=*)", attrs, 0, &result ) != LDAP_SUCCESS ) {
	ldap_perror( ld, "ldap_search_s" );
	return( 1 );
    }

    /* print it out */
    if (( e = ldap_first_entry( ld, result )) != NULL ) {
	if (( vals = ldap_get_values( ld, e, "cn" )) != NULL ) {
	    printf( "Full name:\n" );
	    for ( i = 0; vals[i] != NULL; i++ ) {
		printf( "\t%s\n", vals[i] );
	    }
	    ldap_value_free( vals );
	}
	if (( vals = ldap_get_values( ld, e, "sn" )) != NULL ) {
	    printf( "Last name (surname):\n" );
	    for ( i = 0; vals[i] != NULL; i++ ) {
		printf( "\t%s\n", vals[i] );
	    }
	    ldap_value_free( vals );
	}
	if (( vals = ldap_get_values( ld, e, "mail" )) != NULL ) {
	    printf( "Email address:\n" );
	    for ( i = 0; vals[i] != NULL; i++ ) {
		printf( "\t%s\n", vals[i] );
	    }
	    ldap_value_free( vals );
	}
	if (( vals = ldap_get_values( ld, e, "telephonenumber" )) != NULL ) {
	    printf( "Telephone number:\n" );
	    for ( i = 0; vals[i] != NULL; i++ ) {
		printf( "\t%s\n", vals[i] );
	    }
	    ldap_value_free( vals );
	}
    }
    ldap_msgfree( result );
    ldap_unbind( ld );
    return( 0 );
}
