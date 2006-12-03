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
 * Contributor(s): abobrov@sun.com
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

/* ldap_create_passwordpolicy_control:

Parameters are  

ld              LDAP pointer to the desired connection 

ctrlp           the address of a place to put the constructed control 
*/

int
LDAP_CALL
ldap_create_passwordpolicy_control ( 	
										LDAP *ld, 
										LDAPControl **ctrlp   
																	)
{
	int rc;
	
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}
	
	if ( ctrlp == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return ( LDAP_PARAM_ERROR );
	}
	
	rc = nsldapi_build_control( LDAP_CONTROL_PASSWD_POLICY, 
								NULL, NULL, 0, ctrlp );
	
	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
	return( rc );
}

/* ldap_create_passwordpolicy_control_ext:

Parameters are  

ld              LDAP pointer to the desired connection 

ctl_iscritical  Indicates whether the control is critical of not. If
                this field is non-zero, the operation will only be car-
                ried out if the control is recognized by the server
                and/or client

ctrlp           the address of a place to put the constructed control 
*/

int
LDAP_CALL
ldap_create_passwordpolicy_control_ext ( 	
											LDAP *ld, 
											const char ctl_iscritical,
											LDAPControl **ctrlp   
																		)
{
	int rc;
	
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}
	
	if ( ctrlp == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return ( LDAP_PARAM_ERROR );
	}
	
	rc = nsldapi_build_control( LDAP_CONTROL_PASSWD_POLICY, 
								NULL, NULL, ctl_iscritical, ctrlp );
	
	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
	return( rc );
}

/* ldap_parse_passwordpolicy_control:

Parameters are  

ld              LDAP pointer to the desired connection 

ctrlp           pointer to LDAPControl structure, obtained from
				calling ldap_find_control() or by other means.  

exptimep        result parameter is filled in with the number of seconds before
                the password will expire.

gracep          result parameter is filled in with the number of grace logins 
                after the password has expired.

errorcodep      result parameter is filled in with the error code of the 
                password operation.
*/

int
LDAP_CALL
ldap_parse_passwordpolicy_control ( 	
									LDAP *ld, 
									LDAPControl *ctrlp,  
									ber_int_t *expirep,
									ber_int_t *gracep,
									LDAPPasswordPolicyError *errorp
																	)
{
	ber_len_t	len;
	ber_tag_t	tag;
	ber_int_t	pp_exp = -1;
	ber_int_t	pp_grace = -1;
	ber_int_t	pp_warning = -1;
	ber_int_t	pp_err = PP_noError;
	BerElement	*ber = NULL;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) ) {
	    return( LDAP_PARAM_ERROR );
	}
	
	if ( ctrlp == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_CONTROL_NOT_FOUND, NULL, NULL );
		return ( LDAP_CONTROL_NOT_FOUND );
	}
	
	/*  allocate a Ber element with the contents of the control's struct berval */
	if ( ( ber = ber_init( &ctrlp->ldctl_value ) ) == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( LDAP_NO_MEMORY );
	}
	
	/*
	 * The control value should look like this:
	 *	
	 *	PasswordPolicyResponseValue ::= SEQUENCE {
	 *		warning [0] CHOICE {
	 *			timeBeforeExpiration        [0] INTEGER (0 .. maxInt),
	 *			graceLoginsRemaining        [1] INTEGER (0 .. maxInt) } OPTIONAL
	 *		error       [1] ENUMERATED {
	 *			passwordExpired             (0),
	 *			accountLocked               (1),
	 *			changeAfterReset            (2),
	 *			passwordModNotAllowed       (3),
	 *			mustSupplyOldPassword       (4),
	 *			insufficientPasswordQuality (5),
	 *			passwordTooShort            (6),
	 *			passwordTooYoung            (7),
	 *			passwordInHistory           (8) } OPTIONAL }
	 */

	if ( ber_scanf( ber, "{" ) == LBER_ERROR ) {
        LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
        ber_free( ber, 1 );
        return( LDAP_DECODING_ERROR );
	}
    
	tag = ber_peek_tag( ber, &len );

	while ( (tag != LBER_ERROR) && (tag != LBER_END_OF_SEQORSET) ) {
		if ( tag == ( LBER_CONSTRUCTED | LBER_CLASS_CONTEXT ) ) {
			ber_skip_tag( ber, &len );
			if ( ber_scanf( ber, "ti", &tag, &pp_warning ) == LBER_ERROR ) {
				LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
				ber_free( ber, 1 );
				return( LDAP_DECODING_ERROR );
			}
			if ( tag == ( LBER_CLASS_CONTEXT | 0x01 ) ) {
				pp_exp = pp_warning;
			} else if ( tag == ( LBER_CLASS_CONTEXT | 0x02 ) ) {
				pp_grace = pp_warning;
			}
		} else if ( tag == ( LBER_CLASS_CONTEXT | 0x01 ) ) {
				if ( ber_scanf( ber, "ti", &tag, &pp_err ) == LBER_ERROR ) {
					LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
					ber_free( ber, 1 );
					return( LDAP_DECODING_ERROR );
				}
		}
		if ( tag == LBER_DEFAULT ) {
			LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
			ber_free( ber, 1 );
			return( LDAP_DECODING_ERROR );
		}
		tag = ber_skip_tag( ber, &len );
	}

	if (expirep) *expirep = pp_exp;
	if (gracep) *gracep = pp_grace;
	if (errorp) *errorp = pp_err;

	/* the ber encoding is no longer needed */
	ber_free( ber, 1 );
	return( LDAP_SUCCESS );
}

