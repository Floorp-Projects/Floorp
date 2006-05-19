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
 *  Copyright (c) 1994 The Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  compat.c - compatibility routines.
 *
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1994 The Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"

#if defined(LINUX) || defined(AIX) || defined(HPUX) || defined(_WINDOWS)
/* 
* Copies src to the dstsize buffer at dst. The copy will never 
 * overflow the destination buffer and the buffer will always be null 
 * terminated. 
 */   
size_t nsldapi_compat_strlcpy(char *dst, const char *src, size_t len) 
{ 
	size_t slen = strlen(src); 
	size_t copied; 
	
	if (len == 0) 
		return (slen); 
	
	if (slen >= len) 
		copied = len - 1; 
	else 
		copied = slen; 
	SAFEMEMCPY(dst, src, copied); 
	dst[copied] = '\0'; 
	return (slen); 
}
#endif

#ifdef notdef
#if defined( HPUX10 ) && defined( _REENTRANT )
extern int h_errno;

struct hostent *
nsldapi_compat_gethostbyname_r( const char *name, struct hostent *result,
	char *buffer, int buflen, int *h_errnop )
{
    struct hostent_data	*hep;

    if ( buflen < sizeof(struct hostent_data)) {	/* sanity check */
	*h_errnop = NO_RECOVERY;	/* XXX best error code to use? */
	return( NULL );
    }

    hep = (struct hostent_data *)buffer;
    hep->current = NULL;

    if ( gethostbyname_r( name, result, hep ) == -1) {
	*h_errnop = h_errno; /* XXX don't see anywhere else to get this */
	return NULL;
    }
    return result;
}

char *
nsldapi_compat_ctime_r( const time_t *clock, char *buf, int buflen )
{
    NSLDAPI_CTIME1( clock, buf, buflen );
    return buf;
}
#endif /* HPUX10 && _REENTRANT */
#endif
