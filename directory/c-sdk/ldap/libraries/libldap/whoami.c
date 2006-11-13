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

/* ldap_whoami */
int
LDAP_CALL
ldap_whoami ( 	
				LDAP          *ld, 
				LDAPControl   **serverctrls,
				LDAPControl   **clientctrls,
				int           *msgidp
												)
{
	int				rc;
	struct berval   *requestdata = NULL;
	
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( LDAP_PARAM_ERROR );
	}
	
	rc = ldap_extended_operation( ld, LDAP_EXOP_WHO_AM_I, requestdata, 
									serverctrls, clientctrls, msgidp );
	
	return( rc );
}

/* ldap_parse_whoami */
int
LDAP_CALL
ldap_parse_whoami ( 	
						LDAP			*ld, 
						LDAPMessage		*result,
						struct berval	**authzid
													)
{	
	int				rc;
	char			*retoidp = NULL;
	char			*authzidp = NULL;
	struct berval	*retdatap = NULL;
	
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( LDAP_PARAM_ERROR );
	}
	if ( !result ) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( LDAP_PARAM_ERROR );
	}
	
	*authzid = NULL;
	
	rc = ldap_parse_extended_result( ld, result, &retoidp, authzid, 0 );
	
	if ( rc != LDAP_SUCCESS ) {
		return( rc );
    }
	
	ldap_memfree( retoidp );
	return( LDAP_SUCCESS );
}

/* ldap_whoami_s */
int
LDAP_CALL
ldap_whoami_s ( 	
				LDAP          *ld,
				struct berval **authzid,
				LDAPControl   **serverctrls,
				LDAPControl   **clientctrls
												)
{
	int			rc;
	int			msgid;
	LDAPMessage	*result = NULL;
	
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( LDAP_PARAM_ERROR );
	}	
	
	rc = ldap_whoami( ld, serverctrls, clientctrls, &msgid );
	if ( rc != LDAP_SUCCESS ) {
		return( rc );
	}
	
	rc = ldap_result( ld, msgid, LDAP_MSG_ALL, NULL, &result );
	if ( rc == -1 ) {
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
	}

	rc = ldap_parse_whoami( ld, result, authzid );
	
	ldap_msgfree( result );
	
	return( rc );
}
