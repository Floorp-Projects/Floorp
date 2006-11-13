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

/* ldap_passwd */
int
LDAP_CALL
ldap_passwd (
	LDAP          *ld, 
	struct berval *userid,
	struct berval *oldpasswd,
	struct berval *newpasswd,
	LDAPControl   **serverctrls,
	LDAPControl   **clientctrls,
	int           *msgidp
	)
{
	int				rc;
	BerElement		*ber = NULL;
	struct berval   *requestdata = NULL;
	
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( LDAP_PARAM_ERROR );
	}

	requestdata = NSLDAPI_MALLOC( sizeof( struct berval ) );
	if ( requestdata == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( LDAP_NO_MEMORY );
	}
	
	/* If the requestValue field is provided, it SHALL contain a 
	 * PasswdModifyRequestValue with one or more fields present.
	 */
	if ( userid || oldpasswd || newpasswd ) {
		if ( ( nsldapi_alloc_ber_with_options( ld, &ber ) ) 
			 != LDAP_SUCCESS ) {
			LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
			return( LDAP_NO_MEMORY );
		}
		
		/*
		 * PasswdModifyRequestValue ::= SEQUENCE {
		 *	 userIdentity    [0]  OCTET STRING OPTIONAL
		 *	 oldPasswd       [1]  OCTET STRING OPTIONAL
		 *	 newPasswd       [2]  OCTET STRING OPTIONAL }
		 */
		if ( LBER_ERROR == ( ber_printf( ber, "{" ) ) ) {
			LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
			ber_free( ber, 1 );
			return( LDAP_ENCODING_ERROR );
		}
		
		if ( userid && userid->bv_val && userid->bv_len ) {
			if ( LBER_ERROR == ( ber_printf( ber, "to", LDAP_TAG_PWDMOD_REQ_ID, 
							userid->bv_val, userid->bv_len ) ) ) {
				LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
				ber_free( ber, 1 );
				return( LDAP_ENCODING_ERROR );
			}
		}

		if ( oldpasswd && oldpasswd->bv_val && oldpasswd->bv_len ) {
			if ( LBER_ERROR == ( ber_printf( ber, "to", LDAP_TAG_PWDMOD_REQ_OLD, 
							oldpasswd->bv_val, oldpasswd->bv_len ) ) ) {
				LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
				ber_free( ber, 1 );
				return( LDAP_ENCODING_ERROR );
			}
		}

		if ( newpasswd && newpasswd->bv_val && newpasswd->bv_len ) {
			if ( LBER_ERROR == ( ber_printf( ber, "to", LDAP_TAG_PWDMOD_REQ_NEW, 
							newpasswd->bv_val, newpasswd->bv_len ) ) ) {
				LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
				ber_free( ber, 1 );
				return( LDAP_ENCODING_ERROR );
			}
		}
		
		if ( LBER_ERROR == ( ber_printf( ber, "}" ) ) ) {
			LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
			ber_free( ber, 1 );
			return( LDAP_ENCODING_ERROR );
		}
		
		/* allocate struct berval with contents of the BER encoding */
		rc = ber_flatten( ber, &requestdata );
		if ( rc == -1 ) {
			LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
			ber_free( ber, 1 );
			return( LDAP_NO_MEMORY );
		}
	} else {
		requestdata = NULL;
	}
		
	rc = ldap_extended_operation( ld, LDAP_EXOP_MODIFY_PASSWD, requestdata, 
										serverctrls, clientctrls, msgidp );
	
	/* the ber encoding is no longer needed */
	if ( requestdata ) {
		ber_bvfree( requestdata );
	}

	if ( ber ) {
		ber_free( ber, 1 );
	}

	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
	return( rc );
}

/* ldap_parse_passwd */
int
LDAP_CALL
ldap_parse_passwd ( 	
	LDAP			*ld, 
	LDAPMessage		*result,
	struct berval	*genpasswd
	)
{	
	int			rc;
	char			*retoidp = NULL;
	struct berval		*retdatap = NULL;
	BerElement		*ber = NULL;
	ber_len_t		len;
	ber_tag_t		tag;
	
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( LDAP_PARAM_ERROR );
	}	

	if ( !result ) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( LDAP_PARAM_ERROR );
	}

	if ( !genpasswd ) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( LDAP_PARAM_ERROR );
	}
		
	rc = ldap_parse_extended_result( ld, result, &retoidp, &retdatap, 0 );
	if ( rc != LDAP_SUCCESS ) {
		return( rc );
	}
	
	rc = ldap_get_lderrno( ld, NULL, NULL );
	if ( rc != LDAP_SUCCESS ) {
		return( rc );
	}
	
	if ( retdatap ) {
		/* allocate a Ber element with the contents of the
		 * berval from the response.
		 */
		if ( ( ber = ber_init( retdatap ) ) == NULL ) {
			LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
			return( LDAP_NO_MEMORY );
		}
		
		if ( ( tag = ber_skip_tag( ber, &len ) ) == LBER_ERROR ) {
			LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
			ber_free( ber, 1 );
			ldap_memfree( retoidp );
			return( LDAP_DECODING_ERROR );
		}
		
		if ( ( ( tag = ber_peek_tag( ber, &len ) ) == LBER_ERROR ) ||
			 tag != LDAP_TAG_PWDMOD_RES_GEN ) {
			LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
			ber_free( ber, 1 );
			ldap_memfree( retoidp );
			return( LDAP_DECODING_ERROR );
		}
		
		if ( ber_scanf( ber, "o}", genpasswd ) == LBER_ERROR ) {
			LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
			ber_free( ber, 1 );
			ldap_memfree( retoidp );
			return( LDAP_DECODING_ERROR );
		}
		
		/* the ber encoding is no longer needed */
		ber_free( ber,1 );
	}
	
	ldap_memfree( retoidp );
	return( LDAP_SUCCESS );
}

/* ldap_passwd_s */
int
LDAP_CALL
ldap_passwd_s ( 	
	LDAP          *ld, 
	struct berval *userid,
	struct berval *oldpasswd,
	struct berval *newpasswd,
	struct berval *genpasswd,
	LDAPControl   **serverctrls,
	LDAPControl   **clientctrls
	)
{
	int			rc, msgid;
	LDAPMessage             *result = NULL;
	
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( LDAP_PARAM_ERROR );
	}	

	rc = ldap_passwd( ld, userid, oldpasswd, newpasswd, serverctrls, 
					  clientctrls, &msgid );
	if ( rc != LDAP_SUCCESS ) {
		return( rc );
	}
	
	rc = ldap_result( ld, msgid, LDAP_MSG_ALL, NULL, &result );
	if ( rc == -1 ) {
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
	}

	rc = ldap_parse_passwd( ld, result, genpasswd );

	ldap_msgfree( result );
	return( rc );
}
