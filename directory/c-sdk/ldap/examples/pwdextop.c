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
 * Use the password modify extended operation to change a password.
 */

#include "examples.h"

int
main( int argc, char **argv )
{
    int             version;
    LDAP            *ld;
    char            *target;
    int             rc;
    struct berval   userid;
    struct berval   oldpasswd;
    struct berval   newpasswd;
    struct berval   genpasswd;

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

    /* Authenticate to the directory. */
    if ( ldap_simple_bind_s( ld, ENTRYDN, ENTRYPW ) != LDAP_SUCCESS ) {
        ldap_perror( ld, "ldap_simple_bind_s" );
        return( 1 );
    }

    /* Change the password using the extended operation. */
    userid.bv_val = ENTRYDN;
    userid.bv_len = strlen(userid.bv_val);

    oldpasswd.bv_val = ENTRYPW;
    oldpasswd.bv_len = strlen(oldpasswd.bv_val);

    newpasswd.bv_val = "ChangeMe!";
    newpasswd.bv_len = strlen(newpasswd.bv_val);

    rc = ldap_passwd_s(
        ld, &userid, &oldpasswd, &newpasswd, &genpasswd, NULL, NULL );
    if ( rc != LDAP_SUCCESS ) {
        fprintf( stderr, "ldap_passwd_s: %s\n", ldap_err2string( rc ) );
        ldap_unbind( ld );
        return( 1 );
    } else {
        printf( "Successfully changed password for %s\n", userid.bv_val );
    }

    ldap_unbind( ld );
    return( 0 );
}
