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
 *  search.c
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"

static int nsldapi_timeval2ldaplimit( struct timeval *timeoutp,
	int defaultvalue );
static int nsldapi_search( LDAP *ld, const char *base, int scope,
	const char *filter, char **attrs, int attrsonly,
	LDAPControl **serverctrls, LDAPControl **clientctrls,
	int timelimit, int sizelimit, int *msgidp );
static char *find_right_paren( char *s );
static char *put_complex_filter( BerElement *ber, char *str,
	unsigned long tag, int not );
static int put_filter( BerElement *ber, char *str );
static int unescape_filterval( char *str );
static int hexchar2int( char c );
static int is_valid_attr( char *a );
static int put_simple_filter( BerElement *ber, char *str );
static int put_substring_filter( BerElement *ber, char *type,
	char *str );
static int put_filter_list( BerElement *ber, char *str );
static int nsldapi_search_s( LDAP *ld, const char *base, int scope, 
	const char *filter, char **attrs, int attrsonly,
	LDAPControl **serverctrls, LDAPControl **clientctrls,
	struct timeval *localtimeoutp, int timelimit, int sizelimit,
	LDAPMessage **res );

/*
 * ldap_search - initiate an ldap search operation.  Parameters:
 *
 *	ld		LDAP descriptor
 *	base		DN of the base object
 *	scope		the search scope - one of LDAP_SCOPE_BASE,
 *			    LDAP_SCOPE_ONELEVEL, LDAP_SCOPE_SUBTREE
 *	filter		a string containing the search filter
 *			(e.g., "(|(cn=bob)(sn=bob))")
 *	attrs		list of attribute types to return for matches
 *	attrsonly	1 => attributes only 0 => attributes and values
 *
 * Example:
 *	char	*attrs[] = { "mail", "title", 0 };
 *	msgid = ldap_search( ld, "c=us@o=UM", LDAP_SCOPE_SUBTREE, "cn~=bob",
 *	    attrs, attrsonly );
 */
int
LDAP_CALL
ldap_search(
    LDAP 	*ld,
    const char 	*base,
    int 	scope,
    const char 	*filter,
    char 	**attrs,
    int 	attrsonly
)
{
	int		msgid;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_search\n", 0, 0, 0 );

	if ( ldap_search_ext( ld, base, scope, filter, attrs, attrsonly, NULL,
	    NULL, NULL, -1, &msgid ) == LDAP_SUCCESS ) {
		return( msgid );
	} else {
		return( -1 );	/* error is in ld handle */
	}
}


/*
 * LDAPv3 extended search.
 * Returns an LDAP error code.
 */
int
LDAP_CALL
ldap_search_ext(
    LDAP 		*ld,
    const char 		*base,
    int 		scope,
    const char 		*filter,
    char 		**attrs,
    int 		attrsonly,
    LDAPControl		**serverctrls,
    LDAPControl		**clientctrls,
    struct timeval	*timeoutp,	/* NULL means use ld->ld_timelimit */
    int			sizelimit,
    int			*msgidp
)
{
	/*
	 * It is an error to pass in a zero'd timeval.
	 */
	if ( timeoutp != NULL && timeoutp->tv_sec == 0 &&
	    timeoutp->tv_usec == 0 ) {
		if ( ld != NULL ) {
			LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		}
                return( LDAP_PARAM_ERROR );
        }

	return( nsldapi_search( ld, base, scope, filter, attrs, attrsonly,
	    serverctrls, clientctrls,
	    nsldapi_timeval2ldaplimit( timeoutp, -1 ), sizelimit, msgidp ));
}


/*
 * Like ldap_search_ext() except an integer timelimit is passed instead of
 * using the overloaded struct timeval *timeoutp.
 */
