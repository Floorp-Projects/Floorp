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
 *  Copyright (c) 1995 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  request.c - sending of ldap requests; handling of referrals
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1995 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"

static LDAPConn *find_connection( LDAP *ld, LDAPServer *srv, int any );
static void use_connection( LDAP *ld, LDAPConn *lc );
static void free_servers( LDAPServer *srvlist );
static int chase_one_referral( LDAP *ld, LDAPRequest *lr, LDAPRequest *origreq,
    char *refurl, char *desc, int *unknownp );
static int re_encode_request( LDAP *ld, BerElement *origber,
    int msgid, LDAPURLDesc *ludp, BerElement **berp );

#ifdef LDAP_DNS
static LDAPServer *dn2servers( LDAP *ld, char *dn );
#endif /* LDAP_DNS */


/* returns an LDAP error code and also sets error inside LDAP * */
int
nsldapi_alloc_ber_with_options( LDAP *ld, BerElement **berp )
{
	int	err;

	LDAP_MUTEX_LOCK( ld, LDAP_OPTION_LOCK );
    	if (( *berp = ber_alloc_t( ld->ld_lberoptions )) == NULLBER ) {
		err = LDAP_NO_MEMORY;
		LDAP_SET_LDERRNO( ld, err, NULL, NULL );
	} else {
		err = LDAP_SUCCESS;
#ifdef STR_TRANSLATION
		nsldapi_set_ber_options( ld, *berp );
#endif /* STR_TRANSLATION */
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_OPTION_LOCK );

	return( err );
}


void
nsldapi_set_ber_options( LDAP *ld, BerElement *ber )
{
	ber->ber_options = ld->ld_lberoptions;
#ifdef STR_TRANSLATION
	if (( ld->ld_lberoptions & LBER_OPT_TRANSLATE_STRINGS ) != 0 ) {
		ber_set_string_translators( ber,
		    ld->ld_lber_encode_translate_proc,
		    ld->ld_lber_decode_translate_proc );
	}
#endif /* STR_TRANSLATION */
}


/* returns the message id of the request or -1 if an error occurs */
int
nsldapi_send_initial_request( LDAP *ld, int msgid, unsigned long msgtype,
	char *dn, BerElement *ber )
{
	LDAPServer	*servers;
	
	LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_send_initial_request\n", 0,0,0 );

#ifdef LDAP_DNS
	LDAP_MUTEX_LOCK( ld, LDAP_OPTION_LOCK );
	if (( ld->ld_options & LDAP_BITOPT_DNS ) != 0 && ldap_is_dns_dn( dn )) {
		if (( servers = dn2servers( ld, dn )) == NULL ) {
			ber_free( ber, 1 );
			LDAP_MUTEX_UNLOCK( ld, LDAP_OPTION_LOCK );
			return( -1 );
		}

#ifdef LDAP_DEBUG
		if ( ldap_debug & LDAP_DEBUG_TRACE ) {
			LDAPServer	*srv;
			char    msg[256];

			for ( srv = servers; srv != NULL;
			    srv = srv->lsrv_next ) {
				sprintf( msg,
				    "LDAP server %s:  dn %s, port %d\n",
				    srv->lsrv_host, ( srv->lsrv_dn == NULL ) ?
				    "(default)" : srv->lsrv_dn,
				    srv->lsrv_port );
				ber_err_print( msg );
			}
		}
#endif /* LDAP_DEBUG */
	} else {
#endif /* LDAP_DNS */
		/*
		 * use of DNS is turned off or this is an LDAP DN...
		 * use our default connection
		 */
		servers = NULL;
#ifdef LDAP_DNS
	}	
	LDAP_MUTEX_UNLOCK( ld, LDAP_OPTION_LOCK );
#endif /* LDAP_DNS */

	return( nsldapi_send_server_request( ld, ber, msgid, NULL,
	    servers, NULL, ( msgtype == LDAP_REQ_BIND ) ? dn : NULL, 0 ));
}


