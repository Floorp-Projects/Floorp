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
 *  Copyright (c) 1990, 1994 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  cldap.c - synchronous, retrying interface to the cldap protocol
 */


#ifdef CLDAP

XXX not MT-safe XXX

#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990, 1994 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef macintosh
#include <stdlib.h>
#include "macos.h"
#else /* macintosh */
#ifdef DOS
#include "msdos.h"
#else /* DOS */
#ifdef _WINDOWS
#include <windows.h>
#else /* _WINDOWS */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif /* _WINDOWS */
#endif /* DOS */
#endif /* macintosh */

#include "ldap-int.h"

#define DEF_CLDAP_TIMEOUT	3
#define DEF_CLDAP_TRIES		4

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK	((unsigned long) 0x7f000001)
#endif


struct cldap_retinfo {
	int		cri_maxtries;
	int		cri_try;
	int		cri_useaddr;
	long		cri_timeout;
};

#ifdef NEEDPROTOS
static int add_addr( LDAP *ld, struct sockaddr *sap );
static int cldap_result( LDAP *ld, int msgid, LDAPMessage **res,
	struct cldap_retinfo *crip, char *base );
static int cldap_parsemsg( LDAP *ld, int msgid, BerElement *ber,
	LDAPMessage **res, char *base );
#else /* NEEDPROTOS */
static int add_addr();
static int cldap_result();
static int cldap_parsemsg();
#endif /* NEEDPROTOS */

/*
 * cldap_open - initialize and connect to an ldap server.  A magic cookie to
 * be used for future communication is returned on success, NULL on failure.
 *
 * Example:
 *	LDAP	*ld;
 *	ld = cldap_open( hostname, port );
 */

LDAP *
cldap_open( char *host, int port )
{
    int 		s;
    ldap_x_in_addr_t	address;
    struct sockaddr_in 	sock;
    struct hostent	*hp;
    LDAP		*ld;
    char		*p;
    int			i;

    LDAPDebug( LDAP_DEBUG_TRACE, "cldap_open\n", 0, 0, 0 );

    if ( port == 0 ) {
	    port = LDAP_PORT;
    }

    if ( (s = socket( AF_INET, SOCK_DGRAM, 0 )) < 0 ) {
	return( NULL );
    }

    sock.sin_addr.s_addr = 0;
    sock.sin_family = AF_INET;
    sock.sin_port = 0;
    if ( bind(s, (struct sockaddr *) &sock, sizeof(sock)) < 0)  {
	close( s );
	return( NULL );
    }

    if (( ld = ldap_init( host, port )) == NULL ) {
	close( s );
	return( NULL );
    }
    if ( (ld->ld_sbp->sb_fromaddr = (void *)NSLDAPI_CALLOC( 1,
	    sizeof( struct sockaddr ))) == NULL ) {
	NSLDAPI_FREE( ld );
	close( s );
	return( NULL );
    }	
    ld->ld_sbp->sb_sd = s;
    ld->ld_sbp->sb_naddr = 0;
    ld->ld_version = LDAP_VERSION2;

    sock.sin_family = AF_INET;
    sock.sin_port = htons( port );

    /*
     * 'host' may be a space-separated list.
     */
    if ( host != NULL ) {
	for ( ; host != NULL; host = p ) {
	    if (( p = strchr( host, ' ' )) != NULL ) {
		for (*p++ = '\0'; *p == ' '; p++) {
		    ;
		}
	    }

	    if ( (address = inet_addr( host )) == -1 ) {
/* XXXmcs: need to use DNS callbacks here XXX */
XXX
		if ( (hp = gethostbyname( host )) == NULL ) {
		    LDAP_SET_ERRNO( ld, EHOSTUNREACH );
		    continue;
		}

		for ( i = 0; hp->h_addr_list[ i ] != 0; ++i ) {
		    SAFEMEMCPY( (char *)&sock.sin_addr.s_addr,
			    (char *)hp->h_addr_list[ i ],
			    sizeof(sock.sin_addr.s_addr));
		    if ( add_addr( ld, (struct sockaddr *)&sock ) < 0 ) {
			close( s );
			NSLDAPI_FREE( ld );
			return( NULL );
		    }
		}

	    } else {
		sock.sin_addr.s_addr = address;
		if ( add_addr( ld, (struct sockaddr *)&sock ) < 0 ) {
		    close( s );
		    NSLDAPI_FREE( ld );
		    return( NULL );
		}
	    }

	    if ( ld->ld_host == NULL ) {
		    ld->ld_host = nsldapi_strdup( host );
	    }
	}

    } else {
	address = INADDR_LOOPBACK;
	sock.sin_addr.s_addr = htonl( address );
	if ( add_addr( ld, (struct sockaddr *)&sock ) < 0 ) {
	    close( s );
	    NSLDAPI_FREE( ld );
	    return( NULL );
	}
    }

    if ( ld->ld_sbp->sb_addrs == NULL
	    || ( ld->ld_defconn = nsldapi_new_connection( ld, NULL, 1,0,0 )) == NULL ) {
	NSLDAPI_FREE( ld );
	return( NULL );
    }

    ld->ld_sbp->sb_useaddr = ld->ld_sbp->sb_addrs[ 0 ];
    cldap_setretryinfo( ld, 0, 0 );

#ifdef LDAP_DEBUG
    putchar( '\n' );
    for ( i = 0; i < ld->ld_sbp->sb_naddr; ++i ) {
	LDAPDebug( LDAP_DEBUG_TRACE, "end of cldap_open address %d is %s\n",
		i, inet_ntoa( ((struct sockaddr_in *)
		ld->ld_sbp->sb_addrs[ i ])->sin_addr ), 0 );
    }
#endif

    return( ld );
}



