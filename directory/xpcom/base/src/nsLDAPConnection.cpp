/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is the mozilla.org LDAP XPCOM SDK.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org> (original author)
 *                 Leif Hedstrom <leif@netscape.com>
 *                 Kipp Hickman <kipp@netscape.com>
 *                 Warren Harris <warren@netscape.com>
 *                 Dan Matejka <danm@netscape.com>
 *  
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

#include "nsLDAPInternal.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsIComponentManager.h"
#include "nsLDAPConnection.h"
#include "nsLDAPMessage.h"
#include "nsIEventQueueService.h"
#include "nsIConsoleService.h"

const char kConsoleServiceContractId[] = "@mozilla.org/consoleservice;1";

extern "C" int nsLDAPThreadDataInit(void);
extern "C" int nsLDAPThreadFuncsInit(LDAP *aLDAP);

// constructor
//
nsLDAPConnection::nsLDAPConnection()
    : mConnectionHandle(0),
      mBindName(0),
      mPendingOperations(0),
      mRunnable(0)
{
  NS_INIT_ISUPPORTS();
}

// destructor
//
nsLDAPConnection::~nsLDAPConnection()
{
  int rc;

  PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("unbinding\n"));

  if (mConnectionHandle) {
      rc = ldap_unbind_s(mConnectionHandle);
#ifdef PR_LOGGING
      if (rc != LDAP_SUCCESS) {
          if (gLDAPLogModule) {
              PR_LOG(gLDAPLogModule, PR_LOG_WARNING, 
                     ("nsLDAPConnection::~nsLDAPConnection: %s\n", 
                      ldap_err2string(rc)));
          }
      }
#endif
  }

#ifdef PR_LOGGING
  if (gLDAPLogModule) {
      PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("unbound\n"));
  }
#endif

  if (mBindName) {
      delete mBindName;
  }

  if (mPendingOperations) {
      delete mPendingOperations;
  }

  NS_IF_RELEASE(mRunnable);
}

