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
 *  Copyright (c) 1994 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  getdn.c
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"

char *
LDAP_CALL
ldap_get_dn( LDAP *ld, LDAPMessage *entry )
{
	char			*dn;
	struct berelement	tmp;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_get_dn\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( NULL );		/* punt */
	}

	if ( !NSLDAPI_VALID_LDAPMESSAGE_ENTRY_POINTER( entry )) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( NULL );
	}

	tmp = *entry->lm_ber;	/* struct copy */
	if ( ber_scanf( &tmp, "{a", &dn ) == LBER_ERROR ) {
		LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
		return( NULL );
	}

	return( dn );
}

char *
LDAP_CALL
ldap_dn2ufn( const char *dn )
{
	char	*p, *ufn, *r;
	size_t	plen;
	int	state;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_dn2ufn\n", 0, 0, 0 );

	if ( dn == NULL ) {
		dn = "";
	}

	if ( ldap_is_dns_dn( dn ) || ( p = strchr( dn, '=' )) == NULL )
		return( nsldapi_strdup( (char *)dn ));

	ufn = nsldapi_strdup( ++p );

#define INQUOTE		1
#define OUTQUOTE	2
	state = OUTQUOTE;
	for ( p = ufn, r = ufn; *p; p += plen ) {
	    plen = 1;
		switch ( *p ) {
		case '\\':
			if ( *++p == '\0' )
				plen=0;
			else {
				*r++ = '\\';
				r += (plen = LDAP_UTF8COPY(r,p));
			}
			break;
		case '"':
			if ( state == INQUOTE )
				state = OUTQUOTE;
			else
				state = INQUOTE;
			*r++ = *p;
			break;
		case ';':
		case ',':
			if ( state == OUTQUOTE )
				*r++ = ',';
			else
				*r++ = *p;
			break;
		case '=':
			if ( state == INQUOTE )
				*r++ = *p;
			else {
				char	*rsave = r;
				LDAP_UTF8DEC(r);
				*rsave = '\0';
				while ( !ldap_utf8isspace( r ) && *r != ';'
				    && *r != ',' && r > ufn )
					LDAP_UTF8DEC(r);
				LDAP_UTF8INC(r);

				if ( strcasecmp( r, "c" )
				    && strcasecmp( r, "o" )
				    && strcasecmp( r, "ou" )
				    && strcasecmp( r, "st" )
				    && strcasecmp( r, "l" )
				    && strcasecmp( r, "dc" )
				    && strcasecmp( r, "uid" )
				    && strcasecmp( r, "cn" ) ) {
					r = rsave;
					*r++ = '=';
				}
			}
			break;
		default:
			r += (plen = LDAP_UTF8COPY(r,p));
			break;
		}
	}
	*r = '\0';

	return( ufn );
}

char **
LDAP_CALL
ldap_explode_dns( const char *dn )
{
	int	ncomps, maxcomps;
	char	*s, *cpydn;
	char	**rdns;
#ifdef HAVE_STRTOK_R	/* defined in portable.h */
	char	*lasts;
#endif

	if ( dn == NULL ) {
		dn = "";
	}

	if ( (rdns = (char **)NSLDAPI_MALLOC( 8 * sizeof(char *) )) == NULL ) {
		return( NULL );
	}

	maxcomps = 8;
	ncomps = 0;
	cpydn = nsldapi_strdup( (char *)dn );
	for ( s = STRTOK( cpydn, "@.", &lasts ); s != NULL;
	    s = STRTOK( NULL, "@.", &lasts ) ) {
		if ( ncomps == maxcomps ) {
			maxcomps *= 2;
			if ( (rdns = (char **)NSLDAPI_REALLOC( rdns, maxcomps *
			    sizeof(char *) )) == NULL ) {
				NSLDAPI_FREE( cpydn );
				return( NULL );
			}
		}
		rdns[ncomps++] = nsldapi_strdup( s );
	}
	rdns[ncomps] = NULL;
	NSLDAPI_FREE( cpydn );

	return( rdns );
}

#define LDAP_DN		1
#define LDAP_RDN	2

