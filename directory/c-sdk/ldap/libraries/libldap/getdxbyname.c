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
 *  Copyright (c) 1995 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 * nsldapi_getdxbyname - retrieve DX records from the DNS (from 
 * TXT records for now)
 */

#include <stdio.h>

#ifdef LDAP_DNS

XXX not MT-safe XXX

#include <string.h>
#include <ctype.h>

#ifdef macintosh
#include <stdlib.h>
#include "macos.h"
#endif /* macintosh */

#ifdef _WINDOWS
#include <windows.h>
#endif

#if !defined(macintosh) && !defined(DOS) && !defined( _WINDOWS )
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <resolv.h>
#endif
#include "ldap-int.h"

#if defined( DOS ) 
#include "msdos.h"
#endif /* DOS */


#ifdef NEEDPROTOS
static char ** decode_answer( unsigned char *answer, int len );
#else /* NEEDPROTOS */
static char **decode_answer();
#endif /* NEEDPROTOS */

extern int h_errno;
extern char *h_errlist[];


#define MAX_TO_SORT	32


/*
 * nsldapi_getdxbyname - lookup DNS DX records for domain and return an ordered
 *	array.
 */
char **
nsldapi_getdxbyname( char *domain )
{
    unsigned char	buf[ PACKETSZ ];
    char		**dxs;
    int			rc;

    LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_getdxbyname( %s )\n", domain, 0, 0 );

    memset( buf, 0, sizeof( buf ));

    /* XXX not MT safe XXX */
    if (( rc = res_search( domain, C_IN, T_TXT, buf, sizeof( buf ))) < 0
		|| ( rc > sizeof( buf ))
		|| ( dxs = decode_answer( buf, rc )) == NULL ) {
	/*
	 * punt:  return list consisting of the original domain name only
	 */
	if (( dxs = (char **)NSLDAPI_MALLOC( 2 * sizeof( char * ))) == NULL ||
		( dxs[ 0 ] = nsldapi_strdup( domain )) == NULL ) {
	    if ( dxs != NULL ) {
		NSLDAPI_FREE( dxs );
	    }
	    dxs = NULL;
	} else {
	    dxs[ 1 ] = NULL;
	}
    }

    return( dxs );
}


static char **
decode_answer( unsigned char *answer, int len )
{
    HEADER		*hp;
    char		buf[ 256 ], **dxs;
    unsigned char	*eom, *p;
    int			ancount, err, rc, type, class, dx_count, rr_len;
    int			dx_pref[ MAX_TO_SORT ];

#ifdef LDAP_DEBUG
    if ( ldap_debug & LDAP_DEBUG_PACKETS ) {
/*	__p_query( answer );	*/
    }
#endif /* LDAP_DEBUG */

    dxs = NULL;
    hp = (HEADER *)answer;
    eom = answer + len;

    if ( len < sizeof( *hp ) ) {
	h_errno = NO_RECOVERY;
	return( NULL );
    }

    if ( ntohs( hp->qdcount ) != 1 ) {
	h_errno = NO_RECOVERY;
	return( NULL );
    }

    ancount = ntohs( hp->ancount );
    if ( ancount < 1 ) {
	h_errno = NO_DATA;
	return( NULL );
    }

    /*
     * skip over the query
     */
    p = answer + HFIXEDSZ;
    if (( rc = dn_expand( answer, eom, p, buf, sizeof( buf ))) < 0 ) {
	h_errno = NO_RECOVERY;
	return( NULL );
    }
    p += ( rc + QFIXEDSZ );

    /*
     * pull out the answers we are interested in
     */
    err = dx_count = 0;
    while ( ancount > 0 && err == 0 && p < eom ) {
	if (( rc = dn_expand( answer, eom, p, buf, sizeof( buf ))) < 0 ) {
	    err = NO_RECOVERY;
	    continue;
	}
	p += rc;	/* skip over name */
	if ( p + 3 * INT16SZ + INT32SZ > eom ) {
	    err = NO_RECOVERY;
	    continue;
	}
	type = _getshort( p );
	p += INT16SZ;
	class = _getshort( p );
	p += INT16SZ;
	p += INT32SZ;		/* skip over TTL */
	rr_len = _getshort( p );
	p += INT16SZ;
	if ( p + rr_len > eom ) {
	    err = NO_RECOVERY;
	    continue;
	}
	if ( class == C_IN && type == T_TXT ) {
	    int 	i, n, pref, txt_len;
	    char	*q, *r;

	    q = (char *)p;
	    while ( q < (char *)p + rr_len && err == 0 ) {
		if ( *q >= 3 && strncasecmp( q + 1, "dx:", 3 ) == 0 ) {
		    txt_len = *q - 3;
		    r = q + 4;
		    while ( isspace( *r )) { 
			++r;
			--txt_len;
		    }
		    pref = 0;
		    while ( isdigit( *r )) {
			pref *= 10;
			pref += ( *r - '0' );
			++r;
			--txt_len;
		    }
		    if ( dx_count < MAX_TO_SORT - 1 ) {
			dx_pref[ dx_count ] = pref;
		    }
		    while ( isspace( *r )) { 
			++r;
			--txt_len;
		    }
		    if ( dx_count == 0 ) {
			dxs = (char **)NSLDAPI_MALLOC( 2 * sizeof( char * ));
		    } else {
			dxs = (char **)NSLDAPI_REALLOC( dxs,
				( dx_count + 2 ) * sizeof( char * ));
		    }
		    if ( dxs == NULL || ( dxs[ dx_count ] =
			    (char *)NSLDAPI_CALLOC( 1, txt_len + 1 ))
			    == NULL ) {
			err = NO_RECOVERY;
			continue;
		    }
		    SAFEMEMCPY( dxs[ dx_count ], r, txt_len );
		    dxs[ ++dx_count ] = NULL;
		}
		q += ( *q + 1 );	/* move past last TXT record */
	    }
	}
	p += rr_len;
    }

    if ( err == 0 ) {
	if ( dx_count == 0 ) {
	    err = NO_DATA;
	} else {
	    /*
	     * sort records based on associated preference value
	     */
	    int		i, j, sort_count, tmp_pref;
	    char	*tmp_dx;

	    sort_count = ( dx_count < MAX_TO_SORT ) ? dx_count : MAX_TO_SORT;
	    for ( i = 0; i < sort_count; ++i ) {
		for ( j = i + 1; j < sort_count; ++j ) {
		    if ( dx_pref[ i ] > dx_pref[ j ] ) {
			tmp_pref = dx_pref[ i ];
			dx_pref[ i ] = dx_pref[ j ];
			dx_pref[ j ] = tmp_pref;
			tmp_dx = dxs[ i ];
			dxs[ i ] = dxs[ j ];
			dxs[ j ] = tmp_dx;
		    }
		}
	    }
	}
    }

    h_errno = err;
    return( dxs );
}

#endif /* LDAP_DNS */
