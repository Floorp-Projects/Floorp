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
 
   Interface for representing Local Mail folders.
 
*********************************************************************************************************/

#ifndef nsMsgLocalMailFolder_h__
#define nsMsgLocalMailFolder_h__

#include "nsMsgDBFolder.h" /* include the interface we are going to support */
#include "nsFileSpec.h"
#include "nsICopyMessageListener.h"
#include "nsFileStream.h"
#include "nsIPop3IncomingServer.h"  // need this for an interface ID
#include "nsMsgTxn.h"
#include "nsIMsgMessageService.h"
#include "nsIMsgParseMailMsgState.h"
#include "nsITransactionManager.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIMsgStringService.h"

#define COPY_BUFFER_SIZE 16384

struct nsLocalMailCopyState
{
  nsLocalMailCopyState();
  virtual ~nsLocalMailCopyState();
  
  nsOutputFileStream * m_fileStream;
  nsCOMPtr<nsISupports> m_srcSupport;
  nsCOMPtr<nsISupportsArray> m_messages;
  nsCOMPtr<nsMsgTxn> m_undoMsgTxn;
  nsCOMPtr<nsIMsgDBHdr> m_message; // current copy message
  nsCOMPtr<nsIMsgParseMailMsgState> m_parseMsgState;
  nsCOMPtr<nsIMsgCopyServiceListener> m_listener;
  nsCOMPtr<nsIMsgWindow> m_msgWindow;

  // for displaying status;
  nsCOMPtr <nsIMsgStatusFeedback> m_statusFeedback;
  nsCOMPtr <nsIStringBundle> m_stringBundle;
  PRInt64 m_lastProgressTime;

  nsMsgKey m_curDstKey;
  PRUint32 m_curCopyIndex;
  nsCOMPtr <nsIMsgMessageService> m_messageService;
  PRUint32 m_totalMsgCount;
  PRBool m_isMove;
  PRBool m_isFolder;   // isFolder move/copy
  PRBool m_dummyEnvelopeNeeded;
  char m_dataBuffer[COPY_BUFFER_SIZE+1];
  PRUint32 m_leftOver;
  PRBool m_copyingMultipleMessages;
  PRBool m_fromLineSeen;
  PRBool m_allowUndo;
};

class nsMsgLocalMailFolder : public nsMsgDBFolder,
                             public nsIMsgLocalMailFolder,
                             public nsICopyMessageListener
{
public:
	nsMsgLocalMailFolder(void);
	virtual ~nsMsgLocalMailFolder(void);
  NS_DECL_NSICOPYMESSAGELISTENER
  NS_DECL_NSIMSGLOCALMAILFOLDER
  NS_DECL_ISUPPORTS_INHERITED
#if 0
  static nsresult GetRoot(nsIMsgFolder* *result);
#endif
	// nsIRDFResource methods:
	NS_IMETHOD Init(const char *aURI);
  
	// nsICollection methods:
	NS_IMETHOD Enumerate(nsIEnumerator* *result);

  // nsIUrlListener methods
	NS_IMETHOD OnStartRunningUrl(nsIURI * aUrl);
	NS_IMETHOD OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode);

	// nsIFolder methods:
	NS_IMETHOD GetSubFolders(nsIEnumerator* *result);

	// nsIMsgFolder methods:
	NS_IMETHOD AddUnique(nsISupports* element);
	NS_IMETHOD ReplaceElement(nsISupports* element, nsISupports* newElement);
	NS_IMETHOD GetMessages(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator* *result);
	NS_IMETHOD UpdateFolder(nsIMsgWindow *aWindow);

	NS_IMETHOD CreateSubfolder(const PRUnichar *folderName ,nsIMsgWindow *msgWindow);
  NS_IMETHOD AddSubfolder(nsAutoString *folderName, nsIMsgFolder** newFolder);

  NS_IMETHOD Compact(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow);
  NS_IMETHOD CompactAll(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow, nsISupportsArray *aFolderArray, PRBool aCompactOfflineAlso, nsISupportsArray *aOfflineFolderArray);
  NS_IMETHOD EmptyTrash(nsIMsgWindow *msgWindow, nsIUrlListener *aListener);
	NS_IMETHOD Delete ();
  NS_IMETHOD DeleteSubFolders(nsISupportsArray *folders, nsIMsgWindow *msgWindow);
  NS_IMETHOD CreateStorageIfMissing(nsIUrlListener* urlListener);
	NS_IMETHOD Rename (const PRUnichar *aNewName, nsIMsgWindow *msgWindow);
    NS_IMETHOD RenameSubFolders (nsIMsgWindow *msgWindow, nsIMsgFolder *oldFolder);

	NS_IMETHOD GetPrettyName(PRUnichar** prettyName);	// Override of the base, for top-level mail folder

	NS_IMETHOD GetFolderURL(char **url);

	NS_IMETHOD UpdateSummaryTotals(PRBool force) ;

	NS_IMETHOD GetDeletable (PRBool *deletable); 
	NS_IMETHOD GetRequiresCleanup(PRBool *requiresCleanup);

	NS_IMETHOD GetSizeOnDisk(PRUint32* size);

	NS_IMETHOD UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *authenticate);
	NS_IMETHOD RememberPassword(const char *password);
	NS_IMETHOD GetRememberedPassword(char ** password);

	NS_IMETHOD  GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db);

 	NS_IMETHOD DeleteMessages(nsISupportsArray *messages, 
                            nsIMsgWindow *msgWindow, PRBool
                            deleteStorage, PRBool isMove,
                            nsIMsgCopyServiceListener* listener, PRBool allowUndo);
  NS_IMETHOD CopyMessages(nsIMsgFolder *srcFolder, nsISupportsArray* messages,
                          PRBool isMove, nsIMsgWindow *msgWindow,
                          nsIMsgCopyServiceListener* listener, PRBool isFolder, PRBool allowUndo);
  NS_IMETHOD CopyFolder(nsIMsgFolder *srcFolder, PRBool isMoveFolder, nsIMsgWindow *msgWindow,
                          nsIMsgCopyServiceListener* listener);
  NS_IMETHOD CopyFileMessage(nsIFileSpec* fileSpec, nsIMsgDBHdr* msgToReplace,
                             PRBool isDraftOrTemplate, 
                             nsIMsgWindow *msgWindow,
                             nsIMsgCopyServiceListener* listener);
	NS_IMETHOD GetNewMessages(nsIMsgWindow *aWindow, nsIUrlListener *aListener);
    NS_IMETHOD NotifyCompactCompleted();

