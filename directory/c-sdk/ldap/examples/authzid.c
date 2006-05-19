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
 * Get the authorization ID for an operation.
 */

#include "examples.h"

int
main( int argc, char **argv )
{
    int             version;
    LDAP            *ld;
    int             rc;
    LDAPControl     *authzidctrl = NULL;
    LDAPControl     *requestctrls[ 2 ];
    int             msgid;
    LDAPMessage     *result;
    int             parse_rc;
    char            *matched = NULL;
    char            *errmsg = NULL;
    char            **referrals;
    LDAPControl     **resultctrls = NULL;
    char            *authzid;

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

    /* Create a authorization ID control. */
    rc = ldap_create_authzid_control( ld, 1, &authzidctrl );
    if ( rc != LDAP_SUCCESS ) {
        fprintf( stderr, "ldap_create_authzid_control: %s\n",
                 ldap_err2string( rc ) );
        ldap_unbind( ld );
        return( 1 );
    }
    requestctrls[ 0 ] = authzidctrl;
    requestctrls[ 1 ] = NULL;

    /* Use the authorization ID control for the bind. */
    rc = ldap_set_option( ld, LDAP_OPT_SERVER_CONTROLS, &authzidctrl );
    if ( rc != LDAP_SUCCESS ) {
        ldap_perror( ld, "ldap_set_option" );
        return ( 1 );
    }

    /* Authenticate to the directory, checking for result controls. */
    msgid = ldap_simple_bind( ld, ENTRYDN, ENTRYPW );
    if ( msgid < 0 ) {
        fprintf( stderr, "ldap_simple_bind: %s\n", ldap_err2string( rc ) );
        if ( errmsg != NULL && errmsg != '\0' ) {
            fprintf( stderr, "%s\n", errmsg );
        }
        ldap_unbind( ld );
        return ( 1 );
    }

    rc = ldap_result( ld, msgid, LDAP_MSG_ALL, NULL, &result );
    if ( rc < 0 ) {
        rc = ldap_get_lderrno( ld, NULL, NULL );
        fprintf( stderr, "ldap_result: %s\n", ldap_err2string( rc ) );
        ldap_unbind( ld );
        return ( 1 );
    }

    parse_rc = ldap_parse_result( ld, result, &rc, &matched, &errmsg,
        &referrals, &resultctrls, 0 );
    if ( parse_rc != LDAP_SUCCESS ) {
        fprintf( stderr, "ldap_parse_result: %s\n", ldap_err2string( rc ) );
        ldap_unbind( ld );
        return ( 1 );
    }
    if ( rc != LDAP_SUCCESS ) {
        fprintf( stderr, "ldap_simple_bind: %s\n", ldap_err2string( rc ) );
        if ( errmsg != NULL && errmsg != '\0' ) {
            fprintf( stderr, "%s\n", errmsg );
        }
    }
    if ( resultctrls == NULL ) {
        fprintf( stderr, "No result control from server.\n" );
        ldap_unbind( ld );
        return ( 1 );
    }

    /* Show the authorization ID. */
    parse_rc = ldap_parse_authzid_control( ld, resultctrls, &authzid );
    if ( parse_rc != LDAP_SUCCESS ) {
        fprintf( stderr, "ldap_parse_authzid_control: %s\n",
            ldap_err2string( rc ) );
        ldap_unbind( ld );
        return ( 1 );
    }

    printf( "DN:       %s\n", ENTRYDN );
    printf( "Authz ID: %s\n", authzid );

    ldap_msgfree( result );
    ldap_control_free( authzidctrl );
    ldap_controls_free( resultctrls );
    ldap_unbind( ld );
    return( 0 );
}
