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
 *  Copyright (c) 1990 Regents of the University of Michigan.
 *  All rights reserved.
 */

/*
 *  unbind.c
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"

int
LDAP_CALL
ldap_unbind( LDAP *ld )
{
	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_unbind\n", 0, 0, 0 );

	return( ldap_ld_free( ld, 1 ) );
}


int
ldap_ld_free( LDAP *ld, int close )
{
	int		i;
	LDAPMessage	*lm, *next;
	int		err = LDAP_SUCCESS;
	LDAPRequest	*lr, *nextlr;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}

	if ( ld->ld_sbp->sb_naddr == 0 ) {
		LDAP_MUTEX_LOCK( ld, LDAP_REQ_LOCK );
		/* free LDAP structure and outstanding requests/responses */
		for ( lr = ld->ld_requests; lr != NULL; lr = nextlr ) {
			nextlr = lr->lr_next;
			nsldapi_free_request( ld, lr, 0 );
		}
		LDAP_MUTEX_UNLOCK( ld, LDAP_REQ_LOCK );

		/* free and unbind from all open connections */
		LDAP_MUTEX_LOCK( ld, LDAP_CONN_LOCK );
		while ( ld->ld_conns != NULL ) {
			nsldapi_free_connection( ld, ld->ld_conns, 1, close );
		}
		LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );

	} else {
		int	i;

		for ( i = 0; i < ld->ld_sbp->sb_naddr; ++i ) {
			NSLDAPI_FREE( ld->ld_sbp->sb_addrs[ i ] );
		}
		NSLDAPI_FREE( ld->ld_sbp->sb_addrs );
		NSLDAPI_FREE( ld->ld_sbp->sb_fromaddr );
	}

	LDAP_MUTEX_LOCK( ld, LDAP_RESP_LOCK );
	for ( lm = ld->ld_responses; lm != NULL; lm = next ) {
		next = lm->lm_next;
		ldap_msgfree( lm );
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_RESP_LOCK );

	/* call cache unbind function to allow it to clean up after itself */
	if ( ld->ld_cache_unbind != NULL ) {
		LDAP_MUTEX_LOCK( ld, LDAP_CACHE_LOCK );
		(void)ld->ld_cache_unbind( ld, 0, 0 );
		LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
	}

	if ( ld->ld_error != NULL )
		NSLDAPI_FREE( ld->ld_error );
	if ( ld->ld_matched != NULL )
		NSLDAPI_FREE( ld->ld_matched );
	if ( ld->ld_host != NULL )
		NSLDAPI_FREE( ld->ld_host );
	if ( ld->ld_ufnprefix != NULL )
		NSLDAPI_FREE( ld->ld_ufnprefix );
	if ( ld->ld_filtd != NULL )
		ldap_getfilter_free( ld->ld_filtd );
	if ( ld->ld_abandoned != NULL )
		NSLDAPI_FREE( ld->ld_abandoned );
	if ( ld->ld_sbp != NULL )
		ber_sockbuf_free( ld->ld_sbp );
	if ( ld->ld_selectinfo != NULL )
		nsldapi_free_select_info( ld->ld_selectinfo );
	if ( ld->ld_defhost != NULL )
		NSLDAPI_FREE( ld->ld_defhost );
	if ( ld->ld_servercontrols != NULL )
		ldap_controls_free( ld->ld_servercontrols );
	if ( ld->ld_clientcontrols != NULL )
		ldap_controls_free( ld->ld_clientcontrols );
	if ( ld->ld_preferred_language != NULL )
		NSLDAPI_FREE( ld->ld_preferred_language );

	/*
	 * XXXmcs: should use cache function pointers to hook in memcache
	 */
	if ( ld->ld_memcache != NULL ) {
		ldap_memcache_set( ld, NULL );
	}

        for( i=0; i<LDAP_MAX_LOCK; i++ )
		LDAP_MUTEX_FREE( ld, ld->ld_mutex[i] );

	NSLDAPI_FREE( ld->ld_mutex );
	NSLDAPI_FREE( (char *) ld );

	return( err );
}

int
LDAP_CALL
ldap_unbind_s( LDAP *ld )
{
	return( ldap_ld_free( ld, 1 ));
}


int
nsldapi_send_unbind( LDAP *ld, Sockbuf *sb )
{
	BerElement	*ber;
	int		err, msgid;

	LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_send_unbind\n", 0, 0, 0 );

	/* create a message to send */
	if (( err = nsldapi_alloc_ber_with_options( ld, &ber ))
	    != LDAP_SUCCESS ) {
		return( err );
	}

	/* fill it in */
	LDAP_MUTEX_LOCK( ld, LDAP_MSGID_LOCK );
	msgid = ++ld->ld_msgid;
	LDAP_MUTEX_UNLOCK( ld, LDAP_MSGID_LOCK );

	if ( ber_printf( ber, "{itn", msgid, LDAP_REQ_UNBIND ) == -1 ) {
		ber_free( ber, 1 );
		err = LDAP_ENCODING_ERROR;
		LDAP_SET_LDERRNO( ld, err, NULL, NULL );
		return( err );
	}

	if (( err = nsldapi_put_controls( ld, NULL, 1, ber ))
	    != LDAP_SUCCESS ) {
		ber_free( ber, 1 );
		return( err );
	}

	/* send the message */
	if ( nsldapi_ber_flush( ld, sb, ber, 1, 0 ) != 0 ) {
		ber_free( ber, 1 );
		err = LDAP_SERVER_DOWN;
		LDAP_SET_LDERRNO( ld, err, NULL, NULL );
		return( err );
	}

	return( LDAP_SUCCESS );
}
