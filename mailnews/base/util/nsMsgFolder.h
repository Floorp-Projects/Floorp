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
#include "nsIRDFResourceFactory.h"
#include "nsIDBFolderInfo.h"
#include "nsMsgDatabase.h"

 /* 
  * MsgFolder
  */ 

class nsMsgFolder: public nsRDFResource, public nsIMsgFolder
{
public: 
  nsMsgFolder(void);
  virtual ~nsMsgFolder(void);

  NS_DECL_ISUPPORTS_INHERITED

  // nsICollection methods:
  NS_IMETHOD_(PRUint32) Count(void) const;
  NS_IMETHOD AppendElement(nsISupports *aElement);
  NS_IMETHOD RemoveElement(nsISupports *aElement);
  NS_IMETHOD Enumerate(nsIEnumerator* *result);
  NS_IMETHOD Clear(void);

  // nsIFolder methods:
  NS_IMETHOD GetURI(char* *name) { return nsRDFResource::GetValue(name); }
  NS_IMETHOD GetName(char **name);
  NS_IMETHOD SetName(char *name);
  NS_IMETHOD GetChildNamed(const char *name, nsISupports* *result);
  NS_IMETHOD GetParent(nsIFolder* *parent);
  NS_IMETHOD GetSubFolders(nsIEnumerator* *result);
	NS_IMETHOD AddFolderListener(nsIFolderListener * listener);
	NS_IMETHOD RemoveFolderListener(nsIFolderListener * listener);


  // nsIMsgFolder methods:
  NS_IMETHOD AddUnique(nsISupports* element);
  NS_IMETHOD ReplaceElement(nsISupports* element, nsISupports* newElement);
  NS_IMETHOD GetVisibleSubFolders(nsIEnumerator* *result);
  NS_IMETHOD GetMessages(nsIEnumerator* *result);
  NS_IMETHOD GetPrettyName(char ** name);
  NS_IMETHOD SetPrettyName(char * name);
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

#ifdef DOES_FOLDEROPERATIONS
  NS_IMETHOD StartAsyncCopyMessagesInto(MSG_FolderInfo *dstFolder,
                                         MSG_Pane* sourcePane, 
                                         nsMsgDatabase *sourceDB,
                                         nsMsgKeyArray *srcArray,
                                         int32 srcCount,
                                         MWContext *currentContext,
                                         MSG_UrlQueue *urlQueue,
                                         PRBool deleteAfterCopy,
                                         nsMsgKey nextKey = nsMsgKey_None);

    
  NS_IMETHOD BeginCopyingMessages(MSG_FolderInfo *dstFolder, 
                                   nsMsgDatabase *sourceDB,
                                   nsMsgKeyArray *srcArray, 
                                   MSG_UrlQueue *urlQueue,
                                   int32 srcCount,
                                   MessageCopyInfo *copyInfo);


  NS_IMETHOD FinishCopyingMessages(MWContext *context,
                                    MSG_FolderInfo * srcFolder, 
                                    MSG_FolderInfo *dstFolder, 
                                    nsMsgDatabase *sourceDB,
                                    nsMsgKeyArray **ppSrcArray, 
                                    int32 srcCount,
                                    msg_move_state *state);


  NS_IMETHOD CleanupCopyMessagesInto(MessageCopyInfo **info);

  NS_IMETHOD SaveMessages(nsMsgKeyArray *, const char *fileName, 
                          MSG_Pane *pane, nsMsgDatabase *msgDB,
                          int(*doneCB)(void *, int status) = NULL, void *state = NULL,
                          PRBool addMozillaStatus = TRUE);
#endif

  NS_IMETHOD BuildFolderURL(char ** url);


  NS_IMETHOD GetPrettiestName(char ** name);

#ifdef HAVE_ADMINURL
  NS_IMETHOD GetAdminUrl(MWContext *context, MSG_AdminURLType type);
  NS_IMETHOD HaveAdminUrl(MSG_AdminURLType type, PRBool *hadAdminUrl);
#endif

