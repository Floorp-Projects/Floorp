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
 *  Copyright (c) 1990 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  rename.c
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"

/*
 * ldap_rename - initiate an ldap modifyDN operation. Parameters:
 *
 *	ld		LDAP descriptor
 *	dn		DN of the object to modify
 *	newrdn		RDN that will form leftmost component of entry's new name
 *      newparent       if present, this is the Distinguished Name of the entry
 *                      which becomes the immediate parent of the existing entry
 *	deleteoldrdn	nonzero means to delete old rdn values from the entry
 *                      while zero means to retain them as attributes of the entry
 *      serverctrls     list of LDAP server controls
 *      clientctrls     list of client controls
 *      msgidp          this result parameter will be set to the message id of the
 *                      request if the ldap_rename() call succeeds
 *
 * Example:
 *      int rc;
 *	rc = ldap_rename( ld, dn, newrdn, newparent, deleteoldrdn, serverctrls, clientctrls, &msgid );
 */
int
LDAP_CALL
ldap_rename( 
	   LDAP *ld, 
	   const char *dn, 
	   const char *newrdn, 
	   const char *newparent,
	   int deleteoldrdn, 
	   LDAPControl	**serverctrls,
	   LDAPControl	**clientctrls,  /* not used for anything yet */
	   int *msgidp
)
{
	BerElement	*ber;
	int		rc, err;

	/*
	 * A modify dn request looks like this:
	 *	ModifyDNRequest ::= SEQUENCE {
	 *		entry		LDAPDN,
	 *		newrdn		RelativeLDAPDN,
	 *              newparent       [0] LDAPDN OPTIONAL,
	 *		deleteoldrdn	BOOLEAN
	 *	}
	 */

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_rename\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}
	if ( NULL == newrdn) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( LDAP_PARAM_ERROR );
	}

	/* only ldapv3 or higher can do a proper rename
	 * (i.e. with non-NULL newparent and/or controls)
	 */

	if (( NSLDAPI_LDAP_VERSION( ld ) < LDAP_VERSION3 )
	    && ((newparent != NULL) || (serverctrls != NULL)
	    || (clientctrls != NULL))) {
		LDAP_SET_LDERRNO( ld, LDAP_NOT_SUPPORTED, NULL, NULL );
		return( LDAP_NOT_SUPPORTED );
	}

	if ( msgidp == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
                return( LDAP_PARAM_ERROR );
        }

	LDAP_MUTEX_LOCK( ld, LDAP_MSGID_LOCK );
	*msgidp = ++ld->ld_msgid;
        LDAP_MUTEX_UNLOCK( ld, LDAP_MSGID_LOCK );

	/* see if modRDN or modDN is handled by the cache */
 	if ( ld->ld_cache_on ) {
		if ( newparent == NULL && ld->ld_cache_modrdn != NULL ) {
			LDAP_MUTEX_LOCK( ld, LDAP_CACHE_LOCK );
			if ( (rc = (ld->ld_cache_modrdn)( ld, *msgidp,
			    LDAP_REQ_MODRDN, dn, newrdn, deleteoldrdn ))
			    != 0 ) {   
				*msgidp = rc;
				LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
				return( LDAP_SUCCESS );
			}
			LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
#if 0
		} else if ( ld->ld_cache_rename != NULL ) {
			LDAP_MUTEX_LOCK( ld, LDAP_CACHE_LOCK );
			if ( (rc = (ld->ld_cache_rename)( ld, *msgidp,
			    LDAP_REQ_MODDN, dn, newrdn, newparent,
			    deleteoldrdn )) != 0 ) {   
				*msgidp = rc;
				return( LDAP_SUCCESS );
			}
			LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
#endif
		}
	}

	/* create a message to send */
	if (( err = nsldapi_alloc_ber_with_options( ld, &ber ))
	    != LDAP_SUCCESS ) {
		return( err );
	}

	/* fill it in */
	if ( ber_printf( ber, "{it{ssb", *msgidp, LDAP_REQ_MODDN, dn,
	    newrdn, deleteoldrdn ) == -1 ) {
		LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
		ber_free( ber, 1 );
		return( LDAP_ENCODING_ERROR );
	}

	if ( newparent == NULL ) {
		if ( ber_printf( ber, "}" ) == -1 ) {
			LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
			ber_free( ber, 1 );
			return( LDAP_ENCODING_ERROR );
		}
	} else {
		if ( ber_printf( ber, "ts}", LDAP_TAG_NEWSUPERIOR, newparent )
		    == -1 ) {
			LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
			ber_free( ber, 1 );
			return( LDAP_ENCODING_ERROR );
		}
	}

	if (( rc = nsldapi_put_controls( ld, serverctrls, 1, ber ))
	    != LDAP_SUCCESS ) {
		ber_free( ber, 1 );
		return( rc );
	}

	/* send the message */
	rc = nsldapi_send_initial_request( ld, *msgidp, LDAP_REQ_MODDN,
		(char *) dn, ber );
	*msgidp = rc;
	return( rc < 0 ? LDAP_GET_LDERRNO( ld, NULL, NULL ) : LDAP_SUCCESS );
}

int
LDAP_CALL
ldap_modrdn2( LDAP *ld, const char *dn, const char *newrdn, int deleteoldrdn )
{
	int             msgid;

	if ( ldap_rename( ld, dn, newrdn, NULL, deleteoldrdn, NULL, NULL, &msgid ) == LDAP_SUCCESS ) {
		return( msgid );
	} else {
		return( -1 );	/* error is in ld handle */
	}
}

int
LDAP_CALL
ldap_modrdn( LDAP *ld, const char *dn, const char *newrdn )
{
	return( ldap_modrdn2( ld, dn, newrdn, 1 ) );
}

int
LDAP_CALL
ldap_rename_s( 
	   LDAP *ld, 
	   const char *dn, 
	   const char *newrdn, 
	   const char *newparent,
	   int deleteoldrdn, 
	   LDAPControl	**serverctrls,
	   LDAPControl	**clientctrls  /* not used for anything yet */
)
{
	int		msgid;
	LDAPMessage	*res;

	if ( ldap_rename( ld, dn, newrdn, newparent, deleteoldrdn, serverctrls, clientctrls, &msgid ) != LDAP_SUCCESS ) {
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
	}

 	if ( msgid == -1 ) 
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );

	if ( ldap_result( ld, msgid, 1, (struct timeval *) NULL, &res ) == -1 )
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );

	return( ldap_result2error( ld, res, 1 ) );
}

int
LDAP_CALL
ldap_modrdn2_s( LDAP *ld, const char *dn, const char *newrdn, int deleteoldrdn )
{
        int             msgid;
        LDAPMessage     *res;
 
        if ( (msgid = ldap_modrdn2( ld, dn, newrdn, deleteoldrdn )) == -1 )
                return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
 
        if ( ldap_result( ld, msgid, 1, (struct timeval *) NULL, &res ) == -1 )
                return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
 
        return( ldap_result2error( ld, res, 1 ) );
}

int
LDAP_CALL
ldap_modrdn_s( LDAP *ld, const char *dn, const char *newrdn )
{
	return( ldap_modrdn2_s( ld, dn, newrdn, 1 ) );
}
