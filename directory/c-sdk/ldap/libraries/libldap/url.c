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
 *  Copyright (c) 1996 Regents of the University of Michigan.
 *  All rights reserved.
 *
 */
/*  LIBLDAP url.c -- LDAP URL related routines
 *
 *  LDAP URLs look like this:
 *    l d a p : / / hostport / dn [ ? attributes [ ? scope [ ? filter ] ] ]
 *
 *  where:
 *   attributes is a comma separated list
 *   scope is one of these three strings:  base one sub (default=base)
 *   filter is an string-represented filter as in RFC 1558
 *
 *  e.g.,  ldap://ldap.itd.umich.edu/c=US?o,description?one?o=umich
 *
 *  We also tolerate URLs that look like: <ldapurl> and <URL:ldapurl>
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1996 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"


static int skip_url_prefix( char **urlp, int *enclosedp, int *securep );


int
LDAP_CALL
ldap_is_ldap_url( char *url )
{
	int	enclosed, secure;

	return( url != NULL && skip_url_prefix( &url, &enclosed, &secure ));
}


static int
skip_url_prefix( char **urlp, int *enclosedp, int *securep )
{
/*
 * return non-zero if this looks like a LDAP URL; zero if not
 * if non-zero returned, *urlp will be moved past "ldap://" part of URL
 */
	if ( *urlp == NULL ) {
		return( 0 );
	}

	/* skip leading '<' (if any) */
	if ( **urlp == '<' ) {
		*enclosedp = 1;
		++*urlp;
	} else {
		*enclosedp = 0;
	}

	/* skip leading "URL:" (if any) */
	if ( strlen( *urlp ) >= LDAP_URL_URLCOLON_LEN && strncasecmp(
	    *urlp, LDAP_URL_URLCOLON, LDAP_URL_URLCOLON_LEN ) == 0 ) {
		*urlp += LDAP_URL_URLCOLON_LEN;
	}

	/* check for an "ldap://" prefix */
	if ( strlen( *urlp ) >= LDAP_URL_PREFIX_LEN && strncasecmp( *urlp,
	    LDAP_URL_PREFIX, LDAP_URL_PREFIX_LEN ) == 0 ) {
		/* skip over URL prefix and return success */
		*urlp += LDAP_URL_PREFIX_LEN;
		*securep = 0;
		return( 1 );
	}

	/* check for an "ldaps://" prefix */
	if ( strlen( *urlp ) >= LDAPS_URL_PREFIX_LEN && strncasecmp( *urlp,
	    LDAPS_URL_PREFIX, LDAPS_URL_PREFIX_LEN ) == 0 ) {
		/* skip over URL prefix and return success */
		*urlp += LDAPS_URL_PREFIX_LEN;
		*securep = 1;
		return( 1 );
	}

	return( 0 );	/* not an LDAP URL */
}


int
LDAP_CALL
ldap_url_parse( char *url, LDAPURLDesc **ludpp )
{
/*
 *  Pick apart the pieces of an LDAP URL.
 */
	int	rc;

	if (( rc = nsldapi_url_parse( url, ludpp, 1 )) == 0 ) {
		if ( (*ludpp)->lud_scope == -1 ) {
			(*ludpp)->lud_scope = LDAP_SCOPE_BASE;
		}
		if ( (*ludpp)->lud_filter == NULL ) {
			(*ludpp)->lud_filter = "(objectclass=*)";
		}
		if ( *((*ludpp)->lud_dn) == '\0' ) {
			(*ludpp)->lud_dn = NULL;
		}
	}

	return( rc );
}


/*
 * like ldap_url_parse() with a few exceptions:
 *   1) if dn_required is zero, a missing DN does not generate an error
 *	(we just leave the lud_dn field NULL)
 *   2) no defaults are set for lud_scope and lud_filter (they are set to -1
 *	and NULL respectively if no SCOPE or FILTER are present in the URL).
 *   3) when there is a zero-length DN in a URL we do not set lud_dn to NULL.
 *   4) if an LDAPv3 URL extensions are included, 
 */
