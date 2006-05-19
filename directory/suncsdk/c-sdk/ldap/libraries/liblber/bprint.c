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

/* bprint.c - a printing utility for debuging output */
#include <string.h>
#include "lber-int.h"

#ifdef LDAP_DEBUG
/*
 * Print arbitrary stuff, for debugging.
 */

#define BPLEN	48

void
lber_bprint( char *data, int len )
{
    static char	hexdig[] = "0123456789abcdef";
    char	out[ BPLEN ];
    int		i = 0;

    memset( out, 0, BPLEN );
    for ( ;; ) {
	if ( len < 1 ) {
		char msg[BPLEN + 80];
	    sprintf( msg, "\t%s\n", ( i == 0 ) ? "(end)" : out );
		ber_err_print( msg );
	    break;
	}

#ifndef HEX
	if ( isgraph( (unsigned char)*data )) {
	    out[ i ] = ' ';
	    out[ i+1 ] = *data;
	} else {
#endif
	    out[ i ] = hexdig[ ( *data & 0xf0 ) >> 4 ];
	    out[ i+1 ] = hexdig[ *data & 0x0f ];
#ifndef HEX
	}
#endif
	i += 2;
	len--;
	data++;

	if ( i > BPLEN - 2 ) {
		char msg[BPLEN + 80];
	    sprintf( msg, "\t%s\n", out );
		ber_err_print( msg );
	    memset( out, 0, BPLEN );
	    i = 0;
	    continue;
	}
	out[ i++ ] = ' ';
    }
}

#endif

void ber_err_print( char *data )
{
#ifdef USE_DEBUG_WIN
	OutputDebugString( data );
#else
	fputs( data, stderr );
	fflush( stderr );
#endif
}
