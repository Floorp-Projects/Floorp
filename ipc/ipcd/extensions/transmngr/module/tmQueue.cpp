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

#include "plstr.h"
#include "tmQueue.h"
#include "tmTransaction.h"
#include "tmTransactionManager.h"
#include "tmUtils.h"

///////////////////////////////////////////////////////////////////////////////
// Constructors & Destructor

tmQueue::~tmQueue() {

  // empty the vectors
  PRUint32 index = 0;
  PRUint32 size = mTransactions.Size();
  for ( ; index < size ; index++) {
    if (mTransactions[index])
      delete (tmTransaction *)mTransactions[index];
  }

  // don't need to delete the mListeners because 
  // we just insert PRUint32s, no allocation

  mTM = nsnull;
  mID = 0;
  if (mName)
    PL_strfree(mName);
}

///////////////////////////////////////////////////////////////////////////////
// Public Methods

PRInt32
tmQueue::Init(const char* aName, PRUint32 aID, tmTransactionManager *aTM) {
  PR_ASSERT(mTM == nsnull);

  if (NS_SUCCEEDED(mTransactions.Init()) &&
      NS_SUCCEEDED(mListeners.Init()) &&
      ((mName = PL_strdup(aName)) != nsnull) ) {
    mTM = aTM;
    mID = aID;
    return NS_OK;
  }
  return -1;
}

PRInt32
tmQueue::AttachClient(PRUint32 aClientID) {

  PRInt32 status = NS_OK;                 // success of adding client

  if (!IsAttached(aClientID)) {
    // add the client to the listener list -- null safe call
    status = mListeners.Append((void*) aClientID);
  }
  else
    status = -2;

  // create & init a reply transaction
  tmTransaction trans;
  if (NS_SUCCEEDED(trans.Init(aClientID,        // owner's ipc ID
                              mID,              // client gets our ID
                              TM_ATTACH_REPLY,  // action
                              status,           // success of the add
                              (PRUint8*)mName,  // client matches name to ID
                              PL_strlen(mName)+1))) {
    // send the reply
    mTM->SendTransaction(aClientID, &trans);
  }

  // if we successfully added the client - send all current transactions
  if (status >= 0) { // append returns the index of the added element
    
    PRUint32 size = mTransactions.Size();
    for (PRUint32 index = 0; index < size; index++) {
      if (mTransactions[index])
        mTM->SendTransaction(aClientID, (tmTransaction*) mTransactions[index]);
    }
  }
  return status;
}

PRInt32
tmQueue::DetachClient(PRUint32 aClientID) {

  PRUint32 size = mListeners.Size();
  PRUint32 id = 0;
  PRInt32 status = -1;

  for (PRUint32 index = 0; index < size; index++) {
    id = (PRUint32)NS_PTR_TO_INT32(mListeners[index]);
    if(id == aClientID) {
      mListeners.RemoveAt(index);
      status = NS_OK;
      break;
    }
  }

  tmTransaction trans;
  if (NS_SUCCEEDED(trans.Init(aClientID,
                               mID,
                               TM_DETACH_REPLY,
                               status,
                               nsnull,
                               0))) {
    // send the reply
    mTM->SendTransaction(aClientID, &trans);
  }

  // if we've removed all the listeners, remove the queue.
  if (mListeners.Size() == 0)
    return TM_SUCCESS_DELETE_QUEUE;
  return status;
}

void
tmQueue::FlushQueue(PRUint32 aClientID) {

  if(!IsAttached(aClientID))
    return;

  PRUint32 size = mTransactions.Size();
  for (PRUint32 index = 0; index < size; index++)
    if (mTransactions[index])
      delete (tmTransaction*)mTransactions[index];

  mTransactions.Clear();

  tmTransaction trans;
  if (NS_SUCCEEDED(trans.Init(aClientID,
                               mID, 
                               TM_FLUSH_REPLY, 
                               NS_OK,
                               nsnull,
                               0))) {
    mTM->SendTransaction(aClientID, &trans);
  }
}

PRInt32
tmQueue::PostTransaction(tmTransaction *aTrans) {

  PRInt32 status = -1;
  PRUint32 ownerID = aTrans->GetOwnerID();

  // if we are attached, have the right queue and have successfully
  //    appended the transaction to the queue, send the transaction
  //    to all the listeners.

  if (IsAttached(ownerID) && aTrans->GetQueueID() == mID)
    status = mTransactions.Append(aTrans);

  if (status >= 0) {
    // send the transaction to all members of mListeners except the owner
    PRUint32 size = mListeners.Size();
    PRUint32 id = 0;
    for (PRUint32 index = 0; index < size; index++) {
      id = (PRUint32)NS_PTR_TO_INT32(mListeners[index]);
      if (ownerID != id)
        mTM->SendTransaction(id, aTrans);
    }
  }

  tmTransaction trans;
  if (NS_SUCCEEDED(trans.Init(ownerID,
                              mID,
                              TM_POST_REPLY,
                              status,
                              nsnull,
                              0))) {
    // send the reply
    mTM->SendTransaction(ownerID, &trans);
  }
  return status;
}

PRBool
tmQueue::IsAttached(PRUint32 aClientID) {
  // XXX could be an issue if the aClientID is 0 and there
  // is a "hole" in the mListeners vector. - may NEED to store PRUint32*s
  PRUint32 size = mListeners.Size();
  for (PRUint32 index = 0; index < size; index++) {
    if (aClientID == (PRUint32)NS_PTR_TO_INT32(mListeners[index]))
      return PR_TRUE;
  }
  return PR_FALSE;
}