protected:
	nsresult CopyFolderAcrossServer(nsIMsgFolder *srcFolder, nsIMsgWindow *msgWindow,nsIMsgCopyServiceListener* listener);

	nsresult CreateSubFolders(nsFileSpec &path);
	nsresult AddDirectorySeparator(nsFileSpec &path);
	nsresult GetDatabase(nsIMsgWindow *aMsgWindow);
    nsresult GetTrashFolder(nsIMsgFolder** trashFolder);
    nsresult WriteStartOfNewMessage();
  nsresult IsChildOfTrash(PRBool *result);
  nsresult RecursiveSetDeleteIsMoveTrash(PRBool bVal);
  nsresult AlertFolderExists(nsIMsgWindow *msgWindow);

  nsresult CheckIfFolderExists(const PRUnichar *folderName, nsFileSpec &path, nsIMsgWindow *msgWindow);

	/* Finds the directory associated with this folder.  That is if the path is
	c:\Inbox, it will return c:\Inbox.sbd if it succeeds.  If that path doesn't
	currently exist then it will create it
	*/
	nsresult CreateDirectoryForFolder(nsFileSpec &path);

	nsresult DeleteMessage(nsISupports *message, nsIMsgWindow *msgWindow,
                         PRBool deleteStorage, PRBool commit);

	// copy message helper
	nsresult DisplayMoveCopyStatusMsg();
    nsresult SortMessagesBasedOnKey(nsISupportsArray *messages, nsMsgKeyArray *aKeyArray, nsIMsgFolder *srcFolder);

  nsresult CopyMessageTo(nsISupports *message, nsIMsgFolder *dstFolder,
                         nsIMsgWindow *msgWindow, PRBool isMove);

	// copy multiple messages at a time from this folder
	nsresult CopyMessagesTo(nsISupportsArray *messages, nsIMsgWindow *aMsgWindow,
                                             nsIMsgFolder *dstFolder,
                                             PRBool isMove);
	virtual const char* GetIncomingServerType();
  nsresult InitCopyState(nsISupports* aSupport, nsISupportsArray* messages,
                         PRBool isMove, nsIMsgCopyServiceListener* listener, nsIMsgWindow *msgWindow, PRBool isMoveFolder, PRBool allowUndo);
  void ClearCopyState(PRBool moveCopySucceeded);
	virtual nsresult CreateBaseMessageURI(const char *aURI);
protected:
	PRBool		mHaveReadNameFromDB;
	PRBool		mGettingMail;
	PRBool		mInitialized;
	nsLocalMailCopyState *mCopyState; //We will only allow one of these at a
                                    //time
  const char *mType;
  PRBool      mCheckForNewMessagesAfterParsing;
  PRBool      mParsingInbox;
  nsCOMPtr<nsIMsgStringService> mMsgStringService;

  nsresult setSubfolderFlag(PRUnichar *aFolderName, PRUint32 flags);
  nsresult DeleteMsgsOnPop3Server(nsISupportsArray *messages);
};

#endif // nsMsgLocalMailFolder_h__
