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

/*
 * ldap_sasl_bind - authenticate to the ldap server.  The dn, mechanism,
 * and credentials of the entry to which to bind are supplied. An LDAP
 * error code is returned and if LDAP_SUCCESS is returned *msgidp is set
 * to the id of the request initiated.
 *
 * Example:
 *	struct berval	creds;
 *	LDAPControl	**ctrls;
 *	int		err, msgid;
 *	... fill in creds with credentials ...
 *	... fill in ctrls with server controls ...
 *	err = ldap_sasl_bind( ld, "cn=manager, o=university of michigan, c=us",
 *	    "mechanismname", &creds, ctrls, NULL, &msgid );
 */
int
LDAP_CALL
ldap_sasl_bind(
    LDAP		*ld,
    const char		*dn,
    const char		*mechanism,
    const struct berval	*cred,
    LDAPControl		**serverctrls,
    LDAPControl		**clientctrls,
    int			*msgidp
)
{
	BerElement	*ber;
	int		rc, simple, msgid, ldapversion;

	/*
	 * The ldapv3 bind request looks like this:
	 *	BindRequest ::= SEQUENCE {
	 *		version		INTEGER,
	 *		name		DistinguishedName,	 -- who
	 *		authentication	CHOICE {
	 *			simple		[0] OCTET STRING, -- passwd
	 *			sasl		[3] SaslCredentials -- v3 only
	 *		}
	 *	}
	 *	SaslCredentials ::= SEQUENCE {
	 *		mechanism	LDAPString,
	 * 		credentials	OCTET STRING
	 *	}
	 * all wrapped up in an LDAPMessage sequence.
	 */

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_sasl_bind\n", 0, 0, 0 );

	if ( msgidp == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
                return( LDAP_PARAM_ERROR );
	}

	simple = ( mechanism == LDAP_SASL_SIMPLE );
	ldapversion = NSLDAPI_LDAP_VERSION( ld );

	/* only ldapv3 or higher can do sasl binds */
	if ( !simple && ldapversion < LDAP_VERSION3 ) {
		LDAP_SET_LDERRNO( ld, LDAP_NOT_SUPPORTED, NULL, NULL );
		return( LDAP_NOT_SUPPORTED );
	}

	LDAP_MUTEX_LOCK( ld, LDAP_MSGID_LOCK );
	msgid = ++ld->ld_msgid;
	LDAP_MUTEX_UNLOCK( ld, LDAP_MSGID_LOCK );

	if ( dn == NULL )
		dn = "";

	if ( ld->ld_cache_on && ld->ld_cache_bind != NULL ) {
		LDAP_MUTEX_LOCK( ld, LDAP_CACHE_LOCK );
		if ( (rc = (ld->ld_cache_bind)( ld, msgid, LDAP_REQ_BIND, dn,
		    cred, LDAP_AUTH_SASL )) != 0 ) {
			*msgidp = rc;
			LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
			return( LDAP_SUCCESS );
		}
		LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
	}

	/* create a message to send */
	if (( rc = nsldapi_alloc_ber_with_options( ld, &ber ))
	    != LDAP_SUCCESS ) {
		return( rc );
	}

	/* fill it in */
	if ( simple ) {		/* simple bind; works in LDAPv2 or v3 */
		struct berval	tmpcred;

		if ( cred == NULL ) {
			tmpcred.bv_val = "";
			tmpcred.bv_len = 0;
			cred = &tmpcred;
		}
		rc = ber_printf( ber, "{it{isto}", msgid, LDAP_REQ_BIND,
		    ldapversion, dn, LDAP_AUTH_SIMPLE, cred->bv_val,
		    (int)cred->bv_len /* XXX lossy cast */ );

	} else {		/* SASL bind; requires LDAPv3 or better */
		if ( cred == NULL ) {
			rc = ber_printf( ber, "{it{ist{s}}", msgid,
			    LDAP_REQ_BIND, ldapversion, dn, LDAP_AUTH_SASL,
			    mechanism );
		} else {
			rc = ber_printf( ber, "{it{ist{so}}", msgid,
			    LDAP_REQ_BIND, ldapversion, dn, LDAP_AUTH_SASL,
			    mechanism, cred->bv_val,
			    (int)cred->bv_len /* XXX lossy cast */ );
		}
	}

	if ( rc == -1 ) {
		LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
		ber_free( ber, 1 );
		return( LDAP_ENCODING_ERROR );
	}

	if ( (rc = nsldapi_put_controls( ld, serverctrls, 1, ber ))
	    != LDAP_SUCCESS ) {
		ber_free( ber, 1 );
		return( rc );
	}

	/* send the message */
	rc = nsldapi_send_initial_request( ld, msgid, LDAP_REQ_BIND,
		(char *)dn, ber );
	*msgidp = rc;
	return( rc < 0 ? LDAP_GET_LDERRNO( ld, NULL, NULL ) : LDAP_SUCCESS );
}