static int
nsldapi_search(
    LDAP 		*ld,
    const char 		*base,
    int 		scope,
    const char 		*filter,
    char 		**attrs,
    int 		attrsonly,
    LDAPControl		**serverctrls,
    LDAPControl		**clientctrls,
    int			timelimit,	/* -1 means use ld->ld_timelimit */
    int			sizelimit,	/* -1 means use ld->ld_sizelimit */
    int			*msgidp
)
{
	BerElement	*ber;
	int		rc, rc_key;
	unsigned long	key;	/* XXXmcs: memcache */

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_search_ext\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}

	if ( base == NULL ) {
	    base = "";
	}

	if ( filter == NULL ) {
	    filter = "(objectclass=*)";
	}

	if ( msgidp == NULL || ( scope != LDAP_SCOPE_BASE
	    && scope != LDAP_SCOPE_ONELEVEL && scope != LDAP_SCOPE_SUBTREE )
		|| ( sizelimit < -1 )) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
                return( LDAP_PARAM_ERROR );
        }
	LDAP_MUTEX_LOCK( ld, LDAP_MSGID_LOCK );
	*msgidp = ++ld->ld_msgid;
	LDAP_MUTEX_UNLOCK( ld, LDAP_MSGID_LOCK );

	/*
	 * XXXmcs: should use cache function pointers to hook in memcache
	 */
	if ( ld->ld_memcache == NULL ) {
		rc_key = LDAP_NOT_SUPPORTED;
	} else if (( rc_key = ldap_memcache_createkey( ld, base, scope, filter,
	    attrs, attrsonly, serverctrls, clientctrls, &key)) == LDAP_SUCCESS
	    && ldap_memcache_result( ld, *msgidp, key ) == LDAP_SUCCESS ) {
		return LDAP_SUCCESS;
	}

	/* check the cache */
	if ( ld->ld_cache_on && ld->ld_cache_search != NULL ) {
		LDAP_MUTEX_LOCK( ld, LDAP_CACHE_LOCK );
		if ( (rc = (ld->ld_cache_search)( ld, *msgidp, LDAP_REQ_SEARCH,
		    base, scope, filter, attrs, attrsonly )) != 0 ) {
			*msgidp = rc;
			LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
			return( LDAP_SUCCESS );
		}
		LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
	}

	/* caching off or did not find it in the cache - check the net */
	if (( rc = nsldapi_build_search_req( ld, base, scope, filter, attrs,
	    attrsonly, serverctrls, clientctrls, timelimit, sizelimit,
	    *msgidp, &ber )) != LDAP_SUCCESS ) {
		return( rc );
	}

	/* send the message */
	rc = nsldapi_send_initial_request( ld, *msgidp, LDAP_REQ_SEARCH,
		(char *) base, ber );

	/*
	 * XXXmcs: should use cache function pointers to hook in memcache
	 */
	if ( (rc_key == LDAP_SUCCESS) && (rc >= 0) ) {
		ldap_memcache_new( ld, rc, key, base );
	}

	*msgidp = rc;
	return( rc < 0 ? LDAP_GET_LDERRNO( ld, NULL, NULL ) : LDAP_SUCCESS );
}


/*
 * Convert a non-NULL timeoutp to a value in seconds that is appropriate to
 * send in an LDAP search request.  If timeoutp is NULL, return defaultvalue.
 */
static int
nsldapi_timeval2ldaplimit( struct timeval *timeoutp, int defaultvalue )
{
	int		timelimit;

	if ( NULL == timeoutp ) {
		timelimit = defaultvalue;
	} else if ( timeoutp->tv_sec > 0 ) {
		timelimit = timeoutp->tv_sec;
	} else if ( timeoutp->tv_usec > 0 ) {
		timelimit = 1;	/* minimum we can express in LDAP */
	} else {
		/*
		 * both tv_sec and tv_usec are less than one (zero?) so
		 * to maintain compatiblity with our "zero means no limit"
		 * convention we pass no limit to the server.
		 */
		timelimit = 0;	/* no limit */
	}

	return( timelimit );
}


