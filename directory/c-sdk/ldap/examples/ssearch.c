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
 * Use an SSL connection to search the directory for all people whose
 * surname (last name) is "Jensen".  Since the "sn" attribute is a
 * caseignorestring (cis), case is not significant when searching.
 *
 * Authenticate using a client certificate.
 *
 */

#include "examples.h"
#include "ldap_ssl.h"

#define MY_CERTDB		"/usr/netscape4/alias/client-cert.db"
#define MY_KEYDB		"/usr/netscape4/alias/client-key.db"
#define MY_KEYNICKNAME	"Server-Key"
#define MY_CERTNICKNAME	"Server-Cert"
#define MY_KEYPASSWD	"secret"

int
main( int argc, char **argv )
{
	LDAP		*ld;
	LDAPMessage	*result, *e;
	BerElement	*ber;
	char		*a, *dn;
	char		**vals;
	int		i;

	/* Initialize the client */
	if ( ldapssl_clientauth_init( MY_CERTDB, NULL, 1 /* need key db */,
	    MY_KEYDB, NULL ) < 0 ) {
		perror( "ldapssl_clientauth_init" );
		return( 1 );
	}

	/* get a handle to an LDAP connection */
	if ( (ld = ldapssl_init( MY_HOST, MY_SSL_PORT, 1 )) == NULL ) {
		perror( "ldapssl_init" );
		return( 1 );
	}

	/* use LDAPv3 */
	i = LDAP_VERSION3;
	if ( ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &i ) < 0 ) {
		ldap_perror( ld, "ldap_set_option LDAPv3" );
		ldap_unbind( ld );
		return( 1 );
	}

	/* enable certificate-based client authentication. */
	if ( ldapssl_enable_clientauth( ld, MY_KEYNICKNAME, MY_KEYPASSWD,
			MY_CERTNICKNAME ) != LDAP_SUCCESS ) {
		ldap_perror( ld, "ldapssl_enable_clientauth" );
		ldap_unbind( ld );
		return( 1 );
	}

	if ( ldap_sasl_bind_s( ld, NULL, LDAP_SASL_EXTERNAL, NULL, NULL, NULL,
			NULL ) != LDAP_SUCCESS ) {
		ldap_perror( ld, "ldap_sasl_bind_s EXTERNAL" );
		ldap_unbind( ld );
		return( 1 );
	}

	/* search for all entries with surname of Jensen */
	if ( ldap_search_s( ld, PEOPLE_BASE, LDAP_SCOPE_SUBTREE,
		"(sn=jensen)", NULL, 0, &result ) != LDAP_SUCCESS ) {
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
	ldap_unbind( ld );
	return( 0 );
}