/* returns the message id of the request or -1 if an error occurs */
int
nsldapi_send_server_request(
    LDAP *ld,			/* session handle */
    BerElement *ber,		/* message to send */
    int msgid,			/* ID of message to send */
    LDAPRequest *parentreq,	/* non-NULL for referred requests */
    LDAPServer *srvlist,	/* servers to connect to (NULL for default) */
    LDAPConn *lc,		/* connection to use (NULL for default) */
    char *bindreqdn,		/* non-NULL for bind requests */
    int bind			/* perform a bind after opening new conn.? */
)
{
	LDAPRequest	*lr;
	int		err;
	int		incparent;	/* did we bump parent's ref count? */

	int				res_rc;
	int				epipe_err;
	int				ext_res_rc;
	char			*ext_oid  = NULL;
	struct berval	*ext_data = NULL;
	LDAPMessage		*ext_res  = NULL;

	LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_send_server_request\n", 0, 0, 0 );

	incparent = 0;
	LDAP_MUTEX_LOCK( ld, LDAP_CONN_LOCK );
	if ( lc == NULL ) {
		if ( srvlist == NULL ) {
			if ( ld->ld_defconn == NULL ) {
				LDAP_MUTEX_LOCK( ld, LDAP_OPTION_LOCK );
				if ( bindreqdn == NULL && ( ld->ld_options
				    & LDAP_BITOPT_RECONNECT ) != 0 ) {
					LDAP_SET_LDERRNO( ld, LDAP_SERVER_DOWN,
					    NULL, NULL );
					ber_free( ber, 1 );
					LDAP_MUTEX_UNLOCK( ld, LDAP_OPTION_LOCK );
					LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );
					return( -1 );
				}
				LDAP_MUTEX_UNLOCK( ld, LDAP_OPTION_LOCK );

				if ( nsldapi_open_ldap_defconn( ld ) < 0 ) {
					ber_free( ber, 1 );
					LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );
					return( -1 );
				}
			}
			lc = ld->ld_defconn;
		} else {
			if (( lc = find_connection( ld, srvlist, 1 )) ==
			    NULL ) {
				if ( bind && (parentreq != NULL) ) {
					/* Remember the bind in the parent */
					incparent = 1;
					++parentreq->lr_outrefcnt;
				}

				lc = nsldapi_new_connection( ld, &srvlist, 0,
					1, bind );
			}
			free_servers( srvlist );
		}
	}


    /*
     * the logic here is:
     * if 
     * 1. no connections exists, 
     * or 
     * 2. if the connection is either not in the connected 
     *     or connecting state in an async io model
     * or 
     * 3. the connection is notin a connected state with normal (non async io)
     */
	if (   lc == NULL
		|| (  (ld->ld_options & LDAP_BITOPT_ASYNC 
               && lc->lconn_status != LDAP_CONNST_CONNECTING
		    && lc->lconn_status != LDAP_CONNST_CONNECTED)
              || (!(ld->ld_options & LDAP_BITOPT_ASYNC )
		    && lc->lconn_status != LDAP_CONNST_CONNECTED) ) ) {

		ber_free( ber, 1 );
		if ( lc != NULL ) {
			LDAP_SET_LDERRNO( ld, LDAP_SERVER_DOWN, NULL, NULL );
		}
		if ( incparent ) {
			/* Forget about the bind */
			--parentreq->lr_outrefcnt; 
		}
		LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );
		return( -1 );
	}

	use_connection( ld, lc );
	if (( lr = (LDAPRequest *)NSLDAPI_CALLOC( 1, sizeof( LDAPRequest ))) ==
	    NULL || ( bindreqdn != NULL && ( bindreqdn =
	    nsldapi_strdup( bindreqdn )) == NULL )) {
		if ( lr != NULL ) {
			NSLDAPI_FREE( lr );
		}
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		nsldapi_free_connection( ld, lc, NULL, NULL, 0, 0 );
		ber_free( ber, 1 );
		if ( incparent ) {
			/* Forget about the bind */
			--parentreq->lr_outrefcnt; 
		}
		LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );
		return( -1 );
	} 
	lr->lr_binddn = bindreqdn;
	lr->lr_msgid = msgid;
	lr->lr_status = LDAP_REQST_INPROGRESS;
	lr->lr_res_errno = LDAP_SUCCESS;	/* optimistic */
	lr->lr_ber = ber;
	lr->lr_conn = lc;

	if ( parentreq != NULL ) {	/* sub-request */
		if ( !incparent ) { 
			/* Increment if we didn't do it before the bind */
			++parentreq->lr_outrefcnt;
		}
		lr->lr_origid = parentreq->lr_origid;
		lr->lr_parentcnt = parentreq->lr_parentcnt + 1;
		lr->lr_parent = parentreq;
		if ( parentreq->lr_child != NULL ) {
			lr->lr_sibling = parentreq->lr_child;
		}
		parentreq->lr_child = lr;
	} else {			/* original request */
		lr->lr_origid = lr->lr_msgid;
	}

	LDAP_MUTEX_LOCK( ld, LDAP_REQ_LOCK );
	if (( lr->lr_next = ld->ld_requests ) != NULL ) {
		lr->lr_next->lr_prev = lr;
	}
	ld->ld_requests = lr;
	lr->lr_prev = NULL;

	if (( err = nsldapi_ber_flush( ld, lc->lconn_sb, ber, 0, 1, 1 )) != 0 ) {
		epipe_err = LDAP_GET_ERRNO( ld );
		if ( epipe_err == EPIPE ) {
			res_rc = nsldapi_result_nolock(ld, LDAP_RES_UNSOLICITED, 1, 1, 
									(struct timeval *) NULL, &ext_res);
			if ( ( res_rc == LDAP_RES_EXTENDED ) && ext_res ) {
				ext_res_rc = ldap_parse_extended_result( ld, ext_res, &ext_oid, 
														 &ext_data, 0 );
				if ( ext_res_rc != LDAP_SUCCESS ) {
					if ( ext_res ) {
						ldap_msgfree( ext_res );
					}					
					nsldapi_connection_lost_nolock( ld, lc->lconn_sb );
				} else {
#ifdef LDAP_DEBUG
	LDAPDebug( LDAP_DEBUG_TRACE, 
			   "nsldapi_send_server_request: Unsolicited response\n", 0, 0, 0 );
	if ( ext_oid ) {
		LDAPDebug( LDAP_DEBUG_TRACE, 
				"nsldapi_send_server_request: Unsolicited response oid: %s\n", 
				   ext_oid, 0, 0 );
	}
	if ( ext_data && ext_data->bv_len && ext_data->bv_val ) {
		LDAPDebug( LDAP_DEBUG_TRACE, 
				"nsldapi_send_server_request: Unsolicited response len: %d\n", 
				   ext_data->bv_len, 0, 0 );
		LDAPDebug( LDAP_DEBUG_TRACE, 
				"nsldapi_send_server_request: Unsolicited response val: %s\n", 
				   ext_data->bv_val, 0, 0 );
	}
	if ( !ext_oid && !ext_data ) { 					
		LDAPDebug( LDAP_DEBUG_TRACE, 
				"nsldapi_send_server_request: Unsolicited response is empty\n", 
				   0, 0, 0 );
	}
#endif /* LDAP_DEBUG */
					if ( ext_oid ) {
						if ( strcmp ( ext_oid, LDAP_NOTICE_OF_DISCONNECTION ) == 0 ) {
							if ( ext_data ) {
								ber_bvfree( ext_data );
							}
							if ( ext_oid ) {
								ldap_memfree( ext_oid );
							}
							if ( ext_res ) {
								ldap_msgfree( ext_res );
							}
							nsldapi_connection_lost_nolock( ld, lc->lconn_sb );
							nsldapi_free_request( ld, lr, 0 );
							nsldapi_free_connection( ld, lc, NULL, NULL, 0, 0 );
							LDAP_MUTEX_UNLOCK( ld, LDAP_REQ_LOCK );
							LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );
							return( -1 );					
						}
					}
				}
			} else {
				if ( ext_res ) {
					ldap_msgfree( ext_res );
				}				
				nsldapi_connection_lost_nolock( ld, lc->lconn_sb );
			}
		}

		/* need to continue write later */
		if (ld->ld_options & LDAP_BITOPT_ASYNC && err == -2 ) {	
			lr->lr_status = LDAP_REQST_WRITING;
			nsldapi_iostatus_interest_write( ld, lc->lconn_sb );
		} else {
			LDAP_SET_LDERRNO( ld, LDAP_SERVER_DOWN, NULL, NULL );
			nsldapi_free_request( ld, lr, 0 );
			nsldapi_free_connection( ld, lc, NULL, NULL, 0, 0 );
			LDAP_MUTEX_UNLOCK( ld, LDAP_REQ_LOCK );
			LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );
			return( -1 );
		}

	} else {
		if ( parentreq == NULL ) {
			ber->ber_end = ber->ber_ptr;
			ber->ber_ptr = ber->ber_buf;
		}

		/* sent -- waiting for a response */
		if (ld->ld_options & LDAP_BITOPT_ASYNC) {
			lc->lconn_status = LDAP_CONNST_CONNECTED;
		}

		nsldapi_iostatus_interest_read( ld, lc->lconn_sb );
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_REQ_LOCK );
	LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );

	LDAP_SET_LDERRNO( ld, LDAP_SUCCESS, NULL, NULL );
	return( msgid );
}


