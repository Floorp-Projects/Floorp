/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/********************************************************************************************************
 
   Interface for representing Local Mail folders.
 
*********************************************************************************************************/

#ifndef nsMsgLocalMailFolder_h__
#define nsMsgLocalMailFolder_h__

#include "nsMsgDBFolder.h" /* include the interface we are going to support */
#include "nsFileSpec.h"
#include "nsIMessage.h"
#include "nsICopyMessageListener.h"
#include "nsFileStream.h"
#include "nsIPop3IncomingServer.h"  // need this for an interface ID
#include "nsMsgTxn.h"
#include "nsIMsgMessageService.h"
#include "nsIMsgParseMailMsgState.h"
#include "nsITransactionManager.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIMsgStringService.h"

#define FOUR_K 4096

struct nsLocalMailCopyState
{
  nsLocalMailCopyState();
  virtual ~nsLocalMailCopyState();
  
  nsOutputFileStream * m_fileStream;
  nsCOMPtr<nsISupports> m_srcSupport;
  nsCOMPtr<nsISupportsArray> m_messages;
  nsCOMPtr<nsMsgTxn> m_undoMsgTxn;
  nsCOMPtr<nsIMessage> m_message; // current copy message
  nsCOMPtr<nsIMsgParseMailMsgState> m_parseMsgState;
  nsCOMPtr<nsIMsgCopyServiceListener> m_listener;
  
  nsMsgKey m_curDstKey;
  PRUint32 m_curCopyIndex;
  nsIMsgMessageService* m_messageService;
  PRUint32 m_totalMsgCount;
  PRBool m_isMove;
  PRBool m_dummyEnvelopeNeeded;
  char m_dataBuffer[FOUR_K+1];
  PRUint32 m_leftOver;
  PRBool m_copyingMultipleMessages;
  PRBool m_fromLineSeen;
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

	NS_IMETHOD CreateSubfolder(const PRUnichar *folderName);
  NS_IMETHOD AddSubfolder(nsAutoString *folderName, nsIMsgFolder** newFolder);

  NS_IMETHOD Compact();
  NS_IMETHOD EmptyTrash(nsIMsgWindow *msgWindow, nsIUrlListener *aListener);
	NS_IMETHOD Delete ();
  NS_IMETHOD DeleteSubFolders(nsISupportsArray *folders, nsIMsgWindow *msgWindow);
  NS_IMETHOD CreateStorageIfMissing(nsIUrlListener* urlListener);
	NS_IMETHOD Rename (const PRUnichar *aNewName);
	NS_IMETHOD Adopt(nsIMsgFolder *srcFolder, PRUint32 *outPos);

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
                            deleteStorage, PRBool isMove);
  NS_IMETHOD CopyMessages(nsIMsgFolder *srcFolder, nsISupportsArray* messages,
                          PRBool isMove, nsIMsgWindow *msgWindow,
                          nsIMsgCopyServiceListener* listener);
  NS_IMETHOD CopyFileMessage(nsIFileSpec* fileSpec, nsIMessage* msgToReplace,
                             PRBool isDraftOrTemplate, 
                             nsIMsgWindow *msgWindow,
                             nsIMsgCopyServiceListener* listener);
	NS_IMETHOD CreateMessageFromMsgDBHdr(nsIMsgDBHdr *msgDBHdr, nsIMessage **message);
	NS_IMETHOD GetNewMessages(nsIMsgWindow *aWindow);


protected:
	nsresult CreateSubFolders(nsFileSpec &path);
	nsresult AddDirectorySeparator(nsFileSpec &path);
	nsresult GetDatabase(nsIMsgWindow *aMsgWindow);
    nsresult GetTrashFolder(nsIMsgFolder** trashFolder);
    nsresult WriteStartOfNewMessage();
  nsresult IsChildOfTrash(PRBool *result);
  nsresult RecursiveSetDeleteIsMoveTrash(PRBool bVal);
  nsresult NotifyStoreClosedAllHeaders();

	/* Finds the directory associated with this folder.  That is if the path is
	c:\Inbox, it will return c:\Inbox.sbd if it succeeds.  If that path doesn't
	currently exist then it will create it
	*/
	nsresult CreateDirectoryForFolder(nsFileSpec &path);

	nsresult DeleteMessage(nsIMessage *message, nsIMsgWindow *msgWindow,
                         PRBool deleteStorage);
  // copy message helper
	nsresult CopyMessageTo(nsIMessage *message, nsIMsgFolder *dstFolder,
                         nsIMsgWindow *msgWindow, PRBool isMove);

	// copy multiple messages at a time from this folder
	nsresult CopyMessagesTo(nsISupportsArray *messages, nsIMsgWindow *aMsgWindow,
                                             nsIMsgFolder *dstFolder,
                                             PRBool isMove);

	virtual const char* GetIncomingServerType();
  nsresult SetTransactionManager(nsITransactionManager* txnMgr);
  nsresult InitCopyState(nsISupports* aSupport, nsISupportsArray* messages,
                         PRBool isMove, nsIMsgCopyServiceListener* listener, nsIMsgWindow *msgWindow);
  void ClearCopyState();
	virtual nsresult CreateBaseMessageURI(const char *aURI);

protected:
	PRBool		mHaveReadNameFromDB;
	PRBool		mGettingMail;
	PRBool		mInitialized;
	nsLocalMailCopyState *mCopyState; //We will only allow one of these at a
                                    //time
  nsCOMPtr<nsITransactionManager> mTxnMgr;
  const char *mType;
  nsCOMPtr<nsIMsgStringService> mMsgStringService;

  nsresult setSubfolderFlag(PRUnichar *aFolderName, PRUint32 flags);
  nsresult DeleteMsgsOnPop3Server(nsISupportsArray *messages);
};

#endif // nsMsgLocalMailFolder_h__
