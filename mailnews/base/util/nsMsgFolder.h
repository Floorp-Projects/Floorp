/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/********************************************************************************************************
 
   Interface for representing Messenger folders.
 
*********************************************************************************************************/

#ifndef nsMsgFolder_h__
#define nsMsgFolder_h__

#include "msgCore.h"
#include "nsIMsgFolder.h" /* include the interface we are going to support */
#include "nsRDFResource.h"
#include "nsIDBFolderInfo.h"
#include "nsIMsgDatabase.h"
#include "nsIMsgIncomingServer.h"
#include "nsCOMPtr.h"

 /* 
  * MsgFolder
  */ 

class NS_MSG_BASE nsMsgFolder: public nsRDFResource, public nsIMsgFolder
{
public: 
  nsMsgFolder(void);
  virtual ~nsMsgFolder(void);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIRDFResource

  NS_IMETHOD Init(const char *aURI);
  
  // nsICollection methods:
  NS_IMETHOD Count(PRUint32 *result) {
    return mSubFolders->Count(result);
  }
  NS_IMETHOD GetElementAt(PRUint32 i, nsISupports* *result) {
    return mSubFolders->GetElementAt(i, result);
  }
  NS_IMETHOD SetElementAt(PRUint32 i, nsISupports* value) {
    return mSubFolders->SetElementAt(i, value);
  }
  NS_IMETHOD AppendElement(nsISupports *aElement) {
    return mSubFolders->AppendElement(aElement);
  }
  NS_IMETHOD RemoveElement(nsISupports *aElement) {
    return mSubFolders->RemoveElement(aElement);
  }
  NS_IMETHOD Enumerate(nsIEnumerator* *result) {
    // nsMsgFolders only have subfolders, no message elements
    return mSubFolders->Enumerate(result);
  }
  NS_IMETHOD Clear(void) {
    return mSubFolders->Clear();
  }

  // nsIFolder methods:
  NS_IMETHOD GetURI(char* *name) { return nsRDFResource::GetValue(name); }
  NS_IMETHOD GetName(PRUnichar **name);
  NS_IMETHOD SetName(PRUnichar *name);
  NS_IMETHOD GetChildNamed(const char *name, nsISupports* *result);
  NS_IMETHOD GetSubFolders(nsIEnumerator* *result);
  NS_IMETHOD GetHasSubFolders(PRBool *_retval);
  NS_IMETHOD AddFolderListener(nsIFolderListener * listener);
  NS_IMETHOD RemoveFolderListener(nsIFolderListener * listener);
  NS_IMETHOD GetParent(nsIFolder * *aParent);
  NS_IMETHOD SetParent(nsIFolder * aParent);
  NS_IMETHOD FindSubFolder(const char *subFolderName, nsIFolder **folder);


  // nsIMsgFolder methods:
  NS_IMETHOD AddUnique(nsISupports* element);
  NS_IMETHOD ReplaceElement(nsISupports* element, nsISupports* newElement);
  NS_IMETHOD GetVisibleSubFolders(nsIEnumerator* *result);
  NS_IMETHOD GetMessages(nsISimpleEnumerator* *result);
  NS_IMETHOD GetThreads(nsISimpleEnumerator ** threadEnumerator);
  NS_IMETHOD GetThreadForMessage(nsIMessage *message, nsIMsgThread **thread);
  NS_IMETHOD HasMessage(nsIMessage *message, PRBool *hasMessage);

  NS_IMETHOD GetServer(nsIMsgIncomingServer ** aServer);
  NS_IMETHOD GetIsServer(PRBool *aResult);


  NS_IMETHOD GetPrettyName(PRUnichar ** name);
  NS_IMETHOD SetPrettyName(PRUnichar * name);
#if 0
  static nsresult GetRoot(nsIMsgFolder* *result);
#endif
  // Gets the URL that represents the given message.  Returns a newly
  // created string that must be free'd using XP_FREE().
  // If the db is NULL, then returns a URL that represents the entire
  // folder as a whole.
#ifdef HAVE_DB
  NS_IMETHOD BuildUrl(nsMsgDatabase *db, nsMsgKey key, char ** url);
#endif

#ifdef HAVE_MASTER
  NS_IMETHOD SetMaster(MSG_Master *master);
#endif



  NS_IMETHOD BuildFolderURL(char ** url);


  NS_IMETHOD GetPrettiestName(PRUnichar ** name);

#ifdef HAVE_ADMINURL
  NS_IMETHOD GetAdminUrl(MWContext *context, MSG_AdminURLType type);
  NS_IMETHOD HaveAdminUrl(MSG_AdminURLType type, PRBool *hadAdminUrl);
#endif

  NS_IMETHOD GetDeleteIsMoveToTrash(PRBool *aIsDeleteIsMoveToTrash);
  NS_IMETHOD GetShowDeletedMessages(PRBool *aIsShowDeletedMessages);
    
  NS_IMETHOD OnCloseFolder();
  NS_IMETHOD Delete();

	NS_IMETHOD DeleteSubFolders(nsISupportsArray *folders);
	NS_IMETHOD PropagateDelete(nsIMsgFolder *folder, PRBool deleteStorage);
  NS_IMETHOD RecursiveDelete(PRBool deleteStorage);  // called by PropagateDelete

