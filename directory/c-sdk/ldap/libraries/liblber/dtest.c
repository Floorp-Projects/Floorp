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
 * Copyright (c) 1990 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */
/* dtest.c - lber decoding test program */

#include <stdio.h>
#include <string.h>
#ifdef MACOS
#include <stdlib.h>
#include <console.h>
#else /* MACOS */
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif /* _WIN32 */
#endif /* MACOS */
#include "lber.h"

int
SSL_Recv( int s, char *b, unsigned l, int dummy )
{
	return( read( s, b, l ) );
}

SSL_Send( int s, char *b, unsigned l, int dummy )
{
	return( write( s, b, l ) );
}

static void usage( char *name )
{
	fprintf( stderr, "usage: %s < berfile\n", name );
}

main( int argc, char **argv )
{
	long		i, fd;
	ber_len_t	len;
	ber_tag_t	tag;
	BerElement	*ber;
	Sockbuf		*sb;
	extern int	lber_debug;

	lber_debug = 255;
	if ( argc > 1 ) {
		usage( argv[0] );
		exit( 1 );
	}

	sb = ber_sockbuf_alloc();
	fd = 0;
	ber_sockbuf_set_option( sb, LBER_SOCKBUF_OPT_DESC, &fd );

	if ( (ber = der_alloc()) == NULL ) {
		perror( "ber_alloc" );
		exit( 1 );
	}

	if ( (tag = ber_get_next( sb, &len, ber )) == LBER_ERROR ) {
		perror( "ber_get_next" );
		exit( 1 );
	}
	printf( "message has tag 0x%x and length %ld\n", tag, len );

	return( 0 );
}
