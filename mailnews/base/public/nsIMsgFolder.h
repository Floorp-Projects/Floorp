/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgFolder.idl
 */

#ifndef __gen_nsIMsgFolder_h__
#define __gen_nsIMsgFolder_h__

#include "nsIFolder.h"

class nsISupportsArray;
class nsIMessage;
class nsNativeFileSpec;

/* starting interface nsIMsgFolder */

/* {85e39ff0-b248-11d2-b7ef-00805f05ffa5} */
#define NS_IMSGFOLDER_IID_STR "85e39ff0-b248-11d2-b7ef-00805f05ffa5"
#define NS_IMSGFOLDER_IID \
  {0x85e39ff0, 0xb248, 0x11d2, \
    { 0xb7, 0xef, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsIMsgFolder : public nsIFolder {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGFOLDER_IID; 
    return iid;
  }


  // XXX should these 2 go on nsIFolder or nsICollection?
  NS_IMETHOD AddUnique(nsISupports* element) = 0;
  NS_IMETHOD ReplaceElement(nsISupports* element, nsISupports* newElement) = 0;

  NS_IMETHOD GetVisibleSubFolders(nsIEnumerator* *result) = 0;

  // subset of GetElements such that each element is an nsIMessage
  NS_IMETHOD GetMessages(nsIEnumerator* *result) = 0;

  NS_IMETHOD GetPrettyName(nsString& name) = 0;
  NS_IMETHOD SetPrettyName(const nsString& name) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetPrettiestName(nsString& aPrettiestName) = 0;

  /*  <IDL>  */
  NS_IMETHOD BuildFolderURL(char **_retval) = 0;

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
  NS_IMETHOD CreateSubfolder(const char *leafNameFromUser, nsIMsgFolder **outFolder, PRUint32 *outPos) = 0;

  /*  <IDL>  */
  NS_IMETHOD Rename(const char *name) = 0;

  /*  <IDL>  */
  NS_IMETHOD Adopt(const nsIMsgFolder *srcFolder, PRUint32 *outPos) = 0;

  /*  <IDL>  */
  NS_IMETHOD ContainsChildNamed(const char *name, PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD FindParentOf(const nsIMsgFolder *childFolder, nsIMsgFolder **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsParentOf(const nsIMsgFolder *folder, PRBool deep, PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GenerateUniqueSubfolderName(const char *prefix, const nsIMsgFolder *otherFolder, char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetDepth(PRUint32 *aDepth) = 0;
  NS_IMETHOD SetDepth(PRUint32 aDepth) = 0;

  /*  <IDL>  */
  NS_IMETHOD UpdateSummaryTotals() = 0;

  /*  <IDL>  */
  NS_IMETHOD SummaryChanged() = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNumUnread(PRBool deep, PRUint32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetTotalMessages(PRBool deep, PRUint32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetExpungedBytesCount(PRUint32 *aExpungedBytesCount) = 0;

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
  NS_IMETHOD DisplayRecipients(PRBool *_retval) = 0;

  NS_IMETHOD ReadDBFolderInfo(PRBool force) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetRelativePathName(char * *aRelativePathName) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetSizeOnDisk(PRUint32 *aSizeOnDisk) = 0;

  /*  <IDL>  */
  NS_IMETHOD RememberPassword(const char *password) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetRememberedPassword(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetUsersName(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetHostName(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetFlag(PRUint32 flag) = 0;

  /*  <IDL>  */
  NS_IMETHOD ClearFlag(PRUint32 flag) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFlag(PRUint32 flag, PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD ToggleFlag(PRUint32 flag) = 0;

  /*  <IDL>  */
  NS_IMETHOD OnFlagChange(PRUint32 flag) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFlags(PRUint32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFoldersWithFlag(PRUint32 flags, nsIMsgFolder **result, PRUint32 resultsize, PRUint32 *numFolders) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetExpansionArray(const nsISupportsArray *expansionArray) = 0;
};

////////////////////////////////////////////////////////////////////////////////
/* starting interface nsIMsgMailFolder */

/* {27D2DE40-BAF1-11d2-9578-00805F8AC615} */
#define NS_IMSGLOCALMAILFOLDER_IID_STR "27D2DE40-BAF1-11d2-9578-00805F8AC615"
#define NS_IMSGLOCALMAILFOLDER_IID \
  {0x27D2DE40, 0xBAF1, 0x11d2, \
    { 0x95, 0x78, 0x00, 0x80, 0x5F, 0x8A, 0xC6, 0x15 }}

class nsIMsgLocalMailFolder : public nsISupports {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGLOCALMAILFOLDER_IID;
    return iid;
  }

  /*  <IDL>  */
  NS_IMETHOD GetPath(nsNativeFileSpec& aPathName) = 0;
};

////////////////////////////////////////////////////////////////////////////////

/* {83ebf570-bfc6-11d2-8177-006008119d7a} */
#define NS_IMSGIMAPMAILFOLDER_IID_STR "83ebf570-bfc6-11d2-8177-006008119d7a"
#define NS_IMSGIMAPMAILFOLDER_IID                    \
{ /* 83ebf570-bfc6-11d2-8177-006008119d7a */         \
    0x83ebf570,                                      \
    0xbfc6,                                          \
    0x11d2,                                          \
    {0x81, 0x77, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

class nsIMsgImapMailFolder : public nsISupports {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGIMAPMAILFOLDER_IID;
    return iid;
  }

  /*  <IDL>  */
  NS_IMETHOD GetPathName(nsNativeFileSpec& aPathName) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* __gen_nsIMsgFolder_h__ */