/*
 * returns -1 if a fatal error occurs.  If async is non-zero and the flush
 * would block, -2 is returned. If errno is EPIPE and epipe_handler is set
 * we let the caller to deal with EPIPE and dont call lost_nolock here but
 * the caller should call lost_nolock itself when done with handling EPIPE
 */
int
nsldapi_ber_flush( LDAP *ld, Sockbuf *sb, BerElement *ber, int freeit,
	int async, int epipe_handler )
{
	int	terrno;

	for ( ;; ) {
		 /*
		  * ber_flush() doesn't set errno on EOF, so we pre-set it to
		  * zero to avoid getting tricked by leftover "EAGAIN" errors
		  */
		LDAP_SET_ERRNO( ld, 0 );

		if ( ber_flush( sb, ber, freeit ) == 0 ) {
			return( 0 );	/* success */
		}

		terrno = LDAP_GET_ERRNO( ld );

        if (ld->ld_options & LDAP_BITOPT_ASYNC) {
            if ( terrno != 0 && !NSLDAPI_ERRNO_IO_INPROGRESS( terrno ) ) {
				if ( epipe_handler && ( terrno == EPIPE ) ) {
					return( -1 );	/* fatal error */
				} else {
					nsldapi_connection_lost_nolock( ld, sb );
				}
                return( -1 );	/* fatal error */
            }
        }
        else if ( !NSLDAPI_ERRNO_IO_INPROGRESS( terrno ) ) {
				if ( epipe_handler && ( terrno == EPIPE ) ) {
					return( -1 );	/* fatal error */
				} else {
					nsldapi_connection_lost_nolock( ld, sb );
				}			
				return( -1 );	/* fatal error */
		}

		if ( async ) {
			return( -2 );	/* would block */
		}
	}
}

LDAPConn *
nsldapi_new_connection( LDAP *ld, LDAPServer **srvlistp, int use_ldsb,
	int connect, int bind )
{
    int	rc;
    
	LDAPConn	*lc;
	LDAPServer	*prevsrv, *srv;
	Sockbuf		*sb = NULL;

	/*
	 * make a new LDAP server connection
	 */
	if (( lc = (LDAPConn *)NSLDAPI_CALLOC( 1, sizeof( LDAPConn ))) == NULL
	    || ( !use_ldsb && ( sb = ber_sockbuf_alloc()) == NULL )) {
		if ( lc != NULL ) {
			NSLDAPI_FREE( (char *)lc );
		}
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( NULL );
	}

	LDAP_MUTEX_LOCK( ld, LDAP_OPTION_LOCK );
	if ( !use_ldsb ) {
		/*
		 * we have allocated a new sockbuf
		 * set I/O routines to match those in default LDAP sockbuf
		 */
		IFP				sb_fn;
		struct lber_x_ext_io_fns	extiofns;
		
		extiofns.lbextiofn_size = LBER_X_EXTIO_FNS_SIZE;

		if ( ber_sockbuf_get_option( ld->ld_sbp,
		    LBER_SOCKBUF_OPT_EXT_IO_FNS, &extiofns ) == 0 ) {
			ber_sockbuf_set_option( sb,
			    LBER_SOCKBUF_OPT_EXT_IO_FNS, &extiofns );
		}
		if ( ber_sockbuf_get_option( ld->ld_sbp,
		    LBER_SOCKBUF_OPT_READ_FN, (void *)&sb_fn ) == 0
		    && sb_fn != NULL ) {
			ber_sockbuf_set_option( sb, LBER_SOCKBUF_OPT_READ_FN,
			    (void *)sb_fn );
		}
		if ( ber_sockbuf_get_option( ld->ld_sbp,
		    LBER_SOCKBUF_OPT_WRITE_FN, (void *)&sb_fn ) == 0
		    && sb_fn != NULL ) {
			ber_sockbuf_set_option( sb, LBER_SOCKBUF_OPT_WRITE_FN,
			    (void *)sb_fn );
		}
	}

	lc->lconn_sb = ( use_ldsb ) ? ld->ld_sbp : sb;
	lc->lconn_version = ld->ld_version;	/* inherited */
	LDAP_MUTEX_UNLOCK( ld, LDAP_OPTION_LOCK );

	if ( connect ) {
		prevsrv = NULL;
        /* 
         * save the return code for later
         */ 
		for ( srv = *srvlistp; srv != NULL; srv = srv->lsrv_next ) {
			rc = nsldapi_connect_to_host( ld, lc->lconn_sb,
				   srv->lsrv_host, srv->lsrv_port,
			       (  srv->lsrv_options & LDAP_SRV_OPT_SECURE ) != 0,
					&lc->lconn_krbinstance );
			if (rc != -1) {
				break;
			}
			prevsrv = srv;
		}

		if ( srv == NULL ) {
		    if ( !use_ldsb ) {
			NSLDAPI_FREE( (char *)lc->lconn_sb );
		    }
		    NSLDAPI_FREE( (char *)lc );
		    /* nsldapi_open_ldap_connection has already set ld_errno */
		    return( NULL );
		}

		if ( prevsrv == NULL ) {
		    *srvlistp = srv->lsrv_next;
		} else {
		    prevsrv->lsrv_next = srv->lsrv_next;
		}
		lc->lconn_server = srv;
	}

	if (ld->ld_options & LDAP_BITOPT_ASYNC && rc == -2)
    {
        lc->lconn_status = LDAP_CONNST_CONNECTING;
    }
    else {
        lc->lconn_status = LDAP_CONNST_CONNECTED;
    }
    
	lc->lconn_next = ld->ld_conns;
	ld->ld_conns = lc;

	/*
	 * XXX for now, we always do a synchronous bind.  This will have
	 * to change in the long run...
	 */
	if ( bind ) {
		int		err, lderr, freepasswd, authmethod;
		char		*binddn, *passwd;
		LDAPConn	*savedefconn;

		freepasswd = err = 0;

		if ( ld->ld_rebind_fn == NULL ) {
			binddn = passwd = "";
			authmethod = LDAP_AUTH_SIMPLE;
		} else {
			if (( lderr = (*ld->ld_rebind_fn)( ld, &binddn, &passwd,
			    &authmethod, 0, ld->ld_rebind_arg ))
			    == LDAP_SUCCESS ) {
				freepasswd = 1;
			} else {
				LDAP_SET_LDERRNO( ld, lderr, NULL, NULL );
				err = -1;
			}
		}


		if ( err == 0 ) {
			savedefconn = ld->ld_defconn;
			ld->ld_defconn = lc;
			++lc->lconn_refcnt;	/* avoid premature free */

			/*
			 * when binding, we will back down as low as LDAPv2
			 * if we get back "protocol error" from bind attempts
			 */
			for ( ;; ) {
				/* LDAP_MUTEX_UNLOCK(ld, LDAP_CONN_LOCK); */
				if (( lderr = ldap_bind_s( ld, binddn, passwd,
				    authmethod )) == LDAP_SUCCESS ) {
					/* LDAP_MUTEX_LOCK(ld, LDAP_CONN_LOCK); */
					break;
				}
				/* LDAP_MUTEX_LOCK(ld, LDAP_CONN_LOCK); */
				if ( lc->lconn_version <= LDAP_VERSION2
				    || lderr != LDAP_PROTOCOL_ERROR ) {
					err = -1;
					break;
				}
				--lc->lconn_version;	/* try lower version */
			}
			--lc->lconn_refcnt;
			ld->ld_defconn = savedefconn;
		}

		if ( freepasswd ) {
			(*ld->ld_rebind_fn)( ld, &binddn, &passwd,
				&authmethod, 1, ld->ld_rebind_arg );
		}

		if ( err != 0 ) {
			nsldapi_free_connection( ld, lc, NULL, NULL, 1, 0 );
			lc = NULL;
		}
	}

	return( lc );
}


