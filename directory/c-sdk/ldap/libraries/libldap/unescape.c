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
 *  LIBLDAP unescape.c -- LDAP URL un-escape routines
 *  We also tolerate URLs that look like: <ldapurl> and <URL:ldapurl>
 */

#include "ldap-int.h"


static int unhex( char c );


void
nsldapi_hex_unescape( char *s )
{
/*
 * Remove URL hex escapes from s... done in place.  The basic concept for
 * this routine is borrowed from the WWW library HTUnEscape() routine.
 */
	char	*p;

	for ( p = s; *s != '\0'; ++s ) {
		if ( *s == '%' ) {
			if ( *++s != '\0' ) {
				*p = unhex( *s ) << 4;
			}
			if ( *++s != '\0' ) {
				*p++ += unhex( *s );
			}
		} else {
			*p++ = *s;
		}
	}

	*p = '\0';
}


static int
unhex( char c )
{
	return( c >= '0' && c <= '9' ? c - '0'
	    : c >= 'A' && c <= 'F' ? c - 'A' + 10
	    : c - 'a' + 10 );
}
