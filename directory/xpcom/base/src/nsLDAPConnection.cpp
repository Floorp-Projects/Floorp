/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the mozilla.org LDAP XPCOM SDK.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mosedale <dmose@mozilla.org> (original author)
 *   Leif Hedstrom <leif@netscape.com>
 *   Kipp Hickman <kipp@netscape.com>
 *   Warren Harris <warren@netscape.com>
 *   Dan Matejka <danm@netscape.com>
 *   David Bienvenu <bienvenu@mozilla.org>
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

#include "nsLDAPInternal.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIComponentManager.h"
#include "nsLDAPConnection.h"
#include "nsLDAPMessage.h"
#include "nsIEventQueueService.h"
#include "nsIConsoleService.h"
#include "nsIDNSService.h"
#include "nsIRequestObserver.h"
#include "nsIProxyObjectManager.h"
#include "nsEventQueueUtils.h"
#include "nsNetError.h"
#include "nsLDAPOperation.h"

const char kConsoleServiceContractId[] = "@mozilla.org/consoleservice;1";
const char kDNSServiceContractId[] = "@mozilla.org/network/dns-service;1";

// constructor
//
nsLDAPConnection::nsLDAPConnection()
    : mConnectionHandle(0),
      mPendingOperations(0),
      mRunnable(0),
      mSSL(PR_FALSE),
      mVersion(nsILDAPConnection::VERSION3),
      mDNSRequest(0)
{
}

// destructor
//
nsLDAPConnection::~nsLDAPConnection()
{
  Close();
  // Release the reference to the runnable object.
  //
  NS_IF_RELEASE(mRunnable);
}

// We need our own Release() here, so that we can lock around the delete.
// This is needed to avoid a race condition with the weak reference to us,
// which is used in nsLDAPConnectionLoop. A problem could occur if the
// nsLDAPConnection gets destroyed while do_QueryReferent() is called,
// since converting to the strong reference isn't MT safe.
//
NS_IMPL_THREADSAFE_ADDREF(nsLDAPConnection)
NS_IMPL_THREADSAFE_QUERY_INTERFACE3(nsLDAPConnection,
                                    nsILDAPConnection,
                                    nsISupportsWeakReference,
                                    nsIDNSListener)

nsrefcnt
nsLDAPConnection::Release(void)
{
    nsrefcnt count;

    NS_PRECONDITION(0 != mRefCnt, "dup release");
    count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);
    NS_LOG_RELEASE(this, count, "nsLDAPConnection");
    if (0 == count) {
        // As commented by danm: In the object's destructor, if by some
        // convoluted, indirect means it happens to run into some code
        // that temporarily references it (addref/release), then if the
        // refcount had been left at 0 the unexpected release would
        // attempt to reenter the object's destructor.
        //
        mRefCnt = 1; /* stabilize */

        // If we have a mRunnable object, we need to make sure to lock it's
        // mLock before we try to DELETE. This is to avoid a race condition.
        // We also make sure to keep a strong reference to the runnable
        // object, to make sure it doesn't get GCed from underneath us,
        // while we are still holding a lock for instance.
        //
        if (mRunnable && mRunnable->mLock) {
            nsLDAPConnectionLoop *runnable  = mRunnable;

            NS_ADDREF(runnable);
            PR_Lock(runnable->mLock);
            NS_DELETEXPCOM(this);
            PR_Unlock(runnable->mLock);
            NS_RELEASE(runnable);
        } else {
            NS_DELETEXPCOM(this);
        }

        return 0;
    }
    return count;
}