#define LDAP_CONN_SAMEHOST( h1, h2 ) \
	(( (h1) == NULL && (h2) == NULL ) || \
	( (h1) != NULL && (h2) != NULL && strcasecmp( (h1), (h2) ) == 0 ))

static LDAPConn *
find_connection( LDAP *ld, LDAPServer *srv, int any )
/*
 * return an existing connection (if any) to the server srv
 * if "any" is non-zero, check for any server in the "srv" chain
 */
{
	LDAPConn	*lc;
	LDAPServer	*ls;

	for ( lc = ld->ld_conns; lc != NULL; lc = lc->lconn_next ) {
		for ( ls = srv; ls != NULL; ls = ls->lsrv_next ) {
			if ( LDAP_CONN_SAMEHOST( ls->lsrv_host,
			    lc->lconn_server->lsrv_host )
			    && ls->lsrv_port == lc->lconn_server->lsrv_port
			    && ls->lsrv_options ==
			    lc->lconn_server->lsrv_options ) {
				return( lc );
			}
			if ( !any ) {
				break;
			}
		}
	}

	return( NULL );
}



static void
use_connection( LDAP *ld, LDAPConn *lc )
{
	++lc->lconn_refcnt;
	lc->lconn_lastused = time( 0 );
}


void
nsldapi_free_connection( LDAP *ld, LDAPConn *lc, LDAPControl **serverctrls,
    LDAPControl **clientctrls, int force, int unbind )
{
	LDAPConn	*tmplc, *prevlc;

	LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_free_connection\n", 0, 0, 0 );

	if ( force || --lc->lconn_refcnt <= 0 ) {
		if ( lc->lconn_status == LDAP_CONNST_CONNECTED ) {
			nsldapi_iostatus_interest_clear( ld, lc->lconn_sb );
			if ( unbind ) {
				nsldapi_send_unbind( ld, lc->lconn_sb,
				    serverctrls, clientctrls );
			}
		}
		nsldapi_close_connection( ld, lc->lconn_sb );
		prevlc = NULL;
		for ( tmplc = ld->ld_conns; tmplc != NULL;
		    tmplc = tmplc->lconn_next ) {
			if ( tmplc == lc ) {
				if ( prevlc == NULL ) {
				    ld->ld_conns = tmplc->lconn_next;
				} else {
				    prevlc->lconn_next = tmplc->lconn_next;
				}
				break;
			}
			prevlc = tmplc;
		}
		free_servers( lc->lconn_server );
		if ( lc->lconn_krbinstance != NULL ) {
			NSLDAPI_FREE( lc->lconn_krbinstance );
		}
		/*
		 * if this is the default connection (lc->lconn_sb==ld->ld_sbp)
		 * we do not free the Sockbuf here since it will be freed
		 * later inside ldap_unbind().
		 */
		if ( lc->lconn_sb != ld->ld_sbp ) {
			ber_sockbuf_free( lc->lconn_sb );
			lc->lconn_sb = NULL;
		}
		if ( lc->lconn_ber != NULLBER ) {
			ber_free( lc->lconn_ber, 1 );
		}
		if ( lc->lconn_binddn != NULL ) {
			NSLDAPI_FREE( lc->lconn_binddn );
		}
		NSLDAPI_FREE( lc );
		LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_free_connection: actually freed\n",
		    0, 0, 0 );
	} else {
		lc->lconn_lastused = time( 0 );
		LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_free_connection: refcnt %d\n",
		    lc->lconn_refcnt, 0, 0 );
	}
}