/* returns an LDAP error code and also sets it in ld */
int
nsldapi_build_search_req(
    LDAP		*ld, 
    const char		*base, 
    int			scope, 
    const char		*filter,
    char		**attrs, 
    int			attrsonly,
    LDAPControl		**serverctrls,
    LDAPControl		**clientctrls,	/* not used for anything yet */
    int			timelimit,	/* if -1, ld->ld_timelimit is used */
    int			sizelimit,	/* if -1, ld->ld_sizelimit is used */
    int			msgid,
    BerElement		**berp
)
{
	BerElement	*ber;
	int		err;
	char		*fdup;

	/*
	 * Create the search request.  It looks like this:
	 *	SearchRequest := [APPLICATION 3] SEQUENCE {
	 *		baseObject	DistinguishedName,
	 *		scope		ENUMERATED {
	 *			baseObject	(0),
	 *			singleLevel	(1),
	 *			wholeSubtree	(2)
	 *		},
	 *		derefAliases	ENUMERATED {
	 *			neverDerefaliases	(0),
	 *			derefInSearching	(1),
	 *			derefFindingBaseObj	(2),
	 *			alwaysDerefAliases	(3)
	 *		},
	 *		sizelimit	INTEGER (0 .. 65535),
	 *		timelimit	INTEGER (0 .. 65535),
	 *		attrsOnly	BOOLEAN,
	 *		filter		Filter,
	 *		attributes	SEQUENCE OF AttributeType
	 *	}
	 * wrapped in an ldap message.
	 */

	/* create a message to send */
	if (( err = nsldapi_alloc_ber_with_options( ld, &ber ))
	    != LDAP_SUCCESS ) {
		return( err );
	}

	if ( base == NULL ) {
	    base = "";
	}

	if ( sizelimit == -1 ) {
	    sizelimit = ld->ld_sizelimit;
	}

	if ( timelimit == -1 ) {
	    timelimit = ld->ld_timelimit;
	}

#ifdef CLDAP
	if ( ld->ld_sbp->sb_naddr > 0 ) {
	    err = ber_printf( ber, "{ist{seeiib", msgid,
		ld->ld_cldapdn, LDAP_REQ_SEARCH, base, scope, ld->ld_deref,
		sizelimit, timelimit, attrsonly );
	} else {
#endif /* CLDAP */
		err = ber_printf( ber, "{it{seeiib", msgid,
		    LDAP_REQ_SEARCH, base, scope, ld->ld_deref,
		    sizelimit, timelimit, attrsonly );
#ifdef CLDAP
	}
#endif /* CLDAP */

	if ( err == -1 ) {
		LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
		ber_free( ber, 1 );
		return( LDAP_ENCODING_ERROR );
	}

	fdup = nsldapi_strdup( filter );
	err = put_filter( ber, fdup );
	NSLDAPI_FREE( fdup );

	if ( err == -1 ) {
		LDAP_SET_LDERRNO( ld, LDAP_FILTER_ERROR, NULL, NULL );
		ber_free( ber, 1 );
		return( LDAP_FILTER_ERROR );
	}

	if ( ber_printf( ber, "{v}}", attrs ) == -1 ) {
		LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
		ber_free( ber, 1 );
		return( LDAP_ENCODING_ERROR );
	}

	if ( (err = nsldapi_put_controls( ld, serverctrls, 1, ber ))
	    != LDAP_SUCCESS ) {
		ber_free( ber, 1 );
		return( err );
	}

	*berp = ber;
	return( LDAP_SUCCESS );
}

static char *
find_right_paren( char *s )
{
	int	balance, escape;

	balance = 1;
	escape = 0;
	while ( *s && balance ) {
		if ( escape == 0 ) {
			if ( *s == '(' )
				balance++;
			else if ( *s == ')' )
				balance--;
		}
		if ( *s == '\\' && ! escape )
			escape = 1;
		else
			escape = 0;
		if ( balance )
			s++;
	}

	return( *s ? s : NULL );
}

static char *
put_complex_filter(
    BerElement		*ber, 
    char		*str, 
    unsigned long	tag, 
    int			not 
)
{
	char	*next;

	/*
	 * We have (x(filter)...) with str sitting on
	 * the x.  We have to find the paren matching
	 * the one before the x and put the intervening
	 * filters by calling put_filter_list().
	 */

	/* put explicit tag */
	if ( ber_printf( ber, "t{", tag ) == -1 )
		return( NULL );

	str++;
	if ( (next = find_right_paren( str )) == NULL )
		return( NULL );

	*next = '\0';
	if ( put_filter_list( ber, str ) == -1 )
		return( NULL );
	*next++ = ')';

	/* flush explicit tagged thang */
	if ( ber_printf( ber, "}" ) == -1 )
		return( NULL );

	return( next );
}