NS_IMETHODIMP
nsLDAPConnection::Init(const char *aHost, PRInt32 aPort, PRBool aSSL,
                       const nsACString& aBindName, 
                       nsILDAPMessageListener *aMessageListener,
                       nsISupports *aClosure, PRUint32 aVersion)
{
    nsCOMPtr<nsIDNSListener> selfProxy;
    nsresult rv;

    if ( !aHost || !aMessageListener) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    // Make sure we haven't called Init earlier, i.e. there's a DNS
    // request pending.
    //
    NS_ASSERTION(!mDNSRequest, "nsLDAPConnection::Init() "
                 "Connection was already initialized\n");

    mBindName.Assign(aBindName);

    mClosure = aClosure;

    // Save the port number, SSL flag, and protocol version for later
    // use, once the DNS server(s) has resolved the host part.
    //
    mPort = aPort;
    mSSL = aSSL;
    if (aVersion != nsILDAPConnection::VERSION2 && 
        aVersion != nsILDAPConnection::VERSION3) {
        NS_ERROR("nsLDAPConnection::Init(): illegal version");
        return NS_ERROR_ILLEGAL_VALUE;
    }
    mVersion = aVersion;

    // Save the Init listener reference, we need it when the async
    // DNS resolver has finished.
    //
    mInitListener = aMessageListener;

    // allocate a hashtable to keep track of pending operations.
    // 10 buckets seems like a reasonable size, and we do want it to 
    // be threadsafe
    //
    mPendingOperations = new nsSupportsHashtable(10, PR_TRUE);
    if ( !mPendingOperations) {
        NS_ERROR("failure initializing mPendingOperations hashtable");
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIEventQueue> curEventQ;
    rv = NS_GetCurrentEventQ(getter_AddRefs(curEventQ));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPConnection::Init(): couldn't "
                 "get current event queue");
        return NS_ERROR_FAILURE;
    }
    // Do the pre-resolve of the hostname, using the DNS service. This
    // will also initialize the LDAP connection properly, once we have
    // the IPs resolved for the hostname. This includes creating the
    // new thread for this connection.
    //
    // XXX - What return codes can we expect from the DNS service?
    //
    nsCOMPtr<nsIDNSService>
        pDNSService(do_GetService(kDNSServiceContractId, &rv));

    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPConnection::Init(): couldn't "
                 "create the DNS Service object");

        return NS_ERROR_FAILURE;
    }
    mDNSHost = aHost;

    // if the caller has passed in a space-delimited set of hosts, as the 
    // ldap c-sdk allows, strip off the trailing hosts for now.
    // Soon, we'd like to make multiple hosts work, but now make
    // at least the first one work.
    mDNSHost.CompressWhitespace(PR_TRUE, PR_TRUE);

    PRInt32 spacePos = mDNSHost.FindChar(' ');
    // trim off trailing host(s)
    if (spacePos != kNotFound)
      mDNSHost.Truncate(spacePos);

    rv = pDNSService->AsyncResolve(mDNSHost,
                                   PR_FALSE, this, curEventQ, 
                                   getter_AddRefs(mDNSRequest));

    if (NS_FAILED(rv)) {
        switch (rv) {
        case NS_ERROR_OUT_OF_MEMORY:
        case NS_ERROR_UNKNOWN_HOST:
        case NS_ERROR_FAILURE:
        case NS_ERROR_OFFLINE:
            break;

        default:
            rv = NS_ERROR_UNEXPECTED;
        }
        mDNSHost.Truncate();
    }
    return rv;
}

// this might get exposed to clients, so we've broken it
// out of the destructor.
void
nsLDAPConnection::Close()
{
  int rc;

  PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("unbinding\n"));

  if (mConnectionHandle) {
      // note that the ldap_unbind() call in the 5.0 version of the LDAP C SDK
      // appears to be exactly identical to ldap_unbind_s(), so it may in fact
      // still be synchronous
      //
      rc = ldap_unbind(mConnectionHandle);
#ifdef PR_LOGGING
      if (rc != LDAP_SUCCESS) {
          PR_LOG(gLDAPLogModule, PR_LOG_WARNING, 
                 ("nsLDAPConnection::Close(): %s\n", 
                  ldap_err2string(rc)));
      }
#endif
      mConnectionHandle = nsnull;
  }

  PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("unbound\n"));

  if (mPendingOperations) {
      delete mPendingOperations;
      mPendingOperations = nsnull;
  }

  // Cancel the DNS lookup if needed, and also drop the reference to the
  // Init listener (if still there).
  //
  if (mDNSRequest) {
      mDNSRequest->Cancel();
      mDNSRequest = 0;
  }
  mInitListener = 0;

}