  NS_IMETHOD CreateSubfolder(const char *folderName);

  NS_IMETHOD Compact();
  NS_IMETHOD EmptyTrash();

  NS_IMETHOD Rename(const char *name);
  NS_IMETHOD Adopt(nsIMsgFolder *srcFolder, PRUint32*);

  NS_IMETHOD ContainsChildNamed(const char *name, PRBool *containsChild);
  NS_IMETHOD IsAncestorOf(nsIMsgFolder * folder, PRBool *isParent);

  NS_IMETHOD GenerateUniqueSubfolderName(const char *prefix, nsIMsgFolder *otherFolder,
                                         char **name);

  // updates num messages and num unread - should be pure virtual
  // when I get around to implementing in all subclasses?
  NS_IMETHOD UpdateSummaryTotals(PRBool force);
  NS_IMETHOD SummaryChanged();
  NS_IMETHOD GetNumUnread(PRBool deep, PRInt32 *numUnread);       // How many unread messages in this folder.
  NS_IMETHOD GetTotalMessages(PRBool deep, PRInt32 *totalMessages);   // Total number of messages in this folder.

#ifdef HAVE_DB
  NS_IMETHOD GetTotalMessagesInDB(PRUint32 *totalMessages) const;					// How many messages in database.
#endif
	
#ifdef HAVE_PANE
  NS_IMETHOD	MarkAllRead(MSG_Pane *pane, PRBool deep);
#endif

#ifdef HAVE_DB	
	// These functions are used for tricking the front end into thinking that we have more 
	// messages than are really in the DB.  This is usually after and IMAP message copy where
	// we don't want to do an expensive select until the user actually opens that folder
	// These functions are called when MSG_Master::GetFolderLineById is populating a MSG_FolderLine
	// struct used by the FE
	int32			GetNumPendingUnread(PRBool deep = FALSE);
	int32			GetNumPendingTotalMessages(PRBool deep = FALSE);
	
  void			ChangeNumPendingUnread(int32 delta);
  void			ChangeNumPendingTotalMessages(int32 delta);


  NS_IMETHOD SetFolderPrefFlags(PRUint32 flags);
  NS_IMETHOD GetFolderPrefFlags(PRUint32 *flags);


  NS_IMETHOD SetLastMessageLoaded(nsMsgKey lastMessageLoaded);
  NS_IMETHOD GetLastMessageLoaded();
#endif

  NS_IMETHOD SetFlag(PRUint32 which);
  NS_IMETHOD ClearFlag(PRUint32 which);
  NS_IMETHOD GetFlag(PRUint32 flag, PRBool *_retval);

  NS_IMETHOD ToggleFlag(PRUint32 which);
  NS_IMETHOD OnFlagChange(PRUint32 which);
  NS_IMETHOD GetFlags(PRUint32 *flags);

#ifdef HAVE_PANE
  NS_IMETHOD SetFlagInAllFolderPanes(PRUint32 which);
#endif

  NS_IMETHOD GetFoldersWithFlag(PRUint32 flags, nsIMsgFolder** result,
                                PRUint32 resultsize, PRUint32 *numFolders);

  NS_IMETHOD GetExpansionArray(nsISupportsArray *expansionArray);

#ifdef HAVE_NET
  NS_IMETHOD EscapeMessageId(const char *messageId, const char **escapeMessageID);
#endif

  NS_IMETHOD GetExpungedBytesCount(PRUint32 *count);

  NS_IMETHOD GetDeletable(PRBool *deletable);
  NS_IMETHOD GetCanCreateChildren(PRBool *canCreateChildren);
  NS_IMETHOD GetCanBeRenamed(PRBool *canBeRenamed);
  NS_IMETHOD GetRequiresCleanup(PRBool *requiredCleanup);
  NS_IMETHOD ClearRequiresCleanup() ;
#ifdef HAVE_PANE
	virtual PRBool CanBeInFolderPane();
#endif

  NS_IMETHOD GetKnowsSearchNntpExtension(PRBool *knowsExtension);
  NS_IMETHOD GetAllowsPosting(PRBool *allowsPosting);

  NS_IMETHOD DisplayRecipients(PRBool *displayRecipients);

	//For file contention
	NS_IMETHOD AcquireSemaphore (nsISupports *semHolder);
	NS_IMETHOD ReleaseSemaphore (nsISupports *semHolder);
	NS_IMETHOD TestSemaphore (nsISupports *semHolder, PRBool *isSemaphoreHolder);
	NS_IMETHOD IsLocked(PRBool *isLocked);

	NS_IMETHOD CreateMessageFromMsgDBHdr(nsIMsgDBHdr *msgDBHdr, nsIMessage **message) = 0;


#ifdef HAVE_CACHE
	virtual nsresult WriteToCache(XP_File);
	virtual nsresult ReadFromCache(char *);
	virtual PRBool IsCachable();
	void SkipCacheTokens(char **ppBuf, int numTokens);
#endif

  NS_IMETHOD GetRelativePathName(char **pathName);


