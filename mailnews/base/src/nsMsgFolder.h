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
#include "nsIRDFNode.h"

 /* 
  * MsgFolder
  */ 

 class nsMsgFolder: public nsIMsgFolder
 {
public: 
	nsMsgFolder();
	virtual ~nsMsgFolder();

	/* this macro defines QueryInterface, AddRef and Release for this class */
	NS_DECL_ISUPPORTS

  NS_IMETHOD GetType(FolderType *type);

    // Gets the URL that represents the given message.  Returns a newly
    // created string that must be free'd using XP_FREE().
    // If the db is NULL, then returns a URL that represents the entire
    // folder as a whole.
#ifdef HAVE_DB
  NS_IMETHOD BuildUrl (MessageDB *db, MessageKey key, char ** url);
#endif

#ifdef HAVE_MASTER
  NS_IMETHOD SetMaster(MSG_Master *master);
#endif

#ifdef DOES_FOLDEROPERATIONS
  NS_IMETHOD StartAsyncCopyMessagesInto (MSG_FolderInfo *dstFolder,
                                             MSG_Pane* sourcePane, 
											 MessageDB *sourceDB,
                                             IDArray *srcArray,
                                             int32 srcCount,
                                             MWContext *currentContext,
                                             MSG_UrlQueue *urlQueue,
                                             XP_Bool deleteAfterCopy,
                                             MessageKey nextKey = MSG_MESSAGEKEYNONE);

    
    NS_IMETHOD BeginCopyingMessages (MSG_FolderInfo *dstFolder, 
									  MessageDB *sourceDB,
                                      IDArray *srcArray, 
                                      MSG_UrlQueue *urlQueue,
                                      int32 srcCount,
                                      MessageCopyInfo *copyInfo);


    NS_IMETHOD FinishCopyingMessages (MWContext *context,
                                       MSG_FolderInfo * srcFolder, 
                                       MSG_FolderInfo *dstFolder, 
									   MessageDB *sourceDB,
                                       IDArray **ppSrcArray, 
                                       int32 srcCount,
                                       msg_move_state *state);


    NS_IMETHOD CleanupCopyMessagesInto (MessageCopyInfo **info);

    NS_IMETHOD SaveMessages(IDArray *, const char *fileName, 
                                MSG_Pane *pane, MessageDB *msgDB,
								  int (*doneCB)(void *, int status) = NULL, void *state = NULL,
								  XP_Bool addMozillaStatus = TRUE);
#endif

	NS_IMETHOD GetPrettyName(char * *aPrettyName);
	NS_IMETHOD GetName(char **name);
	NS_IMETHOD SetName(const char *name);
  NS_IMETHOD GetPrettiestName(char **name);

  NS_IMETHOD BuildFolderURL(char ** url);

  NS_IMETHOD GetNameFromPathName(const char *pathName, char ** name);
	NS_IMETHOD HasSubFolders(PRBool *hasSubFolders);
  NS_IMETHOD GetNumSubFolders(PRInt32 *numSubFolders);
  NS_IMETHOD GetNumSubFoldersToDisplay(PRInt32 *numSubFolders);
  NS_IMETHOD GetSubFolder(int which, nsIMsgFolder **aFolder);
  NS_IMETHOD GetSubFolders (nsISupportsArray ** subFolders);
	NS_IMETHOD AddSubFolder(const nsIMsgFolder *folder);
  NS_IMETHOD AddSubfolderIfUnique(const nsIMsgFolder *newSubfolder);
  NS_IMETHOD ReplaceSubfolder(const nsIMsgFolder *oldFolder, const nsIMsgFolder *newFolder);

  NS_IMETHOD RemoveSubFolder (const nsIMsgFolder *which);
#ifdef HAVE_PANE
	NS_IMETHOD GetVisibleSubFolders (nsISupportsArray ** visibleSubFolders);
#endif

#ifdef HAVE_ADMINURL
	NS_IMETHOD GetAdminUrl(MWContext *context, MSG_AdminURLType type);
	NS_IMETHOD HaveAdminUrl(MSG_AdminURLType type, PRBool *hadAdminUrl);
#endif

	NS_IMETHOD GetDeleteIsMoveToTrash(PRBool *aIsDeleteIsMoveToTrash);
	NS_IMETHOD GetShowDeletedMessages(PRBool *aIsShowDeletedMessages);
    
	NS_IMETHOD OnCloseFolder ();
  NS_IMETHOD Delete ();

  NS_IMETHOD PropagateDelete (nsIMsgFolder **folder, PRBool deleteStorage);
	NS_IMETHOD RecursiveDelete (PRBool deleteStorage);  // called by PropagateDelete

  NS_IMETHOD CreateSubfolder (const char *leafNameFromuser, nsIMsgFolder** outFolder, PRInt32* outPos);

  NS_IMETHOD Rename (const char *name);
  NS_IMETHOD Adopt (const nsIMsgFolder *srcFolder, PRInt32*);

  NS_IMETHOD ContainsChildNamed (const char *name, PRBool *containsChild);
  NS_IMETHOD FindChildNamed (const char *name, nsIMsgFolder ** aChild);
  NS_IMETHOD FindParentOf (const nsIMsgFolder * aFolder, nsIMsgFolder ** aParent);
  NS_IMETHOD IsParentOf (const nsIMsgFolder *, PRBool deep, PRBool *isParent);

	NS_IMETHOD GenerateUniqueSubfolderName(const char *prefix, const nsIMsgFolder *otherFolder,
													char **name);

  NS_IMETHOD   GetDepth(PRInt32 *depth);
  NS_IMETHOD   SetDepth(PRInt32 depth);

    // updates num messages and num unread - should be pure virtual
    // when I get around to implementing in all subclasses?
  NS_IMETHOD UpdateSummaryTotals();
  NS_IMETHOD SummaryChanged();
  NS_IMETHOD GetNumUnread(PRBool deep, PRInt32 *numUnread);       // How many unread messages in this folder.
  NS_IMETHOD GetTotalMessages(PRBool deep, PRInt32 *totalMessages);   // Total number of messages in this folder.

#ifdef HAVE_DB
	NS_IMETHOD GetTotalMessagesInDB(PRInt32 *totalMessages) const;					// How many messages in database.
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
	int32			GetNumPendingUnread(XP_Bool deep = FALSE);
	int32			GetNumPendingTotalMessages(XP_Bool deep = FALSE);
	
	void			ChangeNumPendingUnread(int32 delta);
	void			ChangeNumPendingTotalMessages(int32 delta);


  NS_IMETHOD SetFolderPrefFlags(PRInt32 flags);
  NS_IMETHOD GetFolderPrefFlags(PRInt32 *flags);
	NS_IMETHOD SetFolderCSID(PRInt16 csid);
	NS_IMETHOD GetFolderCSID(PRInt16 *csid);


	NS_IMETHOD	SetLastMessageLoaded(MessageKey lastMessageLoaded);
	NS_IMETHOD GetLastMessageLoaded();
#endif

  NS_IMETHOD SetFlag (PRInt32 which);
  NS_IMETHOD ClearFlag (PRInt32 which);
  NS_IMETHOD GetFlag(PRInt32 flag, PRBool *_retval);

  NS_IMETHOD ToggleFlag (PRInt32 which);
	NS_IMETHOD OnFlagChange (PRInt32 which);
  NS_IMETHOD GetFlags(PRInt32 *flags);

#ifdef HAVE_PANE
	NS_IMETHOD SetFlagInAllFolderPanes(PRInt32 which);
#endif

  NS_IMETHOD GetFoldersWithFlag(PRInt32 flags, nsIMsgFolder** result,
                         PRInt32 resultsize, PRInt32 *numFolders);

  NS_IMETHOD GetExpansionArray(const nsISupportsArray *expansionArray);

#ifdef HAVE_NET
  NS_IMETHOD EscapeMessageId(const char *messageId, const char **escapeMessageID);
#endif

	NS_IMETHOD	GetExpungedBytesCount(PRInt32 *count);

  NS_IMETHOD GetDeletable (PRBool *deletable);
  NS_IMETHOD GetCanCreateChildren (PRBool *canCreateChildren);
  NS_IMETHOD GetCanBeRenamed (PRBool *canBeRenamed);
	NS_IMETHOD GetRequiresCleanup(PRBool *requiredCleanup);
	NS_IMETHOD	ClearRequiresCleanup() ;
#ifdef HAVE_PANE
	virtual XP_Bool CanBeInFolderPane ();
#endif

	NS_IMETHOD GetKnowsSearchNntpExtension(PRBool *knowsExtension);
	NS_IMETHOD GetAllowsPosting(PRBool *allowsPosting);

	NS_IMETHOD DisplayRecipients(PRBool *displayRecipients);

#ifdef HAVE_SEMAPHORE
  MsgERR AcquireSemaphore (void *semHolder);
  void ReleaseSemaphore (void *semHolder);
  XP_Bool TestSemaphore (void *semHolder);
  XP_Bool IsLocked () { return m_semaphoreHolder != NULL; }
#endif

#ifdef HAVE_PANE
  MWContext *GetFolderPaneContext();
#endif

#ifdef HAVE_MASTER
  MSG_Master  *GetMaster() {return m_master;}
#endif

#ifdef HAVE_CACHE
	virtual MsgERR WriteToCache (XP_File);
	virtual MsgERR ReadFromCache (char *);
	virtual XP_Bool IsCachable ();
	void SkipCacheTokens (char **ppBuf, int numTokens);
#endif

	NS_IMETHOD GetRelativePathName (char **pathName);


	NS_IMETHOD GetSizeOnDisk(PRInt32 *size);

#ifdef HAVE_NET
	NS_IMETHOD	ShouldPerformOperationOffline(PRBool *performOffline);
#endif

#ifdef DOES_FOLDEROPERATIONS
	int		DownloadToTempFileAndUpload(MessageCopyInfo *copyInfo, IDArray &keysToSave, MSG_FolderInfo *dstFolder, MessageDB *sourceDB);
	void UpdateMoveCopyStatus(MWContext *context, XP_Bool isMove, int32 curMsgCount, int32 totMessages);
#endif

	NS_IMETHOD RememberPassword(const char *password);
	NS_IMETHOD GetRememberedPassword(char ** password);
	NS_IMETHOD UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *needsAuthenticate);
	NS_IMETHOD GetUserName(char **userName);
	NS_IMETHOD GetHostName(char **hostName);

	protected:
	char* mName;
  PRUint32 mFlags;
  PRInt32 mNumUnreadMessages;        /* count of unread messages   (-1 means
                                   unknown; -2 means unknown but we already
                                   tried to find out.) */
  PRInt32 mNumTotalMessages;         /* count of existing messages. */
  nsISupportsArray *mSubFolders;
