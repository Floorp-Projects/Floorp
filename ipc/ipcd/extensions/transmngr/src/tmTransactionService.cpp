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
 * The Original Code is Mozilla Transaction Manager.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt <jgaunt@netscape.com>
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

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "plstr.h"
#include "ipcITransactionObserver.h"
#include "tmTransaction.h"
#include "tmTransactionService.h"
#include "tmUtils.h"

static const nsID kTransModuleID = TRANSACTION_MODULE_ID;

struct tm_waiting_msg {
  tmTransaction trans;      // a transaction waiting to be sent to a queue
  char*         domainName; // the short queue name

  ~tm_waiting_msg();
};

tm_waiting_msg::~tm_waiting_msg() {
  if (domainName)
    PL_strfree(domainName);
}

struct tm_queue_mapping {
  PRInt32 queueID;          // the ID in the TM
  char*   domainName;       // used by the consumers of this service
  char*   joinedQueueName;  // used by the service -- namespace + domain name

  ~tm_queue_mapping();
};

tm_queue_mapping::~tm_queue_mapping() {
  if (domainName)
    PL_strfree(domainName);
  if (joinedQueueName)
    PL_strfree(joinedQueueName);
}

//////////////////////////////////////////////////////////////////////////////
// Constructor and Destructor

tmTransactionService::~tmTransactionService() {

  // just destroy this, it contains 2 pointers it doesn't own.
  if (mObservers)
    PL_HashTableDestroy(mObservers);

  PRUint32 index = 0;
  PRUint32 size = mWaitingMessages.Size();
  tm_waiting_msg *msg = nsnull;
  for ( ; index < size; index ++) {
    msg = (tm_waiting_msg*) mWaitingMessages[index];
    delete msg;
  }

  size = mQueueMaps.Size();
  tm_queue_mapping *qmap = nsnull;
  for (index = 0; index < size; index++) {
    qmap = (tm_queue_mapping*) mQueueMaps[index];
    if (qmap)
      delete qmap;
  }
}

//////////////////////////////////////////////////////////////////////////////
// ISupports

NS_IMPL_ISUPPORTS2(tmTransactionService,
                   ipcITransactionService,
                   ipcIMessageObserver)

//////////////////////////////////////////////////////////////////////////////
// ipcITransactionService

NS_IMETHODIMP
tmTransactionService::Init(const nsACString & aNamespace) {

  // register with the IPC service
  ipcService = do_GetService("@mozilla.org/ipc/service;1");
  if (!ipcService)
    return NS_ERROR_FAILURE;
  if(NS_FAILED(ipcService->SetMessageObserver(kTransModuleID, this)))
    return NS_ERROR_FAILURE;

  // get the lock service
  lockService = do_GetService("@mozilla.org/ipc/lock-service;1");
  if (!lockService)
    return NS_ERROR_FAILURE;

  // create some internal storage
  mObservers = PL_NewHashTable(20, 
                               PL_HashString, 
                               PL_CompareStrings, 
                               PL_CompareValues, 0, 0);
  if (!mObservers)
    return NS_ERROR_FAILURE;

  // init some internal storage
  mQueueMaps.Init();
  mWaitingMessages.Init();

  // store the namespace
  mNamespace.Assign(aNamespace);
  return NS_OK;
}

