/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "ldap-int.h"

/* ldap_create_proxyauth_control:

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
	int				i, rc;

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