/*
 * ldap_sasl_bind_s - bind to the ldap server using sasl authentication
 * The dn, mechanism, and credentials of the entry to which to bind are
 * supplied.  LDAP_SUCCESS is returned upon success, the ldap error code
 * otherwise.
 *
 * Example:
 *	struct berval	creds;
 *	... fill in creds with credentials ...
 *	ldap_sasl_bind_s( ld, "cn=manager, o=university of michigan, c=us",
 *	    "mechanismname", &creds )
 */
int
LDAP_CALL
ldap_sasl_bind_s(
    LDAP		*ld,
    const char		*dn,
    const char		*mechanism,
    const struct berval	*cred,
    LDAPControl		**serverctrls,
    LDAPControl		**clientctrls,
    struct berval	**servercredp
)
{
	int		err, msgid;
	LDAPMessage	*result;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_sasl_bind_s\n", 0, 0, 0 );

	if ( ( err = ldap_sasl_bind( ld, dn, mechanism, cred, serverctrls,
	    clientctrls, &msgid )) != LDAP_SUCCESS )
		return( err );

	if ( ldap_result( ld, msgid, 1, (struct timeval *) 0, &result ) == -1 )
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );

	if (( err = ldap_parse_sasl_bind_result( ld, result, servercredp, 0 ))
	    != LDAP_SUCCESS ) {
		ldap_msgfree( result );
		return( err );
	}

	return( ldap_result2error( ld, result, 1 ) );
}


/* returns an LDAP error code that indicates if parse succeeded or not */
int
LDAP_CALL
ldap_parse_sasl_bind_result(
    LDAP		*ld,
    LDAPMessage		*res,
    struct berval	**servercredp,
    int			freeit
)
{
	BerElement	ber;
	int		rc, err;
	long		along;
	unsigned long	len;
	char		*m, *e;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_parse_sasl_bind_result\n", 0, 0, 0 );

	/*
	 * the ldapv3 SASL bind response looks like this:
	 *
	 *	BindResponse ::= [APPLICATION 1] SEQUENCE {
	 *		COMPONENTS OF LDAPResult,
	 *		serverSaslCreds [7] OCTET STRING OPTIONAL
	 *	}
	 *
	 * all wrapped up in an LDAPMessage sequence.
	 */

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) ||
	    !NSLDAPI_VALID_LDAPMESSAGE_BINDRESULT_POINTER( res )) {
		return( LDAP_PARAM_ERROR );
	}

	/* only ldapv3 or higher can do sasl binds */
	if ( NSLDAPI_LDAP_VERSION( ld ) < LDAP_VERSION3 ) {
		LDAP_SET_LDERRNO( ld, LDAP_NOT_SUPPORTED, NULL, NULL );
		return( LDAP_NOT_SUPPORTED );
	}

	if ( servercredp != NULL ) {
		*servercredp = NULL;
	}

	ber = *(res->lm_ber);	/* struct copy */

	/* skip past message id, matched dn, error message ... */
	rc = ber_scanf( &ber, "{iaa}", &along, &m, &e );

	if ( rc != LBER_ERROR &&
	    ber_peek_tag( &ber, &len ) == LDAP_TAG_SASL_RES_CREDS ) {
		rc = ber_get_stringal( &ber, servercredp );
	}

	if ( freeit ) {
		ldap_msgfree( res );
	}

	if ( rc == LBER_ERROR ) {
		err = LDAP_DECODING_ERROR;
	} else {
		err = (int) along;
	}

	LDAP_SET_LDERRNO( ld, err, m, e );
	/* this is a little kludge for the 3.0 Barracuda/hammerhead relese */
	/* the docs state that the return is either LDAP_DECODING_ERROR */
	/* or LDAP_SUCCESS.  Here we match the docs...  it's cleaner in 3.1 */

	if ( LDAP_DECODING_ERROR == err ) {
		return (LDAP_DECODING_ERROR);
	} else {
		return( LDAP_SUCCESS );
	}
}
