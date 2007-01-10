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
 *  Copyright (c) 1996 Regents of the University of Michigan.
 *  All rights reserved.
 *
 */
/*  LIBLDAP url.c -- LDAP URL related routines
 *
 *  LDAP URLs look like this:
 *    l d a p : / / [ hostport ] [ / dn [ ? [ attributes ] [ ? [ scope ]
 *			[ ? [ filter ] [ ? extensions ] ] ] ] ]
 *
 *  where:
 *   hostport is a host or a host:port list that can be space-separated.
 *   attributes is a comma separated list
 *   scope is one of these three strings:  base one sub (default=base)
 *   filter is a string-represented filter as in RFC 2254
 *   extensions is a comma-separated list of name=value pairs.
 *
 *  e.g.,  ldap://ldap.itd.umich.edu/c=US?o,description?one?o=umich
 *
 *  To accomodate IPv6 addresses, the host portion of a host that appears
 *  in hostport can be enclosed in square brackets, e.g
 *
 *  e.g.,  ldap://[fe80::a00:20ff:fee5:c0b4]:3389/dc=siroe,dc=com
 *
 *  We also tolerate URLs that look like: <ldapurl> and <URL:ldapurl>
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1996 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"


static int skip_url_prefix( const char **urlp, int *enclosedp, int *securep );


int
LDAP_CALL
ldap_is_ldap_url( const char *url )
{
	int	enclosed, secure;

	return( url != NULL
	    && skip_url_prefix( &url, &enclosed, &secure ));
}


static int
skip_url_prefix( const char **urlp, int *enclosedp, int *securep )
{
/*
 * return non-zero if this looks like a LDAP URL; zero if not
 * if non-zero returned, *urlp will be moved past "ldap://" part of URL
 * The data that *urlp points to is not changed by this function.
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
ldap_url_parse_no_defaults( const char *url, LDAPURLDesc **ludpp, int dn_required)
{
    return( nsldapi_url_parse( url, ludpp, dn_required ) );
}


int
LDAP_CALL
ldap_url_parse( const char *url, LDAPURLDesc **ludpp )
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
	} else if ( rc == LDAP_URL_UNRECOGNIZED_CRITICAL_EXTENSION ) {
		rc = LDAP_URL_ERR_PARAM;	/* mapped for backwards compatibility */
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
 *
 * note that LDAPv3 URL extensions are ignored unless they are marked
 * critical, in which case an LDAP_URL_UNRECOGNIZED_CRITICAL_EXTENSION error
 * is returned.
 */
int
nsldapi_url_parse( const char *url, LDAPURLDesc **ludpp, int dn_required )
{

	LDAPURLDesc	*ludp;
	char		*urlcopy, *attrs, *scope, *extensions = NULL, *p, *q;
	int		enclosed, secure, i, nattrs, at_start;

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
	if (( urlcopy = nsldapi_strdup( url )) == NULL ) {
		ldap_free_urldesc( ludp );
		return( LDAP_URL_ERR_MEM );
	}

	if ( enclosed && *((p = urlcopy + strlen( urlcopy ) - 1)) == '>' ) {
		*p = '\0';
	}

	/* initialize scope and filter */
	ludp->lud_scope = -1;
	ludp->lud_filter = NULL;

	/* lud_string is the only malloc'd string space we use */
	ludp->lud_string = urlcopy;

	/* scan forward for '/' that marks end of hostport and begin. of dn */
	if (( ludp->lud_dn = strchr( urlcopy, '/' )) == NULL ) {
		if ( dn_required ) {
			ldap_free_urldesc( ludp );
			return( LDAP_URL_ERR_NODN );
		}
	} else {
		/* terminate hostport; point to start of dn */
		*ludp->lud_dn++ = '\0';
	}


	if ( *urlcopy == '\0' ) {
		ludp->lud_host = NULL;
	} else {
		ludp->lud_host = urlcopy;
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
		if ( *p == '[' && ( q = strchr( p, ']' )) != NULL ) {
			 /* square brackets present -- skip past them */
			p = q++;
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
			scope = p;

			if (( p = strchr( scope, '?' )) != NULL ) {
				/* terminate scope; point to start of filter */
				*p++ = '\0';
				if ( *p != '\0' ) {
					ludp->lud_filter = p;
					/*
					 * scan for the '?' that marks the end
					 * of the filter and the start of any
					 * extensions
					 */
					if (( p = strchr( ludp->lud_filter, '?' ))
					    != NULL ) {
						*p++ = '\0'; /* term. filter */
						extensions = p;
					}
					if ( *ludp->lud_filter == '\0' ) {
						ludp->lud_filter = NULL;
					} else {
						nsldapi_hex_unescape( ludp->lud_filter );
					}
				}
			}

			if ( strcasecmp( scope, "one" ) == 0 ) {
				ludp->lud_scope = LDAP_SCOPE_ONELEVEL;
			} else if ( strcasecmp( scope, "base" ) == 0 ) {
				ludp->lud_scope = LDAP_SCOPE_BASE;
			} else if ( strcasecmp( scope, "sub" ) == 0 ) {
				ludp->lud_scope = LDAP_SCOPE_SUBTREE;
			} else if ( *scope != '\0' ) {
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

	/* if extensions list was included, check for critical ones */
	if ( extensions != NULL && *extensions != '\0' ) {
		/* Note: at present, we do not recognize ANY extensions */
		at_start = 1;
		for ( p = extensions; *p != '\0'; ++p ) {
			if ( at_start ) {
				if ( *p == '!' ) {	/* critical extension */
					ldap_free_urldesc( ludp );
					return( LDAP_URL_UNRECOGNIZED_CRITICAL_EXTENSION );
				}
				at_start = 0;
			} else if ( *p == ',' ) {
				at_start = 1;
			}
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
ldap_url_search( LDAP *ld, const char *url, int attrsonly )
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

	if ( nsldapi_build_search_req( ld, ludp->lud_dn, ludp->lud_scope,
	    ludp->lud_filter, ludp->lud_attrs, attrsonly, NULL, NULL,
	    -1, -1, msgid, &ber ) != LDAP_SUCCESS ) {
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
		if ( ludp->lud_port != 0 ) {
			/* URL includes a port - use it */
			 srv->lsrv_port = ludp->lud_port;
		} else if ( ludp->lud_host == NULL ) {
			/* URL has no port or host - use port from ld */
			srv->lsrv_port = ld->ld_defport;
		} else if (( ludp->lud_options & LDAP_URL_OPT_SECURE ) == 0 ) {
			/* ldap URL has a host but no port - use std. port */
			srv->lsrv_port = LDAP_PORT;
		} else {
			/* ldaps URL has a host but no port - use std. port */
			srv->lsrv_port = LDAPS_PORT;
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
ldap_url_search_st( LDAP *ld, const char *url, int attrsonly,
	struct timeval *timeout, LDAPMessage **res )
{
	int	msgid;

	/*
	 * It is an error to pass in a zero'd timeval.
	 */
	if ( timeout != NULL && timeout->tv_sec == 0 &&
	    timeout->tv_usec == 0 ) {
		if ( ld != NULL ) {
			LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		}
		if ( res != NULL ) {
			*res = NULL;
		}
                return( LDAP_PARAM_ERROR );
        }

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
ldap_url_search_s( LDAP *ld, const char *url, int attrsonly, LDAPMessage **res )
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
