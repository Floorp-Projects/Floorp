/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the mozilla.org LDAP XPCOM component.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "nspr.h"
#include "nsIComponentManager.h"
#include "nsLDAPConnection.h"
#include "nsLDAPMessage.h"
#include "nsIProxyObjectManager.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"

// XXX deal with timeouts better
//
struct timeval nsLDAPConnection::sNullTimeval = {0, 0};

static NS_DEFINE_CID(kLDAPMessageCID, NS_LDAPMESSAGE_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_IID(kILDAPMessageListenerIID, NS_ILDAPMESSAGELISTENER_IID);

extern "C" int nsLDAPThreadDataInit(void);
extern "C" int nsLDAPThreadFuncsInit(LDAP *aLDAP);

// constructor
//
nsLDAPConnection::nsLDAPConnection()
{
  NS_INIT_ISUPPORTS();
}

// destructor
// XXX better error-handling than fprintf
//
nsLDAPConnection::~nsLDAPConnection()
{
  int rc;

#ifdef DEBUG_dmose
    PR_fprintf(PR_STDERR,"unbinding\n");
#endif    

  rc = ldap_unbind_s(this->mConnectionHandle);
  if (rc != LDAP_SUCCESS) {
#ifdef DEBUG
    PR_fprintf(PR_STDERR, "nsLDAPConnection::~nsLDAPConnection: %s\n", 
	    ldap_err2string(rc));
#endif
  }

#ifdef DEBUG_dmose
    PR_fprintf(PR_STDERR,"unbound\n");
#endif

  // XXX use delete here?
  // XXX can delete fail?
  //
  if (mBindName) {
      delete mBindName;
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsLDAPConnection, nsILDAPConnection, 
			      nsIRunnable);

// wrapper for ldap_init()
//
NS_IMETHODIMP
nsLDAPConnection::Init(const char *aHost, PRInt16 aPort, const char *aBindName)
{
    nsresult rv;

    NS_ENSURE_ARG(aHost);
    NS_ENSURE_ARG(aPort);

    // XXXdmose - is a bindname of "" equivalent to a bind name of
    // NULL (which which means bind anonymously)?  if so, we don't
    // need to go through these contortions.
    //
    if (aBindName) {
	mBindName = new nsCString(aBindName);
	if (!mBindName) {
	    return NS_ERROR_OUT_OF_MEMORY;
	}
    } else {
	mBindName = NULL;
    }

    this->mConnectionHandle = ldap_init(aHost, aPort);
    if ( this->mConnectionHandle == NULL) {
	return NS_ERROR_FAILURE;  // the LDAP C SDK API gives no useful error
    }

    // initialize the threading functions for this connection
    //
    if (!nsLDAPThreadFuncsInit(this->mConnectionHandle)) {
	return NS_ERROR_FAILURE;
    }


    // initialize the thread-specific data for the calling thread as necessary
    //
    if (!nsLDAPThreadDataInit()) {
	return NS_ERROR_FAILURE;
    }

#ifdef DEBUG_dmose
    const int lDebug = 0;
    ldap_set_option(this->mConnectionHandle, LDAP_OPT_DEBUG_LEVEL, &lDebug);

#if 0
    const int aSync = 0;
    ldap_set_option(this->mConnectionHandle, LDAP_OPT_ASYNC_CONNECT, &aSync);
#endif
#endif

    // kick off a thread for result listening and marshalling
    // XXXdmose - fourth: should be JOINABLE?
    // 
    rv = NS_NewThread(getter_AddRefs(mThread), this, 0, PR_UNJOINABLE_THREAD);
    if (NS_FAILED(rv)) {
	return rv;
    }

    return NS_OK;
}

  
/**
 * Operations on this connection get their results called back to this
 * interface.  This really should (and eventually will be) implemented
 * implemented on a per operation-basis (to allow for shared 
 * nsILDAPConnections), but for now, this MUST be set before doing any
 * operations on this connection.
 */
NS_IMETHODIMP
nsLDAPConnection::SetMessageListener(nsILDAPMessageListener *aMessageListener)
{
    mMessageListener = aMessageListener;
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPConnection::GetMessageListener(nsILDAPMessageListener **aMessageListener)
{
    NS_ENSURE_ARG_POINTER(aMessageListener);

    *aMessageListener = mMessageListener;
    NS_IF_ADDREF(*aMessageListener);

    return NS_OK;
}

// who we're binding as
//
// readonly attribute string bindName
NS_IMETHODIMP
nsLDAPConnection::GetBindName(char **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    
    // check for NULL (meaning bind anonymously)
    //
    if (!mBindName) {
	*_retval = nsnull;
    } else {

	// otherwise, hand out a copy of the bind name
	//
	*_retval = mBindName->ToNewCString();
	if (!(*_retval)) {
	    return NS_ERROR_OUT_OF_MEMORY;
	}
    }

    return NS_OK;
}

// wrapper for ldap_get_lderrno
// XXX should copy before returning
//
NS_IMETHODIMP
nsLDAPConnection::GetLdErrno(char **matched, char **errString, 
			     PRInt32 *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = ldap_get_lderrno(this->mConnectionHandle, matched, errString);

    return NS_OK;
}

// return the error string corresponding to GetLdErrno.
//
// XXX - deal with optional params
// XXX - how does ldap_perror know to look at the global errno?
// XXX - should copy before returning
//
NS_IMETHODIMP
nsLDAPConnection::GetErrorString(char **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = ldap_err2string(ldap_get_lderrno(this->mConnectionHandle, 
						NULL, NULL));
    return NS_OK;
}

// really only for the internal use of nsLDAPOperation and friends
//
// [ptr] native ldapPtr(LDAP);
// [noscript] readonly attribute ldapPtr connection;
//
NS_IMETHODIMP
nsLDAPConnection::GetConnectionHandle(LDAP* *aConnectionHandle)
{
    NS_ENSURE_ARG_POINTER(aConnectionHandle);

    *aConnectionHandle = mConnectionHandle;
    return NS_OK;
}

// for nsIRunnable.  this thread spins in ldap_result() awaiting the next
// message.  once one arrives, it dispatches it to the nsILDAPMessageListener 
// on the main thread.
NS_IMETHODIMP
nsLDAPConnection::Run(void)
{
    nsresult rv;
    PRInt32 returnCode;
    LDAPMessage *msgHandle;
    nsCOMPtr<nsILDAPMessage> msg;
    nsCOMPtr<nsIProxyObjectManager> proxyObjMgr;
    nsCOMPtr<nsILDAPMessageListener> listener;
    char *errString;

    NS_PRECONDITION(mMessageListener != 0, "MessageListener not set");

    // initialize the thread-specific data for the child thread (as necessary)
    //
    if (!nsLDAPThreadDataInit()) {
	return NS_ERROR_FAILURE;
    }

#ifdef DEBUG_dmose
    PR_fprintf(PR_STDERR, "nsLDAPConnection::Run() entered\n");
#endif

    proxyObjMgr = do_GetService(kProxyObjectManagerCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv); 

    rv = proxyObjMgr->GetProxyForObject(NS_UI_THREAD_EVENTQ,
					kILDAPMessageListenerIID,
					mMessageListener, PROXY_ASYNC, 
					(void **)getter_AddRefs(listener));
    NS_ENSURE_SUCCESS(rv, rv);     
	
    // wait for results
    //
    while(1) {

	// XXX deal with timeouts better
	//
	returnCode = ldap_result(mConnectionHandle, LDAP_RES_ANY,
				 LDAP_MSG_ONE, LDAP_NO_LIMIT, &msgHandle);

	// if we didn't error or timeout, create an nsILDAPMessage
	//	
	switch (returnCode) {

	case 0: // timeout
	    // the connection may not exist yet.  sleep for a while
	    // and try again
#ifdef DEBUG_dmose
	    PR_fprintf(PR_STDERR, "ldap_result() timed out.\n");
#endif
	    PR_Sleep(2000);
	    continue;
	    break;

	case -1: // something went wrong 
	         // XXXdmose should propagate the error to the listener
#ifdef DEBUG
	    (void)this->GetErrorString(&errString);
	    PR_fprintf(PR_STDERR,"\nmyOperation->Result() [URLSearch]: %s\n",
		       errString);
	    ldap_memfree(errString); 
#endif
	    break;

	default: // create & initialize the message

	    msg = do_CreateInstance(kLDAPMessageCID, &rv);
	    NS_ENSURE_SUCCESS(rv, rv); // XXX

	    rv = msg->Init(this, msgHandle);
	    NS_ENSURE_SUCCESS(rv, rv); // XXX
	    break;
	}	

	// invoke the callback through the proxy object
	//
	listener->OnLDAPMessage(msg, returnCode);

	// we're all done with the message here.  make nsCOMPtr release it.
	//
	msg = 0;
    }

    // XXX figure out how to break out of the while() loop and get here to
    // so we can expire (though not if DEBUG is defined, since gdb gets ill
    // if threads exit
    //
    return NS_OK;
}