int
nsldapi_url_parse( char *url, LDAPURLDesc **ludpp, int dn_required )
{

	LDAPURLDesc	*ludp;
	char		*attrs, *p, *q;
	int		enclosed, secure, i, nattrs;

	LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_url_parse(%s)\n", url, 0, 0 );

	if ( url == NULL || ludpp == NULL ) {
		return( LDAP_URL_ERR_PARAM );
	}

	*ludpp = NULL;	/* pessimistic */

	if ( !skip_url_prefix( &url, &enclosed, &secure )) {
		return( LDAP_URL_ERR_NOTLDAP );
	}

	/* allocate return struct */
	if (( ludp = (LDAPURLDesc *)NSLDAPI_CALLOC( 1, sizeof( LDAPURLDesc )))
	    == NULLLDAPURLDESC ) {
		return( LDAP_URL_ERR_MEM );
	}

	if ( secure ) {
		ludp->lud_options |= LDAP_URL_OPT_SECURE;
	}

	/* make working copy of the remainder of the URL */
	if (( url = nsldapi_strdup( url )) == NULL ) {
		ldap_free_urldesc( ludp );
		return( LDAP_URL_ERR_MEM );
	}

	if ( enclosed && *((p = url + strlen( url ) - 1)) == '>' ) {
		*p = '\0';
	}

	/* initialize scope and filter */
	ludp->lud_scope = -1;
	ludp->lud_filter = NULL;

	/* lud_string is the only malloc'd string space we use */
	ludp->lud_string = url;

	/* scan forward for '/' that marks end of hostport and begin. of dn */
	if (( ludp->lud_dn = strchr( url, '/' )) == NULL ) {
		if ( dn_required ) {
			ldap_free_urldesc( ludp );
			return( LDAP_URL_ERR_NODN );
		}
	} else {
		/* terminate hostport; point to start of dn */
		*ludp->lud_dn++ = '\0';
	}


	if ( *url == '\0' ) {
		ludp->lud_host = NULL;
	} else {
		ludp->lud_host = url;
		nsldapi_hex_unescape( ludp->lud_host );

		/*
		 * Locate and strip off optional port number (:#) in host
		 * portion of URL.
		 *
		 * If more than one space-separated host is listed, we only
		 * look for a port number within the right-most one since
		 * ldap_init() will handle host parameters that look like
		 * host:port anyway.
		 */
		if (( p = strrchr( ludp->lud_host, ' ' )) == NULL ) {
			p = ludp->lud_host;
		} else {
			++p;
		}
		if (( p = strchr( p, ':' )) != NULL ) {
			*p++ = '\0';
			ludp->lud_port = atoi( p );
			if ( *ludp->lud_host == '\0' ) {
				ludp->lud_host = NULL;  /* no hostname */
			}
		}
	}

	/* scan for '?' that marks end of dn and beginning of attributes */
	attrs = NULL;
	if ( ludp->lud_dn != NULL &&
	    ( attrs = strchr( ludp->lud_dn, '?' )) != NULL ) {
		/* terminate dn; point to start of attrs. */
		*attrs++ = '\0';

		/* scan for '?' that marks end of attrs and begin. of scope */
		if (( p = strchr( attrs, '?' )) != NULL ) {
			/*
			 * terminate attrs; point to start of scope and scan for
			 * '?' that marks end of scope and begin. of filter
			 */
			*p++ = '\0';

			if (( q = strchr( p, '?' )) != NULL ) {
				/* terminate scope; point to start of filter */
				*q++ = '\0';
				if ( *q != '\0' ) {
					ludp->lud_filter = q;
					nsldapi_hex_unescape( ludp->lud_filter );
				}
			}

			if ( strcasecmp( p, "one" ) == 0 ) {
				ludp->lud_scope = LDAP_SCOPE_ONELEVEL;
			} else if ( strcasecmp( p, "base" ) == 0 ) {
				ludp->lud_scope = LDAP_SCOPE_BASE;
			} else if ( strcasecmp( p, "sub" ) == 0 ) {
				ludp->lud_scope = LDAP_SCOPE_SUBTREE;
			} else if ( *p != '\0' ) {
				ldap_free_urldesc( ludp );
				return( LDAP_URL_ERR_BADSCOPE );
			}
		}
	}

	if ( ludp->lud_dn != NULL ) {
		nsldapi_hex_unescape( ludp->lud_dn );
	}

	/*
	 * if attrs list was included, turn it into a null-terminated array
	 */
	if ( attrs != NULL && *attrs != '\0' ) {
		nsldapi_hex_unescape( attrs );
		for ( nattrs = 1, p = attrs; *p != '\0'; ++p ) {
		    if ( *p == ',' ) {
			    ++nattrs;
		    }
		}

		if (( ludp->lud_attrs = (char **)NSLDAPI_CALLOC( nattrs + 1,
		    sizeof( char * ))) == NULL ) {
			ldap_free_urldesc( ludp );
			return( LDAP_URL_ERR_MEM );
		}

		for ( i = 0, p = attrs; i < nattrs; ++i ) {
			ludp->lud_attrs[ i ] = p;
			if (( p = strchr( p, ',' )) != NULL ) {
				*p++ ='\0';
			}
			nsldapi_hex_unescape( ludp->lud_attrs[ i ] );
		}
	}

	*ludpp = ludp;

	return( 0 );
}