NS_IMETHODIMP
nsLDAPConnection::GetClosure(nsISupports **_retval)
{
    if (!_retval) {
        return NS_ERROR_ILLEGAL_VALUE;
    }
    NS_IF_ADDREF(*_retval = mClosure);
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPConnection::SetClosure(nsISupports *aClosure)
{
    mClosure = aClosure;
    return NS_OK;
}

// who we're binding as
//
// readonly attribute AUTF8String bindName
//
NS_IMETHODIMP
nsLDAPConnection::GetBindName(nsACString& _retval)
{
    _retval.Assign(mBindName);
    return NS_OK;
}

// wrapper for ldap_get_lderrno
// XXX should copy before returning
//
NS_IMETHODIMP
nsLDAPConnection::GetLdErrno(nsACString& matched, nsACString& errString, 
                             PRInt32 *_retval)
{
    char *match, *err;

    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = ldap_get_lderrno(mConnectionHandle, &match, &err);
    matched.Assign(match);
    errString.Assign(err);
    return NS_OK;
}

// return the error string corresponding to GetLdErrno.
//
// XXX - deal with optional params
// XXX - how does ldap_perror know to look at the global errno?
//
NS_IMETHODIMP
nsLDAPConnection::GetErrorString(PRUnichar **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    // get the error string
    //
    char *rv = ldap_err2string(ldap_get_lderrno(mConnectionHandle, 0, 0));
    if (!rv) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // make a copy using the XPCOM shared allocator
    //
    *_retval = UTF8ToNewUnicode(nsDependentCString(rv));
    if (!*_retval) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

/** 
 * Add an nsILDAPOperation to the list of operations pending on
 * this connection.  This is also mainly intended for use by the
 * nsLDAPOperation code.
 */
nsresult
nsLDAPConnection::AddPendingOperation(nsILDAPOperation *aOperation)
{
    PRInt32 msgID;

    if (!aOperation) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    // find the message id
    //
    aOperation->GetMessageID(&msgID);

    // turn it into an nsVoidKey.  note that this is another spot that
    // assumes that sizeof(void*) >= sizeof(PRInt32).  
    //
    // XXXdmose  should really create an nsPRInt32Key.
    //
    nsVoidKey *key = new nsVoidKey(NS_REINTERPRET_CAST(void *, msgID));
    if (!key) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // actually add it to the queue.  if Put indicates that an item in 
    // the hashtable was actually overwritten, something is really wrong.
    //
    if (mPendingOperations->Put(key, aOperation)) {
        NS_ERROR("nsLDAPConnection::AddPendingOperation() "
                 "mPendingOperations->Put() overwrote an item.  msgId "
                 "is supposed to be unique\n");
        delete key;
        return NS_ERROR_UNEXPECTED;
    }

    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, 
           ("pending operation added; total pending operations now = %d\n", 
            mPendingOperations->Count()));

    delete key;
    return NS_OK;
}

/**
 * Remove an nsILDAPOperation from the list of operations pending on this
 * connection.  Mainly intended for use by the nsLDAPOperation code.
 *
 * @param aOperation    operation to add
 * @exception NS_ERROR_INVALID_POINTER  aOperation was NULL
 * @exception NS_ERROR_OUT_OF_MEMORY    out of memory
 * @exception NS_ERROR_FAILURE      could not delete the operation 
 *
 * void removePendingOperation(in nsILDAPOperation aOperation);
 */
nsresult
nsLDAPConnection::RemovePendingOperation(nsILDAPOperation *aOperation)
{
    nsresult rv;
    PRInt32 msgID;

    NS_ENSURE_TRUE(mPendingOperations, NS_OK);
    NS_ENSURE_ARG_POINTER(aOperation);

    // find the message id
    //
    rv = aOperation->GetMessageID(&msgID);
    NS_ENSURE_SUCCESS(rv, rv);

    // turn it into an nsVoidKey.  note that this is another spot that
    // assumes that sizeof(void*) >= sizeof(PRInt32).  
    //
    // XXXdmose  should really create an nsPRInt32Key.
    //
    nsVoidKey *key = new nsVoidKey(NS_REINTERPRET_CAST(void *, msgID));
    if (!key) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // remove the operation if it's still there.  
    //
    if (!mPendingOperations->Remove(key)) {

        PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, 
               ("nsLDAPConnection::RemovePendingOperation(): could not remove "
                "operation; perhaps it already completed."));
    } else {

        PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, 
               ("nsLDAPConnection::RemovePendingOperation(): operation "
                "removed; total pending operations now = %d\n", 
                mPendingOperations->Count()));
    }

    delete key;
    return NS_OK;
}

