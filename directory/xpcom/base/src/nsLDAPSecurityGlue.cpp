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
 * Portions created by the Initial Developer are Copyright (C) 1998-2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Smith <mcs@netscape.com>
 *   Michael Hein <mhein@sun.com>
 *   Dan Mosedale <dmose@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// Only build this code if PSM is being built also
//
#ifdef MOZ_PSM

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsISSLSocketProvider.h"
#include "nsIInterfaceRequestor.h"
#include "nsISSLSocketControl.h"
#include "nsMemory.h"
#include "nsLDAPInternal.h"
#include "plstr.h"
#include "ldap.h"
#include "ldappr.h"

// LDAP per-session data structure.
//
typedef struct {
    char *hostname;
    LDAP_X_EXTIOF_CLOSE_CALLBACK *realClose;
    LDAP_X_EXTIOF_CONNECT_CALLBACK *realConnect;
    LDAP_X_EXTIOF_DISPOSEHANDLE_CALLBACK *realDisposeHandle;
} nsLDAPSSLSessionClosure;

// LDAP per-socket data structure.
//
typedef struct {
    nsLDAPSSLSessionClosure *sessionClosure; /* session info */
} nsLDAPSSLSocketClosure;

// free the per-socket data structure as necessary
//
static void
nsLDAPSSLFreeSocketClosure(nsLDAPSSLSocketClosure **aClosure)
{
    if (aClosure && *aClosure) {
	nsMemory::Free(*aClosure);
	*aClosure = nsnull;
    }
}

// Replacement close() function, which cleans up local stuff associated
// with this socket, and then calls the real close function.
//
extern "C" int LDAP_CALLBACK
nsLDAPSSLClose(int s, struct lextiof_socket_private *socketarg)
{
    PRLDAPSocketInfo socketInfo;
    nsLDAPSSLSocketClosure *socketClosure;
    nsLDAPSSLSessionClosure *sessionClosure;

    // get the socketInfo associated with this socket
    //
    memset(&socketInfo, 0, sizeof(socketInfo));
    socketInfo.soinfo_size = PRLDAP_SOCKETINFO_SIZE;
    if (prldap_get_socket_info(s, socketarg, &socketInfo) != LDAP_SUCCESS) {
	NS_ERROR("nsLDAPSSLClose(): prldap_get_socket_info() failed\n");
	return -1;
    }

    // save off the session closure data in an automatic, since we're going to
    // need to call through it
    //
    socketClosure = NS_REINTERPRET_CAST(nsLDAPSSLSocketClosure *,
					socketInfo.soinfo_appdata);
    sessionClosure = socketClosure->sessionClosure;

    // free the socket closure data
    //
    nsLDAPSSLFreeSocketClosure(
	NS_REINTERPRET_CAST(nsLDAPSSLSocketClosure **,
			    &socketInfo.soinfo_appdata));

    // call the real close function
    //
    return (*(sessionClosure->realClose))(s, socketarg);
}

// Replacement connection function.  Calls the real connect function,
// 
extern "C" int LDAP_CALLBACK
nsLDAPSSLConnect(const char *hostlist, int defport, int timeout,
		 unsigned long options, 
		 struct lextiof_session_private *sessionarg,
		 struct lextiof_socket_private **socketargp )
{
    PRLDAPSocketInfo socketInfo;
    PRLDAPSessionInfo sessionInfo;
    nsLDAPSSLSocketClosure *socketClosure = nsnull;
    nsLDAPSSLSessionClosure *sessionClosure;
    int	intfd = -1;
    nsCOMPtr <nsISupports> securityInfo;
    nsCOMPtr <nsISSLSocketProvider> tlsSocketProvider;
    nsCOMPtr <nsISSLSocketControl> sslSocketControl;
    nsresult rv;

    // Ensure secure option is set.  Also, clear secure bit in options
    // the we pass to the standard connect() function (since it doesn't know
    // how to handle the secure option).
    //
    NS_ASSERTION(options & LDAP_X_EXTIOF_OPT_SECURE, 
		 "nsLDAPSSLConnect(): called for non-secure connection");
    options &= ~LDAP_X_EXTIOF_OPT_SECURE;

    // Retrieve session info. so we can store a pointer to our session info.
    // in our socket info. later.
    //
    memset(&sessionInfo, 0, sizeof(sessionInfo));
    sessionInfo.seinfo_size = PRLDAP_SESSIONINFO_SIZE;
    if (prldap_get_session_info(nsnull, sessionarg, &sessionInfo) 
	!= LDAP_SUCCESS) {
	NS_ERROR("nsLDAPSSLConnect(): unable to get session info");
        return -1;
    }
    sessionClosure = NS_REINTERPRET_CAST(nsLDAPSSLSessionClosure *,
					 sessionInfo.seinfo_appdata);
    
