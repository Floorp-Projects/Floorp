/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIURL.h"
#include "nsWeakReference.h"
#include "nsIMsgFilterList.h"
#include "nsIUrlListener.h"
#include "nsIFileSpec.h"
class nsIStringBundle;

 /* 
  * MsgFolder
  */ 

class NS_MSG_BASE nsMsgFolder : public nsRDFResource,
                                public nsSupportsWeakReference,
                                public nsIMsgFolder
{
public: 
  nsMsgFolder(void);
  virtual ~nsMsgFolder(void);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSISERIALIZABLE       // XXXbe temporary NOT_IMPLEMENTED stubs

  NS_DECL_NSICOLLECTION
  NS_DECL_NSIFOLDER
  // eventually this will be an instantiable class, and we should
  // use this macro:
  // NS_DECL_NSIMSGFOLDER
  
  // right now a few of these methods are left abstract, and
  // are commented out below
  
  // begin NS_DECL_NSIMSGFOLDER
  NS_IMETHOD AddUnique(nsISupports *element);
  NS_IMETHOD ReplaceElement(nsISupports *element, nsISupports *newElement);
  NS_IMETHOD GetMessages(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator **_retval);
  NS_IMETHOD StartFolderLoading(void);
  NS_IMETHOD EndFolderLoading(void);
  NS_IMETHOD UpdateFolder(nsIMsgWindow *window);
  NS_IMETHOD GetFirstNewMessage(nsIMsgDBHdr **firstNewMessage);
  NS_IMETHOD GetVisibleSubFolders(nsIEnumerator **_retval);
  NS_IMETHOD GetPrettiestName(PRUnichar * *aPrettiestName);
  NS_IMETHOD GetFolderURL(char * *aFolderURL);
  NS_IMETHOD GetDeleteIsMoveToTrash(PRBool *aDeleteIsMoveToTrash);
  NS_IMETHOD GetShowDeletedMessages(PRBool *aShowDeletedMessages);
  NS_IMETHOD GetServer(nsIMsgIncomingServer * *aServer);
  NS_IMETHOD GetIsServer(PRBool *aIsServer);
  NS_IMETHOD GetNoSelect(PRBool *aNoSelect);
  NS_IMETHOD GetCanSubscribe(PRBool *aCanSubscribe);
  NS_IMETHOD GetCanFileMessages(PRBool *aCanFileMessages);
  NS_IMETHOD GetFilterList(nsIMsgFilterList **aFilterList);
  NS_IMETHOD GetCanCreateSubfolders(PRBool *aCanCreateSubfolders);
  NS_IMETHOD GetCanRename(PRBool *aCanRename);
  NS_IMETHOD GetCanCompact(PRBool *aCanCompact);
  NS_IMETHOD ForceDBClosed(void);
  NS_IMETHOD Delete(void);
  NS_IMETHOD DeleteSubFolders(nsISupportsArray *folders, nsIMsgWindow *msgWindow);
  NS_IMETHOD CreateStorageIfMissing(nsIUrlListener* urlListener);
  NS_IMETHOD PropagateDelete(nsIMsgFolder *folder, PRBool deleteStorage, nsIMsgWindow *msgWindow);
  NS_IMETHOD RecursiveDelete(PRBool deleteStorage, nsIMsgWindow *msgWindow);
  NS_IMETHOD CreateSubfolder(const PRUnichar *folderName, nsIMsgWindow *msgWindow);
  NS_IMETHOD AddSubfolder(nsAutoString *folderName, nsIMsgFolder **newFolder);
  NS_IMETHOD Compact(nsIUrlListener *aListener, nsIMsgWindow *msgWindow);
  NS_IMETHOD CompactAll(nsIUrlListener *aListener, nsIMsgWindow *msgWindow, nsISupportsArray *aFolderArray, PRBool aCompactOfflineAlso, nsISupportsArray *aOfflineFolderArray);
  NS_IMETHOD EmptyTrash(nsIMsgWindow *msgWindow, nsIUrlListener *aListener);
  NS_IMETHOD Rename(const PRUnichar *name, nsIMsgWindow *msgWindow);
  NS_IMETHOD RenameSubFolders(nsIMsgWindow *msgWindow, nsIMsgFolder *msgFolder);
  NS_IMETHOD ContainsChildNamed(const char *name, PRBool *_retval);
  NS_IMETHOD IsAncestorOf(nsIMsgFolder *folder, PRBool *_retval);
  NS_IMETHOD GenerateUniqueSubfolderName(const char *prefix, nsIMsgFolder *otherFolder, char **_retval); 
  NS_IMETHOD UpdateSummaryTotals(PRBool force);
  NS_IMETHOD SummaryChanged(void);
  NS_IMETHOD GetNumUnread(PRBool deep, PRInt32 *_retval);
  NS_IMETHOD GetTotalMessages(PRBool deep, PRInt32 *_retval);
  NS_IMETHOD GetHasNewMessages(PRBool *hasNewMessages);
  NS_IMETHOD SetHasNewMessages(PRBool hasNewMessages);
  NS_IMETHOD ClearNewMessages();
  NS_IMETHOD GetExpungedBytes(PRUint32 *aExpungedBytesCount);
  NS_IMETHOD GetDeletable(PRBool *aDeletable);
  NS_IMETHOD GetRequiresCleanup(PRBool *aRequiresCleanup);
  NS_IMETHOD ClearRequiresCleanup(void);
  NS_IMETHOD ManyHeadersToDownload(PRBool *_retval);
  NS_IMETHOD GetKnowsSearchNntpExtension(PRBool *aKnowsSearchNntpExtension);
  NS_IMETHOD GetAllowsPosting(PRBool *aAllowsPosting);
  NS_IMETHOD GetDisplayRecipients(PRBool *aDisplayRecipients);
  NS_IMETHOD GetRelativePathName(char * *aRelativePathName);
  NS_IMETHOD GetSizeOnDisk(PRUint32 *aSizeOnDisk);
  NS_IMETHOD RememberPassword(const char *password);
  NS_IMETHOD GetRememberedPassword(char * *aRememberedPassword);
  NS_IMETHOD UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *_retval);
  NS_IMETHOD GetUsername(char * *aUsername);
  NS_IMETHOD GetHostname(char * *aHostname);
  NS_IMETHOD SetDeleteIsMoveToTrash(PRBool bVal);
  NS_IMETHOD RecursiveSetDeleteIsMoveToTrash(PRBool bVal);
  NS_IMETHOD SetFlag(PRUint32 flag);
  NS_IMETHOD SetPrefFlag();
  NS_IMETHOD ClearFlag(PRUint32 flag);
  NS_IMETHOD GetFlag(PRUint32 flag, PRBool *_retval);
  NS_IMETHOD ToggleFlag(PRUint32 flag);
  NS_IMETHOD OnFlagChange(PRUint32 flag);
  NS_IMETHOD SetFlags(PRUint32 aFlags);
  NS_IMETHOD GetFoldersWithFlag(PRUint32 flags, PRUint32 resultsize, PRUint32 *numFolders, nsIMsgFolder **result);
  NS_IMETHOD GetExpansionArray(nsISupportsArray *expansionArray);
  // NS_IMETHOD DeleteMessages(nsISupportsArray *message, nsITransactionManager *txnMgr, PRBool deleteStorage);
  NS_IMETHOD CopyMessages(nsIMsgFolder *srcFolder, nsISupportsArray *messages, PRBool isMove, nsIMsgWindow *window, nsIMsgCopyServiceListener *listener, PRBool isFolder, PRBool allowUndo);
  NS_IMETHOD CopyFolder(nsIMsgFolder *srcFolder,PRBool isMoveFolder, nsIMsgWindow *window, nsIMsgCopyServiceListener *listener);
  NS_IMETHOD CopyFileMessage(nsIFileSpec *fileSpec, nsIMsgDBHdr *msgToReplace, PRBool isDraft, nsIMsgWindow *window, nsIMsgCopyServiceListener *listener);
  NS_IMETHOD AcquireSemaphore(nsISupports *semHolder);
  NS_IMETHOD ReleaseSemaphore(nsISupports *semHolder);
  NS_IMETHOD TestSemaphore(nsISupports *semHolder, PRBool *_retval);
  NS_IMETHOD GetLocked(PRBool *aLocked);
  NS_IMETHOD GetNewMessages(nsIMsgWindow *window, nsIUrlListener *aListener);
  // NS_IMETHOD WriteToFolderCache(nsIMsgFolderCache *folderCache);
  // NS_IMETHOD GetCharset(PRUnichar * *aCharset);
  // NS_IMETHOD SetCharset(const PRUnichar * aCharset);
  NS_IMETHOD GetBiffState(PRUint32 *aBiffState);
  NS_IMETHOD SetBiffState(PRUint32 aBiffState);
  NS_IMETHOD GetNumNewMessages(PRInt32 *aNumNewMessages);
  NS_IMETHOD SetNumNewMessages(PRInt32 aNumNewMessages);
  NS_IMETHOD GetNewMessagesNotificationDescription(PRUnichar * *aNewMessagesNotificationDescription);
  NS_IMETHOD GetRootFolder(nsIMsgFolder * *aRootFolder);
  NS_IMETHOD GetMsgDatabase(nsIMsgWindow *aMsgWindow,
                            nsIMsgDatabase * *aMsgDatabase);
  NS_IMETHOD GetMessageHeader(nsMsgKey msgKey, nsIMsgDBHdr **aMsgHdr);
  NS_IMETHOD GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, 
                                  nsIMsgDatabase **db) = 0;

  NS_IMETHOD GetPath(nsIFileSpec * *aPath);
  NS_IMETHOD SetPath(nsIFileSpec * aPath);
  NS_IMETHOD GetBaseMessageURI (char ** baseMessageURI);
  NS_IMETHOD GetUriForMsg(nsIMsgDBHdr *msgHdr, char **aResult);

  NS_IMETHOD MarkMessagesRead(nsISupportsArray *messages, PRBool markRead);
  NS_IMETHOD AddMessageDispositionState(nsIMsgDBHdr *aMessage, nsMsgDispositionState aDispositionFlag);
  NS_IMETHOD MarkAllMessagesRead(void);
  NS_IMETHOD MarkMessagesFlagged(nsISupportsArray *messages, PRBool markFlagged);
  NS_IMETHOD MarkThreadRead(nsIMsgThread *thread);

  NS_IMETHOD GetChildWithURI(const char *uri, PRBool deep, PRBool caseInsensitive, nsIMsgFolder **_retval); 
  NS_IMETHOD EnableNotifications(PRInt32 notificationType, PRBool enable);
  NS_IMETHOD NotifyCompactCompleted();
  NS_IMETHOD ConfirmFolderDeletionForFilter(nsIMsgWindow *msgWindow, PRBool *confirmed);
  NS_IMETHOD AlertFilterChanged(nsIMsgWindow *msgWindow);
  NS_IMETHOD ThrowAlertMsg(const char* msgName, nsIMsgWindow *msgWindow);
  NS_IMETHOD GetStringWithFolderNameFromBundle(const char* msgName, PRUnichar **aResult);
  NS_IMETHOD GetPersistElided(PRBool *aPersistElided);

  // end NS_DECL_NSIMSGFOLDER
  
  // nsRDFResource overrides
  NS_IMETHOD Init(const char* aURI);
  
  // Gets the URL that represents the given message.  Returns a newly
  // created string that must be free'd using XP_FREE().
  // If the db is NULL, then returns a URL that represents the entire
  // folder as a whole.
	// These functions are used for tricking the front end into thinking that we have more 
	// messages than are really in the DB.  This is usually after and IMAP message copy where
	// we don't want to do an expensive select until the user actually opens that folder
	// These functions are called when MSG_Master::GetFolderLineById is populating a MSG_FolderLine
	// struct used by the FE
  PRInt32 GetNumPendingUnread();
  PRInt32 GetNumPendingTotalMessages();
	
  void			ChangeNumPendingUnread(PRInt32 delta);
  void			ChangeNumPendingTotalMessages(PRInt32 delta);