void
cldap_close( LDAP *ld )
{
	ldap_ld_free( ld, NULL, NULL, 0 );
}


void
cldap_setretryinfo( LDAP *ld, int tries, int timeout )
{
    ld->ld_cldaptries = ( tries <= 0 ) ? DEF_CLDAP_TRIES : tries;
    ld->ld_cldaptimeout = ( timeout <= 0 ) ? DEF_CLDAP_TIMEOUT : timeout;
}


int
cldap_search_s( LDAP *ld, char *base, int scope, char *filter, char **attrs,
	int attrsonly, LDAPMessage **res, char *logdn )
{
    int				ret, msgid;
    struct cldap_retinfo	cri;

    *res = NULLMSG;

    (void) memset( &cri, 0, sizeof( cri ));

    if ( logdn != NULL ) {
	ld->ld_cldapdn = logdn;
    } else if ( ld->ld_cldapdn == NULL ) {
	ld->ld_cldapdn = "";
    }

    do {
	if ( cri.cri_try != 0 ) {
		--ld->ld_msgid;	/* use same id as before */
	}
	ld->ld_sbp->sb_useaddr = ld->ld_sbp->sb_addrs[ cri.cri_useaddr ];

	LDAPDebug( LDAP_DEBUG_TRACE, "cldap_search_s try %d (to %s)\n",
	    cri.cri_try, inet_ntoa( ((struct sockaddr_in *)
	    ld->ld_sbp->sb_useaddr)->sin_addr ), 0 );

	    if ( (msgid = ldap_search( ld, base, scope, filter, attrs,
		attrsonly )) == -1 ) {
		    return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
	    }
#ifndef NO_CACHE
	    if ( ld->ld_cache != NULL && ld->ld_responses != NULL ) {
		LDAPDebug( LDAP_DEBUG_TRACE, "cldap_search_s res from cache\n",
			0, 0, 0 );
		*res = ld->ld_responses;
		ld->ld_responses = ld->ld_responses->lm_next;
		return( ldap_result2error( ld, *res, 0 ));
	    }
#endif /* NO_CACHE */
	    ret = cldap_result( ld, msgid, res, &cri, base );
	} while (ret == -1);

	return( ret );
}


static int
add_addr( LDAP *ld, struct sockaddr *sap )
{
    struct sockaddr	*newsap, **addrs;

    if (( newsap = (struct sockaddr *)NSLDAPI_MALLOC(
	    sizeof( struct sockaddr ))) == NULL ) {
	LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
	return( -1 );
    }

    if ( ld->ld_sbp->sb_naddr == 0 ) {
	addrs = (struct sockaddr **)NSLDAPI_MALLOC( sizeof(struct sockaddr *));
    } else {
	addrs = (struct sockaddr **)NSLDAPI_REALLOC( ld->ld_sbp->sb_addrs,
		( ld->ld_sbp->sb_naddr + 1 ) * sizeof(struct sockaddr *));
    }

    if ( addrs == NULL ) {
	NSLDAPI_FREE( newsap );
	LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
	return( -1 );
    }

    SAFEMEMCPY( (char *)newsap, (char *)sap, sizeof( struct sockaddr ));
    addrs[ ld->ld_sbp->sb_naddr++ ] = newsap;
    ld->ld_sbp->sb_addrs = (void **)addrs;
    return( 0 );
}


static int
cldap_result( LDAP *ld, int msgid, LDAPMessage **res,
	struct cldap_retinfo *crip, char *base )
{
    Sockbuf 		*sb = ld->ld_sbp;
    BerElement		ber;
    char		*logdn;
    int			ret, fromaddr, i;
    ber_int_t		id;
    struct timeval	tv;

    fromaddr = -1;

