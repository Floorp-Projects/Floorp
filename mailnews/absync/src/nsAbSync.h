/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __nsIAbSync_h__
#define __nsIAbSync_h__

#include "nsIAbSync.h"
#include "nsIAbSyncPostListener.h"
#include "nsIAbSyncPostEngine.h"
#include "nsCOMPtr.h"
#include "nsIAddrDatabase.h"
#include "nsIAbDirectory.h"
#include "nsAbSyncCRCModel.h"

//
// Basic Sync Logic
// Keep a Sync mapping table like: 
//  
// ServerRecordID   - Unique ID for a record provided by the
//                    UAB server.
// LocalRecordID    - Local Unique ID, for mobile devices this
//                    is assigned by the mobile device.
// CRC              - CRC of the record last time we synced.
// Flags            - Operation to apply to this record.  ADD
//                    if it's new, MOD if it's been modified,
//                    RET if it was already sent to the server
//                    but an error occured, etc.
//
// Step 1:
// When the user begins a sync, run through the local database and update the 
// sync mapping table.  If the CRC has changed, mark the entry modified, if 
// it's a new record, add an entry and mark it new, if a record was deleted, 
// mark the entry deleted, etc. 
//
// Sync approach - server handles all conflict resolution:
//
// Step 2:
// Using the sync mapping table and the local records database, generate the change 
// list to send to the server.  Mark any modified or new record with the RET (retry) 
// flag. 
//
// Step 3:
// Get the response back from the server.  Since the communication was successful, 
// clear the RET (retry) flag on all records.  Then apply the server changes back 
// to the local database (updating the CRC's in the process).  Save the changes to 
// the local database, clear the sync mapping table flags and save the new sync 
// mapping table. 
//
typedef struct {
  PRInt32       serverID;
  PRInt32       localID;
  ulong         CRC;
  PRUint32      flags;
} syncMappingRecord;

#define     SYNC_MODIFIED     0x0001    // Must modify record on server
#define     SYNC_ADD          0x0002    // Must add record to server   
#define     SYNC_DELETED      0x0004    // Must delete record from server
#define     SYNC_RETRY        0x0008    // Sent to server but failed...must retry!
#define     SYNC_RENUMBER     0x0010    // Renumber on the server
#define     SYNC_PROCESSED    0x8000    // We processed the entry...nothing to do

//
// We need this structure for mapping our field names to the server
// field names
//
#define       kMaxColumns   38

typedef struct {
  const char  *abField;
  const char  *serverField;
} schemaStruct;

class nsAbSync : public nsIAbSync, public nsIAbSyncPostListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIABSYNC
  NS_DECL_NSIABSYNCPOSTLISTENER

  nsAbSync();
  virtual ~nsAbSync();
  /* additional members */

private:
  // Handy methods for listeners...
  nsresult        DeleteListeners();
  nsresult        NotifyListenersOnStartSync(PRInt32 aTransactionID, const PRUint32 aMsgSize);
  nsresult        NotifyListenersOnProgress(PRInt32 aTransactionID, PRUint32 aProgress, PRUint32 aProgressMax);
  nsresult        NotifyListenersOnStatus(PRInt32 aTransactionID, const PRUnichar *aMsg);
  nsresult        NotifyListenersOnStopSync(PRInt32 aTransactionID, nsresult aStatus, const PRUnichar *aMsg);

  nsresult        AnalyzeTheLocalAddressBook();
  nsresult        ProcessServerResponse(const char *aProtocolResponse);
  NS_IMETHOD      InitSchemaColumns();

  NS_IMETHOD      OpenAB(char *aAbName, nsIAddrDatabase **aDatabase);
  NS_IMETHOD      AnalyzeAllRecords(nsIAddrDatabase *aDatabase, nsIAbDirectory *directory);
  NS_IMETHOD      GenerateProtocolForCard(nsIAbCard *aCard, PRInt32 id, nsString &protLine);
  PRBool          ThisCardHasChanged(nsIAbCard *aCard, syncMappingRecord *syncRecord, nsString &protLine);
  nsresult        InternalCleanup();

  nsCOMPtr<nsIAbSyncPostEngine>   mPostEngine;
  nsString                        mPostString;
  nsIAbSyncListener               **mListenerArray;
  PRInt32                         mListenerArrayCount;
  PRInt32                         mCurrentState;

  // Setting for ABSync operations...
  char                            *mAbSyncServer;
  PRInt32                         mAbSyncPort;
  char                            *mAbSyncAddressBook;
  char                            *mAbSyncAddressBookFileName;

  PRInt32                         mTransactionID;
  PRInt32                         mCurrentPostRecord;

  nsCOMPtr<nsIFileSpec>           mHistoryFile;

  PRUint32                        mOldTableSize;
  syncMappingRecord               **mOldSyncMapingTable;
  PRUint32                        mNewTableSize;
  syncMappingRecord               **mNewSyncMapingTable;

  schemaStruct                    mSchemaMappingList[kMaxColumns];
};

#endif /* __nsIAbSync_h__ */