#ifdef HAVE_MASTER
  MSG_Master  *mMaster;
#endif

	PRInt16 mCsid;			// default csid for folder/newsgroup - maintained by fe.
  PRUint8 mDepth;
  PRInt32 mPrefFlags;       // prefs like MSG_PREF_OFFLINE, MSG_PREF_ONE_PANE, etc
#ifdef HAVE_SEMAPHORE
  void *mSemaphoreHolder; // set when the folder is being written to
#endif

#ifdef HAVE_DB
	MessageKey	m_lastMessageLoaded;
	// These values are used for tricking the front end into thinking that we have more 
	// messages than are really in the DB.  This is usually after and IMAP message copy where
	// we don't want to do an expensive select until the user actually opens that folder
	PRInt32 mNumPendingUnreadMessages;
	PRInt32 mNumPendingTotalMessages;
#endif

#ifdef HAVE_CACHE
	PRBool mIsCachable;
#endif

 };

class nsMsgMailFolder : public nsMsgFolder, public nsIMsgMailFolder
{
public:
	nsMsgMailFolder();
	~nsMsgMailFolder();

	NS_IMETHOD QueryInterface(REFNSIID aIID,
							void** aInstancePtr);
	NS_IMETHOD_(nsrefcnt) AddRef(void);
	NS_IMETHOD_(nsrefcnt) Release(void);