#ifdef LDAP_DEBUG
void
nsldapi_dump_connection( LDAP *ld, LDAPConn *lconns, int all )
{
	LDAPConn	*lc;
	char        msg[256];
/* CTIME for this platform doesn't use this. */
#if !defined(SUNOS4) && !defined(_WIN32) && !defined(LINUX)
	char		buf[26];
#endif

	sprintf( msg, "** Connection%s:\n", all ? "s" : "" );
	ber_err_print( msg );
	for ( lc = lconns; lc != NULL; lc = lc->lconn_next ) {
		if ( lc->lconn_server != NULL ) {
			sprintf( msg, "* host: %s  port: %d  secure: %s%s\n",
			    ( lc->lconn_server->lsrv_host == NULL ) ? "(null)"
			    : lc->lconn_server->lsrv_host,
			    lc->lconn_server->lsrv_port,
			    ( lc->lconn_server->lsrv_options &
			    LDAP_SRV_OPT_SECURE ) ? "Yes" :
			    "No", ( lc->lconn_sb == ld->ld_sbp ) ?
			    "  (default)" : "" );
			ber_err_print( msg );
		}
		sprintf( msg, "  refcnt: %d  status: %s\n", lc->lconn_refcnt,
		    ( lc->lconn_status == LDAP_CONNST_NEEDSOCKET ) ?
		    "NeedSocket" : ( lc->lconn_status ==
		    LDAP_CONNST_CONNECTING ) ? "Connecting" :
		    ( lc->lconn_status == LDAP_CONNST_DEAD ) ? "Dead" :
		    "Connected" );
		ber_err_print( msg );
		sprintf( msg, "  last used: %s",
		    NSLDAPI_CTIME( (time_t *) &lc->lconn_lastused, buf,
				sizeof(buf) ));
		ber_err_print( msg );
		if ( lc->lconn_ber != NULLBER ) {
			ber_err_print( "  partial response has been received:\n" );
			ber_dump( lc->lconn_ber, 1 );
		}
		ber_err_print( "\n" );

		if ( !all ) {
			break;
		}
	}
}


void
nsldapi_dump_requests_and_responses( LDAP *ld )
{
	LDAPRequest	*lr;
	LDAPMessage	*lm, *l;
	char        msg[256];

	ber_err_print( "** Outstanding Requests:\n" );
	LDAP_MUTEX_LOCK( ld, LDAP_REQ_LOCK );
	if (( lr = ld->ld_requests ) == NULL ) {
		ber_err_print( "   Empty\n" );
	}
	for ( ; lr != NULL; lr = lr->lr_next ) {
	    sprintf( msg, " * msgid %d,  origid %d, status %s\n",
		lr->lr_msgid, lr->lr_origid, ( lr->lr_status ==
		LDAP_REQST_INPROGRESS ) ? "InProgress" :
		( lr->lr_status == LDAP_REQST_CHASINGREFS ) ? "ChasingRefs" :
		( lr->lr_status == LDAP_REQST_NOTCONNECTED ) ? "NotConnected" :
		( lr->lr_status == LDAP_REQST_CONNDEAD ) ? "Dead" :
		"Writing" );
	    ber_err_print( msg );
	    sprintf( msg, "   outstanding referrals %d, parent count %d\n",
		    lr->lr_outrefcnt, lr->lr_parentcnt );
	    ber_err_print( msg );
	    if ( lr->lr_binddn != NULL ) {
		    sprintf( msg, "   pending bind DN: <%s>\n", lr->lr_binddn );
		    ber_err_print( msg );
	    }
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_REQ_LOCK );

	ber_err_print( "** Response Queue:\n" );
	LDAP_MUTEX_LOCK( ld, LDAP_RESP_LOCK );
	if (( lm = ld->ld_responses ) == NULLMSG ) {
		ber_err_print( "   Empty\n" );
	}
	for ( ; lm != NULLMSG; lm = lm->lm_next ) {
		sprintf( msg, " * msgid %d,  type %d\n",
		    lm->lm_msgid, lm->lm_msgtype );
		ber_err_print( msg );
		if (( l = lm->lm_chain ) != NULL ) {
			ber_err_print( "   chained responses:\n" );
			for ( ; l != NULLMSG; l = l->lm_chain ) {
				sprintf( msg,
				    "  * msgid %d,  type %d\n",
				    l->lm_msgid, l->lm_msgtype );
				ber_err_print( msg );
			}
		}
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_RESP_LOCK );
}
#endif /* LDAP_DEBUG */


void
nsldapi_free_request( LDAP *ld, LDAPRequest *lr, int free_conn )
{
	LDAPRequest	*tmplr, *nextlr;

	LDAPDebug( LDAP_DEBUG_TRACE,
		"nsldapi_free_request 0x%x (origid %d, msgid %d)\n",
		lr, lr->lr_origid, lr->lr_msgid );

	if ( lr->lr_parent != NULL ) {
		--lr->lr_parent->lr_outrefcnt;
	}

	/* free all of our spawned referrals (child requests) */
	for ( tmplr = lr->lr_child; tmplr != NULL; tmplr = nextlr ) {
		nextlr = tmplr->lr_sibling;
		nsldapi_free_request( ld, tmplr, free_conn );
	}

	if ( free_conn ) {
		nsldapi_free_connection( ld, lr->lr_conn, NULL, NULL, 0, 1 );
	}

	if ( lr->lr_prev == NULL ) {
		ld->ld_requests = lr->lr_next;
	} else {
		lr->lr_prev->lr_next = lr->lr_next;
	}

	if ( lr->lr_next != NULL ) {
		lr->lr_next->lr_prev = lr->lr_prev;
	}

	if ( lr->lr_ber != NULL ) {
		ber_free( lr->lr_ber, 1 );
	}

	if ( lr->lr_res_error != NULL ) {
		NSLDAPI_FREE( lr->lr_res_error );
	}

	if ( lr->lr_res_matched != NULL ) {
		NSLDAPI_FREE( lr->lr_res_matched );
	}

	if ( lr->lr_binddn != NULL ) {
		NSLDAPI_FREE( lr->lr_binddn );
	}
	NSLDAPI_FREE( lr );
}


