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
#include "ldap-int.h"

/* ldap_create_proxyauth_control

   Create a "version 1" proxied authorization control.

   Parameters are  

   ld              LDAP pointer to the desired connection 

   dn		   The dn used in the proxy auth

   ctl_iscritical  Indicates whether the control is critical of not. If
                   this field is non-zero, the operation will only be car-
                   ried out if the control is recognized by the server
                   and/or client

   ctrlp           the address of a place to put the constructed control 
*/

int
LDAP_CALL
ldap_create_proxyauth_control (
     LDAP *ld, 
     const char *dn, 
     const char ctl_iscritical,
     LDAPControl **ctrlp   
)
{
	BerElement		*ber;
	int				rc;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}

	if (  ctrlp == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return ( LDAP_PARAM_ERROR );
	}
	if (NULL == dn)
	{
	    dn = "";
	}

	/* create a ber package to hold the controlValue */
	if ( ( nsldapi_alloc_ber_with_options( ld, &ber ) ) != LDAP_SUCCESS ) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( LDAP_NO_MEMORY );
	}



        if ( LBER_ERROR == ber_printf( ber, 
                                       "{s}", 
                                       dn ) ) 
        {
            LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
            ber_free( ber, 1 );
            return( LDAP_ENCODING_ERROR );
        }

	rc = nsldapi_build_control( LDAP_CONTROL_PROXYAUTH, ber, 1,
	    ctl_iscritical, ctrlp );

	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
	return( rc );

}


/* ldap_create_proxiedauth_control

   Create a "version 2" proxied authorization control.

   Parameters are  

   ld              LDAP pointer to the desired connection 

   authzid		   The authorization identity used in the proxy auth,
                   e.g., dn:uid=bjensen,dc=example,dc=com

   ctrlp           the address of a place to put the constructed control 
*/

int
LDAP_CALL
ldap_create_proxiedauth_control (
     LDAP *ld, 
     const char *authzid, 
     LDAPControl **ctrlp   
)
{
	BerElement		*ber;
	int				rc;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}

	if (  ctrlp == NULL || authzid == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return ( LDAP_PARAM_ERROR );
	}

	/* create a ber package to hold the controlValue */
	if ( ( nsldapi_alloc_ber_with_options( ld, &ber ) ) != LDAP_SUCCESS ) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( LDAP_NO_MEMORY );
	}



        if ( LBER_ERROR == ber_printf( ber, 
                                       "s", 
                                       authzid ) ) 
        {
            LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
            ber_free( ber, 1 );
            return( LDAP_ENCODING_ERROR );
        }

	rc = nsldapi_build_control( LDAP_CONTROL_PROXIEDAUTH, ber, 1, 1, ctrlp );

	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
	return( rc );

}
