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
 *  result.c - wait for an ldap result
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"

static int check_response_queue( LDAP *ld, int msgid, int all,
	int do_abandon_check, LDAPMessage **result );
static int ldap_abandoned( LDAP *ld, int msgid );
static int ldap_mark_abandoned( LDAP *ld, int msgid );
static int wait4msg( LDAP *ld, int msgid, int all, int unlock_permitted,
	struct timeval *timeout, LDAPMessage **result );
static int read1msg( LDAP *ld, int msgid, int all, Sockbuf *sb, LDAPConn *lc,
	LDAPMessage **result );
static void check_for_refs( LDAP *ld, LDAPRequest *lr, BerElement *ber,
	int ldapversion, int *totalcountp, int *chasingcountp );
static int build_result_ber( LDAP *ld, BerElement **berp, LDAPRequest *lr );
static void merge_error_info( LDAP *ld, LDAPRequest *parentr, LDAPRequest *lr );
#if defined( CLDAP )
static int cldap_select1( LDAP *ld, struct timeval *timeout );
#endif
static void link_pend( LDAP *ld, LDAPPend *lp );
static void unlink_pend( LDAP *ld, LDAPPend *lp );
static int unlink_msg( LDAP *ld, int msgid, int all );
static int nsldapi_mutex_trylock( LDAP *ld, LDAPLock lockno );

/*
 * ldap_result - wait for an ldap result response to a message from the
 * ldap server.  If msgid is -1, any message will be accepted, otherwise
 * ldap_result will wait for a response with msgid.  If all is 0 the
 * first message with id msgid will be accepted, otherwise, ldap_result
 * will wait for all responses with id msgid and then return a pointer to
 * the entire list of messages.  This is only useful for search responses,
 * which can be of two message types (zero or more entries, followed by an
 * ldap result).  The type of the first message received is returned.
 * When waiting, any messages that have been abandoned are discarded.
 *
 * Example:
 *	ldap_result( s, msgid, all, timeout, result )
 */
int
LDAP_CALL
ldap_result(
    LDAP 		*ld,
    int 		msgid,
    int 		all,
    struct timeval	*timeout,
    LDAPMessage		**result
)
{
	int		rc, ret;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_result\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( -1 );	/* punt */
	}

	while( 1 ) {
		if( (ret = nsldapi_mutex_trylock( ld, LDAP_RESULT_LOCK )) == 0 )
		{
			LDAP_MUTEX_BC_LOCK( ld, LDAP_RESULT_LOCK );
			rc= nsldapi_result_nolock( ld, msgid, all, 1, timeout, result );
			break;
		}
		else {
			rc = nsldapi_wait_result( ld, msgid, all, timeout, result );
			if( rc == -2 )
				continue;
			else
				break;
		}
	}

	return( rc );
}


int
nsldapi_result_nolock( LDAP *ld, int msgid, int all, int unlock_permitted,
    struct timeval *timeout, LDAPMessage **result )
{
	int		rc;

	LDAPDebug( LDAP_DEBUG_TRACE,
		"nsldapi_result_nolock (msgid=%d, all=%d)\n", msgid, all, 0 );

	/*
	 * First, look through the list of responses we have received on
	 * this association and see if the response we're interested in
	 * is there.  If it is, return it.  If not, call wait4msg() to
	 * wait until it arrives or timeout occurs.
	 */

	if ( result == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( -1 );
	}

	if ( check_response_queue( ld, msgid, all, 1, result ) != 0 ) {
		LDAP_SET_LDERRNO( ld, LDAP_SUCCESS, NULL, NULL );
		rc = (*result)->lm_msgtype;
	} else {
		rc = wait4msg( ld, msgid, all, unlock_permitted, timeout,
		    result );
	}

	/*
	 * XXXmcs should use cache function pointers to hook in memcache
	 */
	if ( ld->ld_memcache != NULL && NSLDAPI_SEARCH_RELATED_RESULT( rc ) &&
	     !((*result)->lm_fromcache )) {
		ldap_memcache_append( ld, (*result)->lm_msgid,
		    (all || NSLDAPI_IS_SEARCH_RESULT( rc )), *result );
	}

	LDAP_MUTEX_UNLOCK( ld, LDAP_RESULT_LOCK );
	POST( ld, LDAP_RES_ANY, NULL );
	return( rc );
}


/*
 * Look through the list of queued responses for a message that matches the
 * criteria in the msgid and all parameters.  msgid == LDAP_RES_ANY matches
 * all ids.
 *
 * If an appropriate message is found, a non-zero value is returned and the
 * message is dequeued and assigned to *result.
 *
 * If not, *result is set to NULL and this function returns 0.
 */
