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
ldap_create_geteffectiveRights_control (
     LDAP *ld, 
     const char *authzid,
	 const char **attrlist, 
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
	if (NULL == authzid)
	{
	    authzid = "";
	}

	/* create a ber package to hold the controlValue */
	if ( ( nsldapi_alloc_ber_with_options( ld, &ber ) ) != LDAP_SUCCESS ) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( LDAP_NO_MEMORY );
	}



	if ( LBER_ERROR == ber_printf( ber, 
									"{s", 
									authzid ) ) 
        {
            LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
            ber_free( ber, 1 );
            return( LDAP_ENCODING_ERROR );
        }	


	if ( LBER_ERROR == ber_printf( ber, 
									"{v}}", 
									attrlist ) ) 
        {
            LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
            ber_free( ber, 1 );
            return( LDAP_ENCODING_ERROR );
	}
	
	rc = nsldapi_build_control( LDAP_CONTROL_GETEFFECTIVERIGHTS_REQUEST, ber, 1,
	    ctl_iscritical, ctrlp );

	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
	return( rc );

}


