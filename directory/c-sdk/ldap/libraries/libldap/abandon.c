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
 *  abandon.c
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"

static int do_abandon( LDAP *ld, int origid, int msgid,
    LDAPControl **serverctrls, LDAPControl **clientctrls );
static int nsldapi_send_abandon_message( LDAP *ld, LDAPConn *lc, 
    BerElement *ber, int abandon_msgid );

/*
 * ldap_abandon - perform an ldap abandon operation. Parameters:
 *
 *	ld		LDAP descriptor
 *	msgid		The message id of the operation to abandon
 *
 * ldap_abandon returns 0 if everything went ok, -1 otherwise.
 *
 * Example:
 *	ldap_abandon( ld, msgid );
 */
int
LDAP_CALL
ldap_abandon( LDAP *ld, int msgid )
{
	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_abandon %d\n", msgid, 0, 0 );
	LDAPDebug( LDAP_DEBUG_TRACE, "4e65747363617065\n", msgid, 0, 0 );
	LDAPDebug( LDAP_DEBUG_TRACE, "466f726576657221\n", msgid, 0, 0 );

	if ( ldap_abandon_ext( ld, msgid, NULL, NULL ) == LDAP_SUCCESS ) {
	    return( 0 );
	}

	return( -1 );
}


/*
 * LDAPv3 extended abandon.
 * Returns an LDAP error code.
 */
int
LDAP_CALL
ldap_abandon_ext( LDAP *ld, int msgid, LDAPControl **serverctrls,
	LDAPControl **clientctrls )
{
	int	rc;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_abandon_ext %d\n", msgid, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}

	LDAP_MUTEX_LOCK( ld, LDAP_CONN_LOCK );
	LDAP_MUTEX_LOCK( ld, LDAP_REQ_LOCK );
	rc = do_abandon( ld, msgid, msgid, serverctrls, clientctrls );

	/*
	 * XXXmcs should use cache function pointers to hook in memcache
	 */
	ldap_memcache_abandon( ld, msgid );

	LDAP_MUTEX_UNLOCK( ld, LDAP_REQ_LOCK );
	LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );

	return( rc );
}


/*
 * Abandon all outstanding requests for msgid (included child requests
 * spawned when chasing referrals).  This function calls itself recursively.
 * No locking is done is this function so it must be done by the caller.
 * Returns an LDAP error code and sets it in LDAP *ld as well
 */