static int
put_filter( BerElement *ber, char *str )
{
	char	*next;
	int	parens, balance, escape;

	/*
	 * A Filter looks like this:
	 *      Filter ::= CHOICE {
	 *              and             [0]     SET OF Filter,
	 *              or              [1]     SET OF Filter,
	 *              not             [2]     Filter,
	 *              equalityMatch   [3]     AttributeValueAssertion,
	 *              substrings      [4]     SubstringFilter,
	 *              greaterOrEqual  [5]     AttributeValueAssertion,
	 *              lessOrEqual     [6]     AttributeValueAssertion,
	 *              present         [7]     AttributeType,,
	 *              approxMatch     [8]     AttributeValueAssertion
	 *      }
	 *
	 *      SubstringFilter ::= SEQUENCE {
	 *              type               AttributeType,
	 *              SEQUENCE OF CHOICE {
	 *                      initial          [0] IA5String,
	 *                      any              [1] IA5String,
	 *                      final            [2] IA5String
	 *              }
	 *      }
	 * Note: tags in a choice are always explicit
	 */

	LDAPDebug( LDAP_DEBUG_TRACE, "put_filter \"%s\"\n", str, 0, 0 );

	parens = 0;
	while ( *str ) {
		switch ( *str ) {
		case '(':
			str++;
			parens++;
			switch ( *str ) {
			case '&':
				LDAPDebug( LDAP_DEBUG_TRACE, "put_filter: AND\n",
				    0, 0, 0 );

				if ( (str = put_complex_filter( ber, str,
				    LDAP_FILTER_AND, 0 )) == NULL )
					return( -1 );

				parens--;
				break;

			case '|':
				LDAPDebug( LDAP_DEBUG_TRACE, "put_filter: OR\n",
				    0, 0, 0 );

				if ( (str = put_complex_filter( ber, str,
				    LDAP_FILTER_OR, 0 )) == NULL )
					return( -1 );

				parens--;
				break;

			case '!':
				LDAPDebug( LDAP_DEBUG_TRACE, "put_filter: NOT\n",
				    0, 0, 0 );

				if ( (str = put_complex_filter( ber, str,
				    LDAP_FILTER_NOT, 1 )) == NULL )
					return( -1 );

				parens--;
				break;

			default:
				LDAPDebug( LDAP_DEBUG_TRACE,
				    "put_filter: simple\n", 0, 0, 0 );

				balance = 1;
				escape = 0;
				next = str;
				while ( *next && balance ) {
					if ( escape == 0 ) {
						if ( *next == '(' )
							balance++;
						else if ( *next == ')' )
							balance--;
					}
					if ( *next == '\\' && ! escape )
						escape = 1;
					else
						escape = 0;
					if ( balance )
						next++;
				}
				if ( balance != 0 )
					return( -1 );

				*next = '\0';
				if ( put_simple_filter( ber, str ) == -1 ) {
					return( -1 );
				}
				*next++ = ')';
				str = next;
				parens--;
				break;
			}
			break;

		case ')':
			LDAPDebug( LDAP_DEBUG_TRACE, "put_filter: end\n", 0, 0,
			    0 );
			if ( ber_printf( ber, "]" ) == -1 )
				return( -1 );
			str++;
			parens--;
			break;

		case ' ':
			str++;
			break;

		default:	/* assume it's a simple type=value filter */
			LDAPDebug( LDAP_DEBUG_TRACE, "put_filter: default\n", 0, 0,
			    0 );
			next = strchr( str, '\0' );
			if ( put_simple_filter( ber, str ) == -1 ) {
				return( -1 );
			}
			str = next;
			break;
		}
	}

	return( parens ? -1 : 0 );
}


/*
 * Put a list of filters like this "(filter1)(filter2)..."
 */

