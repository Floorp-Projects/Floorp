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
 * Copyright (c) 1993, 1994 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */
/*
 * dsparse.c:  parsing routines used by display template and search 
 * preference file library routines for LDAP clients.
 *
 */

#include "ldap-int.h"

static int next_line( char **bufp, long *blenp, char **linep );
static char *next_token( char ** sp );

int
nsldapi_next_line_tokens( char **bufp, long *blenp, char ***toksp )
{
    char	*p, *line, *token, **toks;
    int		rc, tokcnt;

    *toksp = NULL;

    if (( rc = next_line( bufp, blenp, &line )) <= 0 ) {
	return( rc );
    }

    if (( toks = (char **)NSLDAPI_CALLOC( 1, sizeof( char * ))) == NULL ) {
	NSLDAPI_FREE( line );
	return( -1 );
    }
    tokcnt = 0;

    p = line;
    while (( token = next_token( &p )) != NULL ) {
	if (( toks = (char **)NSLDAPI_REALLOC( toks, ( tokcnt + 2 ) *
		sizeof( char * ))) == NULL ) {
	    NSLDAPI_FREE( (char *)toks );
	    NSLDAPI_FREE( line );
	    return( -1 );
	}
	toks[ tokcnt ] = token;
	toks[ ++tokcnt ] = NULL;
    }

    if ( tokcnt == 1 && strcasecmp( toks[ 0 ], "END" ) == 0 ) {
	tokcnt = 0;
	nsldapi_free_strarray( toks );
	toks = NULL;
    }

    NSLDAPI_FREE( line );

    if ( tokcnt == 0 ) {
	if ( toks != NULL ) {
	    NSLDAPI_FREE( (char *)toks );
	}
    } else {
	*toksp = toks;
    }

    return( tokcnt );
}


static int
next_line( char **bufp, long *blenp, char **linep )
{
    char	*linestart, *line, *p;
    long	plen;

    linestart = *bufp;
    p = *bufp;
    plen = *blenp;

    do {
	for ( linestart = p; plen > 0; ++p, --plen ) {
	    if ( *p == '\r' ) {
		if ( plen > 1 && *(p+1) == '\n' ) {
		    ++p;
		    --plen;
		}
		break;
	    }

	    if ( *p == '\n' ) {
		if ( plen > 1 && *(p+1) == '\r' ) {
		    ++p;
		    --plen;
		}
		break;
	    }
	}
	++p;
	--plen;
    } while ( plen > 0 && ( *linestart == '#' || linestart + 1 == p ));


    *bufp = p;
    *blenp = plen;


    if ( plen <= 0 ) {
	*linep = NULL;
	return( 0 );	/* end of file */
    }

    if (( line = NSLDAPI_MALLOC( p - linestart )) == NULL ) {
	*linep = NULL;
	return( -1 );	/* fatal error */
    }

    SAFEMEMCPY( line, linestart, p - linestart );
    line[ p - linestart - 1 ] = '\0';
    *linep = line;
    return( strlen( line ));
}


static char *
next_token( char **sp )
{
    int		in_quote = 0;
    char	*p, *tokstart, *t;

    if ( **sp == '\0' ) {
	return( NULL );
    }

    p = *sp;

    while ( ldap_utf8isspace( p )) {		/* skip leading white space */
	++p;
    }

    if ( *p == '\0' ) {
	return( NULL );
    }

    if ( *p == '\"' ) {
	in_quote = 1;
	++p;
    }
    t = tokstart = p;

    for ( ;; ) {
	if ( *p == '\0' || ( ldap_utf8isspace( p ) && !in_quote )) {
	    if ( *p != '\0' ) {
		++p;
	    }
	    *t++ = '\0';		/* end of token */
	    break;
	}

	if ( *p == '\"' ) {
	    in_quote = !in_quote;
	    ++p;
	} else {
	    *t++ = *p++;
	}
    }

    *sp = p;

    if ( t == tokstart ) {
	return( NULL );
    }

    return( nsldapi_strdup( tokstart ));
}


void
nsldapi_free_strarray( char **sap )
{
    int		i;

    if ( sap != NULL ) {
	for ( i = 0; sap[ i ] != NULL; ++i ) {
	    NSLDAPI_FREE( sap[ i ] );
	}
	NSLDAPI_FREE( (char *)sap );
    }
}