static int
do_abandon( LDAP *ld, int origid, int msgid, LDAPControl **serverctrls,
    LDAPControl **clientctrls )
{
	BerElement	*ber;
	int		i, bererr, lderr, sendabandon;
	LDAPRequest	*lr = NULL;

	/*
	 * An abandon request looks like this:
	 *	AbandonRequest ::= MessageID
	 */
	LDAPDebug( LDAP_DEBUG_TRACE, "do_abandon origid %d, msgid %d\n",
		origid, msgid, 0 );

	/* optimistic */
	lderr = LDAP_SUCCESS;	

        /*
	 * Find the request that we are abandoning.  Don't send an
	 * abandon message unless there is something to abandon.
	 */
        sendabandon = 0;
	for ( lr = ld->ld_requests; lr != NULL; lr = lr->lr_next ) {
		if ( lr->lr_msgid == msgid ) {	/* this message */
			if ( origid == msgid && lr->lr_parent != NULL ) {
				/* don't let caller abandon child requests! */
				lderr = LDAP_PARAM_ERROR;
				goto set_errorcode_and_return;
			}
			if ( lr->lr_status == LDAP_REQST_INPROGRESS ) {
			 	/*
				 * We only need to send an abandon message if
				 * the request is in progress.
				 */
			    sendabandon = 1;
			}
			break;
		}
		if ( lr->lr_origid == msgid ) {	/* child:  abandon it */
			(void)do_abandon( ld, msgid, lr->lr_msgid,
			      serverctrls, clientctrls );
			/* we ignore errors from child abandons... */
		}
	}

	if ( ldap_msgdelete( ld, msgid ) == 0 ) {
		/* we had all the results and deleted them */
		goto set_errorcode_and_return;
	}

	if ( lr != NULL && sendabandon ) {
		/* create a message to send */
		if (( lderr = nsldapi_alloc_ber_with_options( ld, &ber )) ==
		    LDAP_SUCCESS ) {
			int	abandon_msgid;

			LDAP_MUTEX_LOCK( ld, LDAP_MSGID_LOCK );
			abandon_msgid = ++ld->ld_msgid;
			LDAP_MUTEX_UNLOCK( ld, LDAP_MSGID_LOCK );
#ifdef CLDAP
			if ( ld->ld_dbp->sb_naddr > 0 ) {
				bererr = ber_printf( ber, "{isti",
				    abandon_msgid, ld->ld_cldapdn,
				    LDAP_REQ_ABANDON, msgid );
			} else {
#endif /* CLDAP */
				bererr = ber_printf( ber, "{iti",
				    abandon_msgid, LDAP_REQ_ABANDON, msgid );
#ifdef CLDAP
			}
#endif /* CLDAP */

			if ( bererr == -1 ||
			    ( lderr = nsldapi_put_controls( ld, serverctrls,
			    1, ber )) != LDAP_SUCCESS ) {
				lderr = LDAP_ENCODING_ERROR;
				ber_free( ber, 1 );
			} else {
				/* try to send the message */
				lderr = nsldapi_send_abandon_message( ld,
				    lr->lr_conn, ber, abandon_msgid );
			}
		}
	}

	if ( lr != NULL ) {
		/*
		 * Always call nsldapi_free_connection() so that the connection's
		 * ref count is correctly decremented.  It is OK to always pass
		 * 1 for the "unbind" parameter because doing so will only affect
		 * connections that resulted from a child request (because the
		 * default connection's ref count never goes to zero).
		 */
		nsldapi_free_connection( ld, lr->lr_conn, NULL, NULL,
			0 /* do not force */, 1 /* send unbind before closing */ );

		/*
		 * Free the entire request chain if we finished abandoning everything.
		 */
		if ( origid == msgid ) {
			nsldapi_free_request( ld, lr, 0 );
		}
	}

	/*
	 * Record the abandoned message ID (used to discard any server responses
	 * that arrive later).
	 */
	LDAP_MUTEX_LOCK( ld, LDAP_ABANDON_LOCK );
	if ( ld->ld_abandoned == NULL ) {
		if ( (ld->ld_abandoned = (int *)NSLDAPI_MALLOC( 2
		    * sizeof(int) )) == NULL ) {
			lderr = LDAP_NO_MEMORY;
			LDAP_MUTEX_UNLOCK( ld, LDAP_ABANDON_LOCK );
			goto set_errorcode_and_return;
		}
		i = 0;
	} else {
		for ( i = 0; ld->ld_abandoned[i] != -1; i++ )
			;	/* NULL */
		if ( (ld->ld_abandoned = (int *)NSLDAPI_REALLOC( (char *)
		    ld->ld_abandoned, (i + 2) * sizeof(int) )) == NULL ) {
			lderr = LDAP_NO_MEMORY;
			LDAP_MUTEX_UNLOCK( ld, LDAP_ABANDON_LOCK );
			goto set_errorcode_and_return;
		}
	}
	ld->ld_abandoned[i] = msgid;
	ld->ld_abandoned[i + 1] = -1;
	LDAP_MUTEX_UNLOCK( ld, LDAP_ABANDON_LOCK );

set_errorcode_and_return:
	LDAP_SET_LDERRNO( ld, lderr, NULL, NULL );
	return( lderr );
}

/*
 * Try to send the abandon message that is encoded in ber.  Returns an
 * LDAP result code.
 */
static int
nsldapi_send_abandon_message( LDAP *ld, LDAPConn *lc, BerElement *ber,
    int abandon_msgid )
{
	int	lderr = LDAP_SUCCESS;
	int	err = 0;

	err = nsldapi_send_ber_message( ld, lc->lconn_sb,
	    ber, 1 /* free ber */, 0 /* will not handle EPIPE */ );
	if ( err == -2 ) {
		/*
		 * "Would block" error.  Queue the abandon as
		 * a pending request.
		 */
		LDAPRequest	*lr;

		lr = nsldapi_new_request( lc, ber, abandon_msgid,
		    0 /* no response expected */ );
		if ( lr == NULL ) {
			lderr = LDAP_NO_MEMORY;
			ber_free( ber, 1 );
		} else {
			lr->lr_status = LDAP_REQST_WRITING;
			nsldapi_queue_request_nolock( ld, lr );
			++lc->lconn_pending_requests;
			nsldapi_iostatus_interest_write( ld,
			    lc->lconn_sb );
		}
	} else if ( err != 0 ) {
		/*
		 * Fatal error (not a "would block" error).
		 */
		lderr = LDAP_SERVER_DOWN;
		ber_free( ber, 1 );
	}

	return( lderr );
}