static int
put_filter_list( BerElement *ber, char *str )
{
	char	*next;
	char	save;

	LDAPDebug( LDAP_DEBUG_TRACE, "put_filter_list \"%s\"\n", str, 0, 0 );

	while ( *str ) {
		while ( *str && isspace( *str ) )
			str++;
		if ( *str == '\0' )
			break;

		if ( (next = find_right_paren( str + 1 )) == NULL )
			return( -1 );
		save = *++next;

		/* now we have "(filter)" with str pointing to it */
		*next = '\0';
		if ( put_filter( ber, str ) == -1 )
			return( -1 );
		*next = save;

		str = next;
	}

	return( 0 );
}


/*
 * is_valid_attr - returns 1 if a is a syntactically valid left-hand side
 * of a filter expression, 0 otherwise.  A valid string may contain only
 * letters, numbers, hyphens, semi-colons, colons and periods. examples:
 *	cn
 *	cn;lang-fr
 *	1.2.3.4;binary;dynamic
 *	mail;dynamic
 *	cn:dn:1.2.3.4
 *
 * For compatibility with older servers, we also allow underscores in
 * attribute types, even through they are not allowed by the LDAPv3 RFCs.
 */
static int
is_valid_attr( char *a )
{
	for ( ; *a; a++ ) {
	    if ( !isascii( *a ) ) {
		return( 0 );
	    } else if ( !isalnum( *a ) ) {
		switch ( *a ) {
		  case '-':
		  case '.':
		  case ';':
		  case ':':
		  case '_':
		    break; /* valid */
		  default:
		    return( 0 );
		}
	    }
	}

	return( 1 );
}

static char *
find_star( char *s )
{
    for ( ; *s; ++s ) {
	switch ( *s ) {
	  case '*': return s;
	  case '\\':
	    ++s;
	    if ( hexchar2int(s[0]) >= 0 && hexchar2int(s[1]) >= 0 ) ++s;
	  default: break;
	}
    }
    return NULL;
}

static int
put_simple_filter( BerElement *ber, char *str )
{
	char		*s, *s2, *s3, filterop;
	char		*value;
	unsigned long	ftype;
	int		rc, len;
	char		*oid;	/* for v3 extended filter */
	int		dnattr;	/* for v3 extended filter */

	LDAPDebug( LDAP_DEBUG_TRACE, "put_simple_filter \"%s\"\n", str, 0, 0 );

	rc = -1;	/* pessimistic */

	if (( str = nsldapi_strdup( str )) == NULL ) {
		return( rc );
	}

	if ( (s = strchr( str, '=' )) == NULL ) {
		goto free_and_return;
	}
	value = s + 1;
	*s-- = '\0';
	filterop = *s;
	if ( filterop == '<' || filterop == '>' || filterop == '~' ||
	    filterop == ':' ) {
		*s = '\0';
	}

	if ( ! is_valid_attr( str ) ) {
		goto free_and_return;
	}

	switch ( filterop ) {
	case '<':
		ftype = LDAP_FILTER_LE;
		break;
	case '>':
		ftype = LDAP_FILTER_GE;
		break;
	case '~':
		ftype = LDAP_FILTER_APPROX;
		break;
	case ':':	/* extended filter - v3 only */
		/*
		 * extended filter looks like this:
		 *
		 *	[type][':dn'][':'oid]':='value
		 *
		 * where one of type or :oid is required.
		 *
		 */
		ftype = LDAP_FILTER_EXTENDED;
		s2 = s3 = NULL;
		if ( (s2 = strrchr( str, ':' )) == NULL ) {
			goto free_and_return;
		}
		if ( strcasecmp( s2, ":dn" ) == 0 ) {
			oid = NULL;
			dnattr = 1;
			*s2 = '\0';
		} else {
			oid = s2 + 1;
			dnattr = 0;
			*s2 = '\0';
			if ( (s3 = strrchr( str, ':' )) != NULL ) {
				if ( strcasecmp( s3, ":dn" ) == 0 ) {
					dnattr = 1;
				} else {
					goto free_and_return;
				}
				*s3 = '\0';
			}
		}
		if ( (rc = ber_printf( ber, "t{", ftype )) == -1 ) {
			goto free_and_return;
		}
		if ( oid != NULL ) {
			if ( (rc = ber_printf( ber, "ts", LDAP_TAG_MRA_OID,
			    oid )) == -1 ) {
				goto free_and_return;
			}
		}
		if ( *str != '\0' ) {
			if ( (rc = ber_printf( ber, "ts",
			    LDAP_TAG_MRA_TYPE, str )) == -1 ) {
				goto free_and_return;
			}
		}
		if (( len = unescape_filterval( value )) < 0 ||
		    ( rc = ber_printf( ber, "totb}", LDAP_TAG_MRA_VALUE,
		    value, len, LDAP_TAG_MRA_DNATTRS, dnattr )) == -1 ) {
			goto free_and_return;
		}
		rc = 0;
		goto free_and_return;
		break;
	default:
		if ( find_star( value ) == NULL ) {
			ftype = LDAP_FILTER_EQUALITY;
		} else if ( strcmp( value, "*" ) == 0 ) {
			ftype = LDAP_FILTER_PRESENT;
		} else {
			rc = put_substring_filter( ber, str, value );
			goto free_and_return;
		}
		break;
	}

	if ( ftype == LDAP_FILTER_PRESENT ) {
		rc = ber_printf( ber, "ts", ftype, str );
	} else if (( len = unescape_filterval( value )) >= 0 ) {
		rc = ber_printf( ber, "t{so}", ftype, str, value, len );
	}
	if ( rc != -1 ) {
		rc = 0;
	}

free_and_return:
	NSLDAPI_FREE( str );
	return( rc );
}