static int
check_response_queue( LDAP *ld, int msgid, int all, int do_abandon_check,
    LDAPMessage **result )
{
	LDAPMessage	*lm, *lastlm, *nextlm;
	LDAPRequest	*lr;

	LDAPDebug( LDAP_DEBUG_TRACE,
	    "=> check_response_queue (msgid=%d, all=%d)\n", msgid, all, 0 );

	*result = NULL;
	lastlm = NULL;
	LDAP_MUTEX_LOCK( ld, LDAP_RESP_LOCK );
	for ( lm = ld->ld_responses; lm != NULL; lm = nextlm ) {
		nextlm = lm->lm_next;

		if ( do_abandon_check && ldap_abandoned( ld, lm->lm_msgid ) ) {
			ldap_mark_abandoned( ld, lm->lm_msgid );

			if ( lastlm == NULL ) {
				ld->ld_responses = lm->lm_next;
			} else {
				lastlm->lm_next = nextlm;
			}

			ldap_msgfree( lm );

			continue;
		}

		if ( msgid == LDAP_RES_ANY || lm->lm_msgid == msgid ) {
			LDAPMessage	*tmp;

			if ( all == 0
			    || (lm->lm_msgtype != LDAP_RES_SEARCH_RESULT
			    && lm->lm_msgtype != LDAP_RES_SEARCH_REFERENCE
			    && lm->lm_msgtype != LDAP_RES_SEARCH_ENTRY) )
				break;

			for ( tmp = lm; tmp != NULL; tmp = tmp->lm_chain ) {
				if ( tmp->lm_msgtype == LDAP_RES_SEARCH_RESULT )
					break;
			}

			if ( tmp == NULL ) {
				LDAP_MUTEX_UNLOCK( ld, LDAP_RESP_LOCK );
				LDAPDebug( LDAP_DEBUG_TRACE,
				    "<= check_response_queue NOT FOUND\n",
				    0, 0, 0 );
				return( 0 );	/* no message to return */
			}

			break;
		}
		lastlm = lm;
	}

	/*
	 * if we did not find a message OR if the one we found is a result for
	 * a request that is still pending, return failure.
	 */
	if ( lm == NULL 
             || ( lr = nsldapi_find_request_by_msgid( ld, lm->lm_msgid ))
		   != NULL && lr->lr_outrefcnt > 0 ) {
		LDAP_MUTEX_UNLOCK( ld, LDAP_RESP_LOCK );
		LDAPDebug( LDAP_DEBUG_TRACE,
		    "<= check_response_queue NOT FOUND\n",
		    0, 0, 0 );
		return( 0 );	/* no message to return */
	}

	if ( all == 0 ) {
		if ( lm->lm_chain == NULL ) {
			if ( lastlm == NULL ) {
				ld->ld_responses = lm->lm_next;
			} else {
				lastlm->lm_next = lm->lm_next;
			}
		} else {
			if ( lastlm == NULL ) {
				ld->ld_responses = lm->lm_chain;
				ld->ld_responses->lm_next = lm->lm_next;
			} else {
				lastlm->lm_next = lm->lm_chain;
				lastlm->lm_next->lm_next = lm->lm_next;
			}
		}
	} else {
		if ( lastlm == NULL ) {
			ld->ld_responses = lm->lm_next;
		} else {
			lastlm->lm_next = lm->lm_next;
		}
	}

	if ( all == 0 ) {
		lm->lm_chain = NULL;
	}
	lm->lm_next = NULL;
	LDAP_MUTEX_UNLOCK( ld, LDAP_RESP_LOCK );

	*result = lm;
	LDAPDebug( LDAP_DEBUG_TRACE,
	    "<= check_response_queue returning msgid %d type %d\n",
	    lm->lm_msgid, lm->lm_msgtype, 0 );
	return( 1 );	/* a message was found and returned in *result */
}


static int
wait4msg( LDAP *ld, int msgid, int all, int unlock_permitted,
	struct timeval *timeout, LDAPMessage **result )
{
	int		rc;
	struct timeval	tv, *tvp;
	long		start_time = 0, tmp_time;
	LDAPConn	*lc, *nextlc;
	LDAPRequest	*lr;

#ifdef LDAP_DEBUG
	if ( timeout == NULL ) {
		LDAPDebug( LDAP_DEBUG_TRACE, "wait4msg (infinite timeout)\n",
		    0, 0, 0 );
	} else {
		LDAPDebug( LDAP_DEBUG_TRACE, "wait4msg (timeout %ld sec, %ld usec)\n",
		    timeout->tv_sec, timeout->tv_usec, 0 );
	}
#endif /* LDAP_DEBUG */

	/* check the cache */
	if ( ld->ld_cache_on && ld->ld_cache_result != NULL ) {
		/* if ( unlock_permitted ) LDAP_MUTEX_UNLOCK( ld ); */
		LDAP_MUTEX_LOCK( ld, LDAP_CACHE_LOCK );
		rc = (ld->ld_cache_result)( ld, msgid, all, timeout, result );
		LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
		/* if ( unlock_permitted ) LDAP_MUTEX_LOCK( ld ); */
		if ( rc != 0 ) {
			return( rc );
		}
		if ( ld->ld_cache_strategy == LDAP_CACHE_LOCALDB ) {
			LDAP_SET_LDERRNO( ld, LDAP_TIMEOUT, NULL, NULL );
			return( 0 );	/* timeout */
		}
	}

