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

#include "ldap-int.h"

/* ldap_create_geteffectiveRights_control

   Create Effective Rights control.

   Parameters are  

   ld              LDAP pointer to the desired connection 

   authzid		   RFC2829 section 9, eg "dn:<DN>".
                   NULL or empty string means get bound user's rights, 
                   just "dn:" means get anonymous user's rights.
                   
   attrlist        additional attributes for which rights info is 
                   requrested. NULL means "just the ones returned 
                   with the search operation".

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

	if ( LBER_ERROR == ber_printf( ber, "{s{v}}", authzid, attrlist ) ) {
        LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
        ber_free( ber, 1 );
        return( LDAP_ENCODING_ERROR );
    }

	rc = nsldapi_build_control( LDAP_CONTROL_GETEFFECTIVERIGHTS_REQUEST, ber, 1,
	    ctl_iscritical, ctrlp );

	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
	return( rc );

}


