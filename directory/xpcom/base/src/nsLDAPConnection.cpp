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
 * The Original Code is the mozilla.org LDAP XPCOM component.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org> (original author)
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
{
  NS_INIT_ISUPPORTS();
}

// destructor
//
nsLDAPConnection::~nsLDAPConnection()
{
  int rc;

  PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("unbinding\n"));

  rc = ldap_unbind_s(this->mConnectionHandle);
  if (rc != LDAP_SUCCESS) {
      PR_LOG(gLDAPLogModule, PR_LOG_WARNING, 
             ("nsLDAPConnection::~nsLDAPConnection: %s\n", 
              ldap_err2string(rc)));
  }

  PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("unbound\n"));

  if (mBindName) {
      delete mBindName;
  }

  if (mPendingOperations) {
      delete mPendingOperations;
  }

}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsLDAPConnection, nsILDAPConnection, 
                              nsIRunnable);

NS_IMETHODIMP
nsLDAPConnection::Init(const char *aHost, PRInt16 aPort, const char *aBindName)
{
    nsresult rv;

    if ( !aHost ) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

#ifdef DEBUG
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
        mBindName = new nsCString(aBindName);
        if (!mBindName) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        mBindName = NULL;
    }

    this->mConnectionHandle = ldap_init(aHost, aPort ? aPort : LDAP_PORT);
    if ( this->mConnectionHandle == NULL ) {
        return NS_ERROR_FAILURE;  // the LDAP C SDK API gives no useful error
    }

    // initialize the threading functions for this connection
    //
    if (!nsLDAPThreadFuncsInit(this->mConnectionHandle)) {
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
    ldap_set_option(this->mConnectionHandle, LDAP_OPT_DEBUG_LEVEL, &lDebug);
    ldap_set_option(this->mConnectionHandle, LDAP_OPT_ASYNC_CONNECT, 
                    NS_REINTERPRET_CAST(void *, 0));
#endif

    // kick off a thread for result listening and marshalling
    // XXXdmose - should this be JOINABLE?
    // 
    rv = NS_NewThread(getter_AddRefs(mThread), this, 0, PR_UNJOINABLE_THREAD);
    if (NS_FAILED(rv)) {
        return NS_ERROR_NOT_AVAILABLE;
    }

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
    if (!aConnectionHandle) {
        NS_ERROR("nsLDAPConnection::GetConnectionHandle(): null pointer "
                 "passed in");
    }

    *aConnectionHandle = mConnectionHandle;
    return NS_OK;
}

/** 
 * Add an nsILDAPOperation to the list of operations pending on
 * this connection.  This is also mainly intended for use by the
 * nsLDAPOperation code.
 */
NS_IMETHODIMP
nsLDAPConnection::AddPendingOperation(nsILDAPOperation *aOperation)
{
    PRInt32 msgId;

    if (!aOperation) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    // find the message id
    //
    aOperation->GetMessageId(&msgId);

    // turn it into an nsVoidKey.  note that this is another spot that
    // assumes that sizeof(void*) >= sizeof(PRInt32).  
    //
    // XXXdmose  should really create an nsPRInt32Key.
    //
    nsVoidKey *key = new nsVoidKey(NS_REINTERPRET_CAST(void *, msgId));
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
NS_IMETHODIMP
nsLDAPConnection::RemovePendingOperation(nsILDAPOperation *aOperation)
{
    nsresult rv;
    PRInt32 msgId;

    NS_ENSURE_ARG_POINTER(aOperation);

    // find the message id
    //
    rv = aOperation->GetMessageId(&msgId);
    NS_ENSURE_SUCCESS(rv, rv);

    // turn it into an nsVoidKey.  note that this is another spot that
    // assumes that sizeof(void*) >= sizeof(PRInt32).  
    //
    // XXXdmose  should really create an nsPRInt32Key.
    //
    nsVoidKey *key = new nsVoidKey(NS_REINTERPRET_CAST(void *, msgId));
    if (!key) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!mPendingOperations->Remove(key)) {
#ifdef DEBUG
        PR_fprintf(PR_STDERR, "nsLDAPConnection::RemovePendingOperation was\n"
                   " unable to remove the requested item from the pending\n"
                   " operations queue.  This probably means that the item\n"
                   " in question didn't exist in the queue, which in turn\n"
                   " probably means that you have found a bug in the code\n"
                   " that calls this function.\n");
#endif

        delete key;
        return NS_ERROR_FAILURE;
    }

    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, 
           ("pending operation removed; total pending operations now = %d\n", 
            mPendingOperations->Count()));

    delete key;
    return NS_OK;
}