	/*
	 * if we are looking for a specific msgid, check to see if it is
	 * associated with a dead connection and return an error if so.
	 */
	if ( msgid != LDAP_RES_ANY ) {
		LDAP_MUTEX_LOCK( ld, LDAP_REQ_LOCK );
		if (( lr = nsldapi_find_request_by_msgid( ld, msgid ))
		    == NULL ) {
			LDAP_MUTEX_UNLOCK( ld, LDAP_REQ_LOCK );
			LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL,
				nsldapi_strdup( "unknown message id" ));
			return( -1 );	/* could not find request for msgid */
		}
		if ( lr->lr_conn != NULL &&
		    lr->lr_conn->lconn_status == LDAP_CONNST_DEAD ) {
			nsldapi_free_request( ld, lr, 1 );
			LDAP_MUTEX_UNLOCK( ld, LDAP_REQ_LOCK );
			LDAP_SET_LDERRNO( ld, LDAP_SERVER_DOWN, NULL, NULL );
			return( -1 );	/* connection dead */
		}
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_REQ_LOCK );

	if ( timeout == NULL ) {
		tvp = NULL;
	} else {
		tv = *timeout;
		tvp = &tv;
		start_time = (long)time( NULL );
	}

	rc = -2;
	while ( rc == -2 ) {
#ifdef LDAP_DEBUG
		if ( ldap_debug & LDAP_DEBUG_TRACE ) {
			nsldapi_dump_connection( ld, ld->ld_conns, 1 );
			nsldapi_dump_requests_and_responses( ld );
		}
#endif /* LDAP_DEBUG */
		LDAP_MUTEX_LOCK( ld, LDAP_CONN_LOCK );
		LDAP_MUTEX_LOCK( ld, LDAP_REQ_LOCK );
		for ( lc = ld->ld_conns; lc != NULL; lc = lc->lconn_next ) {
			if ( lc->lconn_sb->sb_ber.ber_ptr <
			    lc->lconn_sb->sb_ber.ber_end ) {
				rc = read1msg( ld, msgid, all, lc->lconn_sb,
				    lc, result );
				break;
			}
		}
		LDAP_MUTEX_UNLOCK( ld, LDAP_REQ_LOCK );
		LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );

		if ( lc == NULL ) {
			rc = nsldapi_do_ldap_select( ld, tvp );

#if defined( LDAP_DEBUG ) && !defined( macintosh ) && !defined( DOS )
			if ( rc == -1 ) {
			    LDAPDebug( LDAP_DEBUG_TRACE,
				    "nsldapi_do_ldap_select returned -1: errno %d\n",
				    LDAP_GET_ERRNO( ld ), 0, 0 );
			}
#endif

#if !defined( macintosh ) && !defined( DOS )
			if ( rc == 0 || ( rc == -1 && (( ld->ld_options &
			    LDAP_BITOPT_RESTART ) == 0 ||
			    LDAP_GET_ERRNO( ld ) != EINTR ))) {
#else
			if ( rc == -1 || rc == 0 ) {
#endif
				LDAP_SET_LDERRNO( ld, (rc == -1 ?
				    LDAP_SERVER_DOWN : LDAP_TIMEOUT), NULL,
				    NULL );
				if ( rc == -1 ) {
					LDAP_MUTEX_LOCK( ld, LDAP_REQ_LOCK );
					nsldapi_connection_lost_nolock( ld,
						NULL );
					LDAP_MUTEX_UNLOCK( ld, LDAP_REQ_LOCK );
				}
				return( rc );
			}

			if ( rc == -1 ) {
				rc = -2;	/* select interrupted: loop */
			} else {
				rc = -2;
				LDAP_MUTEX_LOCK( ld, LDAP_CONN_LOCK );
				LDAP_MUTEX_LOCK( ld, LDAP_REQ_LOCK );
				for ( lc = ld->ld_conns; rc == -2 && lc != NULL;
				    lc = nextlc ) {
					nextlc = lc->lconn_next;
					if ( lc->lconn_status ==
					    LDAP_CONNST_CONNECTED &&
					    nsldapi_is_read_ready( ld,
					    lc->lconn_sb )) {
						rc = read1msg( ld, msgid, all,
						    lc->lconn_sb, lc, result );
					}
					else if (ld->ld_options & LDAP_BITOPT_ASYNC) {
                        if(   lr
                              && lc->lconn_status == LDAP_CONNST_CONNECTING
                              && nsldapi_is_write_ready( ld, lc->lconn_sb ) ) {
                            rc = nsldapi_ber_flush( ld, lc->lconn_sb, lr->lr_ber, 0, 1 );
                            if ( rc == 0 ) {
                                rc = LDAP_RES_BIND;
                                lc->lconn_status = LDAP_CONNST_CONNECTED;
                                
                                lr->lr_ber->ber_end = lr->lr_ber->ber_ptr;
                                lr->lr_ber->ber_ptr = lr->lr_ber->ber_buf;
                                nsldapi_mark_select_read( ld, lc->lconn_sb );
                            }
                            else if ( rc == -1 ) {
                                LDAP_SET_LDERRNO( ld, LDAP_SERVER_DOWN, NULL, NULL );
                                nsldapi_free_request( ld, lr, 0 );
                                nsldapi_free_connection( ld, lc, 0, 0 );
                            }
                        }
                        
					}
				}
				LDAP_MUTEX_UNLOCK( ld, LDAP_REQ_LOCK );
				LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );
			}
		}

		/*
		 * It is possible that recursion occurred while chasing
		 * referrals and as a result the message we are looking
		 * for may have been placed on the response queue.  Look
		 * for it there before continuing so we don't end up
		 * waiting on the network for a message that we already
		 * received!
		 */
		if ( rc == -2 &&
		    check_response_queue( ld, msgid, all, 0, result ) != 0 ) {
			LDAP_SET_LDERRNO( ld, LDAP_SUCCESS, NULL, NULL );
			rc = (*result)->lm_msgtype;
		}

		/*
		 * honor the timeout if specified
		 */
		if ( rc == -2 && tvp != NULL ) {
			tmp_time = (long)time( NULL );
			if (( tv.tv_sec -=  ( tmp_time - start_time )) <= 0 ) {
				rc = 0;	/* timed out */
				LDAP_SET_LDERRNO( ld, LDAP_TIMEOUT, NULL,
				    NULL );
				break;
			}

			LDAPDebug( LDAP_DEBUG_TRACE, "wait4msg:  %ld secs to go\n",
				tv.tv_sec, 0, 0 );
			start_time = tmp_time;
		}
	}

	return( rc );
}