static void
free_servers( LDAPServer *srvlist )
{
    LDAPServer	*nextsrv;

    while ( srvlist != NULL ) {
	nextsrv = srvlist->lsrv_next;
	if ( srvlist->lsrv_dn != NULL ) {
		NSLDAPI_FREE( srvlist->lsrv_dn );
	}
	if ( srvlist->lsrv_host != NULL ) {
		NSLDAPI_FREE( srvlist->lsrv_host );
	}
	NSLDAPI_FREE( srvlist );
	srvlist = nextsrv;
    }
}


/*
 * Initiate chasing of LDAPv2+ (Umich extension) referrals.
 *
 * Returns an LDAP error code.
 *
 * Note that *hadrefp will be set to 1 if one or more referrals were found in
 * "*errstrp" (even if we can't chase them) and zero if none were found.
 * 
 * XXX merging of errors in this routine needs to be improved.
 */
int
nsldapi_chase_v2_referrals( LDAP *ld, LDAPRequest *lr, char **errstrp,
    int *totalcountp, int *chasingcountp )
{
	char		*p, *ref, *unfollowed;
	LDAPRequest	*origreq;
	int		rc, tmprc, len, unknown;

	LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_chase_v2_referrals\n", 0, 0, 0 );

	*totalcountp = *chasingcountp = 0;

	if ( *errstrp == NULL ) {
		return( LDAP_SUCCESS );
	}

	len = strlen( *errstrp );
	for ( p = *errstrp; len >= LDAP_REF_STR_LEN; ++p, --len ) {
		if (( *p == 'R' || *p == 'r' ) && strncasecmp( p,
		    LDAP_REF_STR, LDAP_REF_STR_LEN ) == 0 ) {
			*p = '\0';
			p += LDAP_REF_STR_LEN;
			break;
		}
	}

	if ( len < LDAP_REF_STR_LEN ) {
		return( LDAP_SUCCESS );
	}

	if ( lr->lr_parentcnt >= ld->ld_refhoplimit ) {
		LDAPDebug( LDAP_DEBUG_TRACE,
		    "more than %d referral hops (dropping)\n",
		    ld->ld_refhoplimit, 0, 0 );
		return( LDAP_REFERRAL_LIMIT_EXCEEDED );
	}

	/* find original request */
	for ( origreq = lr; origreq->lr_parent != NULL;
	     origreq = origreq->lr_parent ) {
		;
	}

	unfollowed = NULL;
	rc = LDAP_SUCCESS;

	/* parse out & follow referrals */
	for ( ref = p; rc == LDAP_SUCCESS && ref != NULL; ref = p ) {
		if (( p = strchr( ref, '\n' )) != NULL ) {
			*p++ = '\0';
		} else {
			p = NULL;
		}

		++*totalcountp;

		rc = chase_one_referral( ld, lr, origreq, ref, "v2 referral",
		    &unknown );

		if ( rc != LDAP_SUCCESS || unknown ) {
			if (( tmprc = nsldapi_append_referral( ld, &unfollowed,
			    ref )) != LDAP_SUCCESS ) {
				rc = tmprc;
			}
		} else {
			++*chasingcountp;
		}
	}

	NSLDAPI_FREE( *errstrp );
	*errstrp = unfollowed;

	return( rc );
}


/* returns an LDAP error code */
int
nsldapi_chase_v3_refs( LDAP *ld, LDAPRequest *lr, char **v3refs,
    int is_reference, int *totalcountp, int *chasingcountp )
{
	int		i, rc, unknown;
	LDAPRequest	*origreq;

	*totalcountp = *chasingcountp = 0;

	if ( v3refs == NULL || v3refs[0] == NULL ) {
		return( LDAP_SUCCESS );
	}

	*totalcountp = 1;

	if ( lr->lr_parentcnt >= ld->ld_refhoplimit ) {
		LDAPDebug( LDAP_DEBUG_TRACE,
		    "more than %d referral hops (dropping)\n",
		    ld->ld_refhoplimit, 0, 0 );
		return( LDAP_REFERRAL_LIMIT_EXCEEDED );
	}

	/* find original request */
	for ( origreq = lr; origreq->lr_parent != NULL;
	    origreq = origreq->lr_parent ) {
		;
	}

	/*
	 * in LDAPv3, we just need to follow one referral in the set.
	 * we dp this by stopping as soon as we succeed in initiating a
	 * chase on any referral (basically this means we were able to connect
	 * to the server and bind).
	 */
	for ( i = 0; v3refs[i] != NULL; ++i ) {
		rc = chase_one_referral( ld, lr, origreq, v3refs[i],
		    is_reference ? "v3 reference" : "v3 referral", &unknown );
		if ( rc == LDAP_SUCCESS && !unknown ) {
			*chasingcountp = 1;
			break;
		}
	}

	/* XXXmcs: should we save unfollowed referrals somewhere? */

	return( rc );	/* last error is as good as any other I guess... */
}


/*
 * returns an LDAP error code
 *
 * XXXmcs: this function used to have #ifdef LDAP_DNS code in it but I
 *	removed it when I improved the parsing (we don't define LDAP_DNS
 *	here at Netscape).
 */
