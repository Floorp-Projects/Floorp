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
