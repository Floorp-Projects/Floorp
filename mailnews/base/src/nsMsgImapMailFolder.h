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

#ifndef nsMsgImapMailFolder_h__
#define nsMsgImapMailFolder_h__

#include "nsMsgFolder.h" /* include the interface we are going to support */

class nsMsgImapMailFolder : public nsMsgFolder, public nsIMsgImapMailFolder
{
public:
	nsMsgImapMailFolder(const char* uri);
	~nsMsgImapMailFolder();

  NS_DECL_ISUPPORTS

#ifdef HAVE_DB	
	virtual MsgERR BeginCopyingMessages(MSG_FolderInfo *dstFolder, 
																				MessageDB *sourceDB,
																				IDArray *srcArray, 
																				MSG_UrlQueue *urlQueue,
																				int32 srcCount,
																				MessageCopyInfo *copyInfo);


	virtual int FinishCopyingMessages(MWContext *context,
																			MSG_FolderInfo * srcFolder, 
																			MSG_FolderInfo *dstFolder, 
																			MessageDB *sourceDB,
																			IDArray **ppSrcArray, 
																			int32 srcCount,
																			msg_move_state *state);
#endif

	NS_IMETHOD CreateSubfolder(const char *leafNameFromUser, nsIMsgFolder **outFolder, PRUint32 *outPos);

	NS_IMETHOD RemoveSubFolder(const nsIMsgFolder *which);
	NS_IMETHOD Delete();
	NS_IMETHOD Rename(const char *newName);
	NS_IMETHOD Adopt(const nsIMsgFolder *srcFolder, PRUint32 *outPos);

  NS_IMETHOD FindChildNamed(const char *name, nsIMsgFolder ** aChild);

  // this override pulls the value from the db
	NS_IMETHOD GetName(char** name);   // Name of this folder(as presented to user).

  NS_IMETHOD BuildFolderURL(char **url);

	NS_IMETHOD UpdateSummaryTotals() ;

	NS_IMETHOD GetExpungedBytesCount(PRUint32 *count);
	NS_IMETHOD GetDeletable(PRBool *deletable); 
	NS_IMETHOD GetCanCreateChildren(PRBool *canCreateChildren) ;
	NS_IMETHOD GetCanBeRenamed(PRBool *canBeRenamed);
	NS_IMETHOD GetRequiresCleanup(PRBool *requiresCleanup);


	NS_IMETHOD GetRelativePathName(char **pathName);


	NS_IMETHOD GetSizeOnDisk(PRUint32 size);

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
	PRUint32  mExpungedBytes;
	PRBool		mHaveReadNameFromDB;
	PRBool		mGettingMail;
};

#endif // nsMsgImapMailFolder_h__
