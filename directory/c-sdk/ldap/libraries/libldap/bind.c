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
 *  bind.c
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"

/*
 * ldap_bind - bind to the ldap server. The dn and password
 * of the entry to which to bind are supplied, along with the authentication
 * method to use.  The msgid of the bind request is returned on success,
 * -1 if there's trouble.  Note, the kerberos support assumes the user already
 * has a valid tgt for now.  ldap_result() should be called to find out the
 * outcome of the bind request.
 *
 * Example:
 *	ldap_bind( ld, "cn=manager, o=university of michigan, c=us", "secret",
 *	    LDAP_AUTH_SIMPLE )
 */

int
LDAP_CALL
ldap_bind( LDAP *ld, const char *dn, const char *passwd, int authmethod )
{
	/*
	 * The bind request looks like this:
	 *	BindRequest ::= SEQUENCE {
	 *		version		INTEGER,
	 *		name		DistinguishedName,	 -- who
	 *		authentication	CHOICE {
	 *			simple		[0] OCTET STRING -- passwd
	 *		}
	 *	}
	 * all wrapped up in an LDAPMessage sequence.
	 */

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_bind\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( -1 );
	}

	switch ( authmethod ) {
	case LDAP_AUTH_SIMPLE:
		return( ldap_simple_bind( ld, dn, passwd ) );

	default:
		LDAP_SET_LDERRNO( ld, LDAP_AUTH_UNKNOWN, NULL, NULL );
		return( -1 );
	}
}

/*
 * ldap_bind_s - bind to the ldap server.  The dn and password
 * of the entry to which to bind are supplied, along with the authentication
 * method to use.  This routine just calls whichever bind routine is
 * appropriate and returns the result of the bind (e.g. LDAP_SUCCESS or
 * some other error indication).  Note, the kerberos support assumes the
 * user already has a valid tgt for now.
 *
 * Examples:
 *	ldap_bind_s( ld, "cn=manager, o=university of michigan, c=us",
 *	    "secret", LDAP_AUTH_SIMPLE )
 *	ldap_bind_s( ld, "cn=manager, o=university of michigan, c=us",
 *	    NULL, LDAP_AUTH_KRBV4 )
 */
int
LDAP_CALL
ldap_bind_s( LDAP *ld, const char *dn, const char *passwd, int authmethod )
{
	int	err;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_bind_s\n", 0, 0, 0 );

	switch ( authmethod ) {
	case LDAP_AUTH_SIMPLE:
		return( ldap_simple_bind_s( ld, dn, passwd ) );

	default:
		err = LDAP_AUTH_UNKNOWN;
		LDAP_SET_LDERRNO( ld, err, NULL, NULL );
		return( err );
	}
}


void
LDAP_CALL
ldap_set_rebind_proc( LDAP *ld, LDAP_REBINDPROC_CALLBACK *rebindproc,
    void *arg )
{
	if ( ld == NULL ) {
		if ( !nsldapi_initialized ) {
			nsldapi_initialize_defaults();
		}
		ld = &nsldapi_ld_defaults;
	}

	if ( NSLDAPI_VALID_LDAP_POINTER( ld )) {
		LDAP_MUTEX_LOCK( ld, LDAP_OPTION_LOCK );
		ld->ld_rebind_fn = rebindproc;
		ld->ld_rebind_arg = arg;
		LDAP_MUTEX_UNLOCK( ld, LDAP_OPTION_LOCK );
	}
}


/*
 * return a pointer to the bind DN for the default connection (a copy is
 * not made).  If there is no bind DN available, NULL is returned.
 */
char *
nsldapi_get_binddn( LDAP *ld )
{
	char	*binddn;

	binddn = NULL;	/* default -- assume they are not bound */

	LDAP_MUTEX_LOCK( ld, LDAP_CONN_LOCK );
	if ( NULL != ld->ld_defconn && LDAP_CONNST_CONNECTED ==
	    ld->ld_defconn->lconn_status && ld->ld_defconn->lconn_bound ) {
		if (( binddn = ld->ld_defconn->lconn_binddn ) == NULL ) {
			binddn = "";
		}
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );

	return( binddn );
}