NS_IMETHODIMP
tmTransactionService::Attach(const nsACString & aDomainName, 
                             ipcITransactionObserver *aObserver,
                             PRBool aLockingCall) {

  // if the queue already exists, then someone else is attached to it. must
  //   return an error here. Only one module attached to a queue per app.
  if (GetQueueID(aDomainName) != TM_NO_ID)
    return TM_ERROR_QUEUE_EXISTS;

  // create the full queue name: namespace + queue
  nsCString jQName;
  jQName.Assign(mNamespace);
  jQName.Append(aDomainName);

  // this char* has two homes, make sure it gets PL_free()ed properly
  char* joinedQueueName = ToNewCString(jQName);
  if (!joinedQueueName)
    return NS_ERROR_OUT_OF_MEMORY;

  // link the observer to the joinedqueuename.  home #1 for joinedQueueName
  // these currently don't get removed until the destructor on this is called.
  PL_HashTableAdd(mObservers, joinedQueueName, aObserver);

  // store the domainName and JoinedQueueName, create a place to store the ID
  tm_queue_mapping *qm = new tm_queue_mapping();
  if (!qm)
    return NS_ERROR_OUT_OF_MEMORY;
  qm->queueID = TM_NO_ID;                   // initially no ID for the queue
  qm->joinedQueueName = joinedQueueName;    // home #2, owner of joinedQueueName
  qm->domainName = ToNewCString(aDomainName);
  if (!qm->domainName) {
    PL_HashTableRemove(mObservers, joinedQueueName);
    delete qm;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mQueueMaps.Append(qm);

  nsresult rv = NS_ERROR_FAILURE;
  tmTransaction trans;

  // acquire a lock if neccessary
  if (aLockingCall)
    lockService->AcquireLock(joinedQueueName, nsnull, PR_TRUE);

  if (NS_SUCCEEDED(trans.Init(0,                             // no IPC client
                              TM_NO_ID,                      // qID gets returned to us
                              TM_ATTACH,                     // action
                              NS_OK,                         // default status
                              (PRUint8 *)joinedQueueName,    // qName gets copied
                              PL_strlen(joinedQueueName)+1))) { // message length
    // send the attach msg
    SendMessage(&trans, PR_TRUE);  // synchronous
    rv = NS_OK;
  }

  // drop the lock if neccessary
  if (aLockingCall)
    lockService->ReleaseLock(joinedQueueName);

  return rv;
}

// actual removal of the observer takes place when we get the detach reply
NS_IMETHODIMP
tmTransactionService::Detach(const nsACString & aDomainName) {

  // asynchronous detach
  return SendDetachOrFlush(GetQueueID(aDomainName), TM_DETACH, PR_FALSE);

}

NS_IMETHODIMP
tmTransactionService::Flush(const nsACString & aDomainName,
                            PRBool aLockingCall) {
  // acquire a lock if neccessary
  if (aLockingCall)
    lockService->AcquireLock(GetJoinedQueueName(aDomainName), nsnull, PR_TRUE);

  // synchronous flush
  nsresult rv = SendDetachOrFlush(GetQueueID(aDomainName), TM_FLUSH, PR_TRUE);

  // drop the lock if neccessary
  if (aLockingCall)
    lockService->ReleaseLock(GetJoinedQueueName(aDomainName));

  return rv;

}

NS_IMETHODIMP
tmTransactionService::PostTransaction(const nsACString & aDomainName, 
                                      const PRUint8 *aData, 
                                      PRUint32 aDataLen) {

  tmTransaction trans;
  if (NS_SUCCEEDED(trans.Init(0,                       // no IPC client
                              GetQueueID(aDomainName), // qID returned to us
                              TM_POST,                 // action
                              NS_OK,                   // default status
                              aData,                   // message data
                              aDataLen))) {             // message length
    if (trans.GetQueueID() == TM_NO_ID) {
      // stack it and pack it
      tm_waiting_msg *msg = new tm_waiting_msg(); 
      if (!msg)
        return NS_ERROR_OUT_OF_MEMORY;
      msg->trans = trans;
      msg->domainName = ToNewCString(aDomainName);
      if (!msg->domainName) {
        delete msg;
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mWaitingMessages.Append(msg);
    }
    else {
      // send it
      SendMessage(&trans, PR_FALSE);
    }
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//////////////////////////////////////////////////////////////////////////////
// ipcIMessageObserver

NS_IMETHODIMP
tmTransactionService::OnMessageAvailable(const nsID & aTarget, 
                                         const PRUint8 *aData, 
                                         PRUint32 aDataLength) {

  nsresult rv = NS_ERROR_OUT_OF_MEMORY; // prime the return value

  tmTransaction *trans = new tmTransaction();
  if (trans) {
    rv = trans->Init(0,                      // no IPC client ID
                     TM_INVALID_ID,          // in aData
                     TM_INVALID_ID,          // in aData
                     TM_INVALID_ID,          // in aData
                     aData,                  // message data
                     aDataLength);           // message length

    if (NS_SUCCEEDED(rv)) {
      switch(trans->GetAction()) {
      case TM_ATTACH_REPLY:
        OnAttachReply(trans);
        break;
      case TM_POST_REPLY:
        // OnPostReply() would be called here
        //   isn't neccessary at the current time 2/19/03
        break;
      case TM_DETACH_REPLY:
        OnDetachReply(trans);
        break;
      case TM_FLUSH_REPLY:
        OnFlushReply(trans);
        break;
      case TM_POST:
        OnPost(trans);
        break;
      default: // error, should not happen
        NS_NOTREACHED("Recieved a TM reply outside of mapped messages");
        break;
      }
    }
    delete trans;
  }
  return rv;
}

//////////////////////////////////////////////////////////////////////////////
// Protected Member Functions

void
tmTransactionService::SendMessage(tmTransaction *aTrans, PRBool aSync) {

  NS_ASSERTION(aTrans, "tmTransactionService::SendMessage called with null transaction");
  NS_ASSERTION(ipcService, "Failed to get the ipcService");

  ipcService->SendMessage(0, 
                          kTransModuleID, 
                          aTrans->GetRawMessage(), 
                          aTrans->GetRawMessageLength(),
                          aSync);
}

void
tmTransactionService::OnAttachReply(tmTransaction *aTrans) {

  // if we attached, store the queue's ID
  if (aTrans->GetStatus() >= 0) {

    PRUint32 size = mQueueMaps.Size();
    tm_queue_mapping *qmap = nsnull;
    for (PRUint32 index = 0; index < size; index++) {
      qmap = (tm_queue_mapping*) mQueueMaps[index];
      if (qmap && 
          PL_strcmp(qmap->joinedQueueName, (char*) aTrans->GetMessage()) == 0) {

        // set the ID in the mapping
        qmap->queueID = aTrans->GetQueueID();
        // send any stored messges to the queue
        DispatchStoredMessages(qmap);
      }
    }
  }

  // notify the observer we have attached (or didn't)
  ipcITransactionObserver *observer = 
    (ipcITransactionObserver *)PL_HashTableLookup(mObservers, 
                                                 (char*)aTrans->GetMessage());
  if (observer)
    observer->OnAttachReply(aTrans->GetQueueID(), aTrans->GetStatus());
}

void
tmTransactionService::OnDetachReply(tmTransaction *aTrans) {

  tm_queue_mapping *qmap = GetQueueMap(aTrans->GetQueueID());

  // get the observer before we release the hashtable entry
  ipcITransactionObserver *observer = 
    (ipcITransactionObserver *)PL_HashTableLookup(mObservers, 
                                                 qmap->joinedQueueName);

  // if it was removed, clean up
  if (aTrans->GetStatus() >= 0) {

    // remove the link between observer and queue
    PL_HashTableRemove(mObservers, qmap->joinedQueueName);

    // remove the mapping of queue names and id
    mQueueMaps.Remove(qmap);
    delete qmap;
  }


  // notify the observer -- could be didn't detach
  if (observer)
    observer->OnDetachReply(aTrans->GetQueueID(), aTrans->GetStatus());
}

void
tmTransactionService::OnFlushReply(tmTransaction *aTrans) {

  ipcITransactionObserver *observer = 
    (ipcITransactionObserver *)PL_HashTableLookup(mObservers, 
                              GetJoinedQueueName(aTrans->GetQueueID()));
  if (observer)
    observer->OnFlushReply(aTrans->GetQueueID(), aTrans->GetStatus());
}

void
tmTransactionService::OnPost(tmTransaction *aTrans) {

  ipcITransactionObserver *observer = 
    (ipcITransactionObserver*) PL_HashTableLookup(mObservers, 
                              GetJoinedQueueName(aTrans->GetQueueID()));
  if (observer)
    observer->OnTransactionAvailable(aTrans->GetQueueID(), 
                                     aTrans->GetMessage(), 
                                     aTrans->GetMessageLength());
}

void
tmTransactionService::DispatchStoredMessages(tm_queue_mapping *aQMapping) {

  PRUint32 size = mWaitingMessages.Size();
  tm_waiting_msg *msg = nsnull;
  for (PRUint32 index = 0; index < size; index ++) {
    msg = (tm_waiting_msg*) mWaitingMessages[index];
    // if the message is waiting on the queue passed in
    if (msg && strcmp(aQMapping->domainName, msg->domainName) == 0) {

      // found a match, send it and remove
      msg->trans.SetQueueID(aQMapping->queueID);
      SendMessage(&(msg->trans), PR_FALSE);

      // clean up
      mWaitingMessages.Remove(msg);
      delete msg;
    }
  }
}

// searches against the short queue name
PRInt32
tmTransactionService::GetQueueID(const nsACString & aDomainName) {

  PRUint32 size = mQueueMaps.Size();
  tm_queue_mapping *qmap = nsnull;
  for (PRUint32 index = 0; index < size; index++) {
    qmap = (tm_queue_mapping*) mQueueMaps[index];
    if (qmap && aDomainName.Equals(qmap->domainName))
      return qmap->queueID;
  }
  return TM_NO_ID;
}

char*
tmTransactionService::GetJoinedQueueName(PRUint32 aQueueID) {

  PRUint32 size = mQueueMaps.Size();
  tm_queue_mapping *qmap = nsnull;
  for (PRUint32 index = 0; index < size; index++) {
    qmap = (tm_queue_mapping*) mQueueMaps[index];
    if (qmap && qmap->queueID == aQueueID)
      return qmap->joinedQueueName;
  }
  return nsnull;
}

char*
tmTransactionService::GetJoinedQueueName(const nsACString & aDomainName) {

  PRUint32 size = mQueueMaps.Size();
  tm_queue_mapping *qmap = nsnull;
  for (PRUint32 index = 0; index < size; index++) {
    qmap = (tm_queue_mapping*) mQueueMaps[index];
    if (qmap && aDomainName.Equals(qmap->domainName))
      return qmap->joinedQueueName;
  }
  return nsnull;
}

tm_queue_mapping*
tmTransactionService::GetQueueMap(PRUint32 aQueueID) {

  PRUint32 size = mQueueMaps.Size();
  tm_queue_mapping *qmap = nsnull;
  for (PRUint32 index = 0; index < size; index++) {
    qmap = (tm_queue_mapping*) mQueueMaps[index];
    if (qmap && qmap->queueID == aQueueID)
      return qmap;
  }
  return nsnull;
}

nsresult
tmTransactionService::SendDetachOrFlush(PRUint32 aQueueID,
                                        PRUint32 aAction, 
                                        PRBool aSync) {

  // if the queue isn't attached to, just return
  if (aQueueID == TM_NO_ID)
    return NS_ERROR_UNEXPECTED;

  tmTransaction trans;
  if (NS_SUCCEEDED(trans.Init(0,         // no IPC client
                              aQueueID,  // qID to detach from
                              aAction,   // action
                              NS_OK,     // default status
                              nsnull,    // no message
                              0))) {      // no message
    // send it
    SendMessage(&trans, aSync);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}
