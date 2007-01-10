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

	return( ldap_ld_free( ld, NULL, NULL, 1 ) );
}


int
LDAP_CALL
ldap_unbind_s( LDAP *ld )
{
	return( ldap_ld_free( ld, NULL, NULL, 1 ));
}


int
LDAP_CALL
ldap_unbind_ext( LDAP *ld, LDAPControl **serverctrls,
    LDAPControl **clientctrls )
{
	return( ldap_ld_free( ld, serverctrls, clientctrls, 1 ));
}


/*
 * Dispose of the LDAP session ld, including all associated connections
 * and resources.  If close is non-zero, an unbind() request is sent as well.
 */
int
ldap_ld_free( LDAP *ld, LDAPControl **serverctrls,
    LDAPControl **clientctrls, int close )
{
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
			nsldapi_free_connection( ld, ld->ld_conns, serverctrls,
			    clientctrls, 1, close );
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

	/* call the dispose handle I/O callback if one is defined */
	if ( ld->ld_extdisposehandle_fn != NULL ) {
	    /*
	     * We always pass the session extended I/O argument to
	     * the dispose handle callback.
	     */
	    ld->ld_extdisposehandle_fn( ld, ld->ld_ext_session_arg );
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
	if ( ld->ld_defhost != NULL )
		NSLDAPI_FREE( ld->ld_defhost );
	if ( ld->ld_servercontrols != NULL )
		ldap_controls_free( ld->ld_servercontrols );
	if ( ld->ld_clientcontrols != NULL )
		ldap_controls_free( ld->ld_clientcontrols );
	if ( ld->ld_preferred_language != NULL )
		NSLDAPI_FREE( ld->ld_preferred_language );
	nsldapi_iostatus_free( ld );
#ifdef LDAP_SASLIO_HOOKS
	NSLDAPI_FREE( ld->ld_def_sasl_mech );
	NSLDAPI_FREE( ld->ld_def_sasl_realm );
	NSLDAPI_FREE( ld->ld_def_sasl_authcid );
	NSLDAPI_FREE( ld->ld_def_sasl_authzid );
#endif

	/*
	 * XXXmcs: should use cache function pointers to hook in memcache
	 */
	if ( ld->ld_memcache != NULL ) {
		ldap_memcache_set( ld, NULL );
	}

	/* free all mutexes we have allocated */
	nsldapi_mutex_free_all( ld );
	NSLDAPI_FREE( ld->ld_mutex );

	NSLDAPI_FREE( (char *) ld );

	return( err );
}



int
nsldapi_send_unbind( LDAP *ld, Sockbuf *sb, LDAPControl **serverctrls,
    LDAPControl **clientctrls )
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

	if (( err = nsldapi_put_controls( ld, serverctrls, 1, ber ))
	    != LDAP_SUCCESS ) {
		ber_free( ber, 1 );
		return( err );
	}

	/* send the message */
        err = nsldapi_send_ber_message( ld, sb, ber, 1 /* free ber */,
        	0 /* will not handle EPIPE */ );
	if ( err != 0 ) {
		ber_free( ber, 1 );
		if (err != -2 ) {
			/*
			 * Message could not be sent, and the reason is
			 * something other than a "would block" error.
			 * Note that we ignore "would block" errors because
			 * it is not critical that an UnBind request
			 * actually reach the LDAP server.
			 */
			err = LDAP_SERVER_DOWN;
			LDAP_SET_LDERRNO( ld, err, NULL, NULL );
			return( err );
		}
	}

	return( LDAP_SUCCESS );
}