nsresult
nsLDAPConnection::InvokeMessageCallback(LDAPMessage *aMsgHandle, 
                                        nsILDAPMessage *aMsg,
                                        PRBool aRemoveOpFromConnQ)
{
    PRInt32 msgId;
    nsresult rv;
    nsCOMPtr<nsILDAPOperation> operation;
    nsCOMPtr<nsILDAPMessageListener> listener;

    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("InvokeMessageCallback entered\n"));

    // get the message id corresponding to this operation
    //
    msgId = ldap_msgid(aMsgHandle);
    if (msgId == -1) {
        NS_ERROR("nsLDAPConnection::GetCallbackByMessage(): "
                 "ldap_msgid() failed\n");
        return NS_ERROR_FAILURE;
    }

    // get this in key form.  note that using nsVoidKey in this way assumes
    // that sizeof(void *) >= sizeof PRInt32
    //
    nsVoidKey *key = new nsVoidKey(NS_REINTERPRET_CAST(void *, msgId));
    if (!key)
        return NS_ERROR_OUT_OF_MEMORY;

    // find the operation in question
    //
    nsISupports *data = mPendingOperations->Get(key);
    if (!data) {

        PR_LOG(gLDAPLogModule, PR_LOG_WARNING, 
               ("Warning: InvokeMessageCallback(): couldn't find "
                "nsILDAPOperation corresponding to this message id\n"));
        delete key;

        // this may well be ok, since it could just mean that the operation
        // was aborted while some number of messages were already in transit.
        //
        return NS_OK;
    }

    operation = getter_AddRefs(NS_STATIC_CAST(nsILDAPOperation *, data));

    // Make sure the mOperation member is set to this operation before
    // we call the callback.
    //
    NS_STATIC_CAST(nsLDAPMessage *, aMsg)->mOperation = operation;

    // get the message listener object (this may be a proxy for a
    // callback which should happen on another thread)
    //
    rv = operation->GetMessageListener(getter_AddRefs(listener));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPConnection::InvokeMessageCallback(): probable "
                 "memory corruption: GetMessageListener() returned error");
        delete key;
        return NS_ERROR_UNEXPECTED;
    }

    // invoke the callback 
    //
    if (listener) {
      listener->OnLDAPMessage(aMsg);
    }
    // if requested (ie the operation is done), remove the operation
    // from the connection queue.
    //
    if (aRemoveOpFromConnQ) {
        nsCOMPtr <nsLDAPOperation> operation = 
          getter_AddRefs(NS_STATIC_CAST(nsLDAPOperation *,
                                        mPendingOperations->Get(key)));
        // try to break cycles
        if (operation)
          operation->Clear();
        rv = mPendingOperations->Remove(key);
        if (NS_FAILED(rv)) {
            NS_ERROR("nsLDAPConnection::InvokeMessageCallback: unable to "
                     "remove operation from the connection queue\n");
            delete key;
            return NS_ERROR_UNEXPECTED;
        }

        PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, 
               ("pending operation removed; total pending operations now ="
                " %d\n", mPendingOperations->Count()));
    }

    delete key;
    return NS_OK;
}