  NS_IMETHOD GetSizeOnDisk(PRUint32 *size);

#ifdef HAVE_NET
  NS_IMETHOD ShouldPerformOperationOffline(PRBool *performOffline);
#endif

#ifdef DOES_FOLDEROPERATIONS
	int DownloadToTempFileAndUpload(MessageCopyInfo *copyInfo, nsMsgKeyArray &keysToSave, MSG_FolderInfo *dstFolder, nsMsgDatabase *sourceDB);
	void UpdateMoveCopyStatus(MWContext *context, PRBool isMove, int32 curMsgCount, int32 totMessages);
#endif

  NS_IMETHOD RememberPassword(const char *password);
  NS_IMETHOD GetRememberedPassword(char ** password);
  NS_IMETHOD UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *needsAuthenticate);
#if 0
  NS_IMETHOD GetUsername(char **userName);
  NS_IMETHOD GetHostname(char **hostName);
#endif
  
	virtual nsresult GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db) = 0;
	NS_IMETHOD DeleteMessages(nsISupportsArray *messages, 
                            nsITransactionManager *txnMgr, PRBool
                            deleteStorage) = 0;
  NS_IMETHOD CopyMessages(nsIMsgFolder* srcFolder,
                          nsISupportsArray *messages,
                          PRBool isMove,
                          nsITransactionManager* txnMgr,
                          nsIMsgCopyServiceListener* listener);
  NS_IMETHOD CopyFileMessage(nsIFileSpec* fileSpec,
                             nsIMessage* messageToReplace,
                             PRBool isDraftOrTemplate,
                             nsITransactionManager* txnMgr,
                             nsIMsgCopyServiceListener* listener);

	NS_IMETHOD GetNewMessages();

	NS_IMETHOD GetCharset(PRUnichar * *aCharset) = 0;
	NS_IMETHOD SetCharset(PRUnichar * aCharset) = 0;

	NS_IMETHOD GetBiffState(PRUint32 *aBiffState);
	NS_IMETHOD SetBiffState(PRUint32 aBiffState);

	NS_IMETHOD GetNumNewMessages(PRInt32 *aNumNewMessages);
	NS_IMETHOD SetNumNewMessages(PRInt32 aNumNewMessages);

	NS_IMETHOD GetNewMessagesNotificationDescription(PRUnichar * *adescription);

	NS_IMETHOD GetRootFolder(nsIMsgFolder * *aRootFolder);
	NS_IMETHOD GetMsgDatabase(nsIMsgDatabase** aMsgDatabase);
	NS_IMETHOD GetPath(nsIFileSpec * *aPath);

	NS_IMETHOD MatchName(nsString *name, PRBool *matches);
	NS_IMETHOD MarkMessagesRead(nsISupportsArray *messages, PRBool markRead);
	NS_IMETHOD MarkAllMessagesRead(void);
	NS_IMETHOD MarkMessagesFlagged(nsISupportsArray *messages, PRBool markRead);

	NS_IMETHOD GetChildWithURI(const char *uri, PRBool deep, nsIMsgFolder ** folder);

protected:
	nsresult NotifyPropertyChanged(char *property, char* oldValue, char* newValue);
	nsresult NotifyPropertyFlagChanged(nsISupports *item, char *property, PRUint32 oldValue,
												PRUint32 newValue);
	nsresult NotifyItemAdded(nsISupports *item);
	nsresult NotifyItemDeleted(nsISupports *item);

	// this is a little helper function that is not part of the public interface. 
	// we use it to get the IID of the incoming server for the derived folder.
	// w/out a function like this we would have to implement GetServer in each
	// derived folder class.
	virtual const char* GetIncomingServerType() = 0;

protected:
  nsString mName;
  PRUint32 mFlags;
  nsIFolder *mParent;     //This won't be refcounted for ownership reasons.
  PRInt32 mNumUnreadMessages;        /* count of unread messages (-1 means
                                         unknown; -2 means unknown but we already
                                         tried to find out.) */
  PRInt32 mNumTotalMessages;         /* count of existing messages. */
  nsCOMPtr<nsISupportsArray> mSubFolders;
  nsVoidArray *mListeners; //This can't be an nsISupportsArray because due to
													 //ownership issues, listeners can't be AddRef'd

  PRInt32 mPrefFlags;       // prefs like MSG_PREF_OFFLINE, MSG_PREF_ONE_PANE, etc
  nsISupports *mSemaphoreHolder; // set when the folder is being written to
								//Due to ownership issues, this won't be AddRef'd.

  nsIMsgIncomingServer* m_server; //this won't be addrefed....ownership issue here

#ifdef HAVE_DB
  nsMsgKey	m_lastMessageLoaded;
#endif
  // These values are used for tricking the front end into thinking that we have more 
  // messages than are really in the DB.  This is usually after and IMAP message copy where
  // we don't want to do an expensive select until the user actually opens that folder
  PRInt32 mNumPendingUnreadMessages;
  PRInt32 mNumPendingTotalMessages;

  PRUint32	mBiffState;
  PRInt32	mNumNewBiffMessages;

  PRBool mIsCachable;
  PRBool mIsServer;
};

#endif
