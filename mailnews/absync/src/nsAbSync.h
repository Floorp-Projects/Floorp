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
#include "nsVoidArray.h"

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

#define SYNC_ALLTAGS             1000
#define SYNC_EMAILS              2000
#define ABSYNC_PROTOCOL          3
#define ABSYNC_VERSION           "1"

#define SYNC_ESCAPE_ADDUSER             "op%3Dadd"
#define SYNC_ESCAPE_MOD                 "op%3Dmod"
#define SYNC_ESCAPE_DEL                 "op%3Ddel"

//mailing list
#define SYNC_ESCAPE_MAIL_ADD            "op%3DmaillistCreate"
#define SYNC_ESCAPE_MAIL_MOD            "op%3DmaillistRen"
#define SYNC_ESCAPE_MAIL_DEL            "op%3DmaillistDel"
#define SYNC_ESCAPE_MAIL_EMAIL_MOD      "op%3DemailstringUpdate"
#define SYNC_ESCAPE_MAIL_CONTACT_ADD    "op%3DmaillistAdd"
#define SYNC_ESCAPE_MAIL_CONTACT_DEL    "op%3DmaillistMemberDel"

// group
#define SYNC_ESCAPE_GROUP_DEL            "op%3DgrpDel"

// Defines for what type of add this may be?
#define SYNC_SINGLE_USER_TYPE            1
#define SYNC_MAILLIST_TYPE               2
#define SYNC_GROUP_TYPE                  3
#define SYNC_UNKNOWN_TYPE                0

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
  NS_IMETHOD      GenerateProtocolForCard(nsIAbCard *aCard, PRBool  aAddId, nsString &protLine);
  PRBool          ThisCardHasChanged(nsIAbCard *aCard, syncMappingRecord *syncRecord, nsString &protLine);
  void            InternalInit();
  nsresult        InternalCleanup();

  nsCOMPtr<nsIAbSyncPostEngine>   mPostEngine;
  nsString                        mPostString;
  nsIAbSyncListener               **mListenerArray;
  PRInt32                         mListenerArrayCount;
  PRInt32                         mCurrentState;

  PRInt32                         mLastChangeNum;
  char                            *mUserName;

  // Setting for ABSync operations...
  char                            *mAbSyncServer;
  PRInt32                         mAbSyncPort;
  char                            *mAbSyncAddressBook;
  char                            *mAbSyncAddressBookFileName;

  PRInt32                         mTransactionID;
  PRInt32                         mCurrentPostRecord;

  nsCOMPtr<nsIFileSpec>           mHistoryFile;

  PRUint32                        mOldTableSize;
  syncMappingRecord               *mOldSyncMapingTable;  // Old history table...
  PRUint32                        mNewTableSize;
  syncMappingRecord               *mNewSyncMapingTable;   // New table after reading address book
  PRUint32                        mNewServerTableSize;
  syncMappingRecord               *mNewServerTable;       // New entries from the server

  char                            *mProtocolResponse;     // what the server said...
  char                            *mProtocolOffset;       // where in the buffer are we?

  schemaStruct                    mSchemaMappingList[kMaxColumns];

  ///////////////////////////////////////////////
  // The following is for protocol parsing
  ///////////////////////////////////////////////
  PRBool          EndOfStream();                          // If this returns true, we are done with the data...
  PRBool          ParseNextSection();                     // Deal with next section
  nsresult        AdvanceToNextLine();
  nsresult        AdvanceToNextSection();
  char            *ExtractCurrentLine();
  nsresult        ExtractInteger(char *aLine, char *aTag, char aDelim, PRInt32 *aRetVal);
  char            *ExtractCharacterString(char *aLine, char *aTag, char aDelim);

  nsresult        PatchHistoryTableWithNewID(PRInt32 clientID, PRInt32 serverID);
  nsresult        DeleteRecord();
  nsresult        DeleteList();
  nsresult        DeleteGroup();
  nsresult        DeleteCardByServerID(PRInt32 aServerID);
  nsresult        LocateClientIDFromServerID(PRInt32 aServerID, PRInt32 *aClientID);
  PRInt32         DetermineTagType(nsStringArray *aArray);
  nsresult        AddNewUsers();
  nsresult        AddValueToNewCard(nsIAbCard *aCard, nsString *aTagName, nsString *aTagValue);

  PRBool          TagHit(char *aTag, PRBool advanceToNextLine); // See if we are sitting on a particular tag...and advance if asked 
  PRBool          ErrorFromServer(char **errString);      // Return true if the server returned an error...
  nsresult        ProcessOpReturn();
  nsresult        ProcessNewRecords();
  nsresult        ProcessDeletedRecords();
  nsresult        ProcessLastChange();

  nsString        mLocale;                                // Charset of returned data!
  nsStringArray   *mDeletedRecordTags;                    // The deleted record tags from the server...
  nsStringArray   *mDeletedRecordValues;                  // The deleted record values from the server...

  nsStringArray   *mNewRecordTags;                        // The new record tags from the server...
  nsStringArray   *mNewRecordValues;                      // The new record values from the server...
};

#endif /* __nsIAbSync_h__ */
