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
 
 /*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgFolder.idl
 */

#ifndef __gen_nsIMsgFolder_h__
#define __gen_nsIMsgFolder_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsISupportsArray.h" /* interface nsISupportsArray */
#include "nsIRDFNode.h" /* interface nsIRDFNode */

enum FolderType {
  FOLDER_MAIL,
  FOLDER_IMAPMAIL,
  FOLDER_NEWSGROUP,
  FOLDER_CONTAINERONLY,
  FOLDER_CATEGORYCONTAINER,
  FOLDER_IMAPSERVERCONTAINER,
  FOLDER_UNKNOWN
};


/* starting interface nsIMsgFolder */

/* {85e39ff0-b248-11d2-b7ef-00805f05ffa5} */
#define NS_IMSGFOLDER_IID_STR "85e39ff0-b248-11d2-b7ef-00805f05ffa5"
#define NS_IMSGFOLDER_IID \
  {0x85e39ff0, 0xb248, 0x11d2, \
    { 0xb7, 0xef, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsIMsgFolder : public nsIRDFResource {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGFOLDER_IID;
    return iid;
  }

  /*  <IDL>  */
  NS_IMETHOD GetType(FolderType *aType) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetPrettyName(char * *aPrettyName) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetName(char * *aName) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetName(const char *name) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetPrettiestName(char * *aPrettiestName) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNameFromPathName(const char *pathName, char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD HasSubFolders(PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNumSubFolders(PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNumSubFoldersToDisplay(PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetSubFolder(PRInt32 which, nsIMsgFolder **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetSubFolders(nsISupportsArray **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddSubFolder(const nsIMsgFolder *folder) = 0;

  /*  <IDL>  */
  NS_IMETHOD RemoveSubFolder(const nsIMsgFolder *folder) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetDeleteIsMoveToTrash(PRBool *aDeleteIsMoveToTrash) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetShowDeletedMessages(PRBool *aShowDeletedMessages) = 0;

  /*  <IDL>  */
  NS_IMETHOD OnCloseFolder() = 0;

  /*  <IDL>  */
  NS_IMETHOD Delete() = 0;

  /*  <IDL>  */
  NS_IMETHOD PropagateDelete(nsIMsgFolder **folder, PRBool deleteStorage) = 0;

  /*  <IDL>  */
  NS_IMETHOD RecursiveDelete(PRBool deleteStorage) = 0;

  /*  <IDL>  */
  NS_IMETHOD CreateSubfolder(const char *leafNameFromUser, nsIMsgFolder **outFolder, PRInt32 *outPos) = 0;

  /*  <IDL>  */
  NS_IMETHOD Rename(const char *name) = 0;

  /*  <IDL>  */
  NS_IMETHOD Adopt(const nsIMsgFolder *srcFolder, PRInt32 *outPos) = 0;

  /*  <IDL>  */
  NS_IMETHOD ContainsChildNamed(const char *name, PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD FindChildNamed(const char *name, nsIMsgFolder **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD FindParentOf(const nsIMsgFolder *childFolder, nsIMsgFolder **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsParentOf(const nsIMsgFolder *folder, PRBool deep, PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GenerateUniqueSubfolderName(const char *prefix, const nsIMsgFolder *otherFolder, char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetDepth(PRInt32 *aDepth) = 0;
  NS_IMETHOD SetDepth(PRInt32 aDepth) = 0;

  /*  <IDL>  */
  NS_IMETHOD UpdateSummaryTotals() = 0;

  /*  <IDL>  */
  NS_IMETHOD SummaryChanged() = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNumUnread(PRBool deep, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetTotalMessages(PRBool deep, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetExpungedBytesCount(PRInt32 *aExpungedBytesCount) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetDeletable(PRBool *aDeletable) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetCanCreateChildren(PRBool *aCanCreateChildren) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetCanBeRenamed(PRBool *aCanBeRenamed) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetRequiresCleanup(PRBool *aRequiresCleanup) = 0;

  /*  <IDL>  */
  NS_IMETHOD ClearRequiresCleanup() = 0;

  /*  <IDL>  */
  NS_IMETHOD GetKnowsSearchNntpExtension(PRBool *aKnowsSearchNntpExtension) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetAllowsPosting(PRBool *aAllowsPosting) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetRelativePathName(char * *aRelativePathName) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetSizeOnDisk(PRInt32 *aSizeOnDisk) = 0;

  /*  <IDL>  */
  NS_IMETHOD RememberPassword(const char *password) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetRememberedPassword(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetUserName(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetHostName(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddSubfolderIfUnique(const nsIMsgFolder *newSubfolder) = 0;

  /*  <IDL>  */
  NS_IMETHOD ReplaceSubfolder(const nsIMsgFolder *oldFolder, const nsIMsgFolder *newFolder) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetFlag(PRInt32 flag) = 0;

  /*  <IDL>  */
  NS_IMETHOD ClearFlag(PRInt32 flag) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFlag(PRInt32 flag, PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD ToggleFlag(PRInt32 flag) = 0;

  /*  <IDL>  */
  NS_IMETHOD OnFlagChange(PRInt32 flag) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFlags(PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFoldersWithFlag(PRInt32 flags, nsIMsgFolder **result, PRInt32 resultsize, PRInt32 *numFolders) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetExpansionArray(const nsISupportsArray *expansionArray) = 0;
};

#endif /* __gen_nsIMsgFolder_h__ */