// constructor
//
nsLDAPConnectionLoop::nsLDAPConnectionLoop()
    : mWeakConn(0),
      mLock(0)
{
}

// destructor
//
nsLDAPConnectionLoop::~nsLDAPConnectionLoop()
{
    // Delete the lock object
    if (mLock)
        PR_DestroyLock(mLock);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLDAPConnectionLoop, nsIRunnable)

NS_IMETHODIMP
nsLDAPConnectionLoop::Init()
{
    if (!mLock) {
        mLock = PR_NewLock();
        if (!mLock) {
            NS_ERROR("nsLDAPConnectionLoop::Init: out of memory ");
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    return NS_OK;
}

// typedef PRBool
// (*PR_CALLBACK nsHashtableEnumFunc)
//      (nsHashKey *aKey, void *aData, void* aClosure);
PRBool PR_CALLBACK
CheckLDAPOperationResult(nsHashKey *aKey, void *aData, void* aClosure)
{
    int lderrno;
    nsresult rv;
    PRInt32 returnCode;
    LDAPMessage *msgHandle;
    nsCOMPtr<nsILDAPMessage> msg;
    PRBool operationFinished = PR_TRUE;
    struct timeval timeout = { 1, 0 }; 
    PRIntervalTime sleepTime = PR_MillisecondsToInterval(40);

    // we need to access some of the connection loop's objects
    //
    nsLDAPConnectionLoop *loop = 
        NS_STATIC_CAST(nsLDAPConnectionLoop *, aClosure);

    // get the console service so we can log messages
    //
    nsCOMPtr<nsIConsoleService> consoleSvc = 
        do_GetService(kConsoleServiceContractId, &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("CheckLDAPOperationResult() couldn't get console service");
        return NS_ERROR_FAILURE;
    }

    returnCode = ldap_result(loop->mRawConn->mConnectionHandle,
                             aKey->HashCode(), LDAP_MSG_ONE,
                                 &timeout, &msgHandle);

        // if we didn't error or timeout, create an nsILDAPMessage
        //      
        switch (returnCode) {

        case 0: // timeout

            // the connection may not exist yet.  sleep for a while
            // and try again
            //
        PR_LOG(gLDAPLogModule, PR_LOG_WARNING, ("ldap_result() timed out.\n"));

            // The sleep here is to avoid a problem where the LDAP
            // Connection/thread isn't ready quite yet, and we want to
            // avoid a very busy loop.
            //
            PR_Sleep(sleepTime);
        return PR_TRUE;

        case -1: // something went wrong 

        lderrno = ldap_get_lderrno(loop->mRawConn->mConnectionHandle, 0, 0);

            // Sleep briefly, to avoid a very busy loop again.
            //
            PR_Sleep(sleepTime);

            switch (lderrno) {

            case LDAP_SERVER_DOWN:
                // We might want to shutdown the thread here, but it has
                // implications to the user of the nsLDAPConnection, so
                // for now we just ignore it. It's up to the owner of
                // the nsLDAPConnection to detect the error, and then
                // create a new connection.
                //
                PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, 
                       ("CheckLDAPOperationResult(): ldap_result returned" 
                        " LDAP_SERVER_DOWN"));
                break;

            case LDAP_DECODING_ERROR:
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: WARNING: decoding error; possible corrupt data received").get());
                NS_WARNING("CheckLDAPOperationResult(): ldaperrno = "
                           "LDAP_DECODING_ERROR after ldap_result()");
                break;

            case LDAP_NO_MEMORY:
                NS_ERROR("CheckLDAPOperationResult(): Couldn't allocate memory"
                         " while getting async operation result");
                // punt and hope things work out better next time around
                break;

            case LDAP_PARAM_ERROR:
                // I think it's possible to hit a race condition where we're
                // continuing to poll for a result after the C SDK connection
                // has removed the operation because the connection has gone
                // dead.  In theory we should fix this.  Practically, it's
                // unclear to me whether it matters.
                //
                NS_WARNING("CheckLDAPOperationResult(): ldap_result returned"
                           " LDAP_PARAM_ERROR");
                break;

            default:
                NS_ERROR("CheckLDAPOperationResult(): lderrno set to "
                           "unexpected value after ldap_result() "
                           "call in nsLDAPConnection::Run()");
                PR_LOG(gLDAPLogModule, PR_LOG_ERROR, 
                       ("lderrno = 0x%x", lderrno));
                break;
            }
            break;

        case LDAP_RES_SEARCH_ENTRY:
        case LDAP_RES_SEARCH_REFERENCE:
            // XXX what should we do with LDAP_RES_SEARCH_EXTENDED?

            // not done yet, so we shouldn't remove the op from the conn q
            operationFinished = PR_FALSE;

            // fall through to default case

        default: // initialize the message and call the callback

            // we want nsLDAPMessage specifically, not a compatible, since
            // we're sharing native objects used by the LDAP C SDK
            //
            nsLDAPMessage *rawMsg;
            NS_NEWXPCOM(rawMsg, nsLDAPMessage);
            if (!rawMsg) {
            NS_ERROR("CheckLDAPOperationResult(): couldn't allocate memory"
                     " for new LDAP message; search entry dropped");
                // punt and hope things work out better next time around
                break;
            }

            // initialize the message, using a protected method not available
            // through nsILDAPMessage (which is why we need the raw pointer)
            //
        rv = rawMsg->Init(loop->mRawConn, msgHandle);

            switch (rv) {

            case NS_OK: 
                break;

            case NS_ERROR_LDAP_DECODING_ERROR:
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: WARNING: decoding error; possible corrupt data received").get());
            NS_WARNING("CheckLDAPOperationResult(): ldaperrno = "
                           "LDAP_DECODING_ERROR after ldap_result()");
            return PR_TRUE;

            case NS_ERROR_OUT_OF_MEMORY:
                // punt and hope things work out better next time around
            return PR_TRUE;

            case NS_ERROR_ILLEGAL_VALUE:
            case NS_ERROR_UNEXPECTED:
            default:
                // shouldn't happen; internal error
                //
            NS_ERROR("CheckLDAPOperationResult(): nsLDAPMessage::Init() "
                           "returned unexpected value.");

                // punt and hope things work out better next time around
            return PR_TRUE;
            }

            // now let the scoping mechanisms provided by nsCOMPtr manage
            // the reference for us.
            //
            msg = rawMsg;

            // invoke the callback on the nsILDAPOperation corresponding to 
            // this message
            //
        rv = loop->mRawConn->InvokeMessageCallback(msgHandle, msg, 
                                                    operationFinished);
            if (NS_FAILED(rv)) {
            NS_ERROR("CheckLDAPOperationResult(): error invoking message"
                     " callback");
                // punt and hope things work out better next time around
            return PR_TRUE;
            }

            break;
        }       

    return PR_TRUE;
}

// for nsIRunnable.  this thread spins in ldap_result() awaiting the next
// message.  once one arrives, it dispatches it to the nsILDAPMessageListener 
// on the main thread.
//
// XXX do all returns from this function need to do thread cleanup?
//
NS_IMETHODIMP
nsLDAPConnectionLoop::Run(void)
{
    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, 
           ("nsLDAPConnection::Run() entered\n"));

    // wait for results
    //
    while(1) {

        // Exit this thread if we no longer have an nsLDAPConnection
        // associated with it. We also aquire a lock here, to make sure
        // to avoid a possible race condition when the nsLDAPConnection
        // is destructed during the call to do_QueryReferent() (since that
        // function isn't MT safe).
        //
        nsresult rv;

        PR_Lock(mLock);
        nsCOMPtr<nsILDAPConnection> strongConn = 
            do_QueryReferent(mWeakConn, &rv);
        PR_Unlock(mLock);

        if (NS_FAILED(rv)) {
            mWeakConn = 0;
            return NS_OK;
        }
        // we use a raw connection because we need to call non-interface
        // methods
        mRawConn = NS_STATIC_CAST(nsLDAPConnection *, 
                                  NS_STATIC_CAST(nsILDAPConnection *, 
                                                 strongConn.get()));

        // XXX deal with timeouts better
        //
        NS_ASSERTION(mRawConn->mConnectionHandle, "nsLDAPConnection::Run(): "
                     "no connection created.\n");

        // We can't enumerate over mPendingOperations itself, because the
        // callback needs to modify mPendingOperations.  So we clone it first,
        // and enumerate over the clone.  It kinda sucks that we need to do
        // this everytime we poll, but the hashtable will pretty much always
        // be small.
        //
        // only clone if the number of pending operations is non-zero
        // otherwise, put the LDAP connection thread to sleep (briefly)
        // until there is pending operations..
        if (mRawConn->mPendingOperations->Count()) {
          nsHashtable *hashtableCopy = mRawConn->mPendingOperations->Clone();
          if (hashtableCopy) {
            hashtableCopy->Enumerate(CheckLDAPOperationResult, this);
            delete hashtableCopy;
          } else {
            // punt and hope it works next time around
            NS_ERROR("nsLDAPConnectionLoop::Run() error cloning hashtable");
          }
        }
        else {
          PR_Sleep(PR_MillisecondsToInterval(40));
        }
    }

    // This will never happen, but here just in case.
    //
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPConnection::OnLookupComplete(nsIDNSRequest *aRequest,
                                   nsIDNSRecord  *aRecord,
                                   nsresult       aStatus)
{    
    nsresult rv = NS_OK;

    if (aRecord) {
        // Build mResolvedIP list
        //
        mResolvedIP.Truncate();

        PRInt32 index = 0;
        char addrbuf[64];
        PRNetAddr addr;

        while (NS_SUCCEEDED(aRecord->GetNextAddr(0, &addr))) {
            // We can only use v4 addresses
            //
            PRBool v4mapped = PR_FALSE;
            if (addr.raw.family == PR_AF_INET6)
                v4mapped = PR_IsNetAddrType(&addr, PR_IpAddrV4Mapped);
            if (addr.raw.family == PR_AF_INET || v4mapped) {
                // If there are more IPs in the list, we separate them with
                // a space, as supported/used by the LDAP C-SDK.
                //
                if (index++)
                    mResolvedIP.Append(' ');

                // Convert the IPv4 address to a string, and append it to our
                // list of IPs.  Strip leading '::FFFF:' (the IPv4-mapped-IPv6 
                // indicator) if present.
                //
                PR_NetAddrToString(&addr, addrbuf, sizeof(addrbuf));
                if ((addrbuf[0] == ':') && (strlen(addrbuf) > 7))
                    mResolvedIP.Append(addrbuf+7);
                else
                    mResolvedIP.Append(addrbuf);
            }
        }
    }

    if (NS_FAILED(aStatus)) {
        // The DNS service failed, lets pass something reasonable
        // back to the listener.
        //
        switch (aStatus) {
        case NS_ERROR_OUT_OF_MEMORY:
        case NS_ERROR_UNKNOWN_HOST:
        case NS_ERROR_FAILURE:
        case NS_ERROR_OFFLINE:
            rv = aStatus;
            break;

        default:
            rv = NS_ERROR_UNEXPECTED;
            break;
        }
    } else if (!mResolvedIP.Length()) {
        // We have no host resolved, that is very bad, and should most
        // likely have been caught earlier.
        //
        NS_ERROR("nsLDAPConnection::OnStopLookup(): the resolved IP "
                 "string is empty.\n");
        
        rv = NS_ERROR_UNKNOWN_HOST;
    } else {
        // We've got the IP(s) for the hostname, now lets setup the
        // LDAP connection using this information. Note that if the
        // LDAP server returns a referral, the C-SDK will perform a
        // new, synchronous DNS lookup, which might hang (but hopefully
        // if we've come this far, DNS is working properly).
        //
        mConnectionHandle = ldap_init(mResolvedIP.get(),
                                      mPort == -1 ? LDAP_PORT : mPort);
        // Check that we got a proper connection, and if so, setup the
        // threading functions for this connection.
        //
        if ( !mConnectionHandle ) {
            rv = NS_ERROR_FAILURE;  // LDAP C SDK API gives no useful error
        } else {
#if defined(DEBUG_dmose) || defined(DEBUG_bienvenu)
            const int lDebug = 0;
            ldap_set_option(mConnectionHandle, LDAP_OPT_DEBUG_LEVEL, &lDebug);
#endif

            // the C SDK currently defaults to v2.  if we're to use v3, 
            // tell it so.
            //
            int version;
            switch (mVersion) {
            case 2:
                break;
            case 3:
                version = LDAP_VERSION3;
                ldap_set_option(mConnectionHandle, LDAP_OPT_PROTOCOL_VERSION, 
                                &version);
		break;
            default:
                NS_ERROR("nsLDAPConnection::OnLookupComplete(): mVersion"
                         " invalid");
            }

#ifdef MOZ_PSM
            // This code sets up the current connection to use PSM for SSL
            // functionality.  Making this use libssldap instead for
            // non-browser user shouldn't be hard.

            extern nsresult nsLDAPInstallSSL(LDAP *ld, const char *aHostName);

            if (mSSL) {
                if (ldap_set_option(mConnectionHandle, LDAP_OPT_SSL,
                                    LDAP_OPT_ON) != LDAP_SUCCESS ) {
                    NS_ERROR("nsLDAPConnection::OnStopLookup(): Error"
                             " configuring connection to use SSL");
                    rv = NS_ERROR_UNEXPECTED;
                }

                rv = nsLDAPInstallSSL(mConnectionHandle, mDNSHost.get());
                if (NS_FAILED(rv)) {
                    NS_ERROR("nsLDAPConnection::OnStopLookup(): Error"
                             " installing secure LDAP routines for"
                             " connection");
                }
            }
#endif
        }

        // Create a new runnable object, and increment the refcnt. The
        // thread will also hold a strong ref to the runnable, but we need
        // to make sure it doesn't get destructed until we are done with
        // all locking etc. in nsLDAPConnection::Release().
        //
        mRunnable = new nsLDAPConnectionLoop();
        NS_ADDREF(mRunnable);
        rv = mRunnable->Init();
        if (NS_FAILED(rv)) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        } else {
            // Here we keep a weak reference in the runnable object to the
            // nsLDAPConnection ("this"). This avoids the problem where a
            // connection can't get destructed because of the new thread
            // keeping a strong reference to it. It also helps us know when
            // we need to exit the new thread: when we can't convert the weak
            // reference to a strong ref, we know that the nsLDAPConnection
            // object is gone, and we need to stop the thread running.
            //
            nsCOMPtr<nsILDAPConnection> conn =
                NS_STATIC_CAST(nsILDAPConnection *, this);

            mRunnable->mWeakConn = do_GetWeakReference(conn);

            // kick off a thread for result listening and marshalling
            // XXXdmose - should this be JOINABLE?
            //
            rv = NS_NewThread(getter_AddRefs(mThread), mRunnable, 0,
                          PR_UNJOINABLE_THREAD);
            if (NS_FAILED(rv)) {
                rv = NS_ERROR_NOT_AVAILABLE;
            }
        }
    }

    // Drop the DNS request object, we no longer need it, and set the flag
    // indicating that DNS has finished.
    //
    mDNSRequest = 0;
    mDNSHost.Truncate();

    // Call the listener, and then we can release our reference to it.
    //
    mInitListener->OnLDAPInit(this, rv);
    mInitListener = 0;

    return rv;
}
