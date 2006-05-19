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

/* ldap_create_userstatus_control:

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
ldap_create_userstatus_control ( 	
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

	rc = nsldapi_build_control( LDAP_CONTROL_ACCOUNT_USABLE, 
								NULL, NULL, ctl_iscritical, ctrlp );

	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
	return( rc );
}

/* ldap_parse_userstatus_control:

   Parameters are  

   ld              LDAP pointer to the desired connection 

   ctrlp           An array of controls obtained from calling  
                   ldap_parse_result on the set of results 
                   returned by the server     

   us              the address of struct LDAPuserstatus
                   to parse control results to 
*/

int
LDAP_CALL
ldap_parse_userstatus_control ( 	
									LDAP *ld, 
									LDAPControl **ctrlp,  
									LDAPuserstatus *us
															)
{
	BerElement *ber = NULL;
	int			i, foundUSControl;
	LDAPControl *USCtrlp = NULL;
	ber_len_t	len;
	ber_tag_t	tag;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) || us == NULL ) {
	    return( LDAP_PARAM_ERROR );
	}

	/* find the control in the list of controls if it exists */
	if ( ctrlp == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_CONTROL_NOT_FOUND, NULL, NULL );
		return ( LDAP_CONTROL_NOT_FOUND );
	} 
	foundUSControl = 0;
	for ( i = 0; (( ctrlp[i] != NULL ) && ( !foundUSControl )); i++ ) {
		foundUSControl = !strcmp( ctrlp[i]->ldctl_oid, LDAP_CONTROL_ACCOUNT_USABLE );
	}
	if ( !foundUSControl ) {
		LDAP_SET_LDERRNO( ld, LDAP_CONTROL_NOT_FOUND, NULL, NULL );
		return ( LDAP_CONTROL_NOT_FOUND );
	} else {
		/* let local var point to the control */
		USCtrlp = ctrlp[i-1];			
	}

	/*  allocate a Ber element with the contents of the control's struct berval */
	if ( ( ber = ber_init( &USCtrlp->ldctl_value ) ) == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( LDAP_NO_MEMORY );
	}
	
    /* The control value should look like this:
     *
     *	ACCOUNT_USABLE_RESPONSE::= CHOICE {
     *      [ tag is 0 ]
     * 		is_available		[0] INTEGER, ** seconds before expiration **
     *		is_not_available	[1] More_info
     *	}
     *	More_info::= SEQUENCE {
     *      [ tag is 1 ]
     *		inactive				[0] BOOLEAN DEFAULT FALSE,    
     *		reset					[1] BOOLEAN DEFAULT FALSE,
     *      expired					[2] BOOLEAN DEFAULT FALSE,
     *      [ tag is 2 ]
     *		remaining_grace			[3] INTEGER OPTIONAL,
     *      [ tag is 3 ]
     *		seconds_before_unlock	[4] INTEGER OPTIONAL
     *	}
     */
	
	if ( ( tag = ber_peek_tag( ber, &len ) ) == LBER_ERROR ) {
		LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
		ber_free( ber, 1 );
		return( LDAP_DECODING_ERROR );
	}
	
	if ( !tag ) {
		us->us_available = 0;
		if ( ber_scanf( ber, "i", &us->us_expire ) == LBER_ERROR ) {
			LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
			ber_free( ber, 1 );
			return( LDAP_DECODING_ERROR );
		}
	} else {
		us->us_available = tag;
	    if ( ber_scanf( ber, "{bbb", &us->us_inactive, &us->us_reset, 
						&us->us_expired ) == LBER_ERROR ) {
			LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
			ber_free( ber, 1 );
			return( LDAP_DECODING_ERROR );
		}
		tag = 0;
		if ( ( tag = ber_peek_tag( ber, &len ) ) == LBER_ERROR ) {
			LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
			ber_free( ber, 1 );
			return( LDAP_DECODING_ERROR );
		}
		if ( tag == 2 ) {
			if ( ber_scanf( ber, "i", &us->us_remaining ) == LBER_ERROR ) {
				LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
				ber_free( ber, 1 );
				return( LDAP_DECODING_ERROR );
			}
			tag = 0;
			if ( ( tag = ber_peek_tag( ber, &len ) ) == LBER_ERROR ) {
				LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
				ber_free( ber, 1 );
				return( LDAP_DECODING_ERROR );
			}
			if ( tag == 3 ) {
				if ( ber_scanf( ber, "i", &us->us_seconds ) == LBER_ERROR ) {
					LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
					ber_free( ber, 1 );
					return( LDAP_DECODING_ERROR );
				}
			}
		} else if ( tag == 3 ) {
			if ( ber_scanf( ber, "i", &us->us_seconds ) == LBER_ERROR ) {
				LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
				ber_free( ber, 1 );
				return( LDAP_DECODING_ERROR );
			}
		} else {
			LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
			ber_free( ber, 1 );
			return( LDAP_DECODING_ERROR );
		}
	}

	/* the ber encoding is no longer needed */
	ber_free(ber,1);

	return( LDAP_SUCCESS );
}
