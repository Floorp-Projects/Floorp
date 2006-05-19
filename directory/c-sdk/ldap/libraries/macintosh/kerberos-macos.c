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
 *  Copyright (c) 1992, 1994 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  kerberos-macos.c
 */

#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1994 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif

#include "lber.h"
#include "ldap.h"

#ifdef KERBEROS

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef THINK_C
#include <pascal.h>
#else /* THINK_C */
#include <Strings.h>
#endif /* THINK_C */
#ifdef AUTHMAN
#include <MixedMode.h>
#include <Errors.h>
#include "authLibrary.h"
#include "ldap-int.h"

/*
 * get_kerberosv4_credentials - obtain kerberos v4 credentials for ldap.
 */

/* ARGSUSED */
char *
get_kerberosv4_credentials( LDAP *ld, char *who, char *service, int *len )
{
	static short	authman_refnum = 0;
	char		*cred, ticket[ MAX_KTXT_LEN ];
	short		version, ticketlen, err;
	Str255		svcps, instps;
	
	/*
	 * make sure RJC's Authentication Manager 2.0 or better is available
	 */
	if ( authman_refnum == 0 && (( err = openAuthMan( &authman_refnum, &version )) != noErr || version < 2 )) {
		authman_refnum = 0;
		ld->ld_errno = LDAP_AUTH_UNKNOWN;
		return( NULL );
	}
	
	strcpy( (char *)svcps, service );
	CtoPstr( (char *)svcps );
	strcpy( (char *)instps, ld->ld_defconn->lconn_krbinstance );

	CtoPstr( (char *)instps );
	if (( err = getV4Ticket( authman_refnum, &ticket, &ticketlen, &svcps, &instps,
			NULL, INFINITE_LIFETIME, 1 )) != noErr ) {
		ld->ld_errno = ( err == userCanceledErr ) ?
			LDAP_USER_CANCELLED : LDAP_INVALID_CREDENTIALS;
		return( NULL );
	}

	if (( cred = malloc( ticketlen )) == NULL ) {
		ld->ld_errno = LDAP_NO_MEMORY;
		return( NULL );
	}

	*len = ticketlen;
	memcpy( cred, (char *)ticket, ticketlen );
	return( cred );
}

#endif
#endif
