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

#ifndef _tmQueue_H_
#define _tmQueue_H_

#include "tmUtils.h"
#include "tmVector.h"

class tmClient;
class tmTransaction;
class tmTransactionManager;

/**
  * This class isn't so much a queue as it is storage for transactions. It
  *   is set up to recieve and store transactions in a growing collection
  *   (using tmVectors). Different messages can be recieved from the 
  *   Transaction Manager(TM) the queue belongs to which can add and remove
  *   listeners, empty the queue (flush), and add messages to the queue.
  *
  * See the documentation in tmTransactionService.h for details on the
  *   messages you can send to and recieve from the queues in the TM
  */
class tmQueue
{

public:

  ////////////////////////////////////////////////////////////////////////////
  // Constructor & Destructor

  /**
    * Set the internal state to default values. Init() must be called 
    *   after construction to allocate the storage and set the name and ID.
    */
  tmQueue(): mID(0), mName(nsnull), mTM(nsnull) { }

  /**
    * Reclaim the memory allocated in Init(). Destroys the transactions in
    *   the transaction storage and the ids in the listener storage
    */
  virtual ~tmQueue();

  ////////////////////////////////////////////////////////////////////////////
  // Public Member Functions

  /**
    * Initialize internal storage vectors and set the name of the queue
    *   and the pointer to the TM container.
    *
    * @returns NS_OK if everything succeeds
    * @returns -1 if initialization fails
    */
  PRInt32 Init(const char* aName, PRUint32 aID, tmTransactionManager *aTM);

  // Queue Operations

  /**
    * Adds the clientID to the list of queue listeners. A reply is created
    *   and sent to the client. The reply contains both the name of the
    *   queue and the id, so the client can match the id to the name and
    *   then use the id in all further communications to the queue. All
    *   current transactions in the queue are then sent to the client.
    *
    * If the client was already attached the reply is sent, but not the
    *   outstanding transactions, the assumption being made that all 
    *   transactions have already been sent to the client.
    *
    * The reply is sent for all cases, with the return value in the status
    *   field.
    *
    * @returns >= 0 if the client was attached successfully
    * @returns -1 if the client was not attached
    * @returns -2 if the client was already attached
    */
  PRInt32 AttachClient(PRUint32 aClientID);

  /**
    * Removes the client from the list of queue listeners. A reply is created
    *   and sent to the client to indicate the success of the removal.
    *
    * The reply is sent for all cases, with the status field set to either
    *   -1 or NS_OK.
    *
    * @returns NS_OK on success
    * @returns -1 if client is not attached to this queue
    * @returns TM_SUCCESS_DELETE_QUEUE if there are no more listeners,
    *          instructing the Transaction Mangaer to delete the queue.
    */
  PRInt32 DetachClient(PRUint32 aClientID);

  /**
    * Removes all the transactions being held in the queue.
    *   A reply is created and sent to the client to indicate the
    *   completion of the operation.
    */
  void FlushQueue(PRUint32 aClientID);

  /**
    * Places the transaction passed in on the queue. Takes ownership of the
    *   transaction, deletes it in the destructor. A reply is created and
    *   sent to the client to indicate the success of the posting of the
    *   transaction.
    *
    * The reply is sent for all cases, with the status field containing the
    *   return value.
    *
    * @returns >= 0 if the message was posted properly. 
    * @returns -1 if the client posting is not attached to this queue,
    *          if the transaction has been posted to the wrong queue or
    *          if an error occured when trying to add the post to the 
    *          internal storage.
    */
  PRInt32 PostTransaction(tmTransaction *aTrans);

  // Accessors

  /**
    * @returns the ID of the queue
    */
  PRUint32 GetID() const { return mID; }

  /**
    * @returns the name of the queue
    */
  const char* GetName() const { return mName; }

protected:

  /**
    * Helper method to determine if the client has already attached.
    *
    * @returns PR_TRUE if the client is attached to the queue.
    * @returns PR_FALSE if the client is not attached to the queue.
    */
  PRBool IsAttached(PRUint32 aClientID);

  ////////////////////////////////////////////////////////////////////////////
  // Protected Member Variables

  // storage
  tmVector mTransactions;     // transactions that have been posted
  tmVector mListeners;        // programs listening to this queue

  // bookkeeping
  PRUint32 mID;               // a number linked to the name in the mTM
  char *mName;                // format: [namespace][domainname(ie prefs)]
  tmTransactionManager *mTM;  // the container that holds the queue

};

#endif