static int
chase_one_referral( LDAP *ld, LDAPRequest *lr, LDAPRequest *origreq,
    char *refurl, char *desc, int *unknownp )
{
	int		rc, tmprc, secure, msgid;
	LDAPServer	*srv;
	BerElement	*ber;
	LDAPURLDesc	*ludp;

	*unknownp = 0;
	ludp = NULLLDAPURLDESC;

	if ( nsldapi_url_parse( refurl, &ludp, 0 ) != 0 ) {
		LDAPDebug( LDAP_DEBUG_TRACE,
		    "ignoring unknown %s <%s>\n", desc, refurl, 0 );
		*unknownp = 1;
		rc = LDAP_SUCCESS;
		goto cleanup_and_return;
	}

	secure = (( ludp->lud_options & LDAP_URL_OPT_SECURE ) != 0 );

/* XXXmcs: can't tell if secure is supported by connect callback */
	if ( secure && ld->ld_extconnect_fn == NULL ) {
		LDAPDebug( LDAP_DEBUG_TRACE,
		    "ignoring LDAPS %s <%s>\n", desc, refurl, 0 );
		*unknownp = 1;
		rc = LDAP_SUCCESS;
		goto cleanup_and_return;
	}

	LDAPDebug( LDAP_DEBUG_TRACE, "chasing LDAP%s %s: <%s>\n",
	    secure ? "S" : "", desc, refurl );

	LDAP_MUTEX_LOCK( ld, LDAP_MSGID_LOCK );
	msgid = ++ld->ld_msgid;
	LDAP_MUTEX_UNLOCK( ld, LDAP_MSGID_LOCK );

	if (( tmprc = re_encode_request( ld, origreq->lr_ber, msgid,
	    ludp, &ber )) != LDAP_SUCCESS ) {
		rc = tmprc;
		goto cleanup_and_return;
	}

	if (( srv = (LDAPServer *)NSLDAPI_CALLOC( 1, sizeof( LDAPServer )))
	    == NULL ) {
		ber_free( ber, 1 );
		rc = LDAP_NO_MEMORY;
		goto cleanup_and_return;
	}

	if ( ludp->lud_host == NULL && ld->ld_defhost == NULL ) {
		srv->lsrv_host = NULL;
	} else {
		if ( ludp->lud_host == NULL ) {
		  srv->lsrv_host =
		    nsldapi_strdup( origreq->lr_conn->lconn_server->lsrv_host );
		  LDAPDebug( LDAP_DEBUG_TRACE,
		    "chase_one_referral: using hostname '%s' from original "
		    "request on new request\n",
		    srv->lsrv_host, 0, 0);
		} else {
		  srv->lsrv_host = nsldapi_strdup( ludp->lud_host );
		  LDAPDebug( LDAP_DEBUG_TRACE,
		    "chase_one_referral: using hostname '%s' as specified "
		    "on new request\n",
		    srv->lsrv_host, 0, 0);
		}

		if ( srv->lsrv_host == NULL ) {
			NSLDAPI_FREE( (char *)srv );
			ber_free( ber, 1 );
			rc = LDAP_NO_MEMORY;
			goto cleanup_and_return;
		}
	}

	if ( ludp->lud_port == 0 && ludp->lud_host == NULL ) {
		srv->lsrv_port = origreq->lr_conn->lconn_server->lsrv_port;
		LDAPDebug( LDAP_DEBUG_TRACE,
		    "chase_one_referral: using port (%d) from original "
		    "request on new request\n",
		    srv->lsrv_port, 0, 0);
	} else if ( ludp->lud_port != 0 ) {
		srv->lsrv_port = ludp->lud_port;
		LDAPDebug( LDAP_DEBUG_TRACE,
		    "chase_one_referral: using port (%d) as specified on "
		    "new request\n",
		    srv->lsrv_port, 0, 0);
	} else {
                srv->lsrv_port = secure ? LDAPS_PORT : LDAP_PORT;
                LDAPDebug( LDAP_DEBUG_TRACE,
			   "chase_one_referral: using default port (%d)\n",
			   srv->lsrv_port, 0, 0 );
	}

	if ( secure ) {
		srv->lsrv_options |= LDAP_SRV_OPT_SECURE;
	}

	if ( nsldapi_send_server_request( ld, ber, msgid,
	    lr, srv, NULL, NULL, 1 ) < 0 ) {
		rc = LDAP_GET_LDERRNO( ld, NULL, NULL );
		LDAPDebug( LDAP_DEBUG_ANY, "Unable to chase %s %s (%s)\n",
		    desc, refurl, ldap_err2string( rc ));
	} else {
		rc = LDAP_SUCCESS;
	}

cleanup_and_return:
	if ( ludp != NULLLDAPURLDESC ) {
		ldap_free_urldesc( ludp );
	}

	return( rc );
}


/* returns an LDAP error code */
int
nsldapi_append_referral( LDAP *ld, char **referralsp, char *s )
{
	int	first;

	if ( *referralsp == NULL ) {
		first = 1;
		*referralsp = (char *)NSLDAPI_MALLOC( strlen( s ) +
		    LDAP_REF_STR_LEN + 1 );
	} else {
		first = 0;
		*referralsp = (char *)NSLDAPI_REALLOC( *referralsp,
		    strlen( *referralsp ) + strlen( s ) + 2 );
	}

	if ( *referralsp == NULL ) {
		return( LDAP_NO_MEMORY );
	}

	if ( first ) {
		strcpy( *referralsp, LDAP_REF_STR );
	} else {
		strcat( *referralsp, "\n" );
	}
	strcat( *referralsp, s );

	return( LDAP_SUCCESS );
}



