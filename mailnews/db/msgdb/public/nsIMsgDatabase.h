/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIMsgDatabase_h__
#define nsIMsgDatabase_h__

#include "nsISupports.h"
#include "MailNewsTypes.h"

class nsIMsgDBHdr;
class nsIDBChangeListener;

#include "nsIDBChangeAnnouncer.h"

class nsIEnumerator;
class nsThreadMessageHdr;       // XXX where's the public interface to this?
class nsMsgKeyArray;
class nsMsgKeySet;
class nsIDBFolderInfo;
class nsIMsgThread;

enum nsMsgDBCommitType {
  kSmallCommit,
  kLargeCommit,
  kSessionCommit,
  kCompressCommit
};

class nsIMsgDatabase : public nsIDBChangeAnnouncer {
public:
  // open local folder...
  NS_IMETHOD Open(nsIFileSpec *folderName, PRBool create, nsIMsgDatabase** pMessageDB, PRBool upgrading) = 0;
  NS_IMETHOD Close(PRBool forceCommit) = 0;
  NS_IMETHOD OpenMDB(const char *dbName, PRBool create) = 0;
  NS_IMETHOD CloseMDB(PRBool commit) = 0;
  NS_IMETHOD Commit(nsMsgDBCommitType commitType) = 0;
  // Force closed is evil, and we should see if we can do without it.
  // In 4.x, it was mainly used to remove corrupted databases.
  NS_IMETHOD ForceClosed(void) = 0;

  NS_IMETHOD GetDBFolderInfo(nsIDBFolderInfo **result) = 0;
  // get a message header for the given key. Caller must release()!
  NS_IMETHOD GetMsgHdrForKey(nsMsgKey key, nsIMsgDBHdr **msg) = 0;
  //Returns whether or not this database contains the given key
  NS_IMETHOD ContainsKey(nsMsgKey key, PRBool *containsKey) = 0;

   // Must call AddNewHdrToDB after creating. The idea is that you create
  // a new header, fill in its properties, and then call AddNewHdrToDB.
  // AddNewHdrToDB will send notifications to any listeners.
  NS_IMETHOD CreateNewHdr(nsMsgKey key, nsIMsgDBHdr **newHdr) = 0;

  NS_IMETHOD AddNewHdrToDB(nsIMsgDBHdr *newHdr, PRBool notify) = 0;

  NS_IMETHOD CopyHdrFromExistingHdr(nsMsgKey key, nsIMsgDBHdr *existingHdr, nsIMsgDBHdr **newHdr) = 0;

#if HAVE_INT_ENUMERATORS
  NS_IMETHOD EnumerateKeys(nsIEnumerator* *outputKeys) = 0;
#else
  NS_IMETHOD ListAllKeys(nsMsgKeyArray &outputKeys) = 0;
#endif
  NS_IMETHOD EnumerateMessages(nsIEnumerator* *result) = 0;
  NS_IMETHOD EnumerateThreads(nsIEnumerator* *result) = 0;

  // helpers for user command functions like delete, mark read, etc.

  NS_IMETHOD MarkHdrRead(nsIMsgDBHdr *msgHdr, PRBool bRead,
                         nsIDBChangeListener *instigator) = 0;

  // MDN support
  NS_IMETHOD MarkMDNNeeded(nsMsgKey key, PRBool bNeeded,
                           nsIDBChangeListener *instigator) = 0;

  // MarkMDNneeded only used when mail server is a POP3 server
  // or when the IMAP server does not support user defined
  // PERMANENTFLAGS
  NS_IMETHOD IsMDNNeeded(nsMsgKey key, PRBool *isNeeded) = 0;

  NS_IMETHOD MarkMDNSent(nsMsgKey key, PRBool bNeeded,
                         nsIDBChangeListener *instigator) = 0;
  NS_IMETHOD IsMDNSent(nsMsgKey key, PRBool *isSent) = 0;

// methods to get and set docsets for ids.
  NS_IMETHOD MarkRead(nsMsgKey key, PRBool bRead, 
                      nsIDBChangeListener *instigator) = 0;

  NS_IMETHOD MarkReplied(nsMsgKey key, PRBool bReplied, 
                         nsIDBChangeListener *instigator) = 0;