    // Call the real connect() callback to make the TCP connection.  If it
    // succeeds, *socketargp is set.
    //
    intfd = (*(sessionClosure->realConnect))(hostlist, defport, timeout, 
					     options, sessionarg, socketargp);
    if ( intfd < 0 ) {
	PR_LOG(gLDAPLogModule, PR_LOG_DEBUG,
	       ("nsLDAPSSLConnect(): standard connect() function returned %d",
		intfd));
        return intfd;
    }

    // Retrieve socket info from the newly created socket so that we
    // have the PRFileDesc onto which we will be layering SSL.
    //
    memset(&socketInfo, 0, sizeof(socketInfo));
    socketInfo.soinfo_size = PRLDAP_SOCKETINFO_SIZE;
    if (prldap_get_socket_info(intfd, *socketargp, &socketInfo)
	!= LDAP_SUCCESS)  {
	NS_ERROR("nsLDAPSSLConnect(): unable to get socket info");
        goto close_socket_and_exit_with_error;
    }

    // Allocate a structure to hold our socket-specific data.
    //
    socketClosure = NS_STATIC_CAST(nsLDAPSSLSocketClosure *, 
				   nsMemory::Alloc(
				       sizeof(nsLDAPSSLSocketClosure)));
    if (!socketClosure) {
	NS_WARNING("nsLDAPSSLConnect(): unable to allocate socket closure");
	goto close_socket_and_exit_with_error;
    }
    memset(socketClosure, 0, sizeof(nsLDAPSSLSocketClosure));
    socketClosure->sessionClosure = sessionClosure;

    // Add the NSPR layer for SSL provided by PSM to this socket. 
    //
    tlsSocketProvider = do_GetService(NS_STARTTLSSOCKETPROVIDER_CONTRACTID, 
				      &rv);
    if (NS_FAILED(rv)) {
	NS_ERROR("nsLDAPSSLConnect(): unable to get socket provider service");
        goto close_socket_and_exit_with_error;
    }
    // XXXdmose: Note that hostlist can be a list of hosts (in the
    // current XPCOM SDK code, it will always be a list of IP
    // addresses).  Because of this, we need to use
    // sessionClosure->hostname which was passed in separately to tell
    // AddToSocket what to match the name in the certificate against.
    // What exactly happen will happen when this is used with some IP
    // address in the list other than the first one is not entirely
    // clear, and I suspect it may depend on the format of the name in
    // the certificate.  Need to investigate.
    //
    rv = tlsSocketProvider->AddToSocket(PR_AF_INET,
					sessionClosure->hostname, defport,
					nsnull, 0, socketInfo.soinfo_prfd,
                                        getter_AddRefs(securityInfo));
    if (NS_FAILED(rv)) {
	NS_ERROR("nsLDAPSSLConnect(): unable to add SSL layer to socket");
        goto close_socket_and_exit_with_error;
    }

    // If possible we want to avoid using SSLv2, as this can confuse
    // some directory servers (notably the netscape 4.1 ds).  The only
    // way that PSM provides for us to do this is to use a socket that can
    // be used for the STARTTLS protocol, because the STARTTLS protocol disallows
    // the use of SSLv2.
    // (Thanks to Brian Ryner for helping figure this out).
    //
    sslSocketControl = do_QueryInterface(securityInfo, &rv);
    if (NS_FAILED(rv)) {
        NS_WARNING("nsLDAPSSLConnect(): unable to QI to nsISSLSocketControl");
    } else {
	rv = sslSocketControl->StartTLS();
	if (NS_FAILED(rv)) {
	    NS_WARNING("nsLDAPSSLConnect(): StartTLS failed");
	}
    }

    // Attach our closure to the socketInfo.
    //
    socketInfo.soinfo_appdata = NS_REINTERPRET_CAST(prldap_socket_private *,
						    socketClosure);
    if (prldap_set_socket_info(intfd, *socketargp, &socketInfo)
	!= LDAP_SUCCESS ) {
	NS_ERROR("nsLDAPSSLConnect(): unable to set socket info");
    }
    return intfd;	// success

close_socket_and_exit_with_error:
    if (socketInfo.soinfo_prfd) {
	PR_Close(socketInfo.soinfo_prfd);
    }
    if (socketClosure) {
        nsLDAPSSLFreeSocketClosure(&socketClosure);
    }
    if ( intfd >= 0 && *socketargp ) {
        (*(sessionClosure->realClose))(intfd, *socketargp);
    }
    return -1;

}

// Free data associated with this session (LDAP *) as necessary.
//
static void
nsLDAPSSLFreeSessionClosure(nsLDAPSSLSessionClosure **aSessionClosure)
{
    if (aSessionClosure && *aSessionClosure) {

	// free the hostname
	//
	if ( (*aSessionClosure)->hostname ) {
	    PL_strfree((*aSessionClosure)->hostname);
	    (*aSessionClosure)->hostname = nsnull;
	}

	// free the structure itself
	//
	nsMemory::Free(*aSessionClosure);
	*aSessionClosure = nsnull;
    }
}

