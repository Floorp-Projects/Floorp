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
 * Seth Spitzer <sspitzer@netscape.com>
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

	NS_IMETHOD CreateSubfolder(const PRUnichar *folderName,nsIMsgWindow *msgWindow);

	NS_IMETHOD Delete ();
	NS_IMETHOD Rename (const PRUnichar *newName, nsIMsgWindow *msgWindow);

	NS_IMETHOD GetAbbreviatedName(PRUnichar * *aAbbreviatedName);

  NS_IMETHOD GetFolderURL(char **url);

	NS_IMETHOD UpdateSummaryTotals(PRBool force) ;

	NS_IMETHOD GetExpungedBytesCount(PRUint32 *count);
	NS_IMETHOD GetDeletable (PRBool *deletable); 
	NS_IMETHOD GetRequiresCleanup(PRBool *requiresCleanup);

	NS_IMETHOD GetSizeOnDisk(PRUint32 *size);

  NS_IMETHOD GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db);

 	NS_IMETHOD DeleteMessages(nsISupportsArray *messages, 
                            nsIMsgWindow *msgWindow, PRBool deleteStorage, PRBool isMove,
                            nsIMsgCopyServiceListener* listener, PRBool allowUndo);
  NS_IMETHOD GetNewMessages(nsIMsgWindow *aWindow, nsIUrlListener *aListener);

	NS_IMETHOD GetCanSubscribe(PRBool *aResult);
	NS_IMETHOD GetCanFileMessages(PRBool *aResult);
	NS_IMETHOD GetCanCreateSubfolders(PRBool *aResult);
	NS_IMETHOD GetCanRename(PRBool *aResult);
    NS_IMETHOD GetCanCompact(PRBool *aResult);
    NS_IMETHOD OnReadChanged(nsIDBChangeListener * aInstigator);

  NS_IMETHOD DownloadMessagesForOffline(nsISupportsArray *messages, nsIMsgWindow *window);
  NS_IMETHOD Compact(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow);
  NS_IMETHOD DownloadAllForOffline(nsIUrlListener *listener, nsIMsgWindow *msgWindow);
  NS_IMETHOD GetSortOrder(PRInt32 *order);
  NS_IMETHOD SetSortOrder(PRInt32 order);

  NS_IMETHOD GetPersistElided(PRBool *aPersistElided);

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
	nsCString mOptionLines;
        nsCString mUnsubscribedNewsgroupLines;
        PRBool m_downloadMessageForOfflineUse;
        nsMsgKeySet *mReadSet; 

	nsCOMPtr<nsIFileSpec> mNewsrcFilePath; 

	// used for auth news
 	char 		*mGroupUsername;
	char		*mGroupPassword;

  // the name of the newsgroup.
  char    *mAsciiName;
  PRInt32 mSortOrder;

private:
    nsresult CreateNewsgroupUsernameUrlForSignon(const char *inUriStr, char **result);
    nsresult CreateNewsgroupPasswordUrlForSignon(const char *inUriStr, char **result);
    nsresult CreateNewsgroupUrlForSignon(const char *inUriStr, const char *ref, char **result);
};

#endif // nsMsgNewsFolder_h__
