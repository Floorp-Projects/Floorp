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
 * Contributor(s): abobrov
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

#include "ldap-int.h"

/* ldap_create_authzid_control:

Parameters are  

ld              LDAP pointer to the desired connection 

ctl_iscritical  Indicates whether the control is critical of not. 
                If this field is non-zero, the operation will only be 
                carried out if the control is recognized by the server
                and/or client

ctrlp           the address of a place to put the constructed control 
*/

int
LDAP_CALL
ldap_create_authzid_control ( 	
							 LDAP        *ld, 
							 const char  ctl_iscritical,
							 LDAPControl **ctrlp   
							)
{
	int				rc;
	
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}
	
	if ( ctrlp == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return ( LDAP_PARAM_ERROR );
	}
	
	rc = nsldapi_build_control( LDAP_CONTROL_AUTHZID_REQ, 
								NULL, NULL, ctl_iscritical, ctrlp );
	
	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
	return( rc );
}

/* ldap_parse_authzid_control:

Parameters are  

ld              LDAP pointer to the desired connection 

ctrlp           An array of controls obtained from calling  
                ldap_parse_result on the set of results 
                returned by the server     

authzid         authorization identity, as defined in 
                RFC 2829, section 9.
*/

int
LDAP_CALL
ldap_parse_authzid_control ( 	
							LDAP        *ld, 
							LDAPControl **ctrlp,  
							char        **authzid
						   )
{
	int			i, foundAUTHZIDControl;
	char        *authzidp = NULL;
	LDAPControl *AUTHZIDCtrlp = NULL;
	
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) ) {
	    return( LDAP_PARAM_ERROR );
	}
	
	/* find the control in the list of controls if it exists */
	if ( ctrlp == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_CONTROL_NOT_FOUND, NULL, NULL );
		return ( LDAP_CONTROL_NOT_FOUND );
	} 
	foundAUTHZIDControl = 0;
	for ( i = 0; (( ctrlp[i] != NULL ) && ( !foundAUTHZIDControl )); i++ ) {
		foundAUTHZIDControl = !strcmp( ctrlp[i]->ldctl_oid, 
									  LDAP_CONTROL_AUTHZID_RES );
	}
	
	/*
	 * The control is only included in a bind response if the resultCode 
	 * for the bind operation is success.
	 */
	if ( !foundAUTHZIDControl ) {
		LDAP_SET_LDERRNO( ld, LDAP_CONTROL_NOT_FOUND, NULL, NULL );
		return ( LDAP_CONTROL_NOT_FOUND );
	} else {
		/* let local var point to the control */
		AUTHZIDCtrlp = ctrlp[i-1];			
	}
	
	/*
	 * If the bind request succeeded and resulted in an identity (not anonymous), 
	 * the controlValue contains the authorization identity (authzid), as
	 * defined in [AUTH] section 9, granted to the requestor.  If the bind
	 * request resulted in an anonymous association, the controlValue field
	 * is a string of zero length.  If the bind request resulted in more
	 * than one authzid, the primary authzid is returned in the controlValue
	 * field.
	 */
	if ( AUTHZIDCtrlp && AUTHZIDCtrlp->ldctl_value.bv_val && 
		 AUTHZIDCtrlp->ldctl_value.bv_len ) {
		authzidp = ( (char *)NSLDAPI_MALLOC( 
							( AUTHZIDCtrlp->ldctl_value.bv_len + 1 ) ) );
		if ( authzid == NULL ) {
			LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
			return( LDAP_NO_MEMORY );
		}
		STRLCPY( authzidp, AUTHZIDCtrlp->ldctl_value.bv_val, 
				 ( AUTHZIDCtrlp->ldctl_value.bv_len + 1 ) );
		*authzid = authzidp;
	} else {
		authzid = NULL;
	}
	
	return( LDAP_SUCCESS );
}
