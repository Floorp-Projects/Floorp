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

#define FOUR_K 4096

struct nsLocalMailCopyState
{
  nsLocalMailCopyState();
  virtual ~nsLocalMailCopyState();
  
  nsOutputFileStream* fileStream;
  nsCOMPtr<nsISupports> srcSupport;
  nsCOMPtr<nsISupportsArray> messages;
  nsCOMPtr<nsMsgTxn> undoMsgTxn;
  nsCOMPtr<nsIMessage> message; // current copy message
  nsCOMPtr<nsIMsgParseMailMsgState> parseMsgState;
  nsCOMPtr<nsIMsgCopyServiceListener> listener;
  
  nsIMsgMessageService* messageService;
  PRBool isMove;
  PRUint32 curCopyIndex;
  nsMsgKey curDstKey;
  PRUint32 totalMsgCount;
  char dataBuffer[FOUR_K];
};

class nsMsgLocalMailFolder : public nsMsgDBFolder,
                             public nsIMsgLocalMailFolder,
                             public nsICopyMessageListener
{
public:
	nsMsgLocalMailFolder(void);
	virtual ~nsMsgLocalMailFolder(void);

  NS_DECL_ISUPPORTS_INHERITED
#if 0
  static nsresult GetRoot(nsIMsgFolder* *result);
#endif
	// nsIRDFResource methods:
	NS_IMETHOD Init(const char *aURI);
  
	// nsICollection methods:
	NS_IMETHOD Enumerate(nsIEnumerator* *result);

	// nsIFolder methods:
	NS_IMETHOD GetSubFolders(nsIEnumerator* *result);

	// nsIMsgFolder methods:
	NS_IMETHOD AddUnique(nsISupports* element);
	NS_IMETHOD ReplaceElement(nsISupports* element, nsISupports* newElement);
	NS_IMETHOD GetMessages(nsIEnumerator* *result);


	NS_IMETHOD CreateSubfolder(const char *folderName);

	NS_IMETHOD RemoveSubFolder (nsIMsgFolder *which);
	NS_IMETHOD Delete ();
	NS_IMETHOD Rename (const char *newName);
	NS_IMETHOD Adopt(nsIMsgFolder *srcFolder, PRUint32 *outPos);

	// this override pulls the value from the db
	NS_IMETHOD GetName(PRUnichar ** name);   // Name of this folder (as presented to user).
	NS_IMETHOD GetPrettyName(PRUnichar** prettyName);	// Override of the base, for top-level mail folder

	NS_IMETHOD BuildFolderURL(char **url);

	NS_IMETHOD UpdateSummaryTotals() ;

	NS_IMETHOD GetExpungedBytesCount(PRUint32 *count);
	NS_IMETHOD GetDeletable (PRBool *deletable); 
	NS_IMETHOD GetCanCreateChildren (PRBool *canCreateChildren) ;
	NS_IMETHOD GetCanBeRenamed (PRBool *canBeRenamed);
	NS_IMETHOD GetRequiresCleanup(PRBool *requiresCleanup);

	NS_IMETHOD GetSizeOnDisk(PRUint32* size);

	NS_IMETHOD GetUsername(char** userName);
	NS_IMETHOD GetHostname(char** hostName);
	NS_IMETHOD UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *authenticate);
	NS_IMETHOD RememberPassword(const char *password);
	NS_IMETHOD GetRememberedPassword(char ** password);

	virtual nsresult GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db);

 	NS_IMETHOD DeleteMessages(nsISupportsArray *messages, 
                            nsITransactionManager *txnMgr, PRBool
                            deleteStorage);
  NS_IMETHOD CopyMessages(nsIMsgFolder *srcFolder, nsISupportsArray* messages,
                          PRBool isMove, nsITransactionManager* txnMgr,
                          nsIMsgCopyServiceListener* listener);
  NS_IMETHOD CopyFileMessage(nsIFileSpec* fileSpec, nsIMessage* msgToReplace,
                             PRBool isDraftOrTemplate, 
                             nsITransactionManager* txnMgr,
                             nsIMsgCopyServiceListener* listener);
	NS_IMETHOD CreateMessageFromMsgDBHdr(nsIMsgDBHdr *msgDBHdr, nsIMessage **message);
	NS_IMETHOD GetNewMessages();

	// nsIMsgMailFolder
	NS_IMETHOD GetPath(nsIFileSpec ** aPathName);

	//nsICopyMessageListener
	NS_IMETHOD BeginCopy(nsIMessage *message);
	NS_IMETHOD CopyData(nsIInputStream *aIStream, PRInt32 aLength);
	NS_IMETHOD EndCopy(PRBool copySucceeded);

	NS_IMETHOD FindSubFolder(const char *subFolderName, nsIFolder **folder);

protected:
	nsresult ParseFolder(nsFileSpec& path);
	nsresult CreateSubFolders(nsFileSpec &path);
	nsresult AddDirectorySeparator(nsFileSpec &path);
	nsresult GetDatabase();
  nsresult GetTrashFolder(nsIMsgFolder** trashFolder);

	/* Finds the directory associated with this folder.  That is if the path is
	c:\Inbox, it will return c:\Inbox.sbd if it succeeds.  If that path doesn't
	currently exist then it will create it
	*/
	nsresult CreateDirectoryForFolder(nsFileSpec &path);

	//Creates a subfolder with the name 'name' and adds it to the list of children.
	//Returns the child as well.
	nsresult AddSubfolder(nsAutoString name, nsIMsgFolder **child);

	nsresult DeleteMessage(nsIMessage *message, nsITransactionManager *txnMgr,
                         PRBool deleteStorage);
  // copy message helper
	nsresult CopyMessageTo(nsIMessage *message, nsIMsgFolder *dstFolder,
                         PRBool isMove);

	virtual const char* GetIncomingServerType() {return "pop3";}
  nsresult SetTransactionManager(nsITransactionManager* txnMgr);
  nsresult InitCopyState(nsISupports* aSupport, nsISupportsArray* messages,
                         PRBool isMove, nsIMsgCopyServiceListener* listener);
  void ClearCopyState();

protected:
	nsNativeFileSpec *mPath;
	PRUint32  mExpungedBytes;
	PRBool		mHaveReadNameFromDB;
	PRBool		mGettingMail;
	PRBool		mInitialized;
	nsLocalMailCopyState *mCopyState; //We will only allow one of these at a
                                    //time
  nsCOMPtr<nsITransactionManager> mTxnMgr;
};


#endif // nsMsgLocalMailFolder_h__