#ifdef HAVE_ADMINURL
  NS_IMETHOD GetAdminUrl(MWContext *context, MSG_AdminURLType type);
  NS_IMETHOD HaveAdminUrl(MSG_AdminURLType type, PRBool *hadAdminUrl);
#endif


#ifdef HAVE_NET
  NS_IMETHOD EscapeMessageId(const char *messageId, const char **escapeMessageID);
  NS_IMETHOD ShouldPerformOperationOffline(PRBool *performOffline);
#endif

#ifdef DOES_FOLDEROPERATIONS
	int DownloadToTempFileAndUpload(MessageCopyInfo *copyInfo, nsMsgKeyArray &keysToSave, MSG_FolderInfo *dstFolder, nsMsgDatabase *sourceDB);
	void UpdateMoveCopyStatus(MWContext *context, PRBool isMove, int32 curMsgCount, int32 totMessages);
#endif


	NS_IMETHOD MatchName(nsString *name, PRBool *matches);

	NS_IMETHOD GenerateMessageURI(nsMsgKey msgKey, char **aURI);
        NS_IMETHOD GetSortOrder(PRInt32 *order);
        NS_IMETHOD SetSortOrder(PRInt32 order);


protected:
  
	// this is a little helper function that is not part of the public interface. 
	// we use it to get the IID of the incoming server for the derived folder.
	// w/out a function like this we would have to implement GetServer in each
	// derived folder class.
	virtual const char* GetIncomingServerType() = 0;

	virtual nsresult CreateBaseMessageURI(const char *aURI);


  // helper routine to parse the URI and update member variables
  nsresult parseURI(PRBool needServer=PR_FALSE);
  nsresult GetBaseStringBundle(nsIStringBundle **aBundle);
  nsresult GetStringFromBundle(const char* msgName, PRUnichar **aResult);
  nsresult ThrowConfirmationPrompt(nsIMsgWindow *msgWindow, const PRUnichar *confirmString, PRBool *confirmed);
  nsresult GetWarnFilterChanged(PRBool *aVal);
  nsresult SetWarnFilterChanged(PRBool aVal);