static char **
ldap_explode( const char *dn, const int notypes, const int nametype )
{
	char	*p, *q, *rdnstart, **rdns = NULL;
	size_t	plen = 0;
	int	state, count = 0, endquote, len, goteq;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_explode\n", 0, 0, 0 );

	if ( dn == NULL ) {
		dn = "";
	}

#if 0
	if ( ldap_is_dns_dn( dn ) ) {
		return( ldap_explode_dns( dn ) );
	}
#endif

	while ( ldap_utf8isspace( (char *)dn )) { /* ignore leading spaces */
		++dn;
	}

	p = rdnstart = (char *) dn;
	state = OUTQUOTE;
	goteq = 0;

	do {
		p += plen;
		plen = 1;
		switch ( *p ) {
		case '\\':
			if ( *++p == '\0' )
				p--;
			else
				plen = LDAP_UTF8LEN(p);
			break;
		case '"':
			if ( state == INQUOTE )
				state = OUTQUOTE;
			else
				state = INQUOTE;
			break;
		case '+': if ( nametype != LDAP_RDN ) break;
		case ';':
		case ',':
		case '\0':
			if ( state == OUTQUOTE ) {
				/*
				 * semicolon and comma are not valid RDN
				 * separators.
				 */
				if ( nametype == LDAP_RDN && 
					( *p == ';' || *p == ',' || !goteq)) {
					ldap_charray_free( rdns );
					return NULL;
				}
				if ( (*p == ',' || *p == ';') && !goteq ) {
                                   /* If we get here, we have a case similar
				    * to <attr>=<value>,<string>,<attr>=<value>
				    * This is not a valid dn */
				    ldap_charray_free( rdns );
				    return NULL;
				}
				goteq = 0;
				++count;
				if ( rdns == NULL ) {
					if (( rdns = (char **)NSLDAPI_MALLOC( 8
						 * sizeof( char *))) == NULL )
						return( NULL );
				} else if ( count >= 8 ) {
					if (( rdns = (char **)NSLDAPI_REALLOC(
					    rdns, (count+1) *
					    sizeof( char *))) == NULL )
						return( NULL );
				}
				rdns[ count ] = NULL;
				endquote = 0;
				if ( notypes ) {
					for ( q = rdnstart;
					    q < p && *q != '='; ++q ) {
						;
					}
					if ( q < p ) { /* *q == '=' */
						rdnstart = ++q;
					}
					if ( *rdnstart == '"' ) {
						++rdnstart;
					}
					
					if ( *(p-1) == '"' ) {
						endquote = 1;
						--p;
					}
				}

				len = p - rdnstart;
				if (( rdns[ count-1 ] = (char *)NSLDAPI_CALLOC(
				    1, len + 1 )) != NULL ) {
				    	SAFEMEMCPY( rdns[ count-1 ], rdnstart,
					    len );
					if ( !endquote ) {
						/* trim trailing spaces */
						while ( len > 0 &&
						    ldap_utf8isspace(
						    &rdns[count-1][len-1] )) {
							--len;
						}
					}
					rdns[ count-1 ][ len ] = '\0';
				}

				/*
				 *  Don't forget to increment 'p' back to where
				 *  it should be.  If we don't, then we will
				 *  never get past an "end quote."
				 */
				if ( endquote == 1 )
					p++;

				rdnstart = *p ? p + 1 : p;
				while ( ldap_utf8isspace( rdnstart ))
					++rdnstart;
			}
			break;
		case '=':
			if ( state == OUTQUOTE ) {
				goteq = 1;
			}
			/* FALL */
		default:
			plen = LDAP_UTF8LEN(p);
			break;
		}
	} while ( *p );

	return( rdns );
}

char **
LDAP_CALL
ldap_explode_dn( const char *dn, const int notypes )
{
	return( ldap_explode( dn, notypes, LDAP_DN ) );
}

char **
LDAP_CALL
ldap_explode_rdn( const char *rdn, const int notypes )
{
	return( ldap_explode( rdn, notypes, LDAP_RDN ) );
}

int
LDAP_CALL
ldap_is_dns_dn( const char *dn )
{
	return( dn != NULL && dn[ 0 ] != '\0' && strchr( dn, '=' ) == NULL &&
	    strchr( dn, ',' ) == NULL );
}
