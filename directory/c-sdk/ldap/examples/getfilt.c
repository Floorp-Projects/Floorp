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
 * Search for a user-provided string, using a filter configuration file.
 * The filter config file controls which type of search is done, based
 * on what the user types.
 */

#include "examples.h"

int
main( int argc, char **argv )
{
    LDAP		*ld;
    LDAPMessage		*result, *e;
    BerElement		*ber;
    char		*a, *dn;
    char		**vals;
    int			i;
    LDAPFiltDesc	*ldfp;
    LDAPFiltInfo	*ldfi;
    char		buf[ 80 ];
    int			found;

    /* Open the filter config file */
    if (( ldfp = ldap_init_getfilter( MY_FILTERFILE )) == NULL ) {
	fprintf( stderr, "Can't open filter file \"%s\"\n", MY_FILTERFILE );
	return( 1 );
    }
    /* Read a string to search for */
    printf( "Enter a string to search for: " );
    gets( buf );
    if ( strlen( buf ) == 0 ) {
	fprintf( stderr, "usage: %s search-string\n", argv[ 0 ]);
	return( 1 );
    }
    /* get a handle to an LDAP connection */
    printf( "Initializing LDAP library... " ); fflush( stdout );
    if ( (ld = ldap_init( MY_HOST, MY_PORT )) == NULL ) {
	perror( "ldap_init" );
	return( 1 );
    }
    printf( "done.\n" ); fflush( stdout );
    /* authenticate to the directory as nobody */
    printf( "Binding to the directory... " ); fflush( stdout );
    if ( ldap_simple_bind_s( ld, NULL, NULL ) != LDAP_SUCCESS ) {
	ldap_perror( ld, "ldap_simple_bind_s" );
	return( 1 );
    }
    printf( "done.\n" ); fflush( stdout );
    /* search for what the user typed */
    found = 0;
    for ( ldfi = ldap_getfirstfilter( ldfp, MY_FILTERTAG, buf );
	    ldfi != NULL; ldfi = ldap_getnextfilter( ldfp )) {
	printf( "Filter: \"%s\"... ", ldfi->lfi_filter );
	fflush( stdout );
	if ( ldap_search_s( ld, MY_SEARCHBASE, ldfi->lfi_scope,
		ldfi->lfi_filter, NULL, 0, &result ) != LDAP_SUCCESS ) {
	    ldap_perror( ld, "ldap_search_s" );
	    return( 1 );
	} else {
	    found = ldap_count_entries( ld, result );
	    printf( "done.  Entries found: %d\n", found ); fflush( stdout );
	    if ( found > 0 ) {
		/* Matching entries found - break */
		break;
	    } else {
		ldap_msgfree( result );
	    }
	}
    }
    if ( found == 0  ) {
	printf( "No matching entries found\n" );
    } else {
	printf( "Found %d %s match%s for \"%s\"\n\n", found,
		ldfi->lfi_desc, found == 1 ? "" : "es", buf );
	/* for each entry print out name + all attrs and values */
	for ( e = ldap_first_entry( ld, result ); e != NULL;
		e = ldap_next_entry( ld, e ) ) {
	    if ( (dn = ldap_get_dn( ld, e )) != NULL ) {
		printf( "dn: %s\n", dn );
		ldap_memfree( dn );
	    }
	    for ( a = ldap_first_attribute( ld, e, &ber );
		    a != NULL; a = ldap_next_attribute( ld, e, ber ) ) {
		if (( vals = ldap_get_values( ld, e, a )) != NULL ) {
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
    }
    ldap_getfilter_free( ldfp );
    ldap_unbind( ld );
    return( 0 );
}