  NS_IMETHOD MarkForwarded(nsMsgKey key, PRBool bForwarded, 
                           nsIDBChangeListener *instigator) = 0;

  NS_IMETHOD MarkHasAttachments(nsMsgKey key, PRBool bHasAttachments, 
                                nsIDBChangeListener *instigator) = 0;

  NS_IMETHOD MarkThreadIgnored(nsThreadMessageHdr *thread, nsMsgKey threadKey, PRBool bIgnored,
                               nsIDBChangeListener *instigator) = 0;
  NS_IMETHOD MarkThreadWatched(nsThreadMessageHdr *thread, nsMsgKey threadKey, PRBool bWatched,
                               nsIDBChangeListener *instigator) = 0;

  NS_IMETHOD IsRead(nsMsgKey key, PRBool *pRead) = 0;
  NS_IMETHOD IsIgnored(nsMsgKey key, PRBool *pIgnored) = 0;
  NS_IMETHOD IsMarked(nsMsgKey key, PRBool *pMarked) = 0;
  NS_IMETHOD HasAttachments(nsMsgKey key, PRBool *pHasThem) = 0;

  NS_IMETHOD MarkAllRead(nsMsgKeyArray *thoseMarked) = 0;
  NS_IMETHOD MarkReadByDate (time_t te, time_t endDate, nsMsgKeyArray *markedIds) = 0;

  NS_IMETHOD DeleteMessages(nsMsgKeyArray* nsMsgKeys, nsIDBChangeListener *instigator) = 0;
  NS_IMETHOD DeleteMessage(nsMsgKey key, 
                           nsIDBChangeListener *instigator,
                           PRBool commit) = 0;
  NS_IMETHOD DeleteHeader(nsIMsgDBHdr *msgHdr, nsIDBChangeListener *instigator,
                          PRBool commit, PRBool notify) = 0;

  NS_IMETHOD UndoDelete(nsIMsgDBHdr *msgHdr) = 0;

  NS_IMETHOD MarkLater(nsMsgKey key, time_t *until) = 0;
  NS_IMETHOD MarkMarked(nsMsgKey key, PRBool mark,
                        nsIDBChangeListener *instigator) = 0;
  NS_IMETHOD MarkOffline(nsMsgKey key, PRBool offline,
                         nsIDBChangeListener *instigator) = 0;

  // returns NS_OK on success, NS_COMFALSE on failure
  NS_IMETHOD AllMsgKeysImapDeleted(const nsMsgKeyArray *keys) = 0;

  NS_IMETHOD MarkImapDeleted(nsMsgKey key, PRBool deleted,
                             nsIDBChangeListener *instigator) = 0;

  NS_IMETHOD GetFirstNew(nsMsgKey *result) = 0;
  NS_IMETHOD HasNew(void) = 0;  // returns NS_OK if true, NS_COMFALSE if false
  NS_IMETHOD ClearNewList(PRBool notify) = 0;
  NS_IMETHOD AddToNewList(nsMsgKey key) = 0;

  // used mainly to force the timestamp of a local mail folder db to
  // match the time stamp of the corresponding berkeley mail folder,
  // but also useful to tell the summary to mark itself invalid
  NS_IMETHOD SetSummaryValid(PRBool valid) = 0;

  // offline operations
  NS_IMETHOD ListAllOfflineOpIds(nsMsgKeyArray *offlineOpIds) = 0;
  NS_IMETHOD ListAllOfflineDeletes(nsMsgKeyArray *offlineDeletes) = 0;

  // thread interfaces.
  NS_IMETHOD GetThreadForMsgKey(nsMsgKey msgKey, nsIMsgThread **result) = 0;
  NS_IMETHOD GetThreadContainingMsgHdr(nsIMsgDBHdr *msgHdr, nsIMsgThread **result) = 0;

  NS_IMETHOD GetMsgKeySet(nsMsgKeySet **pSet) = 0;
  NS_IMETHOD SetMsgKeySet(char * setStr) = 0;

  NS_IMETHOD                GetHighWaterArticleNum(nsMsgKey *key) =0;
  NS_IMETHOD                GetLowWaterArticleNum(nsMsgKey *key) =0;
};

#endif // nsIMsgDatabase_h__