static int
read1msg( LDAP *ld, int msgid, int all, Sockbuf *sb, LDAPConn *lc,
    LDAPMessage **result )
{
	BerElement	*ber;
	LDAPMessage	*new, *l, *prev, *chainprev, *tmp;
	long		id;
	unsigned long	tag, len;
	int		terrno, lderr, foundit = 0;
	LDAPRequest	*lr;
	int		rc, simple_request, has_parent, message_can_be_returned;
	int		manufactured_result = 0;

	LDAPDebug( LDAP_DEBUG_TRACE, "read1msg\n", 0, 0, 0 );

	message_can_be_returned = 1;	/* the usual case... */

	/*
	 * if we are not already in the midst of reading a message, allocate
	 * a ber that is associated with this connection
	 */
	if ( lc->lconn_ber == NULLBER && nsldapi_alloc_ber_with_options( ld,
	    &lc->lconn_ber ) != LDAP_SUCCESS ) {
		return( -1 );
	}

	/*
	 * ber_get_next() doesn't set errno on EOF, so we pre-set it to
	 * zero to avoid getting tricked by leftover "EAGAIN" errors
	 */
	LDAP_SET_ERRNO( ld, 0 );

	/* get the next message */
	if ( (tag = ber_get_next( sb, &len, lc->lconn_ber ))
	    != LDAP_TAG_MESSAGE ) {
		terrno = LDAP_GET_ERRNO( ld );
		if ( terrno == EWOULDBLOCK || terrno == EAGAIN ) {
		    return( -2 );	/* try again */
		}
		LDAP_SET_LDERRNO( ld, (tag == LBER_DEFAULT ? LDAP_SERVER_DOWN :
                    LDAP_LOCAL_ERROR), NULL, NULL );
		if ( tag == LBER_DEFAULT ) {
			nsldapi_connection_lost_nolock( ld, sb );
		}
		return( -1 );
	}

	/*
	 * Since we have received a complete message now, we pull this ber
	 * out of the connection structure and never read into it again.
	 */
	ber = lc->lconn_ber;
	lc->lconn_ber = NULLBER;

	/* message id */
	if ( ber_get_int( ber, &id ) == LBER_ERROR ) {
		LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
		return( -1 );
	}

	/* if it's been abandoned, toss it */
	if ( ldap_abandoned( ld, (int)id ) ) {
		ber_free( ber, 1 );
		return( -2 );	/* continue looking */
	}

	if (( lr = nsldapi_find_request_by_msgid( ld, id )) == NULL ) {
		LDAPDebug( LDAP_DEBUG_ANY,
		    "no request for response with msgid %ld (tossing)\n",
		    id, 0, 0 );
		ber_free( ber, 1 );
		return( -2 );	/* continue looking */
	}

	/* the message type */
	if ( (tag = ber_peek_tag( ber, &len )) == LBER_ERROR ) {
		LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
		return( -1 );
	}
	LDAPDebug( LDAP_DEBUG_TRACE, "got %s msgid %ld, original id %d\n",
	    ( tag == LDAP_RES_SEARCH_ENTRY ) ? "ENTRY" :
	    ( tag == LDAP_RES_SEARCH_REFERENCE ) ? "REFERENCE" : "RESULT", id,
	    lr->lr_origid );

	id = lr->lr_origid;
	simple_request = 0;
	rc = -2;	/* default is to keep looking (no response found) */
	lr->lr_res_msgtype = tag;

	if ( tag == LDAP_RES_SEARCH_REFERENCE ||
	    tag != LDAP_RES_SEARCH_ENTRY ) {
		int		refchasing, reftotal;

		check_for_refs( ld, lr, ber, lc->lconn_version, &reftotal,
		    &refchasing );

		if ( refchasing > 0 ) {
			/*
			 * we're chasing one or more new refs...
			 */
			ber_free( ber, 1 );
			ber = NULLBER;
			lr->lr_status = LDAP_REQST_CHASINGREFS;
			message_can_be_returned = 0;

		} else if ( tag != LDAP_RES_SEARCH_REFERENCE ) {
			/*
			 * this request is complete...
			 */
			has_parent = ( lr->lr_parent != NULL );

			if ( lr->lr_outrefcnt <= 0 && !has_parent ) {
				/* request without any refs */
				simple_request = ( reftotal == 0 );
			}

			/*
			 * if this was a successful bind request and not a
			 * child request, set the connection's bind DN to
			 * match this request's.
			 */
			if ( !has_parent &&
			    LDAP_RES_BIND == lr->lr_res_msgtype &&
			    lr->lr_conn != NULL && LDAP_SUCCESS ==
			    nsldapi_parse_result( ld, lr->lr_res_msgtype, ber,
			    &lderr, NULL, NULL, NULL, NULL ) &&
			    LDAP_SUCCESS == lderr ) {
				if ( lr->lr_conn->lconn_binddn != NULL ) {
					NSLDAPI_FREE(
					    lr->lr_conn->lconn_binddn );
				}
				lr->lr_conn->lconn_binddn = lr->lr_binddn;
				lr->lr_conn->lconn_bound = 1;
				lr->lr_binddn = NULL;
			}

			/*
			 * if this response is to a child request, we toss
			 * the message contents and just merge error info.
			 * into the parent.
			 */
			if ( has_parent ) {
				ber_free( ber, 1 );
				ber = NULLBER;
			}
			while ( lr->lr_parent != NULL ) {
				merge_error_info( ld, lr->lr_parent, lr );

				lr = lr->lr_parent;
				if ( --lr->lr_outrefcnt > 0 ) {
					break;	/* not completely done yet */
				}
			}

			/*
			 * we recognize a request as complete when:
			 *  1) it has no outstanding referrals
			 *  2) it is not a child request
			 *  3) we have received a result for the request (i.e.,
			 *     something other than an entry or a reference).
			 */
			if ( lr->lr_outrefcnt <= 0 && lr->lr_parent == NULL &&
			    lr->lr_res_msgtype != LDAP_RES_SEARCH_ENTRY &&
			    lr->lr_res_msgtype != LDAP_RES_SEARCH_REFERENCE ) {
				id = lr->lr_msgid;
				tag = lr->lr_res_msgtype;
				LDAPDebug( LDAP_DEBUG_TRACE,
				    "request %ld done\n", id, 0, 0 );
LDAPDebug( LDAP_DEBUG_TRACE,
"res_errno: %d, res_error: <%s>, res_matched: <%s>\n",
lr->lr_res_errno, lr->lr_res_error ? lr->lr_res_error : "",
lr->lr_res_matched ? lr->lr_res_matched : "" );
				if ( !simple_request ) {
					if ( ber != NULLBER ) {
						ber_free( ber, 1 );
						ber = NULLBER;
					}
					if ( build_result_ber( ld, &ber, lr )
					    != LDAP_SUCCESS ) {
						rc = -1; /* fatal error */
					} else {
						manufactured_result = 1;
					}
				}

				nsldapi_free_request( ld, lr, 1 );
			} else {
				message_can_be_returned = 0;
			}
		}
	}

	if ( ber == NULLBER ) {
		return( rc );
	}

	/* make a new ldap message */
	if ( (new = (LDAPMessage*)NSLDAPI_CALLOC( 1, sizeof(struct ldapmsg) ))
	    == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( -1 );
	}
	new->lm_msgid = (int)id;
	new->lm_msgtype = tag;
	new->lm_ber = ber;

	/*
	 * if this is a search entry or if this request is complete (i.e.,
	 * there are no outstanding referrals) then add to cache and check
	 * to see if we should return this to the caller right away or not.
	 */
	if ( message_can_be_returned ) {
		if ( ld->ld_cache_on ) {
			nsldapi_add_result_to_cache( ld, new );
		}

		if ( msgid == LDAP_RES_ANY || id == msgid ) {
			if ( new->lm_msgtype == LDAP_RES_SEARCH_RESULT ) {
				/*
				 * return the first response we have for this
				 * search request later (possibly an entire
				 * chain of messages).
				 */
				foundit = 1;
			} else if ( all == 0
			    || (new->lm_msgtype != LDAP_RES_SEARCH_REFERENCE
			    && new->lm_msgtype != LDAP_RES_SEARCH_ENTRY) ) {
				*result = new;
				LDAP_SET_LDERRNO( ld, LDAP_SUCCESS, NULL,
				    NULL );
				return( tag );
			}
		}
	}

	/* 
	 * if not, we must add it to the list of responses.  if
	 * the msgid is already there, it must be part of an existing
	 * search response.
	 */

	prev = NULL;
	LDAP_MUTEX_LOCK( ld, LDAP_RESP_LOCK );
	for ( l = ld->ld_responses; l != NULL; l = l->lm_next ) {
		if ( l->lm_msgid == new->lm_msgid )
			break;
		prev = l;
	}

	/* not part of an existing search response */
	if ( l == NULL ) {
		if ( foundit ) {
			LDAP_MUTEX_UNLOCK( ld, LDAP_RESP_LOCK );
			*result = new;
			LDAP_SET_LDERRNO( ld, LDAP_SUCCESS, NULL, NULL );
			return( tag );
		}

		new->lm_next = ld->ld_responses;
		ld->ld_responses = new;
		LDAPDebug( LDAP_DEBUG_TRACE,
		    "adding new response id %d type %d (looking for id %d)\n",
		    new->lm_msgid, new->lm_msgtype, msgid );
		LDAP_MUTEX_UNLOCK( ld, LDAP_RESP_LOCK );
		if( message_can_be_returned )
			POST( ld, new->lm_msgid, new );
		return( -2 );	/* continue looking */
	}

	LDAPDebug( LDAP_DEBUG_TRACE,
	    "adding response id %d type %d (looking for id %d)\n",
	    new->lm_msgid, new->lm_msgtype, msgid );

	/*
	 * part of a search response - add to end of list of entries
	 *
	 * the first step is to find the end of the list of entries and
	 * references.  after the following loop is executed, tmp points to
	 * the last entry or reference in the chain.  If there are none,
	 * tmp points to the search result.
	 */
	chainprev = NULL;
	for ( tmp = l; tmp->lm_chain != NULL &&
	    ( tmp->lm_chain->lm_msgtype == LDAP_RES_SEARCH_ENTRY
	    || tmp->lm_chain->lm_msgtype == LDAP_RES_SEARCH_REFERENCE );
	    tmp = tmp->lm_chain ) {
		chainprev = tmp;
	}

	/*
	 * If this is a manufactured result message and a result is already
	 * queued we throw away the one that is queued and replace it with
	 * our new result.  This is necessary so we don't end up returning
	 * more than one result.
	 */
	if ( manufactured_result &&
	    tmp->lm_msgtype == LDAP_RES_SEARCH_RESULT ) {
		/*
		 * the result is the only thing in the chain... replace it.
		 */
		new->lm_chain = tmp->lm_chain;
		new->lm_next = tmp->lm_next;
		if ( chainprev == NULL ) {
			if ( prev == NULL ) {
				ld->ld_responses = new;
			} else {
				prev->lm_next = new;
			}
		} else {
		    chainprev->lm_chain = new;
		}
		if ( l == tmp ) {
			l = new;
		}
		ldap_msgfree( tmp );

	} else if ( manufactured_result && tmp->lm_chain != NULL
	    && tmp->lm_chain->lm_msgtype == LDAP_RES_SEARCH_RESULT ) {
		/*
		 * entries or references are also present, so the result
		 * is the next entry after tmp.  replace it.
		 */
		new->lm_chain = tmp->lm_chain->lm_chain;
		new->lm_next = tmp->lm_chain->lm_next;
		ldap_msgfree( tmp->lm_chain );
		tmp->lm_chain = new;

	} else if ( tmp->lm_msgtype == LDAP_RES_SEARCH_RESULT ) {
		/*
		 * the result is the only thing in the chain... add before it.
		 */
		new->lm_chain = tmp;
		if ( chainprev == NULL ) {
			if ( prev == NULL ) {
				ld->ld_responses = new;
			} else {
				prev->lm_next = new;
			}
		} else {
		    chainprev->lm_chain = new;
		}
		if ( l == tmp ) {
			l = new;
		}

	} else {
		/*
		 * entries and/or references are present... add to the end
		 * of the entry/reference part of the chain.
		 */
		new->lm_chain = tmp->lm_chain;
		tmp->lm_chain = new;
	}

	/*
	 * return the first response or the whole chain if that's what
	 * we were looking for....
	 */
	if ( foundit ) {
		if ( all == 0 && l->lm_chain != NULL ) {
			/*
			 * only return the first response in the chain
			 */
			if ( prev == NULL ) {
				ld->ld_responses = l->lm_chain;
			} else {
				prev->lm_next = l->lm_chain;
			}
			l->lm_chain = NULL;
			tag = l->lm_msgtype;
		} else {
			/*
			 * return all of the responses (may be a chain)
			 */
			if ( prev == NULL ) {
				ld->ld_responses = l->lm_next;
			} else {
				prev->lm_next = l->lm_next;
			}
		}
		*result = l;
		LDAP_MUTEX_UNLOCK( ld, LDAP_RESP_LOCK );
		LDAP_SET_LDERRNO( ld, LDAP_SUCCESS, NULL, NULL );
		return( tag );
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_RESP_LOCK );
	return( -2 );	/* continue looking */
}