    if ( crip->cri_try == 0 ) {
	crip->cri_maxtries = ld->ld_cldaptries * sb->sb_naddr;
	crip->cri_timeout = ld->ld_cldaptimeout;
	crip->cri_useaddr = 0;
	LDAPDebug( LDAP_DEBUG_TRACE, "cldap_result tries %d timeout %d\n",
		ld->ld_cldaptries, ld->ld_cldaptimeout, 0 );
    }

    if ((tv.tv_sec = crip->cri_timeout / sb->sb_naddr) < 1 ) {
	tv.tv_sec = 1;
    }
    tv.tv_usec = 0;

    LDAPDebug( LDAP_DEBUG_TRACE,
	    "cldap_result waiting up to %d seconds for a response\n",
	    tv.tv_sec, 0, 0 );
    ber_init_w_nullchar( &ber, 0 );
    nsldapi_set_ber_options( ld, &ber );

    if ( cldap_getmsg( ld, &tv, &ber ) == -1 ) {
	ret = LDAP_GET_LDERRNO( ld, NULL, NULL );
	LDAPDebug( LDAP_DEBUG_TRACE, "cldap_getmsg returned -1 (%d)\n",
		ret, 0, 0 );
    } else if ( LDAP_GET_LDERRNO( ld, NULL, NULL ) == LDAP_TIMEOUT ) {
	LDAPDebug( LDAP_DEBUG_TRACE,
	    "cldap_result timed out\n", 0, 0, 0 );
	/*
	 * It timed out; is it time to give up?
	 */
	if ( ++crip->cri_try >= crip->cri_maxtries ) {
	    ret = LDAP_TIMEOUT;
	    --crip->cri_try;
	} else {
	    if ( ++crip->cri_useaddr >= sb->sb_naddr ) {
		/*
		 * new round: reset address to first one and
		 * double the timeout
		 */
		crip->cri_useaddr = 0;
		crip->cri_timeout <<= 1;
	    }
	    ret = -1;
	}

    } else {
	/*
	 * Got a response.  It should look like:
	 * { msgid, logdn, { searchresponse...}}
	 */
	logdn = NULL;

	if ( ber_scanf( &ber, "ia", &id, &logdn ) == LBER_ERROR ) {
	    NSLDAPI_FREE( ber.ber_buf );	/* gack! */
	    ret = LDAP_DECODING_ERROR;
	    LDAPDebug( LDAP_DEBUG_TRACE,
		    "cldap_result: ber_scanf returned LBER_ERROR (%d)\n",
		    ret, 0, 0 );
	} else if ( id != msgid ) {
	    NSLDAPI_FREE( ber.ber_buf );	/* gack! */
	    LDAPDebug( LDAP_DEBUG_TRACE,
		    "cldap_result: looking for msgid %d; got %ld\n",
		    msgid, id, 0 );
	    ret = -1;	/* ignore and keep looking */
	} else {
	    /*
	     * got a result: determine which server it came from
	     * decode into ldap message chain
	     */
	    for ( fromaddr = 0; fromaddr < sb->sb_naddr; ++fromaddr ) {
		    if ( memcmp( &((struct sockaddr_in *)
			    sb->sb_addrs[ fromaddr ])->sin_addr,
			    &((struct sockaddr_in *)sb->sb_fromaddr)->sin_addr,
			    sizeof( struct in_addr )) == 0 ) {
			break;
		    }
	    }
	    ret = cldap_parsemsg( ld, msgid, &ber, res, base );
	    NSLDAPI_FREE( ber.ber_buf );	/* gack! */
	    LDAPDebug( LDAP_DEBUG_TRACE,
		"cldap_result got result (%d)\n", ret, 0, 0 );
	}

	if ( logdn != NULL ) {
		NSLDAPI_FREE( logdn );
	}
    }
    

    /*
     * If we are giving up (successfully or otherwise) then 
     * abandon any outstanding requests.
     */
    if ( ret != -1 ) {
	i = crip->cri_try;
	if ( i >= sb->sb_naddr ) {
	    i = sb->sb_naddr - 1;
	}

	for ( ; i >= 0; --i ) {
	    if ( i == fromaddr ) {
		continue;
	    }
	    sb->sb_useaddr = sb->sb_addrs[ i ];
	    LDAPDebug( LDAP_DEBUG_TRACE, "cldap_result abandoning id %d (to %s)\n",
		msgid, inet_ntoa( ((struct sockaddr_in *)
		sb->sb_useaddr)->sin_addr ), 0 );
	    (void) ldap_abandon( ld, msgid );
	}
    }

    LDAP_SET_LDERRNO( ld, ret, NULL, NULL );
    return( ret );
}