// We need our own Release() here, so that we can lock around the delete.
// This is needed to avoid a race condition with the weak reference to us,
// which is used in nsLDAPConnectionLoop. A problem could occur if the
// nsLDAPConnection gets destroyed while do_QueryReferent() is called,
// since converting to the strong reference isn't MT safe.
//
NS_IMPL_THREADSAFE_ADDREF(nsLDAPConnection);
NS_IMPL_THREADSAFE_QUERY_INTERFACE2(nsLDAPConnection, nsILDAPConnection,
                                    nsISupportsWeakReference);

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
nsLDAPConnection::Init(const char *aHost, PRInt16 aPort,
                       const PRUnichar *aBindName)
{
    nsresult rv;

    if ( !aHost ) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

#ifdef PR_LOGGING
    // initialize logging, if it hasn't been already
    //
    if (!gLDAPLogModule) {
        gLDAPLogModule = PR_NewLogModule("ldap");

        NS_ABORT_IF_FALSE(gLDAPLogModule, 
                          "failed to initialize LDAP log module");
    }
#endif

    // XXXdmose - is a bindname of "" equivalent to a bind name of
    // NULL (which which means bind anonymously)?  if so, we don't
    // need to go through these contortions.
    //
    if (aBindName) {
        mBindName = new nsString(aBindName);
        if (!mBindName) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        mBindName = 0;
    }

    // initialize the connection
    //
    mConnectionHandle = ldap_init(aHost, aPort == -1 ? LDAP_PORT : aPort);
    if ( !mConnectionHandle ) {
        return NS_ERROR_FAILURE;  // the LDAP C SDK API gives no useful error
    }

    // initialize the threading functions for this connection
    //
    if (!nsLDAPThreadFuncsInit(mConnectionHandle)) {
        return NS_ERROR_UNEXPECTED;
    }

    // initialize the thread-specific data for the calling thread as necessary
    //
    if (!nsLDAPThreadDataInit()) {
        return NS_ERROR_FAILURE;
    }

    // allocate a hashtable to keep track of pending operations.
    // 10 buckets seems like a reasonable size, and we do want it to 
    // be threadsafe
    //
    mPendingOperations = new nsSupportsHashtable(10, PR_TRUE);
    if ( !mPendingOperations) {
        NS_ERROR("failure initializing mPendingOperations hashtable");
        return NS_ERROR_FAILURE;
    }

#ifdef DEBUG_dmose
    const int lDebug = 0;
    ldap_set_option(mConnectionHandle, LDAP_OPT_DEBUG_LEVEL, &lDebug);
    ldap_set_option(mConnectionHandle, LDAP_OPT_ASYNC_CONNECT, 
                    NS_REINTERPRET_CAST(void *, 0));
#endif

    // Create a new runnable object, and increment the refcnt. The
    // thread will also hold a strong ref to the runnable, but we need
    // to make sure it doesn't get destructed until we are done with
    // all locking etc. in nsLDAPConnection::Release().
    //
    mRunnable = new nsLDAPConnectionLoop();
    NS_ADDREF(mRunnable);
    rv = mRunnable->Init();
    if (NS_FAILED(rv)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Here we keep a weak reference in the runnable object to the
    // nsLDAPConnection ("this"). This avoids the problem where a connection
    // can't get destructed because of the new thread keeping a strong
    // reference to it. It also helps us know when we need to exit the new
    // thread: when we can't convert the weak reference to a strong ref, we
    // know that the nsLDAPConnection object is gone, and we need to stop
    // the thread running.
    //
    nsCOMPtr<nsILDAPConnection> conn = NS_STATIC_CAST(nsILDAPConnection *,
                                                      this);
    mRunnable->mWeakConn = do_GetWeakReference(conn);

    // kick off a thread for result listening and marshalling
    // XXXdmose - should this be JOINABLE?
    //
    rv = NS_NewThread(getter_AddRefs(mThread), mRunnable, 0,
                      PR_UNJOINABLE_THREAD);
    if (NS_FAILED(rv)) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    return NS_OK;
}

// who we're binding as
//
// readonly attribute string bindName
NS_IMETHODIMP
nsLDAPConnection::GetBindName(PRUnichar **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    
    // check for NULL (meaning bind anonymously)
    //
    if (!mBindName) {
        *_retval = 0;
    } else {

        // otherwise, hand out a copy of the bind name
        //
        *_retval = mBindName->ToNewUnicode();
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
nsLDAPConnection::GetLdErrno(PRUnichar **matched, PRUnichar **errString, 
                             PRInt32 *_retval)
{
    char *match, *err;

    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = ldap_get_lderrno(mConnectionHandle, &match, &err);
    *matched = NS_ConvertUTF8toUCS2(match).ToNewUnicode();
    *errString = NS_ConvertUTF8toUCS2(err).ToNewUnicode();

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
    *_retval = NS_ConvertUTF8toUCS2(rv).ToNewUnicode();
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
    if (!NS_SUCCEEDED(rv)) {
        NS_ERROR("nsLDAPConnection::InvokeMessageCallback(): probable "
                 "memory corruption: GetMessageListener() returned error");
        delete key;
        return NS_ERROR_UNEXPECTED;
    }

    // invoke the callback 
    //
    listener->OnLDAPMessage(aMsg);

    // if requested (ie the operation is done), remove the operation
    // from the connection queue.
    //
    if (aRemoveOpFromConnQ) {
        rv = mPendingOperations->Remove(key);
        if (!NS_SUCCEEDED(rv)) {
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
  NS_INIT_ISUPPORTS();
}

// destructor
//
nsLDAPConnectionLoop::~nsLDAPConnectionLoop()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLDAPConnectionLoop, nsIRunnable);

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


// for nsIRunnable.  this thread spins in ldap_result() awaiting the next
// message.  once one arrives, it dispatches it to the nsILDAPMessageListener 
// on the main thread.
//
// XXX do all returns from this function need to do thread cleanup?
//
NS_IMETHODIMP
nsLDAPConnectionLoop::Run(void)
{
    int lderrno;
    nsresult rv;
    PRInt32 returnCode;
    LDAPMessage *msgHandle;
    nsCOMPtr<nsILDAPMessage> msg;
    struct timeval timeout = { 1, 0 }; 
    PRIntervalTime sleepTime = PR_MillisecondsToInterval(40);

    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, 
           ("nsLDAPConnection::Run() entered\n"));

    // get the console service so we can log messages
    //
    nsCOMPtr<nsIConsoleService> consoleSvc = 
        do_GetService(kConsoleServiceContractId, &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPConnection::Run() couldn't get console service");
        return NS_ERROR_FAILURE;
    }

    // initialize the thread-specific data for the child thread (as necessary)
    //
    if (!nsLDAPThreadDataInit()) {
        NS_ERROR("nsLDAPConnection::Run() couldn't initialize "
                   "thread-specific data");
        return NS_ERROR_FAILURE;
    }

    // wait for results
    //
    while(1) {
        PRBool operationFinished = PR_TRUE;
        nsCOMPtr<nsILDAPConnection> conn;

        // Exit this thread if we no longer have an nsLDAPConnection
        // associcated with it. We also aquire a lock here, to make sure
        // to avoid a possible race condition when the nsLDAPConnection
        // is destructed during the call to do_QueryReferent() (since
        // that function isn't MT safe).
        //
        PR_Lock(mLock);
        conn = do_QueryReferent(mWeakConn);
        PR_Unlock(mLock);

        if (!conn) {
            mWeakConn = 0;
            return NS_OK;
        }

        nsLDAPConnection *rawConn = NS_STATIC_CAST(nsLDAPConnection *,
                             NS_STATIC_CAST(nsILDAPConnection *, conn));

        // in case something went wrong on the last iteration, be sure to 
        // cause nsCOMPtr to release the message before going to sleep in
        // ldap_result
        //
        msg = 0;

        // XXX deal with timeouts better
        //
        // returnCode = ldap_result(rawConn->mConnectionHandle,
        returnCode = ldap_result(rawConn->mConnectionHandle,
                                 LDAP_RES_ANY, LDAP_MSG_ONE,
                                 &timeout, &msgHandle);

        // if we didn't error or timeout, create an nsILDAPMessage
        //      
        switch (returnCode) {

        case 0: // timeout

            // the connection may not exist yet.  sleep for a while
            // and try again
            //
            PR_LOG(gLDAPLogModule, PR_LOG_WARNING, 
                   ("ldap_result() timed out.\n"));
            conn = 0;

            // The sleep here is to avoid a problem where the LDAP
            // Connection/thread isn't ready quite yet, and we want to
            // avoid a very busy loop.
            //
            PR_Sleep(sleepTime);
            continue;

        case -1: // something went wrong 

            lderrno = ldap_get_lderrno(rawConn->mConnectionHandle, 0, 0);

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
                break;

            case LDAP_DECODING_ERROR:
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: WARNING: decoding error; possible corrupt data received").get());
                NS_WARNING("nsLDAPConnection::Run(): ldaperrno = "
                           "LDAP_DECODING_ERROR after ldap_result()");
                break;

            case LDAP_NO_MEMORY:
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: ERROR: couldn't allocate memory while getting async operation result").get());
                // punt and hope things work out better next time around
                break;

            default:
                // shouldn't happen; internal error
                //
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: DEBUG: ldaperrno set to unexpected value after ldap_result() call in nsLDAPConnection::Run()").get());
                NS_WARNING("nsLDAPConnection::Run(): ldaperrno set to "
                           "unexpected value after ldap_result() "
                           "call in nsLDAPConnection::Run()");
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
            nsLDAPMessage *rawMsg = new nsLDAPMessage();
            if (!rawMsg) {
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: ERROR: couldn't allocate memory for new LDAP message; search entry dropped").get());
                // punt and hope things work out better next time around
                break;
            }

            // initialize the message, using a protected method not available
            // through nsILDAPMessage (which is why we need the raw pointer)
            //
            rv = rawMsg->Init(conn, msgHandle);

            switch (rv) {

            case NS_OK: 
                break;

            case NS_ERROR_LDAP_DECODING_ERROR:
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: WARNING: decoding error; possible corrupt data received").get());
                NS_WARNING("nsLDAPConnection::Run(): ldaperrno = "
                           "LDAP_DECODING_ERROR after ldap_result()");
                continue;

            case NS_ERROR_OUT_OF_MEMORY:
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: ERROR: couldn't allocate memory for new LDAP message; search entry dropped").get());
                // punt and hope things work out better next time around
                continue;

            case NS_ERROR_ILLEGAL_VALUE:
            case NS_ERROR_UNEXPECTED:
            default:
                // shouldn't happen; internal error
                //
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: DEBUG: nsLDAPConnection::Run(): nsLDAPMessage::Init() returned unexpected value").get());
                NS_WARNING("nsLDAPConnection::Run(): nsLDAPMessage::Init() "
                           "returned unexpected value.");

                // punt and hope things work out better next time around
                continue;
            }

            // now let the scoping mechanisms provided by nsCOMPtr manage
            // the reference for us.
            //
            msg = rawMsg;

            // invoke the callback on the nsILDAPOperation corresponding to 
            // this message
            //
            rv = rawConn->InvokeMessageCallback(msgHandle, msg,
                                                    operationFinished);
            if (NS_FAILED(rv)) {
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: ERROR: problem invoking message callback").get());
                NS_ERROR("LDAP: ERROR: problem invoking message callback");
                // punt and hope things work out better next time around
                continue;
            }

#if 0
            // sleep for a while to workaround event queue flooding 
            // (bug 50104) so that it's possible to test cancelling, firing
            // status, etc.
            //
            PR_Sleep(1000);
#endif
            break;
        }       

    }

    // This will never happen, but here just in case.
    //
    return NS_OK;
}
