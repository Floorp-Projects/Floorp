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

int
LDAP_CALL
ldap_msgid( LDAPMessage *lm )
{
	if ( !NSLDAPI_VALID_LDAPMESSAGE_POINTER( lm )) {
		return( -1 );
	}

	return( lm->lm_msgid );
}

int
LDAP_CALL
ldap_msgtype( LDAPMessage *lm )
{
	if ( !NSLDAPI_VALID_LDAPMESSAGE_POINTER( lm )) {
		return( -1 );
	}

	return( lm->lm_msgtype );
}


LDAPMessage *
LDAP_CALL
ldap_first_message( LDAP *ld, LDAPMessage *chain )
{
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( NULLMSG );		/* punt */
	}

	return( chain );
}


LDAPMessage *
LDAP_CALL
ldap_next_message( LDAP *ld, LDAPMessage *msg )
{
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( NULLMSG );		/* punt */
	}

	if ( msg == NULLMSG || msg->lm_chain == NULLMSG ) {
		return( NULLMSG );
	}

	return( msg->lm_chain );
}


int
LDAP_CALL
ldap_count_messages( LDAP *ld, LDAPMessage *chain )
{
	int	i;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( -1 );
	}

	for ( i = 0; chain != NULL; chain = chain->lm_chain ) {
		i++;
	}

	return( i );
}
