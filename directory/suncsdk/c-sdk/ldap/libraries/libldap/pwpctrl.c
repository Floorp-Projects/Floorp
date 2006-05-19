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

/* ldap_create_pwdpolicy_control:

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
ldap_create_pwdpolicy_control ( 	
							LDAP *ld, 
							const char ctl_iscritical,
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
	
	rc = nsldapi_build_control( LDAP_CONTROL_PASSWD_POLICY, 
								NULL, NULL, ctl_iscritical, ctrlp );
	
	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
	return( rc );
}

/* ldap_parse_pwdpolicy_control:

Parameters are  

ld              LDAP pointer to the desired connection 

ctrlp           An array of controls obtained from calling  
                ldap_parse_result on the set of results 
                returned by the server     

pp              the address of struct LDAPpwdpolicy 
                to parse control results to 
*/

int
LDAP_CALL
ldap_parse_pwdpolicy_control ( 	
							LDAP *ld, 
							LDAPControl **ctrlp,  
							LDAPpwdpolicy *pp
													)
{
	BerElement *ber = NULL;
	int			i, foundPPControl;
	LDAPControl *PPCtrlp = NULL;
	
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) || pp == NULL ) {
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
	
	/*  allocate a Ber element with the contents of the control's struct berval */
	if ( ( ber = ber_init( &PPCtrlp->ldctl_value ) ) == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( LDAP_NO_MEMORY );
	}
	
    /* The control value should look like this:
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
	 *
	 * controlValue: BER octet string formatted like {tii} where:
	 *
	 * t: tag defined which warning is set if any. Possible values are:
	 *	
	 *	LDAP_PP_WARNING_RESP_NONE  (0x00L)
	 *	LDAP_PP_WARNING_RESP_EXP   (0x01L)
	 *	LDAP_PP_WARNING_RESP_GRACE (0x02L)
	 *	
	 * i: warning information:
	 *	  if tag is LDAP_PP_WARNING_RESP_NONE, warning is -1
	 *	  if tag is LDAP_PP_WARNING_RESP_EX, warning is the number of seconds before expiration
	 *	  if tag is LDAP_PP_WARNING_RESP_GRACE, warning is the number of remaining grace logins
	 * i: error information:
	 *	  if tag is LDAP_PP_WARNING_RESP_NONE, error contains one of the following value:
	 *	     pp_resp_no_error             (-1)
	 *	     pp_resp_expired_error         (0)
	 *	     pp_resp_locked_error          (1)
	 *	     pp_resp_need_change_error     (2)
	 *	     pp_resp_mod_not_allowed_error (3)
	 *	     pp_resp_give_old_error        (4)
	 *	     pp_resp_bad_qa_error          (5)
	 *	     pp_resp_too_short_error       (6)
	 *	     pp_resp_too_young_error       (7)
	 *	     pp_resp_in_hist_error         (8)
	 */
	
	if ( ber_scanf( ber, "{tii}", &pp->pp_warning, &pp->pp_warning_info, &pp->pp_error ) 
		 == LBER_ERROR ) {
		LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
		ber_free( ber, 1 );
		return( LDAP_DECODING_ERROR );
	}
	
	if ( pp->pp_warning == LDAP_PP_WARNING_NONE ) {
		pp->pp_warning_info = -1;
	}
	
	if ( ( pp->pp_warning == LDAP_PP_WARNING_EXP ) || 
		 ( pp->pp_warning == LDAP_PP_WARNING_GRACE ) ) {
		pp->pp_error = -1;
	}
	
	/* the ber encoding is no longer needed */
	ber_free(ber,1);
	
	return( LDAP_SUCCESS );
}