/* ldap_parse_passwordpolicy_control_ext:

Parameters are  

ld              LDAP pointer to the desired connection 

ctrlp           An array of controls obtained from calling  
                ldap_parse_result on the set of results 
                returned by the server

exptimep        result parameter is filled in with the number of seconds before
                the password will expire.

gracep          result parameter is filled in with the number of grace logins 
                after the password has expired.

errorcodep      result parameter is filled in with the error code of the 
                password operation.
*/

int
LDAP_CALL
ldap_parse_passwordpolicy_control_ext ( 	
										LDAP *ld, 
										LDAPControl **ctrlp,  
										ber_int_t *expirep,
										ber_int_t *gracep,
										LDAPPasswordPolicyError *errorp
																		)
{
	int i, foundPPControl;
	LDAPControl *PPCtrlp = NULL;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) ) {
	    return( LDAP_PARAM_ERROR );
	}
	
	/* find the control in the list of controls if it exists */
	if ( ctrlp == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_CONTROL_NOT_FOUND, NULL, NULL );
		return ( LDAP_CONTROL_NOT_FOUND );
	} 
	foundPPControl = 0;
	for ( i = 0; (( ctrlp[i] != NULL ) && ( !foundPPControl )); i++ ) {
		foundPPControl = !strcmp( ctrlp[i]->ldctl_oid, LDAP_CONTROL_PASSWD_POLICY );
	}
	if ( !foundPPControl ) {
		LDAP_SET_LDERRNO( ld, LDAP_CONTROL_NOT_FOUND, NULL, NULL );
		return ( LDAP_CONTROL_NOT_FOUND );
	} else {
		/* let local var point to the control */
		PPCtrlp = ctrlp[i-1];			
	}
	
	return (
	ldap_parse_passwordpolicy_control( ld, PPCtrlp, expirep, gracep, errorp ));
}

const char *
LDAP_CALL
ldap_passwordpolicy_err2txt( LDAPPasswordPolicyError err )
{
	switch(err) {
		case PP_passwordExpired: 
			return "Password expired";
		case PP_accountLocked: 
			return "Account locked";
		case PP_changeAfterReset: 
			return "Password must be changed";
		case PP_passwordModNotAllowed: 
			return "Policy prevents password modification";
		case PP_mustSupplyOldPassword: 
			return "Policy requires old password in order to change password";
		case PP_insufficientPasswordQuality: 
			return "Password fails quality checks";
		case PP_passwordTooShort: 
			return "Password is too short for policy";
		case PP_passwordTooYoung: 
			return "Password has been changed too recently";
		case PP_passwordInHistory: 
			return "New password is in list of old passwords";
		case PP_noError: 
			return "No error";
		default: 
			return "Unknown error code";
	}
}