/*
 * check for LDAPv2+ (UMich extension) or LDAPv3 referrals or references
 * errors are merged in "lr".
 */
static void
check_for_refs( LDAP *ld, LDAPRequest *lr, BerElement *ber,
    int ldapversion, int *totalcountp, int *chasingcountp )
{
	int		err, origerr;
	char		*errstr, *matcheddn, **v3refs;

	LDAPDebug( LDAP_DEBUG_TRACE, "check_for_refs\n", 0, 0, 0 );

	*chasingcountp = *totalcountp = 0;

	if ( ldapversion < LDAP_VERSION2 || ( lr->lr_parent == NULL
	    && ( ld->ld_options & LDAP_BITOPT_REFERRALS ) == 0 )) {
		/* referrals are not supported or are disabled */
		return;
	}

	if ( lr->lr_res_msgtype == LDAP_RES_SEARCH_REFERENCE ) {
		err = nsldapi_parse_reference( ld, ber, &v3refs, NULL );
		origerr = LDAP_REFERRAL;	/* a small lie... */
		matcheddn = errstr = NULL;
	} else {
		err = nsldapi_parse_result( ld, lr->lr_res_msgtype, ber,
		    &origerr, &matcheddn, &errstr, &v3refs, NULL );
	}

	if ( err != LDAP_SUCCESS ) {
		/* parse failed */
		return;
	}

	if ( origerr == LDAP_REFERRAL ) {	/* ldapv3 */
		if ( v3refs != NULL ) {
			err = nsldapi_chase_v3_refs( ld, lr, v3refs,
			    ( lr->lr_res_msgtype == LDAP_RES_SEARCH_REFERENCE ),
			    totalcountp, chasingcountp );
			ldap_value_free( v3refs );
		}
	} else if ( ldapversion == LDAP_VERSION2
	    && origerr != LDAP_SUCCESS ) {
		/* referrals may be present in the error string */
		err = nsldapi_chase_v2_referrals( ld, lr, &errstr,
		    totalcountp, chasingcountp );
	}

	/* set LDAP errno, message, and matched string appropriately */
	if ( lr->lr_res_error != NULL ) {
		NSLDAPI_FREE( lr->lr_res_error );
	}
	lr->lr_res_error = errstr;

	if ( lr->lr_res_matched != NULL ) {
		NSLDAPI_FREE( lr->lr_res_matched );
	}
	lr->lr_res_matched = matcheddn;

	if ( err == LDAP_SUCCESS && ( *chasingcountp == *totalcountp )) {
		if ( *totalcountp > 0 && ( origerr == LDAP_PARTIAL_RESULTS
		    || origerr == LDAP_REFERRAL )) {
			/* substitute success for referral error codes */
			lr->lr_res_errno = LDAP_SUCCESS;
		} else {
			/* preserve existing non-referral error code */
			lr->lr_res_errno = origerr;
		}
	} else if ( err != LDAP_SUCCESS ) {
		/* error occurred while trying to chase referrals */
		lr->lr_res_errno = err;
	} else {
		/* some referrals were not recognized */
		lr->lr_res_errno = ( ldapversion == LDAP_VERSION2 )
		    ? LDAP_PARTIAL_RESULTS : LDAP_REFERRAL;
	}
		
	LDAPDebug( LDAP_DEBUG_TRACE,
	    "check_for_refs: new result: msgid %d, res_errno %d, ",
	    lr->lr_msgid, lr->lr_res_errno, 0 );
	LDAPDebug( LDAP_DEBUG_TRACE, " res_error <%s>, res_matched <%s>\n",
	    lr->lr_res_error ? lr->lr_res_error : "",
	    lr->lr_res_matched ? lr->lr_res_matched : "", 0 );
	LDAPDebug( LDAP_DEBUG_TRACE,
	    "check_for_refs: %d new refs(s); chasing %d of them\n",
	    *totalcountp, *chasingcountp, 0 );
}


