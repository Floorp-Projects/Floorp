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
 
   Interface for representing News folders.
 
*********************************************************************************************************/

#ifndef nsMsgNewsFolder_h__
#define nsMsgNewsFolder_h__

#include "nsMsgDBFolder.h" 
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsINntpIncomingServer.h" // need this for the IID
#include "nsNewsUtils.h"
#include "nsMsgLineBuffer.h"
#include "nsMsgKeySet.h"
#include "nsCOMPtr.h"

class nsMsgNewsFolder : public nsMsgDBFolder, public nsIMsgNewsFolder, public nsMsgLineBuffer
{
public:
	nsMsgNewsFolder(void);
	virtual ~nsMsgNewsFolder(void);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIMSGNEWSFOLDER
#if 0
  static nsresult GetRoot(nsIMsgFolder* *result);
#endif
  // nsICollection methods:
  NS_IMETHOD Enumerate(nsIEnumerator* *result);

  // nsIFolder methods:
  NS_IMETHOD GetSubFolders(nsIEnumerator* *result);

  // nsIMsgFolder methods:
  NS_IMETHOD AddUnique(nsISupports* element);
  NS_IMETHOD ReplaceElement(nsISupports* element, nsISupports* newElement);
  NS_IMETHOD GetMessages(nsISimpleEnumerator* *result);
	NS_IMETHOD UpdateFolder(nsIMsgWindow *aWindow);

	NS_IMETHOD CreateSubfolder(const char *folderName);

	NS_IMETHOD Delete ();
	NS_IMETHOD Rename (const char *newName);
	NS_IMETHOD Adopt(nsIMsgFolder *srcFolder, PRUint32 *outPos);

  NS_IMETHOD GetChildNamed(const char* name, nsISupports ** aChild);
	NS_IMETHODIMP GetAbbreviatedName(PRUnichar * *aAbbreviatedName);

  NS_IMETHOD GetFolderURL(char **url);

	NS_IMETHOD UpdateSummaryTotals(PRBool force) ;

	NS_IMETHOD GetExpungedBytesCount(PRUint32 *count);
	NS_IMETHOD GetDeletable (PRBool *deletable); 
	NS_IMETHOD GetRequiresCleanup(PRBool *requiresCleanup);

	NS_IMETHOD GetSizeOnDisk(PRUint32 *size);

	NS_IMETHOD UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *authenticate);
	NS_IMETHOD RememberPassword(const char *password);
	NS_IMETHOD GetRememberedPassword(char ** password);

  virtual nsresult GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db);

 	NS_IMETHOD DeleteMessages(nsISupportsArray *messages, 
                            nsIMsgWindow *msgWindow, PRBool deleteStorage);
	NS_IMETHOD CreateMessageFromMsgDBHdr(nsIMsgDBHdr *msgDBHdr, nsIMessage **message);
  NS_IMETHOD GetNewMessages(nsIMsgWindow *aWindow);

  NS_IMETHOD GetPath(nsIFileSpec** aPathName);
	NS_IMETHOD GetCanSubscribe(PRBool *aResult);
	NS_IMETHOD GetCanFileMessages(PRBool *aResult);
	NS_IMETHOD GetCanCreateSubfolders(PRBool *aResult);
	NS_IMETHOD GetCanRename(PRBool *aResult);

protected:
	nsresult AddNewsgroupToNewsrcFile(const char *newsgroupname);
	nsresult AbbreviatePrettyName(PRUnichar ** prettyName, PRInt32 fullwords);
	nsresult ParseFolder(nsFileSpec& path);
	nsresult CreateSubFolders(nsFileSpec &path);
	nsresult AddDirectorySeparator(nsFileSpec &path);
	nsresult GetDatabase();

	//Creates a subfolder with the name 'name' and adds it to the list of children.
	//Returns the child as well.
	nsresult AddSubfolder(nsAutoString name, nsIMsgFolder **child, char *setStr);

  nsresult LoadNewsrcFileAndCreateNewsgroups();
  PRInt32 RememberLine(char *line);
  nsresult ForgetLine(void);

  PRInt32 HandleLine(char *line, PRUint32 line_size);
  virtual const char *GetIncomingServerType() {return "nntp";}
  
  nsByteArray		m_inputStream;

protected:
	nsNativeFileSpec *mPath;
	PRUint32  mExpungedBytes;
	PRBool		mGettingNews;
	PRBool		mInitialized;
	nsISupportsArray *mMessages;
	char      *mOptionLines;
  
	// cache this until we open the db.
	nsCString	m_unreadSet;

	nsCOMPtr<nsIFileSpec> mNewsrcFilePath; 
};

#endif // nsMsgNewsFolder_h__