static int
cldap_parsemsg( LDAP *ld, int msgid, BerElement *ber,
	LDAPMessage **res, char *base )
{
    ber_tag_t	tag;
    ber_len_t len;
    int			baselen, slen, rc;
    char		*dn, *p, *cookie;
    LDAPMessage		*chain, *prev, *ldm;
    struct berval	*bv;

    rc = LDAP_DECODING_ERROR;	/* pessimistic */
    ldm = chain = prev = NULLMSG;
    baselen = ( base == NULL ) ? 0 : strlen( base );
    bv = NULL;

    for ( tag = ber_first_element( ber, &len, &cookie );
	    tag != LBER_ERROR && tag != LBER_END_OF_SEQOFSET
	    && rc != LDAP_SUCCESS;
	    tag = ber_next_element( ber, &len, cookie )) {
	if (( ldm = (LDAPMessage *)NSLDAPI_CALLOC( 1, sizeof(LDAPMessage)))
		== NULL ) {
	    rc = LDAP_NO_MEMORY;
	    break;	/* return with error */
	} else if (( rc = nsldapi_alloc_ber_with_options( ld, &ldm->lm_ber ))
		!= LDAP_SUCCESS ) {
	    break;	/* return with error*/
	}
	ldm->lm_msgid = msgid;
	ldm->lm_msgtype = tag;

	if ( tag == LDAP_RES_SEARCH_RESULT ) {
	    LDAPDebug( LDAP_DEBUG_TRACE, "cldap_parsemsg got search result\n",
		    0, 0, 0 );

	    if ( ber_get_stringal( ber, &bv ) == LBER_DEFAULT ) {
		break;	/* return w/error */
	    }

	    if ( ber_printf( ldm->lm_ber, "to", tag, bv->bv_val,
		    bv->bv_len ) == -1 ) {
		break;	/* return w/error */
	    }
	    ber_bvfree( bv );
	    bv = NULL;
	    rc = LDAP_SUCCESS;

	} else if ( tag == LDAP_RES_SEARCH_ENTRY ) {
	    if ( ber_scanf( ber, "{aO", &dn, &bv ) == LBER_ERROR ) {
		break;	/* return w/error */
	    }
	    LDAPDebug( LDAP_DEBUG_TRACE, "cldap_parsemsg entry %s\n", dn, 0, 0 );
	    if ( dn != NULL && *(dn + ( slen = strlen(dn)) - 1) == '*' &&
		    baselen > 0 ) {
		/*
		 * substitute original searchbase for trailing '*'
		 */
		if (( p = (char *)NSLDAPI_MALLOC( slen + baselen )) == NULL ) {
		    rc = LDAP_NO_MEMORY;
		    NSLDAPI_FREE( dn );
		    break;	/* return w/error */
		}
		strcpy( p, dn );
		strcpy( p + slen - 1, base );
		NSLDAPI_FREE( dn );
		dn = p;
	    }

	    if ( ber_printf( ldm->lm_ber, "t{so}", tag, dn, bv->bv_val,
		    bv->bv_len ) == -1 ) {
		break;	/* return w/error */
	    }
	    NSLDAPI_FREE( dn );
	    ber_bvfree( bv );
	    bv = NULL;
		
	} else {
	    LDAPDebug( LDAP_DEBUG_TRACE, "cldap_parsemsg got unknown tag %d\n",
		    tag, 0, 0 );
	    rc = LDAP_PROTOCOL_ERROR;
	    break;	/* return w/error */
	}

	/* Reset message ber so we can read from it later.  Gack! */
	ldm->lm_ber->ber_end = ldm->lm_ber->ber_ptr;
	ldm->lm_ber->ber_ptr = ldm->lm_ber->ber_buf;

#ifdef LDAP_DEBUG
	if ( ldap_debug & LDAP_DEBUG_PACKETS ) {
		char msg[80];
	    sprintf( msg, "cldap_parsemsg add message id %d type %d:\n",
		    ldm->lm_msgid, ldm->lm_msgtype  );
		ber_err_print( msg );
	    ber_dump( ldm->lm_ber, 1 );
	}
#endif /* LDAP_DEBUG */

#ifndef NO_CACHE
	    if ( ld->ld_cache != NULL ) {
		nsldapi_add_result_to_cache( ld, ldm );
	    }
#endif /* NO_CACHE */

	if ( chain == NULL ) {
	    chain = ldm;
	} else {
	    prev->lm_chain = ldm;
	}
	prev = ldm;
	ldm = NULL;
    }

    /* dispose of any leftovers */
    if ( ldm != NULL ) {
	if ( ldm->lm_ber != NULLBER ) {
	    ber_free( ldm->lm_ber, 1 );
	}
	NSLDAPI_FREE( ldm );
    }
    if ( bv != NULL ) {
	ber_bvfree( bv );
    }

    /* return chain, calling result2error if we got anything at all */
    *res = chain;
    return(( *res == NULLMSG ) ? rc : ldap_result2error( ld, *res, 0 ));
}
#endif /* CLDAP */