/* returns an LDAP error code and also sets it in LDAP * */
static int
build_result_ber( LDAP *ld, BerElement **berp, LDAPRequest *lr )
{
	unsigned long	len;
	long		along;
	BerElement	*ber;
	int		err;

	if (( err = nsldapi_alloc_ber_with_options( ld, &ber ))
	    != LDAP_SUCCESS ) {
		return( err );
	}
	*berp = ber;
	if ( ber_printf( ber, "{it{ess}}", lr->lr_msgid,
	    (long)lr->lr_res_msgtype, lr->lr_res_errno,
	    lr->lr_res_matched ? lr->lr_res_matched : "",
	    lr->lr_res_error ? lr->lr_res_error : "" ) == -1 ) {
		return( LDAP_ENCODING_ERROR );
	}

	ber_reset( ber, 1 );
	if ( ber_skip_tag( ber, &len ) == LBER_ERROR ||
	    ber_get_int( ber, &along ) == LBER_ERROR ||
	    ber_peek_tag( ber, &len ) == LBER_ERROR ) {
		return( LDAP_DECODING_ERROR );
	}

	return( LDAP_SUCCESS );
}


static void
merge_error_info( LDAP *ld, LDAPRequest *parentr, LDAPRequest *lr )
{
/*
 * Merge error information in "lr" with "parentr" error code and string.
 */
	if ( lr->lr_res_errno == LDAP_PARTIAL_RESULTS ) {
		parentr->lr_res_errno = lr->lr_res_errno;
		if ( lr->lr_res_error != NULL ) {
			(void)nsldapi_append_referral( ld, &parentr->lr_res_error,
			    lr->lr_res_error );
		}
	} else if ( lr->lr_res_errno != LDAP_SUCCESS &&
	    parentr->lr_res_errno == LDAP_SUCCESS ) {
		parentr->lr_res_errno = lr->lr_res_errno;
		if ( parentr->lr_res_error != NULL ) {
			NSLDAPI_FREE( parentr->lr_res_error );
		}
		parentr->lr_res_error = lr->lr_res_error;
		lr->lr_res_error = NULL;
		if ( NAME_ERROR( lr->lr_res_errno )) {
			if ( parentr->lr_res_matched != NULL ) {
				NSLDAPI_FREE( parentr->lr_res_matched );
			}
			parentr->lr_res_matched = lr->lr_res_matched;
			lr->lr_res_matched = NULL;
		}
	}

	LDAPDebug( LDAP_DEBUG_TRACE, "merged parent (id %d) error info:  ",
	    parentr->lr_msgid, 0, 0 );
	LDAPDebug( LDAP_DEBUG_TRACE, "result lderrno %d, error <%s>, matched <%s>\n",
	    parentr->lr_res_errno, parentr->lr_res_error ?
	    parentr->lr_res_error : "", parentr->lr_res_matched ?
	    parentr->lr_res_matched : "" );
}

#if defined( CLDAP )
#if !defined( macintosh ) && !defined( DOS ) && !defined( _WINDOWS ) && !defined(XP_OS2)
static int
cldap_select1( LDAP *ld, struct timeval *timeout )
{
	fd_set		readfds;
	static int	tblsize = 0;

	if ( tblsize == 0 ) {
#ifdef USE_SYSCONF
		tblsize = sysconf( _SC_OPEN_MAX );
#else /* USE_SYSCONF */
		tblsize = getdtablesize();
#endif /* USE_SYSCONF */
	}

	if ( tblsize >= FD_SETSIZE ) {
		/*
		 * clamp value so we don't overrun the fd_set structure
		 */
		tblsize = FD_SETSIZE - 1;
	}

	FD_ZERO( &readfds );
	FD_SET( ld->ld_sbp->sb_sd, &readfds );

	if ( ld->ld_select_fn != NULL ) {
		return( ld->ld_select_fn( tblsize, &readfds, 0, 0, timeout ) );
	} else {
		/* XXXmcs: UNIX platforms should use poll() */
		return( select( tblsize, &readfds, 0, 0, timeout ) );
	}
}
#endif /* !macintosh */


#ifdef macintosh
static int
cldap_select1( LDAP *ld, struct timeval *timeout )
{
	return( tcpselect( ld->ld_sbp->sb_sd, timeout ));
}
#endif /* macintosh */


#if (defined( DOS ) && defined( WINSOCK )) || defined( _WINDOWS ) || defined(XP_OS2)
static int
cldap_select1( LDAP *ld, struct timeval *timeout )
{
    fd_set          readfds;
    int             rc;

    FD_ZERO( &readfds );
    FD_SET( ld->ld_sbp->sb_sd, &readfds );

    if ( ld->ld_select_fn != NULL ) {
		rc = ld->ld_select_fn( 1, &readfds, 0, 0, timeout );
	} else {
		rc = select( 1, &readfds, 0, 0, timeout );
	}
    return( rc == SOCKET_ERROR ? -1 : rc );
}
#endif /* WINSOCK || _WINDOWS */
#endif /* CLDAP */

