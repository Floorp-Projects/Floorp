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
 
   Interface for representing News folders.
 
*********************************************************************************************************/

#ifndef nsMsgNewsFolder_h__
#define nsMsgNewsFolder_h__

#include "nsMsgFolder.h" /* include the interface we are going to support */
#include "nsFileSpec.h"
#include "nsIDBChangeListener.h"
#include "nsFileStream.h"

class nsMsgNewsFolder : public nsMsgFolder, public nsIMsgNewsFolder, public nsIDBChangeListener
{
public:
	nsMsgNewsFolder(void);
	virtual ~nsMsgNewsFolder(void);

  NS_DECL_ISUPPORTS_INHERITED
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
  NS_IMETHOD GetMessages(nsIEnumerator* *result);
	NS_IMETHOD GetThreads(nsIEnumerator** threadEnumerator);
	NS_IMETHOD GetThreadForMessage(nsIMessage *message, nsIMsgThread **thread);


	NS_IMETHOD CreateSubfolder(const char *folderName);

	NS_IMETHOD RemoveSubFolder (nsIMsgFolder *which);
	NS_IMETHOD Delete ();
	NS_IMETHOD Rename (const char *newName);
	NS_IMETHOD Adopt(nsIMsgFolder *srcFolder, PRUint32 *outPos);

  NS_IMETHOD GetChildNamed(nsString& name, nsISupports ** aChild);

  // this override pulls the value from the db
	NS_IMETHOD GetName(char ** name);   // Name of this folder (as presented to user).
	NS_IMETHOD GetPrettyName(nsString& prettyName);	// Override of the base, for top-level news folder

  NS_IMETHOD BuildFolderURL(char **url);

	NS_IMETHOD UpdateSummaryTotals() ;

	NS_IMETHOD GetExpungedBytesCount(PRUint32 *count);
	NS_IMETHOD GetDeletable (PRBool *deletable); 
	NS_IMETHOD GetCanCreateChildren (PRBool *canCreateChildren) ;
	NS_IMETHOD GetCanBeRenamed (PRBool *canBeRenamed);
	NS_IMETHOD GetRequiresCleanup(PRBool *requiresCleanup);

	NS_IMETHOD GetSizeOnDisk(PRUint32 size);

	NS_IMETHOD GetUsersName(char** userName);
	NS_IMETHOD GetHostName(char** hostName);
	NS_IMETHOD UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *authenticate);
	NS_IMETHOD RememberPassword(char *password);
	NS_IMETHOD GetRememberedPassword(char ** password);

  virtual nsresult GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db);

 	NS_IMETHOD DeleteMessage(nsIMessage *message);
	NS_IMETHOD CreateMessageFromMsgDBHdr(nsIMsgDBHdr *msgDBHdr, nsIMessage **message);

	// nsIMsgNewsFolder
  NS_IMETHOD GetPath(nsNativeFileSpec& aPathName);

	//nsIDBChangeListener
	NS_IMETHOD OnKeyChange(nsMsgKey aKeyChanged, int32 aFlags, 
                         nsIDBChangeListener * aInstigator);
	NS_IMETHOD OnKeyDeleted(nsMsgKey aKeyChanged, int32 aFlags, 
                          nsIDBChangeListener * aInstigator);
	NS_IMETHOD OnKeyAdded(nsMsgKey aKeyChanged, int32 aFlags, 
                        nsIDBChangeListener * aInstigator);
	NS_IMETHOD OnAnnouncerGoingAway(nsIDBChangeAnnouncer * instigator);

protected:
	nsresult ParseFolder(nsFileSpec& path);
	nsresult CreateSubFolders(nsFileSpec &path);
	nsresult AddDirectorySeparator(nsFileSpec &path);
	nsresult GetDatabase();

	/* Finds the directory associated with this folder.  That is if the path is
	c:\Inbox, it will return c:\Inbox.sbd if it succeeds.  If that path doesn't
	currently exist then it will create it
	*/
	nsresult CreateDirectoryForFolder(nsFileSpec &path);

	//Creates a subfolder with the name 'name' and adds it to the list of children.
	//Returns the child as well.
	nsresult AddSubfolder(nsAutoString name, nsIMsgFolder **child);


protected:
  nsNativeFileSpec mPath;
	PRUint32  mExpungedBytes;
	PRBool		mHaveReadNameFromDB;
	PRBool		mGettingNews;
	PRBool		mInitialized;
	nsISupportsArray *mMessages;
	nsIMsgDatabase* mNewsDatabase;
};

#endif // nsMsgNewsFolder_h__