protected:
  PRUint32 mFlags;
  nsWeakPtr mParent;     //This won't be refcounted for ownership reasons.
  PRInt32 mNumUnreadMessages;        /* count of unread messages (-1 means
                                         unknown; -2 means unknown but we already
                                         tried to find out.) */
  PRInt32 mNumTotalMessages;         /* count of existing messages. */
  PRBool mNotifyCountChanges;
  PRUint32 mExpungedBytes;
  nsCOMPtr<nsISupportsArray> mSubFolders;
  nsVoidArray *mListeners; //This can't be an nsISupportsArray because due to
													 //ownership issues, listeners can't be AddRef'd

  PRBool mInitializedFromCache;
  nsISupports *mSemaphoreHolder; // set when the folder is being written to
								//Due to ownership issues, this won't be AddRef'd.

  nsWeakPtr mServer;

  // These values are used for tricking the front end into thinking that we have more 
  // messages than are really in the DB.  This is usually after and IMAP message copy where
  // we don't want to do an expensive select until the user actually opens that folder
  PRInt32 mNumPendingUnreadMessages;
  PRInt32 mNumPendingTotalMessages;

  PRUint32	mBiffState;
  PRInt32	mNumNewBiffMessages;

  PRBool mIsCachable;
  //
  // stuff from the uri
  //
  PRBool mHaveParsedURI;        // is the URI completely parsed?
  PRBool mIsServerIsValid;
  PRBool mIsServer;
  nsString mName;
  nsCOMPtr<nsIFileSpec> mPath;
  PRBool mDeleteIsMoveToTrash;
  char * mBaseMessageURI; //The uri with the message scheme

  // static stuff for cross-instance objects like atoms
  static nsrefcnt gInstanceCount;

  static nsresult initializeStrings();

  static PRUnichar *kInboxName;
  static PRUnichar *kTrashName;
  static PRUnichar *kSentName;
  static PRUnichar *kDraftsName;
  static PRUnichar *kTemplatesName;
  static PRUnichar *kUnsentName;
  
  static nsIAtom* kTotalUnreadMessagesAtom;
  static nsIAtom* kBiffStateAtom;
  static nsIAtom* kNewMessagesAtom;
  static nsIAtom* kNumNewBiffMessagesAtom;
  static nsIAtom* kTotalMessagesAtom;
  static nsIAtom* kStatusAtom;
  static nsIAtom* kFlaggedAtom;
  static nsIAtom* kNameAtom;
  static nsIAtom* kSynchronizeAtom;
  static nsIAtom* kOpenAtom;

#ifdef MSG_FASTER_URI_PARSING
  // cached parsing URL object
  static nsCOMPtr<nsIURL> mParsingURL;
  static PRBool mParsingURLInUse;
#endif

};

#endif
