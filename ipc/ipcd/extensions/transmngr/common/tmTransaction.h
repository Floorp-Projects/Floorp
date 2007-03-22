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

#ifndef _tmTransaction_H_
#define _tmTransaction_H_

#include "tmUtils.h"

//////////////////////////////////////////////////////////////////////////////
//
// Message format
//
// |------------------------------------|--
// |QueueID                             |  |
// |------------------------------------|  |
// | Action - Post/Flush/Attach etc     |  |- this is the tmHeader struct
// |------------------------------------|  |
// |Status                              |  | 
// |------------------------------------|  |
// |Padding                             |  |
// |------------------------------------|--
// |Message Data (payload)              |
// |------------------------------------|
//
// The Attach call is a special case in that it doesn't have a QueueID yet. A
//   QueueID will be 0's. The message Data will be the Queue Name String which
//   will be the profile name with a domain attached, a domain being 
//   [prefs|cookies|etc]
//
//////////////////////////////////////////////////////////////////////////////

/**
  * tmHeader contains various flags identifying 
  */
struct tmHeader {
  PRInt32  queueID;     // will be the index of the queue in the TM, can be < 0
  PRUint32 action;      // defined by tmUtils.h will be > 0
  PRInt32  status;      // return values from methods, could be < 0
  PRUint32 reserved;    // not currently used, maintaining word alignment
};

/**
  * Using tmTransaction:
  *
  *   After creating a tmTransaction either through new or as a member 
  *   or local variable a process must call Init() with the proper set of
  *   arguements to initialize the state of the transaction. tmTransaction is
  *   set up to accept 3 types of initialization.
  *
  *   1) Raw message - All data is carried in the byte pointer aMessage,
  *      args 2,3,4 should be set to TM_NO_ID and aLength 
  *      must be set to the full length of aMessage, including null
  *      termination if the payload is a null-term string and the size of the
  *      tmHeader struct preceeding the message. Currently this
  *      format is used at the IPC boundary, where we receive a byte pointer
  *      from the IPC Daemon.
  *
  *   2) Flags only - aQueueID, aAction and aStatus are all set. aMessage
  *      should be set to nsnull and aLength to 0. This format is used when
  *      sending reply messages (except for ATTACH_REPLY) and when the TS
  *      Transaction Service is sending "control" messages to the Manager -
  *      flush, detach, etc...
  *
  *   3) Flags and message - All arguements are set. The aMessage is only
  *      the message for the client app. aLength should be set to the length
  *      of aMessage and not include the length of the tmHeader struct.
  *   
  *   The only data member you can set post initialization is the QueueID.
  *      You should only call Init() once in the lifetime of a tmTransaction
  *      as it doesn't clean up the exisiting data before assigning the new
  *      data. Therefore it would leak most heinously if Init() were to be
  *      called twice.
  *
  *   mOwnerID only has relevance on the IPC daemon side of things. The 
  *      Transaction Service has no knowledge of this ID and makes no use
  *      of it.
  */
class tmTransaction
{

public:

  ////////////////////////////////////////////////////////////////////////////
  // Constructor(s) & Destructor

  tmTransaction(): mHeader(nsnull), mRawMessageLength(0), mOwnerID(0) { }

  virtual ~tmTransaction();

  ////////////////////////////////////////////////////////////////////////////
  // Public Member Functions

  // Initializer ////////////

  /**
    * Sets up the internal data of the transaction. Allows for 3 basic ways
    *   to call this function: No flags and just one big raw message, Just
    *   flags and no message, and finally flags and message. If the message
    *   exists it is copied into the transaction.
    *
    * @param aOwnerID is given to us by the IPC Daemon and is specific
    *        to that transport layer. It is only set when transactions
    *        are sent from the TM to the TS.
    *
    * @param aQueueID is the either the destination queue, or the queue from
    *        where this transaction is eminating
    *
    * @param aAction is the action that occured to generate this transaction
    *
    * @param aStatus is the success state of the action.
    *
    * @param aMessage can be a raw message including the 3 flags above or it
    *        can be just the "payload" of the transaction that the destination
    *        process is going deal with.
    *
    * @param aLength is the length of the message. If there is a null
    *        terminated string in the message make sure the length includes
    *        the null termination.
    *
    * @returns NS_OK if everything was successful
    * @returns NS_ERROR_OUT_OF_MEMORY if allocation of space for the
    *          copy of the message fails.
    */
  nsresult Init(PRUint32 aOwnerID,
                PRInt32 aQueueID,
                PRUint32 aAction,
                PRInt32 aStatus,
                const PRUint8 *aMessage, 
                PRUint32 aLength);

  // Data Accessors /////////

  /**
    * @returns a const byte pointer to the message
    */
  const PRUint8* GetMessage() const { return (PRUint8*)(mHeader + 1); }

  /**
    * @returns the length of the message
    */
  PRUint32 GetMessageLength() const { 
    return (mRawMessageLength > sizeof(tmHeader)) ?
      (mRawMessageLength - sizeof(tmHeader)) : 0;
  }

  /**
    * @returns a const pointer to the memory containing the
    *          flag information followed immediately by the message
    *          data.
    */
  const PRUint8* GetRawMessage() const { return (PRUint8*) mHeader; }

  /**
    * @returns the length of the flags and message combined
    */
  PRUint32 GetRawMessageLength() const { return mRawMessageLength; }

  /**
    * @returns the id of the destination or sending queue, depending on the
    *          direction of the transaction.
    */
  PRInt32 GetQueueID() const { return mHeader->queueID; }

  /**
    * @returns the action represented by this transaction
    */
  PRUint32 GetAction() const { return mHeader->action; }

  /**
    * @returns the success state, if applicable of the action leading
    *          up to this message
    */
  PRInt32 GetStatus() const { return mHeader->status; }

  /**
    * @returns the client ID (in IPC daemon terms) of the client who initiated
    *          the exchange that generated this transaction.
    */
  PRUint32 GetOwnerID() const { return mOwnerID; }

  // Data Mutator ///////////

  /**
    * Sets the ID of the destination or source queue. Init should have been
    *   called before the call to this function.
    */
  void SetQueueID(PRInt32 aID) { mHeader->queueID = aID; }

protected:

  ////////////////////////////////////////////////////////////////////////////
  // Protected Member Variables

  tmHeader* mHeader;            // points to beginning of entire message
  PRUint32  mRawMessageLength;  // length of entire message, incl tmHeader
  PRUint32  mOwnerID;           // client who sent this trans. - a IPC ClientID

};

#endif
