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
 * Seth Spitzer <sspitzer@netscape.com>
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
#include "nsIMsgNewsFolder.h"
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
  NS_IMETHOD GetMessages(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator* *result);
	NS_IMETHOD UpdateFolder(nsIMsgWindow *aWindow);

	NS_IMETHOD CreateSubfolder(const PRUnichar *folderName);

	NS_IMETHOD Delete ();
	NS_IMETHOD Rename (const PRUnichar *newName);
	NS_IMETHOD Adopt(nsIMsgFolder *srcFolder, PRUint32 *outPos);

	NS_IMETHOD GetAbbreviatedName(PRUnichar * *aAbbreviatedName);

  NS_IMETHOD GetFolderURL(char **url);

	NS_IMETHOD UpdateSummaryTotals(PRBool force) ;

	NS_IMETHOD GetExpungedBytesCount(PRUint32 *count);
	NS_IMETHOD GetDeletable (PRBool *deletable); 
	NS_IMETHOD GetRequiresCleanup(PRBool *requiresCleanup);

	NS_IMETHOD GetSizeOnDisk(PRUint32 *size);

  NS_IMETHOD GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db);

 	NS_IMETHOD DeleteMessages(nsISupportsArray *messages, 
                            nsIMsgWindow *msgWindow, PRBool deleteStorage, PRBool isMove);
	NS_IMETHOD CreateMessageFromMsgDBHdr(nsIMsgDBHdr *msgDBHdr, nsIMessage **message);
  NS_IMETHOD GetNewMessages(nsIMsgWindow *aWindow);

	NS_IMETHOD GetCanSubscribe(PRBool *aResult);
	NS_IMETHOD GetCanFileMessages(PRBool *aResult);
	NS_IMETHOD GetCanCreateSubfolders(PRBool *aResult);
	NS_IMETHOD GetCanRename(PRBool *aResult);
    NS_IMETHOD OnReadChanged(nsIDBChangeListener * aInstigator);

	// for nsMsgLineBuffer
	virtual PRInt32 HandleLine(char *line, PRUint32 line_size);

protected:
	nsresult AbbreviatePrettyName(PRUnichar ** prettyName, PRInt32 fullwords);
	nsresult ParseFolder(nsFileSpec& path);
	nsresult CreateSubFolders(nsFileSpec &path);
	nsresult AddDirectorySeparator(nsFileSpec &path);
	nsresult GetDatabase(nsIMsgWindow *aMsgWindow);

  nsresult LoadNewsrcFileAndCreateNewsgroups();
  PRInt32 RememberLine(const char *line);
  nsresult RememberUnsubscribedGroup(const char *newsgroup, const char *setStr);
  nsresult ForgetLine(void);
	nsresult GetNewsMessages(nsIMsgWindow *aMsgWindow, PRBool getOld);

  PRInt32 HandleNewsrcLine(char *line, PRUint32 line_size);
  virtual const char *GetIncomingServerType() {return "nntp";}
  virtual nsresult CreateBaseMessageURI(const char *aURI);

	nsByteArray		m_newsrcInputStream;

protected:
	PRUint32  mExpungedBytes;
	PRBool		mGettingNews;
	PRBool		mInitialized;
	nsISupportsArray *mMessages;
	nsCAutoString mOptionLines;
    nsCAutoString mUnsubscribedNewsgroupLines;

	// cache this until we open the db.
	char        *mCachedNewsrcLine;

	nsCOMPtr<nsIFileSpec> mNewsrcFilePath; 

	// used for auth news
 	char 		*mGroupUsername;
	char		*mGroupPassword;

private:
    nsresult CreateNewsgroupUsernameUrlForSignon(const char *inUriStr, char **result);
    nsresult CreateNewsgroupPasswordUrlForSignon(const char *inUriStr, char **result);
    nsresult CreateNewsgroupUrlForSignon(const char *inUriStr, const char *ref, char **result);
};

#endif // nsMsgNewsFolder_h__