/* returns an LDAP error code */
static int
re_encode_request( LDAP *ld, BerElement *origber, int msgid, LDAPURLDesc *ludp,
    BerElement **berp )
{
/*
 * XXX this routine knows way too much about how the lber library works!
 */
	unsigned long		along, tag;
	long			ver;
	int			rc;
	BerElement		*ber;
	struct berelement	tmpber;
	char			*dn, *orig_dn;

	LDAPDebug( LDAP_DEBUG_TRACE,
	    "re_encode_request: new msgid %d, new dn <%s>\n",
	    msgid, ( ludp->lud_dn == NULL ) ? "NONE" : ludp->lud_dn, 0 );

	tmpber = *origber;

	/*
	 * All LDAP requests are sequences that start with a message id.  For
	 * everything except delete requests, this is followed by a sequence
	 * that is tagged with the operation code.  For deletes, there is just
	 * a DN that is tagged with the operation code.
	 */

	/* skip past msgid and get operation tag */
	if ( ber_scanf( &tmpber, "{it", &along, &tag ) == LBER_ERROR ) {
		return( LDAP_DECODING_ERROR );
	}

	/*
	 * XXXmcs: we don't support scope or filters in search referrals yet,
	 * so if either were present we return an error which is probably
	 * better than just ignoring the extra info.
	 */
	if ( tag == LDAP_REQ_SEARCH &&
	    ( ludp->lud_scope != -1 || ludp->lud_filter != NULL )) {
		return( LDAP_LOCAL_ERROR );
	}

	if ( tag == LDAP_REQ_BIND ) {
		/* bind requests have a version number before the DN */
		rc = ber_scanf( &tmpber, "{ia", &ver, &orig_dn );
	} else if ( tag == LDAP_REQ_DELETE ) {
		/* delete requests DNs are not within a sequence */
		rc = ber_scanf( &tmpber, "a", &orig_dn );
	} else {
		rc = ber_scanf( &tmpber, "{a", &orig_dn );
	}

	if ( rc == LBER_ERROR ) {
		return( LDAP_DECODING_ERROR );
	}

	if ( ludp->lud_dn == NULL ) {
		dn = orig_dn;
	} else {
		dn = ludp->lud_dn;
		NSLDAPI_FREE( orig_dn );
		orig_dn = NULL;
	}

	/* allocate and build the new request */
        if (( rc = nsldapi_alloc_ber_with_options( ld, &ber ))
	    != LDAP_SUCCESS ) {
		if ( orig_dn != NULL ) {
			NSLDAPI_FREE( orig_dn );
		}
                return( rc );
        }

	if ( tag == LDAP_REQ_BIND ) {
		rc = ber_printf( ber, "{it{is", msgid, tag,
		    (int)ver /* XXX lossy cast */, dn );
	} else if ( tag == LDAP_REQ_DELETE ) {
		rc = ber_printf( ber, "{its}", msgid, tag, dn );
	} else {
		rc = ber_printf( ber, "{it{s", msgid, tag, dn );
	}

	if ( orig_dn != NULL ) {
		NSLDAPI_FREE( orig_dn );
	}
/*
 * can't use "dn" or "orig_dn" from this point on (they've been freed)
 */

	if ( rc == -1 ) {
		ber_free( ber, 1 );
		return( LDAP_ENCODING_ERROR );
	}

	if ( tag != LDAP_REQ_DELETE &&
	    ( ber_write( ber, tmpber.ber_ptr, ( tmpber.ber_end -
	    tmpber.ber_ptr ), 0 ) != ( tmpber.ber_end - tmpber.ber_ptr )
	    || ber_printf( ber, "}}" ) == -1 )) {
		ber_free( ber, 1 );
		return( LDAP_ENCODING_ERROR );
	}

#ifdef LDAP_DEBUG
	if ( ldap_debug & LDAP_DEBUG_PACKETS ) {
		LDAPDebug( LDAP_DEBUG_ANY, "re_encode_request new request is:\n",
		    0, 0, 0 );
		ber_dump( ber, 0 );
	}
#endif /* LDAP_DEBUG */

	*berp = ber;
	return( LDAP_SUCCESS );
}


LDAPRequest *
nsldapi_find_request_by_msgid( LDAP *ld, int msgid )
{
    	LDAPRequest	*lr;

	for ( lr = ld->ld_requests; lr != NULL; lr = lr->lr_next ) {
		if ( msgid == lr->lr_msgid ) {
			break;
		}
	}

	return( lr );
}


/*
 * nsldapi_connection_lost_nolock() resets "ld" to a non-connected, known
 * state.  It should be called whenever a fatal error occurs on the
 * Sockbuf "sb."  sb == NULL means we don't know specifically where
 * the problem was so we assume all connections are bad.
 */
void
nsldapi_connection_lost_nolock( LDAP *ld, Sockbuf *sb )
{
	LDAPRequest	*lr;

	/*
	 * change status of all pending requests that are associated with "sb
	 *	to "connection dead."
	 * also change the connection status to "dead" and remove it from
	 *	the list of sockets we are interested in.
	 */
	for ( lr = ld->ld_requests; lr != NULL; lr = lr->lr_next ) {
		if ( sb == NULL ||
		    ( lr->lr_conn != NULL && lr->lr_conn->lconn_sb == sb )) {
			lr->lr_status = LDAP_REQST_CONNDEAD;
			if ( lr->lr_conn != NULL ) {
				lr->lr_conn->lconn_status = LDAP_CONNST_DEAD;
				nsldapi_iostatus_interest_clear( ld,
				    lr->lr_conn->lconn_sb );
			}
		}
	}
}


#ifdef LDAP_DNS
static LDAPServer *
dn2servers( LDAP *ld, char *dn )	/* dn can also be a domain.... */
{
	char		*p, *domain, *host, *server_dn, **dxs;
	int		i, port;
	LDAPServer	*srvlist, *prevsrv, *srv;

	if (( domain = strrchr( dn, '@' )) != NULL ) {
		++domain;
	} else {
		domain = dn;
	}

	if (( dxs = nsldapi_getdxbyname( domain )) == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( NULL );
	}

	srvlist = NULL;

	for ( i = 0; dxs[ i ] != NULL; ++i ) {
		port = LDAP_PORT;
		server_dn = NULL;
		if ( strchr( dxs[ i ], ':' ) == NULL ) {
			host = dxs[ i ];
		} else if ( strlen( dxs[ i ] ) >= 7 &&
		    strncmp( dxs[ i ], "ldap://", 7 ) == 0 ) {
			host = dxs[ i ] + 7;
			if (( p = strchr( host, ':' )) == NULL ) {
				p = host;
			} else {
				*p++ = '\0';
				port = atoi( p );
			}
			if (( p = strchr( p, '/' )) != NULL ) {
				server_dn = ++p;
				if ( *server_dn == '\0' ) {
					server_dn = NULL;
				}
			}
		} else {
			host = NULL;
		}

		if ( host != NULL ) {	/* found a server we can use */
			if (( srv = (LDAPServer *)NSLDAPI_CALLOC( 1,
			    sizeof( LDAPServer ))) == NULL ) {
				free_servers( srvlist );
				srvlist = NULL;
				break;		/* exit loop & return */
			}

			/* add to end of list of servers */
			if ( srvlist == NULL ) {
				srvlist = srv;
			} else {
				prevsrv->lsrv_next = srv;
			}
			prevsrv = srv;
			
			/* copy in info. */
			if (( srv->lsrv_host = nsldapi_strdup( host )) == NULL
			    || ( server_dn != NULL && ( srv->lsrv_dn =
			    nsldapi_strdup( server_dn )) == NULL )) {
				free_servers( srvlist );
				srvlist = NULL;
				break;		/* exit loop & return */
			}
			srv->lsrv_port = port;
		}
	}

	ldap_value_free( dxs );

	if ( srvlist == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_SERVER_DOWN, NULL, NULL );
	}

	return( srvlist );
}
#endif /* LDAP_DNS */
