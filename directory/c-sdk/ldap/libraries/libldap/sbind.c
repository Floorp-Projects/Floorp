/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*
 *  Copyright (c) 1993 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  sbind.c
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1993 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"

static int simple_bind_nolock( LDAP *ld, const char *dn, const char *passwd,
	int unlock_permitted );
static int simple_bindifnot_s( LDAP *ld, const char *dn, const char *passwd );

/*
 * ldap_simple_bind - bind to the ldap server (and X.500).  The dn and
 * password of the entry to which to bind are supplied.  The message id
 * of the request initiated is returned.
 *
 * Example:
 *	ldap_simple_bind( ld, "cn=manager, o=university of michigan, c=us",
 *	    "secret" )
 */

int
LDAP_CALL
ldap_simple_bind( LDAP *ld, const char *dn, const char *passwd )
{
	int	rc;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_simple_bind\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( -1 );
	}

	rc = simple_bind_nolock( ld, dn, passwd, 1 );

	return( rc );
}


static int
simple_bind_nolock( LDAP *ld, const char *dn, const char *passwd,
    int unlock_permitted )
{
	BerElement	*ber;
	int		rc, msgid;

	/*
	 * The bind request looks like this:
	 *	BindRequest ::= SEQUENCE {
	 *		version		INTEGER,
	 *		name		DistinguishedName,	 -- who
	 *		authentication	CHOICE {
	 *			simple		[0] OCTET STRING -- passwd
	 *		}
	 *	}
	 * all wrapped up in an LDAPMessage sequence.
	 */

	LDAP_MUTEX_LOCK( ld, LDAP_MSGID_LOCK );
	msgid = ++ld->ld_msgid;
	LDAP_MUTEX_UNLOCK( ld, LDAP_MSGID_LOCK );

	if ( dn == NULL )
		dn = "";
	if ( passwd == NULL )
		passwd = "";

	if ( ld->ld_cache_on && ld->ld_cache_bind != NULL ) {
		struct berval	bv;

		bv.bv_val = (char *)passwd;
		bv.bv_len = strlen( passwd );
		/* if ( unlock_permitted ) LDAP_MUTEX_UNLOCK( ld ); */
		LDAP_MUTEX_LOCK( ld, LDAP_CACHE_LOCK );
		rc = (ld->ld_cache_bind)( ld, msgid, LDAP_REQ_BIND, dn, &bv,
		    LDAP_AUTH_SIMPLE );
		LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
		/* if ( unlock_permitted ) LDAP_MUTEX_LOCK( ld ); */
		if ( rc != 0 ) {
			return( rc );
		}
	}

	/* create a message to send */
	if (( rc = nsldapi_alloc_ber_with_options( ld, &ber ))
	    != LDAP_SUCCESS ) {
		return( -1 );
	}

	/* fill it in */
	if ( ber_printf( ber, "{it{ists}", msgid, LDAP_REQ_BIND,
	    NSLDAPI_LDAP_VERSION( ld ), dn, LDAP_AUTH_SIMPLE, passwd ) == -1 ) {
		LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
		ber_free( ber, 1 );
		return( -1 );
	}

	if ( nsldapi_put_controls( ld, NULL, 1, ber ) != LDAP_SUCCESS ) {
		ber_free( ber, 1 );
		return( -1 );
	}

	/* send the message */
	return( nsldapi_send_initial_request( ld, msgid, LDAP_REQ_BIND,
		(char *)dn, ber ));
}


/*
 * ldap_simple_bind - bind to the ldap server (and X.500) using simple
 * authentication.  The dn and password of the entry to which to bind are
 * supplied.  LDAP_SUCCESS is returned upon success, the ldap error code
 * otherwise.
 *
 * Example:
 *	ldap_simple_bind_s( ld, "cn=manager, o=university of michigan, c=us",
 *	    "secret" )
 */
int
LDAP_CALL
ldap_simple_bind_s( LDAP *ld, const char *dn, const char *passwd )
{
	int		msgid;
	LDAPMessage	*result;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_simple_bind_s\n", 0, 0, 0 );

	if ( NSLDAPI_VALID_LDAP_POINTER( ld ) &&
	    ( ld->ld_options & LDAP_BITOPT_RECONNECT ) != 0 ) {
		return( simple_bindifnot_s( ld, dn, passwd ));
	}

	if ( (msgid = ldap_simple_bind( ld, dn, passwd )) == -1 )
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );

	if ( ldap_result( ld, msgid, 1, (struct timeval *) 0, &result ) == -1 )
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );

	return( ldap_result2error( ld, result, 1 ) );
}


/*
 * simple_bindifnot_s() is like ldap_simple_bind_s() except that it only does
 * a bind if the default connection is not currently bound.
 * If a successful bind using the same DN has already taken place we just
 * return LDAP_SUCCESS without conversing with the server at all.
 */
static int
simple_bindifnot_s( LDAP *ld, const char *dn, const char *passwd )
{
	int		msgid, rc;
	LDAPMessage	*result;
	char		*binddn;

	LDAPDebug( LDAP_DEBUG_TRACE, "simple_bindifnot_s\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}

	if ( dn == NULL ) {
		dn = "";	/* to make comparisons simpler */
	}

	/*
	 * if we are already bound using the same DN, just return LDAP_SUCCESS.
	 */
	if ( NULL != ( binddn = nsldapi_get_binddn( ld ))
	    && 0 == strcmp( dn, binddn )) {
		rc = LDAP_SUCCESS;
		LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
		goto unlock_and_return;
	}

	/*
	 * if the default connection has been lost and is now marked dead,
	 * dispose of the default connection so it will get re-established.
	 */
	LDAP_MUTEX_LOCK( ld, LDAP_CONN_LOCK );
	if ( NULL != ld->ld_defconn 
		&& LDAP_CONNST_DEAD == ld->ld_defconn->lconn_status ) {
	    nsldapi_free_connection( ld, ld->ld_defconn, 1, 0 );
	    ld->ld_defconn = NULL;
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );

	/*
	 * finally, bind (this will open a new connection if necessary)
	 */
	if ( (msgid = simple_bind_nolock( ld, dn, passwd, 0 )) == -1 ) {
		rc = LDAP_GET_LDERRNO( ld, NULL, NULL );
		goto unlock_and_return;
	}

	if ( nsldapi_result_nolock( ld, msgid, 1, 0, (struct timeval *) 0,
	    &result ) == -1 ) {
		rc = LDAP_GET_LDERRNO( ld, NULL, NULL );
		goto unlock_and_return;
	}

	rc = ldap_result2error( ld, result, 1 );

unlock_and_return:
	return( rc );
}
