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
 * The Original Code is Sun LDAP C SDK.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 *
 * Portions created by Sun Microsystems, Inc are Copyright (C) 2005
 * Sun Microsystems, Inc. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 * Use the control to get only virtual attributes.
 * First load suffix data from Example-roles.ldif.
 */

#include "examples.h"

int
main( int argc, char **argv )
{
    LDAPControl     *ctrl = NULL;
    LDAPControl     *requestctrls[ 2 ];
    char            **attrlist;
    int             version;
    LDAP            *ld;
    char            *target;
    int             rc;
    LDAPMessage     *result;
    LDAPMessage     *entry;
    char            *dn;
    char            *attr;
    BerElement      *ber;
    char            **vals;
    int             i;

    /* Prepare a virtual attributes only request control. */
    if ( !(ctrl = (LDAPControl *)malloc(sizeof(LDAPControl))) ) {
        perror( "malloc" );
        return( 1 );
    }
    ctrl->ldctl_oid = strdup( LDAP_CONTROL_VIRTUAL_ATTRS_ONLY );
    ctrl->ldctl_iscritical = 1;
    requestctrls[ 0 ] = ctrl;
    requestctrls[ 1 ] = NULL;

    /* Create a list of attributes to retrieve. */
    if ( !( attrlist = (char**)malloc(sizeof(char * [ 3 ]) ) ) ) {
        perror( "malloc" );
        return ( 1 );
    }
    attrlist[ 0 ] = "cn";               /* Real attribute */
    attrlist[ 1 ] = "nsrole";           /* Virtual attribute */
    attrlist[ 2 ] = NULL;

    /* Use LDAPv3. */
    version = LDAP_VERSION3;
    if ( ldap_set_option( NULL, LDAP_OPT_PROTOCOL_VERSION, &version )
         != 0 ) {
        fprintf( stderr,
                 "ldap_set_option protocol version to %d failed\n",
                 version );
        return ( 1 );
    }

    /* Get a handle to an LDAP connection. */
    if ( (ld = ldap_init( MY_HOST, MY_PORT )) == NULL ) {
        perror( "ldap_init" );
        return( 1 );
    }

    /* Authenticate to the directory to read an entry. */
    if ( ldap_simple_bind_s( ld, USER_DN, USER_PW ) != LDAP_SUCCESS ) {
        ldap_perror( ld, "ldap_simple_bind_s" );
        return( 1 );
    }

    /* Read an entry using the control. */
    target = "uid=kvaughan,ou=people,dc=example,dc=com";
    rc = ldap_search_ext_s( ld, target, LDAP_SCOPE_BASE, "(objectclass=*)",
        attrlist, 0, requestctrls, NULL, NULL, 0, &result );
    if ( rc != LDAP_SUCCESS ) {
        fprintf( stderr, "ldap_search_ext_s: %s\n", ldap_err2string( rc ) );
        ldap_unbind( ld );
        return( 1 );
    }

    /* Examine the results. */
    for ( entry = ldap_first_entry( ld, result );
          entry != NULL;
          entry = ldap_next_entry( ld, entry ) ) {
        if ( (dn = ldap_get_dn( ld, entry )) != NULL ) {
            printf( "dn: %s\n", dn );
            ldap_memfree( dn );
        }
        for ( attr = ldap_first_attribute( ld, entry, &ber );
              attr != NULL;
              attr = ldap_next_attribute ( ld, entry, ber) ) {
            if ( (vals = ldap_get_values( ld, entry, attr ) ) != NULL) {
                for ( i = 0; vals[i] != NULL; ++i ) {
                    printf( "%s: %s\n", attr, vals[i] );
                }
                ldap_value_free( vals );
            }
            ldap_memfree( attr );
        }
        if ( ber != NULL ) {
            ber_free( ber, 0 );
        }
    }
    printf( "\n" );

    ldap_msgfree( result );
    ldap_control_free( ctrl );
    ldap_unbind( ld );
    return( 0 );
}