int
LDAP_CALL
ldap_msgfree( LDAPMessage *lm )
{
	LDAPMessage	*next;
	int		type = 0;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_msgfree\n", 0, 0, 0 );

	for ( ; lm != NULL; lm = next ) {
		next = lm->lm_chain;
		type = lm->lm_msgtype;
		ber_free( lm->lm_ber, 1 );
		NSLDAPI_FREE( (char *) lm );
	}

	return( type );
}

/*
 * ldap_msgdelete - delete a message.  It returns:
 *	0	if the entire message was deleted
 *	-1	if the message was not found, or only part of it was found
 */
int
ldap_msgdelete( LDAP *ld, int msgid )
{
	LDAPMessage	*lm, *prev;
	int		msgtype;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_msgdelete\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( -1 );	/* punt */
	}

	prev = NULL;
        LDAP_MUTEX_LOCK( ld, LDAP_RESP_LOCK );
	for ( lm = ld->ld_responses; lm != NULL; lm = lm->lm_next ) {
		if ( lm->lm_msgid == msgid )
			break;
		prev = lm;
	}

	if ( lm == NULL )
	{
        	LDAP_MUTEX_UNLOCK( ld, LDAP_RESP_LOCK );
		return( -1 );
	}

	if ( prev == NULL )
		ld->ld_responses = lm->lm_next;
	else
		prev->lm_next = lm->lm_next;
        LDAP_MUTEX_UNLOCK( ld, LDAP_RESP_LOCK );

	msgtype = ldap_msgfree( lm );
	if ( msgtype == LDAP_RES_SEARCH_ENTRY
	    || msgtype == LDAP_RES_SEARCH_REFERENCE ) {
		return( -1 );
	}

	return( 0 );
}


/*
 * return 1 if message msgid is waiting to be abandoned, 0 otherwise
 */
static int
ldap_abandoned( LDAP *ld, int msgid )
{
	int	i;

	LDAP_MUTEX_LOCK( ld, LDAP_ABANDON_LOCK );
	if ( ld->ld_abandoned == NULL )
	{
		LDAP_MUTEX_UNLOCK( ld, LDAP_ABANDON_LOCK );
		return( 0 );
	}

	for ( i = 0; ld->ld_abandoned[i] != -1; i++ )
		if ( ld->ld_abandoned[i] == msgid )
		{
			LDAP_MUTEX_UNLOCK( ld, LDAP_ABANDON_LOCK );
			return( 1 );
		}

	LDAP_MUTEX_UNLOCK( ld, LDAP_ABANDON_LOCK );
	return( 0 );
}


static int
ldap_mark_abandoned( LDAP *ld, int msgid )
{
	int	i;

	LDAP_MUTEX_LOCK( ld, LDAP_ABANDON_LOCK );
	if ( ld->ld_abandoned == NULL )
	{
		LDAP_MUTEX_UNLOCK( ld, LDAP_ABANDON_LOCK );
		return( -1 );
	}

	for ( i = 0; ld->ld_abandoned[i] != -1; i++ )
		if ( ld->ld_abandoned[i] == msgid )
			break;

	if ( ld->ld_abandoned[i] == -1 )
	{
		LDAP_MUTEX_UNLOCK( ld, LDAP_ABANDON_LOCK );
		return( -1 );
	}

	for ( ; ld->ld_abandoned[i] != -1; i++ ) {
		ld->ld_abandoned[i] = ld->ld_abandoned[i + 1];
	}

	LDAP_MUTEX_UNLOCK( ld, LDAP_ABANDON_LOCK );
	return( 0 );
}


#ifdef CLDAP
int
cldap_getmsg( LDAP *ld, struct timeval *timeout, BerElement **ber )
{
	int		rc;
	unsigned long	tag, len;

	if ( ld->ld_sbp->sb_ber.ber_ptr >= ld->ld_sbp->sb_ber.ber_end ) {
		rc = cldap_select1( ld, timeout );
		if ( rc == -1 || rc == 0 ) {
			LDAP_SET_LDERRNO( ld, (rc == -1 ? LDAP_SERVER_DOWN :
			    LDAP_TIMEOUT), NULL, NULL );
			return( rc );
		}
	}

	/* get the next message */
	if ( (tag = ber_get_next( ld->ld_sbp, &len, ber ))
	    != LDAP_TAG_MESSAGE ) {
		LDAP_SET_LDERRNO( ld, (tag == LBER_DEFAULT ? LDAP_SERVER_DOWN :
		    LDAP_LOCAL_ERROR), NULL, NULL );
		return( -1 );
	}

	return( tag );
}
#endif /* CLDAP */

int
nsldapi_wait_result( LDAP *ld, int msgid, int all, struct timeval *timeout,
    LDAPMessage **result )
{
	LDAPPend	*lp;

	LDAP_MUTEX_LOCK( ld, LDAP_PEND_LOCK );
	for( lp = ld->ld_pend; lp != NULL; lp = lp->lp_next )
	{
		if( lp->lp_msgid == msgid )
			break;
	}
	if( lp == NULL )
	{
		lp = (LDAPPend *) NSLDAPI_CALLOC( 1, sizeof( LDAPPend ) );
		if( lp == NULL )
		{
			LDAP_MUTEX_UNLOCK( ld, LDAP_PEND_LOCK );
			LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
			*result = NULL;
			return (-1);
		}

		lp->lp_sema = (void *) LDAP_SEMA_ALLOC( ld );
		if( lp->lp_sema == NULL )
		{
			NSLDAPI_FREE( lp );
			LDAP_MUTEX_UNLOCK( ld, LDAP_PEND_LOCK );
			LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
			*result = NULL;
			return (-1);
		}

		lp->lp_msgid = msgid;
		lp->lp_result = NULL;
		link_pend( ld, lp );
	}
	else
	{
		int rc;

		if( (rc = unlink_msg( ld, lp->lp_msgid, all )) == -2  )
		{
			*result = NULL;
		}
		else
		{
			*result = lp->lp_result;
		}
		unlink_pend( ld, lp );
		LDAP_MUTEX_UNLOCK( ld, LDAP_PEND_LOCK );
		NSLDAPI_FREE( lp );
		LDAP_SET_LDERRNO( ld, LDAP_SUCCESS, NULL, NULL );
		return ( rc );
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_PEND_LOCK );

	LDAP_SEMA_WAIT( ld, lp );
	LDAP_MUTEX_LOCK( ld, LDAP_PEND_LOCK );
	*result = lp->lp_result;
	{
		int rc;

		if( *result == NULL )
			rc = -2;
		else
		{
			if( (rc = unlink_msg( ld, lp->lp_msgid, all )) == -2 )
			{
				*result = NULL;
			}
			else
			{
				if ( ld->ld_memcache != NULL &&
					NSLDAPI_SEARCH_RELATED_RESULT( rc ) &&
					!((*result)->lm_fromcache ))
				{
				  ldap_memcache_append( ld, (*result)->lm_msgid,
		    			(all || NSLDAPI_IS_SEARCH_RESULT( rc )),
						*result );
				}
			}

		}
		unlink_pend( ld, lp );
		LDAP_MUTEX_UNLOCK( ld, LDAP_PEND_LOCK );
		LDAP_SEMA_FREE( ld, lp );
		NSLDAPI_FREE( lp );
		return ( rc );
	}
}

