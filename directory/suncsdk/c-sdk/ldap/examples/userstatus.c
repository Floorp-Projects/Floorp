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
 * Get account status using the account status control.
 */

#include "examples.h"

int
main( int argc, char **argv )
{
    LDAPuserstatus  *status;
    int             version;
    LDAP            *ld;
    int             rc;
    LDAPControl     *status_ctrl = NULL;
    LDAPControl     *requestctrls[ 2 ];
    LDAPMessage     *result;
    char            *matched = NULL;
    char            *errmsg = NULL;
    char            **referrals;
    LDAPControl     **resultctrls = NULL;
    LDAPMessage     *msg;
    LDAPControl     **ectrls = NULL;

    /* Allocate the LDAPuserstatus structure. */
    if ( !( status = (LDAPuserstatus*)malloc(sizeof(LDAPuserstatus)) ) ) {
        perror("malloc");
        return ( 1 );
    }

    /* Use LDAPv3. */
    version = LDAP_VERSION3;
    if ( ldap_set_option( NULL, LDAP_OPT_PROTOCOL_VERSION, &version )
         != 0 ) {
            fprintf( stderr,
                "ldap_set_option protocol version to %d failed\\n",
                version );
            return ( 1 );
    }

    /* Get a handle to an LDAP connection. */
    if ( (ld = ldap_init( MY_HOST, MY_PORT )) == NULL ) {
        perror( "ldap_init" );
        return( 1 );
    }

    /* Create an account status control. */
    rc = ldap_create_userstatus_control( ld, 1, &status_ctrl);
    if ( rc != LDAP_SUCCESS ) {
        fprintf( stderr, "ldap_create_userstatus_control: %s\\n",
                 ldap_err2string( rc ) );
        ldap_unbind( ld );
        return( 1 );
    }
    requestctrls[ 0 ] = status_ctrl;
    requestctrls[ 1 ] = NULL;

    /* Authenticate to the directory as a user. */
    if ( ldap_simple_bind_s( ld, USER_DN, USER_PW ) != LDAP_SUCCESS ) {
        ldap_perror( ld, "ldap_simple_bind_s" );
        return( 1 );
    }

    /* Read the account entry using the control. */
    rc = ldap_search_ext_s( ld, ENTRYDN, LDAP_SCOPE_BASE,
        "(objectclass=*)", NULL, 0, requestctrls, NULL, NULL, 0, &result );
    if ( rc != LDAP_SUCCESS ) {
        fprintf( stderr, "ldap_search_ext_s: %s\\n", ldap_err2string( rc ) );
        ldap_unbind( ld );
        return( 1 );
    }

    /* Show the account status. */
    rc = ldap_parse_result( ld, result, &rc, &matched, &errmsg,
        &referrals, &resultctrls, 0 );
    if ( rc != LDAP_SUCCESS ) {
        fprintf( stderr, "ldap_parse_result: %s\\n", ldap_err2string( rc ) );
        ldap_unbind( ld );
        return( 1 );
    }

    for ( msg = ldap_first_message( ld, result );
          msg != NULL;
          msg = ldap_next_message ( ld, msg) ) {
        if ( ldap_msgtype( msg ) != LDAP_RES_SEARCH_ENTRY ) continue;
        if ( ldap_get_entry_controls( ld, msg, &ectrls ) != LDAP_SUCCESS ) {
            ldap_perror ( ld, "ldap_get_entry_controls" );
        } else {
            rc = ldap_parse_userstatus_control( ld, ectrls, status );
            if ( rc != LDAP_SUCCESS ) {
                fprintf( stderr,
                    "ldap_parse_userstatus_control: %s\\n",
                    ldap_err2string( rc ) );
            } else {
                printf( "DN: %s\\n", ENTRYDN );
                if ( LDAP_US_ACCOUNT_USABLE == status->us_available ) {
                    printf( " Account is usable:\\tY\\n" );
                } else {
                    printf( " Account is usable:\\tN\\n" );
                }
                printf( " Password expires in:\\t%ld s\\n",
                    status->us_expire );
                if ( LDAP_US_ACCOUNT_INACTIVE == status->us_inactive ) {
                    printf( " Account is locked:\\tY\\n" );
                } else {
                    printf( " Account is locked:\\tN\\n" );
                }
                if ( LDAP_US_ACCOUNT_RESET == status->us_reset ) {
                    printf( " Password was reset:\\tY\\n" );
                } else {
                    printf( " Password was reset:\\tN\\n" );
                }
                if ( LDAP_US_ACCOUNT_EXPIRED == status->us_expired ) {
                    printf( " Password has expired:\\tY\\n" );
                } else {
                    printf( " Password has expired:\\tN\\n" );
                }
                printf( " Grace logins left:\\t%d\\n",
                    status->us_remaining );
                printf( " Account unlocks in:\\t%d s\\n",
                    status->us_seconds );
            }
        }
    }
    
    ldap_msgfree( result );
    ldap_control_free( status_ctrl );
    ldap_controls_free( resultctrls );
    ldap_unbind( ld );
    return( 0 );
}