void
LDAP_CALL
ldap_free_urldesc( LDAPURLDesc *ludp )
{
	if ( ludp != NULLLDAPURLDESC ) {
		if ( ludp->lud_string != NULL ) {
			NSLDAPI_FREE( ludp->lud_string );
		}
		if ( ludp->lud_attrs != NULL ) {
			NSLDAPI_FREE( ludp->lud_attrs );
		}
		NSLDAPI_FREE( ludp );
	}
}


int
LDAP_CALL
ldap_url_search( LDAP *ld, char *url, int attrsonly )
{
	int		err, msgid;
	LDAPURLDesc	*ludp;
	BerElement	*ber;
	LDAPServer	*srv;
	char		*host;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( -1 );		/* punt */
	}

	if ( ldap_url_parse( url, &ludp ) != 0 ) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( -1 );
	}

	LDAP_MUTEX_LOCK( ld, LDAP_MSGID_LOCK );
	msgid = ++ld->ld_msgid;
	LDAP_MUTEX_UNLOCK( ld, LDAP_MSGID_LOCK );

	if ( ldap_build_search_req( ld, ludp->lud_dn, ludp->lud_scope,
	    ludp->lud_filter, ludp->lud_attrs, attrsonly, NULL, NULL,
	    NULL, -1, msgid, &ber ) != LDAP_SUCCESS ) {
		return( -1 );
	}

	err = 0;

	if ( ludp->lud_host == NULL ) {
		host = ld->ld_defhost;
	} else {
		host = ludp->lud_host;
	}

	if (( srv = (LDAPServer *)NSLDAPI_CALLOC( 1, sizeof( LDAPServer )))
	    == NULL || ( host != NULL &&
	    ( srv->lsrv_host = nsldapi_strdup( host )) == NULL )) {
		if ( srv != NULL ) {
			NSLDAPI_FREE( srv );
		}
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		err = -1;
	} else {
		if ( ludp->lud_port == 0 ) {
			if (( ludp->lud_options & LDAP_URL_OPT_SECURE ) == 0 ) {
				srv->lsrv_port = LDAP_PORT;
			} else {
				srv->lsrv_port = LDAPS_PORT;
			}
		} else {
			 srv->lsrv_port = ludp->lud_port;
		}
	}

	if (( ludp->lud_options & LDAP_URL_OPT_SECURE ) != 0 ) {
		srv->lsrv_options |= LDAP_SRV_OPT_SECURE;
	}

	if ( err != 0 ) {
		ber_free( ber, 1 );
	} else {
		err = nsldapi_send_server_request( ld, ber, msgid, NULL, srv,
		    NULL, NULL, 1 );
	}

	ldap_free_urldesc( ludp );
	return( err );
}


int
LDAP_CALL
ldap_url_search_st( LDAP *ld, char *url, int attrsonly,
	struct timeval *timeout, LDAPMessage **res )
{
	int	msgid;

	if (( msgid = ldap_url_search( ld, url, attrsonly )) == -1 ) {
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
	}

	if ( ldap_result( ld, msgid, 1, timeout, res ) == -1 ) {
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
	}

	if ( LDAP_GET_LDERRNO( ld, NULL, NULL ) == LDAP_TIMEOUT ) {
		(void) ldap_abandon( ld, msgid );
		LDAP_SET_LDERRNO( ld, LDAP_TIMEOUT, NULL, NULL );
		return( LDAP_TIMEOUT );
	}

	return( ldap_result2error( ld, *res, 0 ));
}


int
LDAP_CALL
ldap_url_search_s( LDAP *ld, char *url, int attrsonly, LDAPMessage **res )
{
	int	msgid;

	if (( msgid = ldap_url_search( ld, url, attrsonly )) == -1 ) {
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
	}

	if ( ldap_result( ld, msgid, 1, (struct timeval *)NULL, res ) == -1 ) {
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
	}

	return( ldap_result2error( ld, *res, 0 ));
}
