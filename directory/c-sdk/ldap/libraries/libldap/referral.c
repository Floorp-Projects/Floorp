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
/*
 *  referral.c - routines for handling LDAPv3 referrals and references.
 */

#include "ldap-int.h"


LDAPMessage *
LDAP_CALL
ldap_first_reference( LDAP *ld, LDAPMessage *res )
{
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) || res == NULLMSG ) {
		return( NULLMSG );
	}

	if ( res->lm_msgtype == LDAP_RES_SEARCH_REFERENCE ) {
		return( res );
	}

	return( ldap_next_reference( ld, res ));
}


LDAPMessage *
LDAP_CALL
ldap_next_reference( LDAP *ld, LDAPMessage *ref )
{
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) || ref == NULLMSG ) {
		return( NULLMSG );		/* punt */
	}

	for ( ref = ref->lm_chain; ref != NULLMSG; ref = ref->lm_chain ) {
		if ( ref->lm_msgtype == LDAP_RES_SEARCH_REFERENCE ) {
			return( ref );
		}
	}

	return( NULLMSG );
}


int
LDAP_CALL
ldap_count_references( LDAP *ld, LDAPMessage *res )
{
	int	i;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( -1 );
	}

	for ( i = 0; res != NULL; res = res->lm_chain ) {
		if ( res->lm_msgtype == LDAP_RES_SEARCH_REFERENCE ) {
			++i;
		}
	}

	return( i );
}


/*
 * returns an LDAP error code.
 */
int
LDAP_CALL
ldap_parse_reference( LDAP *ld, LDAPMessage *ref, char ***referralsp,
    LDAPControl ***serverctrlsp, int freeit )
{
	int		err;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) ||
	    !NSLDAPI_VALID_LDAPMESSAGE_REFERENCE_POINTER( ref )) {
		return( LDAP_PARAM_ERROR );
	}

	err = nsldapi_parse_reference( ld, ref->lm_ber, referralsp,
		serverctrlsp );

	LDAP_SET_LDERRNO( ld, err, NULL, NULL );

	if ( freeit ) {
		ldap_msgfree( ref );
	}

	return( err );
}


/*
 * returns an LDAP error code indicating success or failure of parsing
 * does NOT set any error information inside "ld"
 */
int
nsldapi_parse_reference( LDAP *ld, BerElement *rber, char ***referralsp,
    LDAPControl ***serverctrlsp )
{
	int		err;
	BerElement	ber;
	char		**refs;

	/*
	 * Parse a searchResultReference message.  These are used in LDAPv3
	 * and beyond and look like this:
	 *
	 *	SearchResultReference ::= [APPLICATION 19] SEQUENCE OF LDAPURL
	 *
	 * all wrapped up in an LDAPMessage sequence which looks like this:
	 *
	 *	LDAPMessage ::= SEQUENCE {
	 *		messageID	MessageID,
	 *		SearchResultReference
	 *		controls	[0] Controls OPTIONAL
	 *	}
	 *
	 * ldap_result() pulls out the message id, so by the time a result
	 * message gets here we are conveniently sitting at the start of the
	 * SearchResultReference itself.
	 */
	err = LDAP_SUCCESS;	/* optimistic */
	ber = *rber;		/* struct copy */

	if ( ber_scanf( &ber, "{v", &refs ) == LBER_ERROR ) {
	    err = LDAP_DECODING_ERROR;
	} else if ( serverctrlsp != NULL ) {
	    /* pull out controls (if requested and any are present) */
	    if ( ber_scanf( &ber, "}" ) == LBER_ERROR ) {
		err = LDAP_DECODING_ERROR;
	    } else {
		err = nsldapi_get_controls( &ber, serverctrlsp );
	    }
	}

	if ( referralsp == NULL ) {
	    ldap_value_free( refs );
	} else {
	    *referralsp = refs;
	}

	return( err );
}
