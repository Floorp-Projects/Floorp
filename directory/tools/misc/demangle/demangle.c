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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1996-2003
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 *	Mark Smith <MarkCSmithWork@aol.com>
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
 * C++ name demangle utility for Win32 MSVC++
 */
#define WIN32_LEAN_AND_MEAN 1
#include <stdio.h>
#include <Windows.h>
#include <ImageHlp.h>
#include "getopt.h"

static int dem( const char *symbol );

int
main( int argc, char *argv[] )
{
	int		i, rc;

	if ( argc < 2 ) {
		fprintf( stderr, "usage: %s name...\n", argv[0] );
		return 2;
	}

	rc = 0;
	for ( i = 1; i < argc; ++i ) {
		rc |= dem( argv[i] );
	}

	return rc;
}

static int
dem( const char *symbol )
{
	int		status;
	char	buf[512];

	status = UnDecorateSymbolName( symbol, buf, sizeof(buf),
				UNDNAME_32_BIT_DECODE | UNDNAME_COMPLETE );
	if ( status ) {
		printf( "%s -> %s\n", symbol, buf );
	} else {
		perror( symbol );
	}

	return status ? 0 : 1;
}
