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
 * Public interface for libprldap -- use NSPR (Netscape Portable Runtime)
 * I/O, threads, etc. with libldap.
 *
 */

#include "ldappr-int.h"
#include <private/pprio.h>

/*
 * Function: prldap_init().
 *
 * Create a new LDAP session handle, but with NSPR I/O, threading, and DNS
 * functions installed.
 *
 * Pass a non-zero value for the 'shared' parameter if you plan to use
 * this LDAP * handle from more than one thread.
 *
 * prldap_init() returns an LDAP session handle (or NULL if an error occurs).
 */
LDAP * LDAP_CALL
prldap_init( const char *defhost, int defport, int shared )
{
    LDAP	*ld;

    if (( ld = ldap_init( defhost, defport )) != NULL ) {
	if ( prldap_install_routines( ld, shared ) != LDAP_SUCCESS ) {
	    prldap_set_system_errno( EINVAL );	/* XXXmcs: just a guess! */
	    ldap_unbind( ld );
	    ld = NULL;
	}
    }

    return( ld );
}


/*
 * Function: prldap_install_routines().
 *
 * Install NSPR I/O, threading, and DNS functions so they will be used by
 * 'ld'.
 *
 * If 'ld' is NULL, the functions are installed as the default functions
 * for all new LDAP * handles).
 *
 * Pass a non-zero value for the 'shared' parameter if you plan to use
 * this LDAP * handle from more than one thread.
 *
 * prldap_install_routines() returns an LDAP API error code (LDAP_SUCCESS
 * if all goes well).
 */
int LDAP_CALL
prldap_install_routines( LDAP *ld, int shared )
{

    if ( prldap_install_io_functions( ld, shared ) != 0
		|| prldap_install_thread_functions( ld, shared ) != 0
		|| prldap_install_dns_functions( ld ) != 0 ) {
	return( ldap_get_lderrno( ld, NULL, NULL ));
    }

    return( LDAP_SUCCESS );
}


/*
 * Function: prldap_set_session_option().
 * 
 * Given an LDAP session handle or a session argument such is passed to
 * SOCKET, POLL, NEWHANDLE, or DISPOSEHANDLE extended I/O callbacks, set
 * an option that affects the prldap layer.
 * 
 * If 'ld' and 'session" are both NULL, the option is set as the default
 * for all new prldap sessions. 
 * 
 * Returns an LDAP API error code (LDAP_SUCCESS if all goes well).
 */
int LDAP_CALL
prldap_set_session_option( LDAP *ld, void *sessionarg, int option, ... )
{
    int				rc = LDAP_SUCCESS;	/* optimistic */
    PRLDAPIOSessionArg		*prsessp = NULL;
    va_list			ap;

    if ( NULL != ld ) {
	if ( LDAP_SUCCESS !=
		( rc = prldap_session_arg_from_ld( ld, &prsessp ))) {
	    return( rc );
	}
    } else if ( NULL != sessionarg ) {
	prsessp = (PRLDAPIOSessionArg *)sessionarg;
    }

    va_start( ap, option );
    switch ( option ) {
    case PRLDAP_OPT_IO_MAX_TIMEOUT:
	rc = prldap_set_io_max_timeout( prsessp, va_arg( ap, int ));
	break;
    default:
	rc = LDAP_PARAM_ERROR;
    }
    va_end( ap );

    return( rc );
}


/*
 * Function: prldap_get_session_option().
 *
 * Given an LDAP session handle or a session argument such is passed to
 * SOCKET, POLL, NEWHANDLE, or DISPOSEHANDLE extended I/O callbacks, retrieve
 * the setting for an option that affects the prldap layer.
 *
 * If 'ld' and 'session" are both NULL, the default option value for all new
 * new prldap sessions is retrieved.
 *
 * Returns an LDAP API error code (LDAP_SUCCESS if all goes well).
 */