int
nsldapi_post_result( LDAP *ld, int msgid, LDAPMessage *result )
{
	LDAPPend	*lp;

	LDAP_MUTEX_LOCK( ld, LDAP_PEND_LOCK );
	if( msgid == LDAP_RES_ANY )
		lp = ld->ld_pend;
	else
	{
		for( lp = ld->ld_pend; lp != NULL; lp = lp->lp_next )
		{
			if( lp->lp_msgid == msgid )
				break;
		}
	}

	if( lp == NULL && msgid != LDAP_RES_ANY )
	{
		lp = (LDAPPend *) NSLDAPI_CALLOC( 1, sizeof( LDAPPend ) );
		if( lp == NULL )
		{
			LDAP_MUTEX_UNLOCK( ld, LDAP_PEND_LOCK );
			LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
			return (-1);
		}
		lp->lp_msgid = msgid;
		lp->lp_result = result;
		link_pend( ld, lp );
	}
	else if( lp != NULL )
	{
		lp->lp_msgid = msgid;
		lp->lp_result = result;
		LDAP_SEMA_POST( ld, lp );
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_PEND_LOCK );
	return (0);
}

static void
link_pend( LDAP *ld, LDAPPend *lp )
{
	if (( lp->lp_next = ld->ld_pend ) != NULL )
	{
		lp->lp_next->lp_prev = lp;    
	} 
	ld->ld_pend = lp; 
	lp->lp_prev = NULL; 
}

static void
unlink_pend( LDAP *ld, LDAPPend *lp )
{
        if ( lp->lp_prev == NULL ) {
                ld->ld_pend = lp->lp_next;
        } else { 
                lp->lp_prev->lp_next = lp->lp_next;
        }
 
        if ( lp->lp_next != NULL ) {
                lp->lp_next->lp_prev = lp->lp_prev;
        }
}

static int
unlink_msg( LDAP *ld, int msgid, int all )
{
	int rc;
	LDAPMessage	*lm, *lastlm, *nextlm;

	lastlm = NULL;
	LDAP_MUTEX_LOCK( ld, LDAP_RESP_LOCK );
	for ( lm = ld->ld_responses; lm != NULL; lm = nextlm )
	{
		nextlm = lm->lm_next;

		if ( lm->lm_msgid == msgid )
		{
			LDAPMessage	*tmp;

			if ( all == 0
			    || (lm->lm_msgtype != LDAP_RES_SEARCH_RESULT
			    && lm->lm_msgtype != LDAP_RES_SEARCH_REFERENCE
			    && lm->lm_msgtype != LDAP_RES_SEARCH_ENTRY) )
				break;

			for ( tmp = lm; tmp != NULL; tmp = tmp->lm_chain ) {
				if ( tmp->lm_msgtype == LDAP_RES_SEARCH_RESULT )
					break;
			}
			if( tmp != NULL )
				break;
		}
		lastlm = lm;
	}

	if( lm != NULL )
	{

		if ( all == 0 )
		{
			if ( lm->lm_chain == NULL )
			{
				if ( lastlm == NULL )
					ld->ld_responses = lm->lm_next;
				else
					lastlm->lm_next = lm->lm_next;
			}
			else
			{
				if ( lastlm == NULL )
				{
					ld->ld_responses = lm->lm_chain;
					ld->ld_responses->lm_next = lm->lm_next;
				}
				else
				{
					lastlm->lm_next = lm->lm_chain;
					lastlm->lm_next->lm_next = lm->lm_next;
				}
			}
		}
		else
		{
			if ( lastlm == NULL )
				ld->ld_responses = lm->lm_next;
			else
				lastlm->lm_next = lm->lm_next;
		}

		if ( all == 0 )
			lm->lm_chain = NULL;
		lm->lm_next = NULL;
		rc = lm->lm_msgtype;
	}
	else
	{
		rc = -2;
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_RESP_LOCK );
	return ( rc );
}

static int nsldapi_mutex_trylock( LDAP *ld, LDAPLock i )
{
 
        if( (ld)->ld_mutex_trylock_fn == NULL ) {
                return (0) ;
        }
        else {
                int ret;

                if( (ld)->ld_threadid_fn != NULL ) {
                        (ld)->ld_mutex_lock_fn ( (ld)->ld_mutex[LDAP_THREADID_LOCK] );
                        if( (ld)->ld_mutex_threadid[i] == (ld)->ld_threadid_fn() ) {
                                (ld)->ld_mutex_refcnt[i]++ ;
                        	(ld)->ld_mutex_unlock_fn ( (ld)->ld_mutex[LDAP_THREADID_LOCK] );
                        }
                        else if( (ld)->ld_mutex_threadid[i] == (void *) -1 ) {
                                ret = (ld)->ld_mutex_trylock_fn( (ld)->ld_mutex[i] );
				if( ret == 0 ) {
                                	(ld)->ld_mutex_threadid[i] =
						(ld)->ld_threadid_fn() ;
                                	(ld)->ld_mutex_refcnt[i]++ ;
                        		(ld)->ld_mutex_unlock_fn (
					   (ld)->ld_mutex[LDAP_THREADID_LOCK] );
				}
                        }
			else {
				ret = -1;
                        	(ld)->ld_mutex_unlock_fn ( (ld)->ld_mutex[LDAP_THREADID_LOCK] );
			}
                }
                else {
                        ret = (ld)->ld_mutex_trylock_fn( (ld)->ld_mutex[i] ) ;
                }
                return (ret);
        }
}
