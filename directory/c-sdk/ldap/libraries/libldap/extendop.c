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

/*
 * ldap_extended_operation - initiate an arbitrary ldapv3 extended operation.
 * the oid and data of the extended operation are supplied. Returns an
 * LDAP error code.
 *
 * Example:
 *	struct berval	exdata;
 *	char		*exoid;
 *	int		err, msgid;
 *	... fill in oid and data ...
 *	err = ldap_extended_operation( ld, exoid, &exdata, NULL, NULL, &msgid );
 */

int
LDAP_CALL
ldap_extended_operation(
    LDAP		*ld,
    const char		*exoid,
    const struct berval	*exdata,
    LDAPControl		**serverctrls,
    LDAPControl		**clientctrls,
    int			*msgidp
)
{
	BerElement	*ber;
	int		rc, msgid;

	/*
	 * the ldapv3 extended operation request looks like this:
	 *
	 *	ExtendedRequest ::= [APPLICATION 23] SEQUENCE {
	 *		requestName	LDAPOID,
	 *		requestValue	OCTET STRING
	 *	}
	 *
	 * all wrapped up in an LDAPMessage sequence.
	 */

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_extended_operation\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}


	/* only ldapv3 or higher can do extended operations */
	if ( NSLDAPI_LDAP_VERSION( ld ) < LDAP_VERSION3 ) {
		rc = LDAP_NOT_SUPPORTED;
		LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
		return( rc );
	}

	if ( msgidp == NULL || exoid == NULL || *exoid == '\0' ) {
		rc = LDAP_PARAM_ERROR;
		LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
		return( rc );
	}

	LDAP_MUTEX_LOCK( ld, LDAP_MSGID_LOCK );
	msgid = ++ld->ld_msgid;
	LDAP_MUTEX_UNLOCK( ld, LDAP_MSGID_LOCK );

#if 0
	if ( ld->ld_cache_on && ld->ld_cache_extendedop != NULL ) {
		LDAP_MUTEX_LOCK( ld, LDAP_CACHE_LOCK );
		if ( (rc = (ld->ld_cache_extendedop)( ld, msgid,
		    LDAP_REQ_EXTENDED, exoid, cred )) != 0 ) {
			LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
			return( rc );
		}
		LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
	}
#endif

	/* create a message to send */
	if (( rc = nsldapi_alloc_ber_with_options( ld, &ber ))
	    != LDAP_SUCCESS ) {
		return( rc );
	}

	/* fill it in */
	if ( exdata ) {
		if ( ber_printf( ber, "{it{tsto}", msgid, LDAP_REQ_EXTENDED,
						 LDAP_TAG_EXOP_REQ_OID, exoid, LDAP_TAG_EXOP_REQ_VALUE,
						 exdata->bv_val, exdata->bv_len ) == -1 ) {
			rc = LDAP_ENCODING_ERROR;
			LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
			ber_free( ber, 1 );
			return( rc );
		}
	} else { /* some implementations are pretty strict on empty values */
		if ( ber_printf( ber, "{it{ts}", msgid, LDAP_REQ_EXTENDED,
						 LDAP_TAG_EXOP_REQ_OID, exoid ) == -1 ) {
			rc = LDAP_ENCODING_ERROR;
			LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
			ber_free( ber, 1 );
			return( rc );
		}
	}

	if (( rc = nsldapi_put_controls( ld, serverctrls, 1, ber ))
	    != LDAP_SUCCESS ) {
		ber_free( ber, 1 );
		return( rc );
	}

	/* send the message */
	rc = nsldapi_send_initial_request( ld, msgid, LDAP_REQ_EXTENDED, NULL,
		ber );
	*msgidp = rc;
	return( rc < 0 ? LDAP_GET_LDERRNO( ld, NULL, NULL ) : LDAP_SUCCESS );
}


/*
 * ldap_extended_operation_s - perform an arbitrary ldapv3 extended operation.
 * the oid and data of the extended operation are supplied. LDAP_SUCCESS
 * is returned upon success, the ldap error code otherwise.
 *
 * Example:
 *	struct berval	exdata, exretval;
 *	char		*exoid;
 *	int		rc;
 *	... fill in oid and data ...
 *	rc = ldap_extended_operation_s( ld, exoid, &exdata, &exretval );
 */
int
LDAP_CALL
ldap_extended_operation_s(
    LDAP		*ld,
    const char		*requestoid,
    const struct berval	*requestdata,
    LDAPControl		**serverctrls,
    LDAPControl		**clientctrls,
    char		**retoidp,
    struct berval	**retdatap
)
{
	int		err, msgid;
	LDAPMessage	*result;

	if (( err = ldap_extended_operation( ld, requestoid, requestdata,
	    serverctrls, clientctrls, &msgid )) != LDAP_SUCCESS ) {
		return( err );
	}

	if ( ldap_result( ld, msgid, 1, (struct timeval *) 0, &result )
	    == -1 ) {
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
	}

	if (( err = ldap_parse_extended_result( ld, result, retoidp, retdatap,
		0 )) != LDAP_SUCCESS ) {
	    ldap_msgfree( result );
	    return( err );
	}

	return( ldap_result2error( ld, result, 1 ) );
}


/*
 * Pull the oid returned by the server and the data out of an extended
 * operation result.  Return an LDAP error code.
 */
int
LDAP_CALL
ldap_parse_extended_result(
    LDAP		*ld,
    LDAPMessage		*res,
    char		**retoidp,	/* may be NULL */
    struct berval	**retdatap,	/* may be NULL */
    int			freeit
)
{
	struct berelement	ber;
	ber_len_t			len;
	ber_int_t			err;
	char				*m, *e, *roid;
	struct berval		*rdata;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_parse_extended_result\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}

        if ( !NSLDAPI_VALID_LDAPMESSAGE_EXRESULT_POINTER( res )) {
		return( LDAP_PARAM_ERROR );
	}

	m = e = NULL;
	ber = *(res->lm_ber);
	if ( NSLDAPI_LDAP_VERSION( ld ) < LDAP_VERSION3 ) {
		LDAP_SET_LDERRNO( ld, LDAP_NOT_SUPPORTED, NULL, NULL );
		return( LDAP_NOT_SUPPORTED );
	}

	if ( ber_scanf( &ber, "{iaa", &err, &m, &e ) == LBER_ERROR ) {
		goto decoding_error;
	}
	roid = NULL;
	if ( ber_peek_tag( &ber, &len ) == LDAP_TAG_EXOP_RES_OID ) {
		if ( ber_scanf( &ber, "a", &roid ) == LBER_ERROR ) {
			goto decoding_error;
		}
	}
	if ( retoidp != NULL ) {
		*retoidp = roid;
	} else if ( roid != NULL ) {
		NSLDAPI_FREE( roid );
	}

	rdata = NULL;
	if ( ber_peek_tag( &ber, &len ) == LDAP_TAG_EXOP_RES_VALUE ) {
		if ( ber_scanf( &ber, "O", &rdata ) == LBER_ERROR ) {
			goto decoding_error;
		}
	}
	if ( retdatap != NULL ) {
		*retdatap = rdata;
	} else if ( rdata != NULL ) {
		ber_bvfree( rdata );
	}

	LDAP_SET_LDERRNO( ld, err, m, e );

	if ( freeit ) {
		ldap_msgfree( res );
	}

	return( LDAP_SUCCESS );

decoding_error:;
	LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
	return( LDAP_DECODING_ERROR );
}
