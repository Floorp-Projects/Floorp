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

#ifndef _tmTransactionManager_H_
#define _tmTransactionManager_H_

#include "plhash.h"
#include "tmUtils.h"
#include "tmVector.h"
#include "tmIPCModule.h"

// forward declarations
class tmQueue;
class tmClient;
class tmTransaction;

/**
  * This class manages the flow of messages from the IPC daemon (coming to
  *   it through the tmIPCModule) that ultimately come from a Transaction
  *   Service (TS) in a mozilla based client somewhere. The message is
  *   delivered to the proper queue, where it is dealt with.
  *
  * New queues get created here as clients request them.
  */
class tmTransactionManager
{

public:

  ////////////////////////////////////////////////////////////////////////////
  // Constructor(s) & Destructor & Initializer

  /**
    * reclaim the memory allcoated during initialization
    */
  virtual ~tmTransactionManager();

  /**
    * Set up the storage of the queues - initialize the vector
    *
    * @returns NS_OK if successful
    * @returns -1 if initialization fails
    */
  PRInt32 Init();

  ////////////////////////////////////////////////////////////////////////////
  // Public Member Functions

  /**
    * Called from the tmIPCModule. Decide where to send the message and
    *   dispatch it. 
    */
  void HandleTransaction(tmTransaction *aTrans);

  /**
    * Called by the queues when they need to get a message back out to a 
    *   client.
    */
  void SendTransaction(PRUint32 aDestClientIPCID, tmTransaction *aTrans) {
    PR_ASSERT(aTrans);
    tmIPCModule::SendMsg(aDestClientIPCID, aTrans);
  }

protected:

  ////////////////////////////////////////////////////////////////////////////
  // Protected Member Functions

  // Queue management

  /**
    * @returns the queue indexed by the ID passed in, which could be nsnull
    */
  tmQueue* GetQueue(PRUint32 aQueueID) {
    return (tmQueue*) mQueues[aQueueID];
  }

  /**
    * @returns the queue with the name passed in
    * @returns nsnull if there is no queue with that name
    */
  tmQueue* GetQueue(const char *aQueueName);

  /**
    * If all is successful a new queue with the name provided will be created,
    *   and added to the collection of queues. It will be initialized and ready
    *   to have transactions added.
    *
    * This doesn't check for the existance of a queue with this name. IF
    *   there is already a queue with this name then you will
    *   get that when using GetQueue(qName) and never get the new queue
    *   created here. A call to GetQueue(qID) will be able to get at the new
    *   queue, however you had better cache the ID.
    *
    * @returns -1 if the queue can't be created, or is not added
    * @returns >= 0 if the queue was added successfully
    */
  PRInt32 AddQueue(const char *aQueueType);

  /**
    */
  void RemoveQueue(PRUint32 aQueueID);

  ////////////////////////////////////////////////////////////////////////////
  // Protected Member Variables

  tmVector mQueues;

private:

};

#endif