// Replacement session handle disposal code.  First cleans up our local
// stuff, then calls the original session handle disposal function.
//
extern "C" void LDAP_CALLBACK
nsLDAPSSLDisposeHandle(LDAP *ld, struct lextiof_session_private *sessionarg)
{
    PRLDAPSessionInfo sessionInfo;
    nsLDAPSSLSessionClosure *sessionClosure;
    LDAP_X_EXTIOF_DISPOSEHANDLE_CALLBACK *disposehdl_fn;

    memset(&sessionInfo, 0, sizeof(sessionInfo));
    sessionInfo.seinfo_size = PRLDAP_SESSIONINFO_SIZE;
    if (prldap_get_session_info(ld, nsnull, &sessionInfo) == LDAP_SUCCESS) {
	sessionClosure = NS_REINTERPRET_CAST(nsLDAPSSLSessionClosure *,
					     sessionInfo.seinfo_appdata);
	disposehdl_fn = sessionClosure->realDisposeHandle;
	nsLDAPSSLFreeSessionClosure(&sessionClosure);
	(*disposehdl_fn)(ld, sessionarg);
    } 
}

// Installs appropriate routines and data for making this connection
// handle SSL.  The aHostName is ultimately passed to PSM and is used to
// validate certificates.
//
nsresult
nsLDAPInstallSSL( LDAP *ld, const char *aHostName)
{
    struct ldap_x_ext_io_fns iofns;
    nsLDAPSSLSessionClosure *sessionClosure;
    PRLDAPSessionInfo sessionInfo;

    // Allocate our own session information.
    //
    sessionClosure = NS_STATIC_CAST(nsLDAPSSLSessionClosure *, 
				    nsMemory::Alloc(
					sizeof(nsLDAPSSLSessionClosure)));
    if (!sessionClosure) {
	return NS_ERROR_OUT_OF_MEMORY;
    }
    memset(sessionClosure, 0, sizeof(nsLDAPSSLSessionClosure));

    // Override a few functions, saving a pointer to the original function
    // in each case so we can call it from our SSL savvy functions.
    //
    memset(&iofns, 0, sizeof(iofns));
    iofns.lextiof_size = LDAP_X_EXTIO_FNS_SIZE;
    if (ldap_get_option(ld, LDAP_X_OPT_EXTIO_FN_PTRS, 
			NS_STATIC_CAST(void *, &iofns)) != LDAP_SUCCESS) {
	NS_ERROR("nsLDAPInstallSSL(): unexpected error getting"
		 " LDAP_X_OPT_EXTIO_FN_PTRS");
	nsLDAPSSLFreeSessionClosure(&sessionClosure);
        return NS_ERROR_UNEXPECTED;
    }

    // Make a copy of the hostname to pass to AddToSocket later
    //
    sessionClosure->hostname = PL_strdup(aHostName);
    if (!sessionClosure->hostname) {
	NS_ERROR("nsLDAPInstallSSL(): PL_strdup failed\n");
	nsLDAPSSLFreeSessionClosure(&sessionClosure);
	return NS_ERROR_OUT_OF_MEMORY;
    }

    // Override functions
    //
    sessionClosure->realClose = iofns.lextiof_close;
    iofns.lextiof_close = nsLDAPSSLClose;
    sessionClosure->realConnect = iofns.lextiof_connect;
    iofns.lextiof_connect = nsLDAPSSLConnect;
    sessionClosure->realDisposeHandle = iofns.lextiof_disposehandle;
    iofns.lextiof_disposehandle = nsLDAPSSLDisposeHandle;

    if (ldap_set_option(ld, LDAP_X_OPT_EXTIO_FN_PTRS, 
			NS_STATIC_CAST(void *, &iofns)) != LDAP_SUCCESS) {
	NS_ERROR("nsLDAPInstallSSL(): error setting LDAP_X_OPT_EXTIO_FN_PTRS");
	nsLDAPSSLFreeSessionClosure(&sessionClosure);
        return NS_ERROR_FAILURE;
    }

    // Store session info. for later retrieval.
    //
    sessionInfo.seinfo_size = PRLDAP_SESSIONINFO_SIZE;
    sessionInfo.seinfo_appdata = NS_REINTERPRET_CAST(prldap_session_private *,
						     sessionClosure);
    if (prldap_set_session_info(ld, nsnull, &sessionInfo) != LDAP_SUCCESS) {
	NS_ERROR("nsLDAPInstallSSL(): error setting prldap session info");
	nsMemory::Free(sessionClosure);
	return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
}

#endif