int LDAP_CALL prldap_get_session_option( LDAP *ld, void *sessionarg,
        int option, ... )
{
    int				rc = LDAP_SUCCESS;	/* optimistic */
    PRLDAPIOSessionArg		*prsessp = NULL;
    va_list			ap;

    if ( NULL != ld ) {
	if ( LDAP_SUCCESS !=
		( rc = prldap_session_arg_from_ld( ld, &prsessp ))) {
	    return( rc );
	}
    } else if ( NULL != sessionarg ) {
	prsessp = (PRLDAPIOSessionArg *)sessionarg;
    }

    va_start( ap, option );
    switch ( option ) {
    case PRLDAP_OPT_IO_MAX_TIMEOUT:
	rc = prldap_get_io_max_timeout( prsessp, va_arg( ap, int * ));
	break;
    default:
	rc = LDAP_PARAM_ERROR;
    }
    va_end( ap );

    return( rc );
}


/*
 * Function: prldap_set_session_info().
 *
 * Given an LDAP session handle, set some application-specific data.
 *
 * Returns an LDAP API error code (LDAP_SUCCESS if all goes well).
 */
int LDAP_CALL
prldap_set_session_info( LDAP *ld, void *sessionarg, PRLDAPSessionInfo *seip )
{
    int				rc;
    PRLDAPIOSessionArg		*prsessp;

    if ( seip == NULL || PRLDAP_SESSIONINFO_SIZE != seip->seinfo_size ) {
	ldap_set_lderrno( ld, LDAP_PARAM_ERROR, NULL, NULL );
	return( LDAP_PARAM_ERROR );
    }

    if ( NULL != ld ) {
	if ( LDAP_SUCCESS !=
		( rc = prldap_session_arg_from_ld( ld, &prsessp ))) {
	    return( rc );
	}
    } else if ( NULL != sessionarg ) {
	prsessp = (PRLDAPIOSessionArg *)sessionarg;
    } else {
	ldap_set_lderrno( ld, LDAP_PARAM_ERROR, NULL, NULL );
	return( LDAP_PARAM_ERROR );
    }

    prsessp->prsess_appdata = seip->seinfo_appdata;
    return( LDAP_SUCCESS );
}


/*
 * Function: prldap_get_session_info().
 *
 * Given an LDAP session handle, retrieve some application-specific data.
 *
 * Returns an LDAP API error code (LDAP_SUCCESS if all goes well, in
 * which case the fields in the structure that seip points to are filled in).
 */
int LDAP_CALL
prldap_get_session_info( LDAP *ld, void *sessionarg, PRLDAPSessionInfo *seip )
{
    int				rc;
    PRLDAPIOSessionArg		*prsessp;

    if ( seip == NULL || PRLDAP_SESSIONINFO_SIZE != seip->seinfo_size ) {
	ldap_set_lderrno( ld, LDAP_PARAM_ERROR, NULL, NULL );
	return( LDAP_PARAM_ERROR );
    }

    if ( NULL != ld ) {
	if ( LDAP_SUCCESS !=
		( rc = prldap_session_arg_from_ld( ld, &prsessp ))) {
	    return( rc );
	}
    } else if ( NULL != sessionarg ) {
	prsessp = (PRLDAPIOSessionArg *)sessionarg;
    } else {
	ldap_set_lderrno( ld, LDAP_PARAM_ERROR, NULL, NULL );
	return( LDAP_PARAM_ERROR );
    }

    seip->seinfo_appdata = prsessp->prsess_appdata;
    return( LDAP_SUCCESS );
}


/*
 * Function: prldap_set_socket_info().
 *
 * Given an integer fd and a void * argument such as those passed to the
 * extended I/O callback functions, set socket specific information.
 *
 * Returns an LDAP API error code (LDAP_SUCCESS if all goes well).
 *
 * Note: it is only safe to change soinfo_prfd from within the SOCKET
 * extended I/O callback function.
 */
