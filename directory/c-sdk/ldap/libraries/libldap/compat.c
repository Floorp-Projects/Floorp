/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
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

#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1994 The Regents of the University of Michigan.\nAll rights reserved.\n";
#endif

#include "ldap-int.h"

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
    (void) ctime_r( clock, buf, buflen );
    return buf;
}
#endif /* HPUX10 && _REENTRANT */
