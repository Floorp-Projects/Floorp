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
	Sockbuf		*sb;
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
 * this is not the best implementation...  
 * the code special cases the when async io is enabled.
 * The logic is clear this way, at the cost of code bloat.
 * This logic should be cleaned up post nova 4.5 rtm
 */
    if (ld->ld_options & LDAP_BITOPT_ASYNC)
    {
        /* Don't send an abandon message unless there is something to abandon. */
        sendabandon = 0;

        /* Find the request that we are abandoning. */
        if (ld->ld_requests != NULL) {
            for ( lr = ld->ld_requests; lr != NULL; lr = lr->lr_next ) {
                if ( lr->lr_msgid == msgid ) {	/* this message */
                    if ( origid == msgid && lr->lr_parent != NULL ) {
                        /* don't let caller abandon child requests! */
                        lderr = LDAP_PARAM_ERROR;
                        goto set_errorcode_and_return;
                    }
                    if ( lr->lr_status == LDAP_REQST_INPROGRESS ) {
                        /* We only need to send an abandon message if the request
                         * is in progress.
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
        }
    }
    else
    {
        sendabandon = 1;
        /* find the request that we are abandoning */
        for ( lr = ld->ld_requests; lr != NULL; lr = lr->lr_next ) {
            if ( lr->lr_msgid == msgid ) {	/* this message */
                break;
            }
            if ( lr->lr_origid == msgid ) {	/* child:  abandon it */
                (void)do_abandon( ld, msgid, lr->lr_msgid,
                                  serverctrls, clientctrls );
                /* we ignore errors from child abandons... */
            }
        }
        
        if ( lr != NULL ) {
            if ( origid == msgid && lr->lr_parent != NULL ) {
                /* don't let caller abandon child requests! */
                lderr = LDAP_PARAM_ERROR;
                goto set_errorcode_and_return;
            }
            if ( lr->lr_status != LDAP_REQST_INPROGRESS ) {
                /* no need to send abandon message */
                sendabandon = 0;
            }
        }
    }
	if ( ldap_msgdelete( ld, msgid ) == 0 ) {
		/* we had all the results and deleted them */
		goto set_errorcode_and_return;
	}

	if ( sendabandon ) {
		/* create a message to send */
		if (( lderr = nsldapi_alloc_ber_with_options( ld, &ber )) ==
		    LDAP_SUCCESS ) {
			LDAP_MUTEX_LOCK( ld, LDAP_MSGID_LOCK );
#ifdef CLDAP
			if ( ld->ld_dbp->sb_naddr > 0 ) {
				bererr = ber_printf( ber, "{isti",
				    ++ld->ld_msgid, ld->ld_cldapdn,
				    LDAP_REQ_ABANDON, msgid );
			} else {
#endif /* CLDAP */
				bererr = ber_printf( ber, "{iti",
				    ++ld->ld_msgid, LDAP_REQ_ABANDON, msgid );
#ifdef CLDAP
			}
#endif /* CLDAP */
			LDAP_MUTEX_UNLOCK( ld, LDAP_MSGID_LOCK );

			if ( bererr == -1 ||
			    ( lderr = nsldapi_put_controls( ld, serverctrls,
			    1, ber )) != LDAP_SUCCESS ) {
				lderr = LDAP_ENCODING_ERROR;
				ber_free( ber, 1 );
			} else {
				/* send the message */
				if ( lr != NULL ) {
					sb = lr->lr_conn->lconn_sb;
				} else {
					sb = ld->ld_sbp;
				}
				if ( nsldapi_ber_flush( ld, sb, ber, 1, 0 )
				    != 0 ) {
					lderr = LDAP_SERVER_DOWN;
				}
			}
		}
	}

	if ( lr != NULL ) {
		if ( sendabandon ) {
			nsldapi_free_connection( ld, lr->lr_conn, NULL, NULL,
			    0, 1 );
		}
		if ( origid == msgid ) {
			nsldapi_free_request( ld, lr, 0 );
		}
	}


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