// for nsIRunnable.  this thread spins in ldap_result() awaiting the next
// message.  once one arrives, it dispatches it to the nsILDAPMessageListener 
// on the main thread.
//
// XXX do all returns from this function need to do thread cleanup?
//
NS_IMETHODIMP
nsLDAPConnection::Run(void)
{
    int lderrno;
    nsresult rv;
    PRInt32 returnCode;
    LDAPMessage *msgHandle;
    nsCOMPtr<nsILDAPMessage> msg;

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
        
        // in case something went wrong on the last iteration, be sure to 
        // cause nsCOMPtr to release the message before going to sleep in
        // ldap_result
        //
        msg = 0;

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
            //
            PR_LOG(gLDAPLogModule, PR_LOG_WARNING, 
                   ("ldap_result() timed out.\n"));
            PR_Sleep(2000); // XXXdmose - reasonable timeslice?
            continue;

            break;

        case -1: // something went wrong 

            lderrno = ldap_get_lderrno(mConnectionHandle, 0, 0);

            switch (lderrno) {

            case LDAP_SERVER_DOWN:
                // XXXreconnect or fail  ?
                break;

            case LDAP_DECODING_ERROR:
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: WARNING: decoding error; possible corrupt data received"));
                NS_WARNING("nsLDAPConnection::Run(): ldaperrno = "
                           "LDAP_DECODING_ERROR after ldap_result()");
                break;

            case LDAP_NO_MEMORY:
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: ERROR: couldn't allocate memory while getting async operation result"));
                // punt and hope things work out better next time around
                break;

            default:
                // shouldn't happen; internal error
                //
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: DEBUG: ldaperrno set to unexpected value after ldap_result() call in nsLDAPConnection::Run()"));
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
            msg = new nsLDAPMessage();
            if (!msg) {
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: ERROR: couldn't allocate memory for new LDAP message; search entry dropped"));
                // punt and hope things work out better next time around
                break;
            }

            rv = msg->Init(this, msgHandle);
            switch (rv) {

            case NS_OK: 
                break;

            case NS_ERROR_LDAP_DECODING_ERROR:
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: WARNING: decoding error; possible corrupt data received"));
                NS_WARNING("nsLDAPConnection::Run(): ldaperrno = "
                           "LDAP_DECODING_ERROR after ldap_result()");
                continue;
                break;

            case NS_ERROR_OUT_OF_MEMORY:
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: ERROR: couldn't allocate memory for new LDAP message; search entry dropped"));
                // punt and hope things work out better next time around
                continue;
                break;

            case NS_ERROR_ILLEGAL_VALUE:
            case NS_ERROR_UNEXPECTED:
            default:
                // shouldn't happen; internal error
                //
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: DEBUG: nsLDAPConnection::Run(): nsLDAPMessage::Init() returned unexpected value"));
                NS_WARNING("nsLDAPConnection::Run(): nsLDAPMessage::Init() "
                           "returned unexpected value.");

                // punt and hope things work out better next time around
                continue;
                break;
            }

            // invoke the callback on the nsILDAPOperation corresponding to 
            // this message
            //
            rv = InvokeMessageCallback(msgHandle, msg, operationFinished);
            if (NS_FAILED(rv)) {
                consoleSvc->LogStringMessage(
                    NS_LITERAL_STRING("LDAP: ERROR: problem invoking message callback"));
                NS_ERROR("LDAP: ERROR: problem invoking message callback");
                // punt and hope things work out better next time around
                continue;
                break;
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

    // XXX figure out how to break out of the while() loop and get here to
    // so we can expire (though not if DEBUG is defined, since gdb gets ill
    // if threads exit
    //
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
    if (data == nsnull) {

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
