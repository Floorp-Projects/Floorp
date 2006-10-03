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
 * Search the directory for all people. Ask the server to sort the entries
 * first by sn (last name) and then in reverse order by givenname (first name).
 */

#include "examples.h"

int
main( int argc, char **argv )
{
	LDAP			*ld;
	LDAPMessage		*result, *e;
	char			*attrfail, *matched = NULL, *errmsg = NULL;
	char			**vals, **referrals;
	int			rc, parse_rc, version;
	ber_int_t		sortrc;
	LDAPControl		*sortctrl = NULL;
	LDAPControl		*requestctrls[ 2 ];
	LDAPControl		**resultctrls = NULL;
	LDAPsortkey		**sortkeylist;

	/* Arrange for all connections to use LDAPv3 */
	version = LDAP_VERSION3;
	if ( ldap_set_option( NULL, LDAP_OPT_PROTOCOL_VERSION, &version )
	    != 0 ) {
		fprintf( stderr,
		    "ldap_set_option protocol version to %d failed\n",
		    version );
		return( 1 );
	}

	/* Get a handle to an LDAP connection */
	if ( (ld = ldap_init( MY_HOST, MY_PORT ) ) == NULL ) {
		perror( "ldap_init" );
		return( 1 );
	}

	/* Authenticate as Directory Manager */
	if ( ldap_simple_bind_s( ld, MGR_DN, MGR_PW ) != LDAP_SUCCESS ) {
		ldap_perror( ld, "ldap_simple_bind_s" );
		ldap_unbind( ld );
		return( 1 );
	}

	/*
	 * Create a sort key list that specifies the sort order of the results.
	 * Sort the results by last name first, then by first name.
	 */
	ldap_create_sort_keylist( &sortkeylist, "description -givenname" );

	/* Create the sort control. */
	rc = ldap_create_sort_control( ld, sortkeylist, 1, &sortctrl );
	if ( rc != LDAP_SUCCESS ) {
		fprintf( stderr, "ldap_create_sort_control: %s\n",
		    ldap_err2string( rc ) );
		ldap_unbind( ld );
		return( 1 );
	}
	requestctrls[ 0 ] = sortctrl;
	requestctrls[ 1 ] = NULL;

	/* Search for all entries in Sunnyvale */

	rc = ldap_search_ext_s( ld, PEOPLE_BASE, LDAP_SCOPE_SUBTREE,
	    "(objectclass=person)", NULL, 0, requestctrls, NULL, NULL, 0,
	    &result );

	if ( rc != LDAP_SUCCESS ) {
		fprintf( stderr, "ldap_search_ext_s: %s\n",
		    ldap_err2string( rc ) );
		ldap_unbind( ld );
		return( 1 );
	}

	parse_rc = ldap_parse_result( ld, result, &rc, &matched, &errmsg,
	    &referrals, &resultctrls, 0 );

	if ( parse_rc != LDAP_SUCCESS ) {
		fprintf( stderr, "ldap_parse_result: %s\n",
		    ldap_err2string( parse_rc ) );
		ldap_unbind( ld );
		return( 1 );
	}

	if ( rc != LDAP_SUCCESS ) {
		fprintf( stderr, "ldap_search_ext_s: %s\n",
		    ldap_err2string( rc ) );
		if ( errmsg != NULL && *errmsg != '\0' ) {
			fprintf( stderr, "%s\n", errmsg );
		}

		ldap_unbind( ld );
		return( 1 );
	}

	parse_rc = ldap_parse_sort_control( ld, resultctrls, &sortrc,
	    &attrfail );

	if ( parse_rc != LDAP_SUCCESS ) {
		fprintf( stderr, "ldap_parse_sort_control: %s\n",
		    ldap_err2string( parse_rc ) );
		ldap_unbind( ld );
		return( 1 );
	}

	if ( sortrc != LDAP_SUCCESS ) {
		fprintf( stderr, "Sort error: %s\n", ldap_err2string( sortrc ));
		if ( attrfail != NULL && *attrfail != '\0' ) {
			fprintf( stderr, "Bad attribute: %s\n", attrfail);
		}
		ldap_unbind( ld );
		return( 1 );
	}

	/* for each entry print out name + all attrs and values */
	for ( e = ldap_first_entry( ld, result ); e != NULL;
	    e = ldap_next_entry( ld, e ) ) {
		if ((vals = ldap_get_values( ld, e, "sn")) != NULL ) {
			if ( vals[0] != NULL ) {
				printf( "%s", vals[0] );
			}
			ldap_value_free( vals );
		}

		if ((vals = ldap_get_values( ld, e, "givenname")) != NULL ) {
			if ( vals[0] != NULL ) {
				printf( "\t%s", vals[0] );
			}
			ldap_value_free( vals );
		}

		putchar( '\n' );
	}

	ldap_msgfree( result );
	ldap_free_sort_keylist( sortkeylist );
	ldap_control_free( sortctrl );
	ldap_controls_free( resultctrls );
	ldap_unbind( ld );

	return( 0 );
}