	NS_IMETHOD GetType(FolderType *type);

#ifdef HAVE_DB	
	virtual MsgERR BeginCopyingMessages (MSG_FolderInfo *dstFolder, 
																				MessageDB *sourceDB,
																				IDArray *srcArray, 
																				MSG_UrlQueue *urlQueue,
																				int32 srcCount,
																				MessageCopyInfo *copyInfo);


	virtual int FinishCopyingMessages (MWContext *context,
																			MSG_FolderInfo * srcFolder, 
																			MSG_FolderInfo *dstFolder, 
																			MessageDB *sourceDB,
																			IDArray **ppSrcArray, 
																			int32 srcCount,
																			msg_move_state *state);
#endif

	NS_IMETHOD CreateSubfolder(const char *leafNameFromUser, nsIMsgFolder **outFolder, PRInt32 *outPos);

	NS_IMETHOD RemoveSubFolder (const nsIMsgFolder *which);
	NS_IMETHOD Delete ();
	NS_IMETHOD Rename (const char *newName);
	NS_IMETHOD Adopt(const nsIMsgFolder *srcFolder, PRInt32 *outPos);

		// this override pulls the value from the db
	NS_IMETHOD GetName(char** name);   // Name of this folder (as presented to user).
	NS_IMETHOD GetPrettyName(char ** prettyName);	// Override of the base, for top-level mail folder

  NS_IMETHOD BuildFolderURL(char **url);

	NS_IMETHOD GenerateUniqueSubfolderName(const char *prefix, const nsIMsgFolder *otherFolder, char** name);

	NS_IMETHOD UpdateSummaryTotals() ;

	NS_IMETHOD GetExpungedBytesCount(PRInt32 *count);
	NS_IMETHOD GetDeletable (PRBool *deletable); 
	NS_IMETHOD GetCanCreateChildren (PRBool *canCreateChildren) ;
	NS_IMETHOD GetCanBeRenamed (PRBool *canBeRenamed);
	NS_IMETHOD GetRequiresCleanup(PRBool *requiresCleanup);


	NS_IMETHOD GetRelativePathName (char **pathName);


	NS_IMETHOD GetSizeOnDisk(PRInt32 size);

	NS_IMETHOD GetUserName(char** userName);
	NS_IMETHOD GetHostName(char** hostName);
	NS_IMETHOD UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *authenticate);
	NS_IMETHOD RememberPassword(const char *password);
	NS_IMETHOD GetRememberedPassword(char ** password);

	//nsIMsgMailFolder
  NS_IMETHOD GetPathName(char * *aPathName);
  NS_IMETHOD SetPathName(char * aPathName);

protected:
	char*			mPathName;
	PRInt32		mExpungedBytes;
	PRBool		mHaveReadNameFromDB;
	PRBool		mGettingMail;
};

#endif
