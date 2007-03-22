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

#ifndef _tmTransactionService_H_
#define _tmTransactionService_H_

#include "ipcdclient.h"
#include "ipcILockService.h"
#include "ipcIMessageObserver.h"
#include "ipcITransactionService.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "plhash.h"
#include "tmTransaction.h"
#include "tmVector.h"

struct tm_queue_mapping;

/**
  * The tmTransactionService shares packets of information
  *   (transactions) with other Gecko based applications interested in the same
  *   namespace and domain. An application registers with the Transaction Service
  *   for a particular namespace and domain and then can post transactions to the
  *   service and receive transactions from the service. 
  *
  * For applications using the Transaction Service to share changes in state that
  *   get reflected in files on disk there are certain pattersn to follow to ensure
  *   data loss does not occur. 
  *
  *   Startup: XXX docs needed
  *
  *   Shutdown/writing to disk: XXX docs needed
  *
  *
  */
class tmTransactionService : public ipcITransactionService,
                             public ipcIMessageObserver
{

public:

  ////////////////////////////////////////////////////////////////////////////
  // Constructor & Destructor
  tmTransactionService() : mObservers(0) {};

  /**
    * Reclaim all the memory allocated: PL_hashtable, tmVectors
    */
  virtual ~tmTransactionService();

  ////////////////////////////////////////////////////////////////////////////
  // Interface Declarations

  // for API docs, see the respective *.idl files
  NS_DECL_ISUPPORTS
  NS_DECL_IPCITRANSACTIONSERVICE
  NS_DECL_IPCIMESSAGEOBSERVER

protected:

  ////////////////////////////////////////////////////////////////////////////
  // Protected Member Functions

  /**
    * Pulls the raw message out of the transaction and sends it to the IPC
    *   service to be delivered to the TM.
    *
    * @param aTrans
    *        The transaction to send to the TM
    *
    * @param aSync 
    *        If TRUE, calling thread will be blocked until a reply is
    *        received.
    */
  void SendMessage(tmTransaction *aTrans, PRBool aSync);

  // handlers for reply messages from TransactionManager

  /**
    * Pulls the queueID out of the ATTACH_REPLY message and stores it in the
    *   proper tm_queue_mapping object. Calls DispatchStoredMessages() to make
    *   sure we send any messages that have been waiting on the ATTACH_REPLY.
    *   Also calls the OnAttachReply() method for the observer of the queue.
    */
  void OnAttachReply(tmTransaction *aTrans);

  /**
    * Removes the tm_queue_mapping object and calls the OnDetachReply() method
    *   on the observer of the queue detached.
    */
  void OnDetachReply(tmTransaction *aTrans);

  /**
    * Calls the OnFlushReply method of the observer of the queue.
    */
  void OnFlushReply(tmTransaction *aTrans);

  /**
    * Calls the OnPost method of the observer of the queue.
    */
  void OnPost(tmTransaction *aTrans);

  // other helper functions

  /**
    * Cycle through the collection of transactions waiting to go out and
    *   send any that are waiting on an ATTACH_REPLY from the queue
    *   specified by the tm_queue_mapping passed in.
    */
  void DispatchStoredMessages(tm_queue_mapping *aQMapping);

  // helper methods for accessing the void arrays

  /**
    * @returns the ID corresponding to the domain name passed in
    * @returns TM_NO_ID if the name is not found.
    */
  PRInt32 GetQueueID(const nsACString & aDomainName);

  /**
    * @returns the joined queue name - namespace + domain 
    *          (prefs, cookies etc) corresponding to the ID passed in.
    * @returns nsnull if the ID is not found.
    */
  char* GetJoinedQueueName(PRUint32 aQueueID);

  /**
    * @returns the joined queue name - namespace + domain 
    *          (prefs, cookies etc) corresponding to the ID passed in.
    * @returns nsnull if the ID is not found.
    */
  char* GetJoinedQueueName(const nsACString & aDomainName);

  /**
    * @returns the tm_queue_mapping object that contains the ID passed in.
    * @returns nsnull if the ID is not found.
    */
  tm_queue_mapping* GetQueueMap(PRUint32 aQueueID);

  /**
    * Helper method for Detach and Flush requests.
    */
  nsresult SendDetachOrFlush(PRUint32 aQueueID,
                             PRUint32 aAction,
                             PRBool aSync);

  ////////////////////////////////////////////////////////////////////////////
  // Protected Member Variables

  nsCString mNamespace;               // limit domains to the namespace
  PLHashTable *mObservers;            // maps qName -> ipcITransactionObserver

  tmVector mQueueMaps;                // queue - name - domain mappings
  tmVector mWaitingMessages;          // messages sent before ATTACH_REPLY

  nsCOMPtr<ipcILockService> lockService;  // cache the lock service

private:

};

#endif