/*
 * Undo in place both LDAPv2 (RFC-1960) and LDAPv3 (hexadecimal) escape
 * sequences within the null-terminated string 'val'.  The resulting value
 * may contain null characters.
 *
 * If 'val' contains invalid escape sequences we return -1.
 * Otherwise the length of the unescaped value is returned.
 */
static int
unescape_filterval( char *val )
{
	int	escape, firstdigit, ival; 
	char	*s, *d;

	escape = firstdigit = 0;
	for ( s = d = val; *s; s++ ) {
		if ( escape ) {
			/*
			 * first try LDAPv3 escape (hexadecimal) sequence
			 */
			if (( ival = hexchar2int( *s )) < 0 ) {
				if ( firstdigit ) {
					/*
					 * LDAPv2 (RFC1960) escape sequence
					 */
					*d++ = *s;
					escape = 0;
				} else {
					return(-1);
				}
			}
			if ( firstdigit ) {
			    *d = ( ival<<4 );
			    firstdigit = 0;
			} else {
			    *d++ |= ival;
			    escape = 0;
			}

		} else if ( *s != '\\' ) {
			*d++ = *s;
			escape = 0;

		} else {
			escape = 1;
			firstdigit = 1;
		}
	}

	return( d - val );
}


/*
 * convert character 'c' that represents a hexadecimal digit to an integer.
 * if 'c' is not a hexidecimal digit [0-9A-Fa-f], -1 is returned.
 * otherwise the converted value is returned.
 */
static int
hexchar2int( char c )
{
    if ( c >= '0' && c <= '9' ) {
	return( c - '0' );
    }
    if ( c >= 'A' && c <= 'F' ) {
	return( c - 'A' + 10 );
    }
    if ( c >= 'a' && c <= 'f' ) {
	return( c - 'a' + 10 );
    }
    return( -1 );
}