int LDAP_CALL
prldap_set_socket_info( int fd, void *socketarg, PRLDAPSocketInfo *soip )
{
    PRLDAPIOSocketArg	*prsockp;

    if ( NULL == socketarg || NULL == soip ||
		PRLDAP_SOCKETINFO_SIZE != soip->soinfo_size ) {
	return( LDAP_PARAM_ERROR );
    }

    prsockp = (PRLDAPIOSocketArg *)socketarg;
    prsockp->prsock_prfd = soip->soinfo_prfd;
    prsockp->prsock_appdata = soip->soinfo_appdata;

    return( LDAP_SUCCESS );
}


/*
 * Function: prldap_get_socket_info().
 *
 * Given an integer fd and a void * argument such as those passed to the
 * extended I/O callback functions, retrieve socket specific information.
 *
 * Returns an LDAP API error code (LDAP_SUCCESS if all goes well, in
 * which case the fields in the structure that soip points to are filled in).
 */
int LDAP_CALL
prldap_get_socket_info( int fd, void *socketarg, PRLDAPSocketInfo *soip )
{
    PRLDAPIOSocketArg	*prsockp;

    if ( NULL == socketarg || NULL == soip ||
		PRLDAP_SOCKETINFO_SIZE != soip->soinfo_size ) {
	return( LDAP_PARAM_ERROR );
    }

    prsockp = (PRLDAPIOSocketArg *)socketarg;
    soip->soinfo_prfd = prsockp->prsock_prfd;
    soip->soinfo_appdata = prsockp->prsock_appdata;

    return( LDAP_SUCCESS );
}

/*
* Function: prldap_import_connection().
* 
* Given the LDAP handle the connection parameters for the
* file descriptor are imported into NSPR layer.
* 
* Returns an LDAP API code (LDAP_SUCCESS) if all goes well.
*/
int LDAP_CALL
prldap_import_connection (LDAP *ld)
{
	int rc = LDAP_SUCCESS; /* optimistic */
	int shared = 1; /* Assume shared init */
	int orig_socket = -1;
	PRLDAPIOSessionArg *prsessp = NULL;
	PRLDAPIOSocketArg *prsockp = NULL;
	PRFileDesc *pr_socket = NULL;
	
	/* Check for invalid ld handle */
    	if ( ld == NULL) {
	    ldap_set_lderrno( ld, LDAP_PARAM_ERROR, NULL, NULL );
	    return( LDAP_PARAM_ERROR );
    	}
	
	/* Retrieve TCP socket's integer file descriptor */
    	if ( ldap_get_option( ld, LDAP_OPT_DESC, &orig_socket ) < 0 ) {
	    return( ldap_get_lderrno( ld, NULL, NULL ));
    	}
	
	/* Check for NSPR functions on ld */
   	if ( prldap_is_installed(ld)) {	/* Error : NSPR already Installed */
	    ldap_set_lderrno( ld, LDAP_LOCAL_ERROR, NULL, NULL );
	    return( LDAP_LOCAL_ERROR );
	}
	
    	if (LDAP_SUCCESS != (rc = prldap_install_routines(ld, shared))) {
	    return( rc );
    	}

	if (LDAP_SUCCESS != (rc = prldap_session_arg_from_ld( ld, &prsessp ))) {
	    return( rc );
	}
		
	/* Get NSPR Socket Arg  */
	if ( NULL == ( prsockp = prldap_socket_arg_alloc( prsessp ))) {
	    ldap_set_lderrno( ld, LDAP_NO_MEMORY, NULL, NULL );
	    return( LDAP_NO_MEMORY );
	}
	
	/* Import file descriptor of connection made via ldap_init() */
	if (NULL == (pr_socket = PR_ImportTCPSocket(orig_socket)) ) {
	    ldap_set_lderrno( ld, LDAP_LOCAL_ERROR, NULL, NULL );
	    return( LDAP_LOCAL_ERROR );
	}

	prsockp->prsock_prfd = pr_socket;

	/* Set Socket Arg in Extended I/O Layer */
	if ( ldap_set_option( ld, LDAP_X_OPT_SOCKETARG, prsockp) != 0 ) {
	    return( ldap_get_lderrno( ld, NULL, NULL ));
	}

	return( rc );
}