  NS_IMETHOD GetDeleteIsMoveToTrash(PRBool *aIsDeleteIsMoveToTrash);
  NS_IMETHOD GetShowDeletedMessages(PRBool *aIsShowDeletedMessages);
    
  NS_IMETHOD OnCloseFolder();
  NS_IMETHOD Delete();

  NS_IMETHOD PropagateDelete(nsIMsgFolder **folder, PRBool deleteStorage);
  NS_IMETHOD RecursiveDelete(PRBool deleteStorage);  // called by PropagateDelete

  NS_IMETHOD CreateSubfolder(const char *leafNameFromuser, nsIMsgFolder** outFolder, PRUint32* outPos);

  NS_IMETHOD Rename(const char *name);
  NS_IMETHOD Adopt(const nsIMsgFolder *srcFolder, PRUint32*);

  NS_IMETHOD ContainsChildNamed(const char *name, PRBool *containsChild);
  NS_IMETHOD FindParentOf(nsIMsgFolder * aFolder, nsIMsgFolder ** aParent);
  NS_IMETHOD IsParentOf(nsIMsgFolder *, PRBool deep, PRBool *isParent);

  NS_IMETHOD GenerateUniqueSubfolderName(const char *prefix, nsIMsgFolder *otherFolder,
                                         char **name);

  NS_IMETHOD GetDepth(PRUint32 *depth);
  NS_IMETHOD SetDepth(PRUint32 depth);

  // updates num messages and num unread - should be pure virtual
  // when I get around to implementing in all subclasses?
  NS_IMETHOD UpdateSummaryTotals();
  NS_IMETHOD SummaryChanged();
  NS_IMETHOD GetNumUnread(PRBool deep, PRUint32 *numUnread);       // How many unread messages in this folder.
  NS_IMETHOD GetTotalMessages(PRBool deep, PRUint32 *totalMessages);   // Total number of messages in this folder.

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
  NS_IMETHOD SetFolderCSID(PRInt16 csid);
  NS_IMETHOD GetFolderCSID(PRInt16 *csid);


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

   NS_IMETHOD ReadDBFolderInfo(PRBool force);


	//For file contention
	NS_IMETHOD AcquireSemaphore (nsISupports *semHolder);
	NS_IMETHOD ReleaseSemaphore (nsISupports *semHolder);
	NS_IMETHOD TestSemaphore (nsISupports *semHolder, PRBool *isSemaphoreHolder);
	NS_IMETHOD IsLocked(PRBool *isLocked);

#ifdef HAVE_PANE
  MWContext *GetFolderPaneContext();
#endif

#ifdef HAVE_MASTER
  MSG_Master *GetMaster() {return m_master;}
#endif

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
  NS_IMETHOD GetUsersName(char **userName);
  NS_IMETHOD GetHostName(char **hostName);

 virtual nsresult GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsMsgDatabase **db) = 0;
 	NS_IMETHOD DeleteMessage(nsIMessage *message) = 0;


protected:
  nsString mName;
  PRUint32 mFlags;
  PRInt32 mNumUnreadMessages;        /* count of unread messages (-1 means
                                         unknown; -2 means unknown but we already
                                         tried to find out.) */
  PRInt32 mNumTotalMessages;         /* count of existing messages. */
  nsISupportsArray *mSubFolders;
	nsISupportsArray *mListeners;
#ifdef HAVE_MASTER
  MSG_Master  *mMaster;
#endif

  PRInt16 mCsid;			// default csid for folder/newsgroup - maintained by fe.
  PRUint8 mDepth;
  PRInt32 mPrefFlags;       // prefs like MSG_PREF_OFFLINE, MSG_PREF_ONE_PANE, etc
  nsISupports *mSemaphoreHolder; // set when the folder is being written to

#ifdef HAVE_DB
  nsMsgKey	m_lastMessageLoaded;
#endif
  // These values are used for tricking the front end into thinking that we have more 
  // messages than are really in the DB.  This is usually after and IMAP message copy where
  // we don't want to do an expensive select until the user actually opens that folder
  PRInt32 mNumPendingUnreadMessages;
  PRInt32 mNumPendingTotalMessages;

	PRBool mIsCachable;

};

#endif
