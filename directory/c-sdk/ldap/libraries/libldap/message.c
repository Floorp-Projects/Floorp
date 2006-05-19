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
