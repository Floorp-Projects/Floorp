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
#include <stdlib.h>
#include "tmQueue.h"
#include "tmTransactionManager.h"
#include "tmTransaction.h"
#include "tmUtils.h"

///////////////////////////////////////////////////////////////////////////////
// Constructors & Destructor & Initializer

tmTransactionManager::~tmTransactionManager() {

  PRUint32 size = mQueues.Size();
  tmQueue *queue = nsnull;
  for (PRUint32 index = 0; index < size; index++) {
     queue = (tmQueue *)mQueues[index];
     if (queue) {
       delete queue;
     }
  }
}

PRInt32
tmTransactionManager::Init() {
  return mQueues.Init();
}

///////////////////////////////////////////////////////////////////////////////
// public transaction module methods

void
tmTransactionManager::HandleTransaction(tmTransaction *aTrans) {

  PRUint32 action = aTrans->GetAction();
  PRUint32 ownerID = aTrans->GetOwnerID();
  tmQueue *queue = nsnull;

  // get the right queue -- attaches do it differently
  if (action == TM_ATTACH) {
    const char *name = (char*) aTrans->GetMessage(); // is qName for Attaches
    queue = GetQueue(name);  
    if (!queue) {
      PRInt32 index = AddQueue(name);
      if (index >= 0)
        queue = GetQueue(index); // GetQueue may return nsnull
    }
  }
  else  // all other trans should have a valid queue ID already
    queue = GetQueue(aTrans->GetQueueID());

  if (queue) {
    // All possible actions should have a case, default is not valid
    //   delete trans when done with them, let the queue own the trans
    //   that are posted to them.
    PRInt32 result = 0;
    switch (action) {
    case TM_ATTACH:
      queue->AttachClient(ownerID);
      break;
    case TM_POST:
      result = queue->PostTransaction(aTrans);
      if (result >= 0) // post failed, aTrans cached in a tmQueue
        return;
      break;
    case TM_FLUSH:
      queue->FlushQueue(ownerID);
      break;
    case TM_DETACH:
      if (queue->DetachClient(ownerID) == TM_SUCCESS_DELETE_QUEUE) {
        // the last client has been removed, remove the queue
        RemoveQueue(aTrans->GetQueueID()); // this _could_ be out of bounds
      }
      break;
    default:
      PR_NOT_REACHED("bad action in the transaction");
    }
  }
  delete aTrans;
}

///////////////////////////////////////////////////////////////////////////////
// Protected member functions

//
// Queue Handling
//

tmQueue*
tmTransactionManager::GetQueue(const char *aQueueName) {

  PRUint32 size = mQueues.Size();
  tmQueue *queue = nsnull;
  for (PRUint32 index = 0; index < size; index++) {
    queue = (tmQueue*) mQueues[index];
    if (queue && strcmp(queue->GetName(), aQueueName) == 0)
      return queue;
  }
  return nsnull;
}

// if successful the nsresult contains the index of the added queue
PRInt32
tmTransactionManager::AddQueue(const char *aQueueName) {

  tmQueue* queue = new tmQueue();
  if (!queue)
    return -1;
  PRInt32 index = mQueues.Append(queue);
  if (index < 0)
    delete queue;
  else
    queue->Init(aQueueName, index, this);
  return index;
}

void
tmTransactionManager::RemoveQueue(PRUint32 aQueueID) {
  PR_ASSERT(aQueueID <= mQueues.Size());

  tmQueue *queue = (tmQueue*)mQueues[aQueueID];
  if (queue) {
    mQueues.RemoveAt(aQueueID);
    delete queue;
  }
}