static int
put_substring_filter( BerElement *ber, char *type, char *val )
{
	char		*nextstar, gotstar = 0;
	unsigned long	ftype;
	int		len;

	LDAPDebug( LDAP_DEBUG_TRACE, "put_substring_filter \"%s=%s\"\n", type,
	    val, 0 );

	if ( ber_printf( ber, "t{s{", LDAP_FILTER_SUBSTRINGS, type ) == -1 ) {
		return( -1 );
	}

	for ( ; val != NULL; val = nextstar ) {
		if ( (nextstar = find_star( val )) != NULL ) {
			*nextstar++ = '\0';
		}

		if ( gotstar == 0 ) {
			ftype = LDAP_SUBSTRING_INITIAL;
		} else if ( nextstar == NULL ) {
			ftype = LDAP_SUBSTRING_FINAL;
		} else {
			ftype = LDAP_SUBSTRING_ANY;
		}
		if ( *val != '\0' ) {
			if (( len = unescape_filterval( val )) < 0 ||
			    ber_printf( ber, "to", ftype, val, len ) == -1 ) {
				return( -1 );
			}
		}

		gotstar = 1;
	}

	if ( ber_printf( ber, "}}" ) == -1 ) {
		return( -1 );
	}

	return( 0 );
}

int
LDAP_CALL
ldap_search_st(
    LDAP		*ld, 
    const char 		*base, 
    int 		scope, 
    const char 		*filter, 
    char 		**attrs,
    int 		attrsonly, 
    struct timeval	*timeout, 
    LDAPMessage 	**res
)
{
	return( nsldapi_search_s( ld, base, scope, filter, attrs, attrsonly,
	    NULL, NULL, timeout, -1, -1, res ));
}

int
LDAP_CALL
ldap_search_s(
    LDAP	*ld, 
    const char 	*base, 
    int 	scope, 
    const char 	*filter, 
    char 	**attrs,
    int		attrsonly, 
    LDAPMessage	**res
)
{
	return( nsldapi_search_s( ld, base, scope, filter, attrs, attrsonly,
	    NULL, NULL, NULL, -1, -1, res ));
}

int LDAP_CALL
ldap_search_ext_s(
    LDAP		*ld, 
    const char 		*base, 
    int 		scope, 
    const char 		*filter, 
    char 		**attrs,
    int			attrsonly, 
    LDAPControl		**serverctrls,
    LDAPControl		**clientctrls,
    struct timeval	*timeoutp,
    int			sizelimit,
    LDAPMessage		**res
)
{
	return( nsldapi_search_s( ld, base, scope, filter, attrs, attrsonly,
	    serverctrls, clientctrls, timeoutp,
	    nsldapi_timeval2ldaplimit( timeoutp, -1 ), sizelimit, res ));
}


static int 
nsldapi_search_s(
    LDAP		*ld, 
    const char 		*base, 
    int 		scope, 
    const char 		*filter, 
    char 		**attrs,
    int			attrsonly, 
    LDAPControl		**serverctrls,
    LDAPControl		**clientctrls,
    struct timeval	*localtimeoutp,
    int			timelimit,	/* -1 means use ld->ld_timelimit */
    int			sizelimit,	/* -1 means use ld->ld_sizelimit */
    LDAPMessage		**res
)
{
	int	err, msgid;

	/*
	 * It is an error to pass in a zero'd timeval.
	 */
	if ( localtimeoutp != NULL && localtimeoutp->tv_sec == 0 &&
	    localtimeoutp->tv_usec == 0 ) {
		if ( ld != NULL ) {
			LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		}
		if ( res != NULL ) {
			*res = NULL;
		}
                return( LDAP_PARAM_ERROR );
        }

	if (( err = nsldapi_search( ld, base, scope, filter, attrs, attrsonly,
	    serverctrls, clientctrls, timelimit, sizelimit, &msgid ))
	    != LDAP_SUCCESS ) {
		if ( res != NULL ) {
			*res = NULL;
		}
		return( err );
	}

	if ( ldap_result( ld, msgid, 1, localtimeoutp, res ) == -1 ) {
		/*
		 * Error.  ldap_result() sets *res to NULL for us.
		 */
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
	}

	if ( LDAP_GET_LDERRNO( ld, NULL, NULL ) == LDAP_TIMEOUT ) {
		(void) ldap_abandon( ld, msgid );
		err = LDAP_TIMEOUT;
		LDAP_SET_LDERRNO( ld, err, NULL, NULL );
		if ( res != NULL ) {
			*res = NULL;
		}
		return( err );
	}

	return( ldap_result2error( ld, *res, 0 ) );
}
