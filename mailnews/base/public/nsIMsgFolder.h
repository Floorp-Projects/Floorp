/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgFolder.idl
 */

#ifndef __gen_nsIMsgFolder_h__
#define __gen_nsIMsgFolder_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgThread.h" /* interface nsIMsgThread */
#include "nsISupportsArray.h" /* interface nsISupportsArray */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIFolderListener.h" /* interface nsIFolderListener */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsIFolder.h" /* interface nsIFolder */
#include "nsIMessage.h" /* interface nsIMessage */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
#include "nsFileSpec.h"


/* starting interface nsIMsgFolder */

/* {85e39ff0-b248-11d2-b7ef-00805f05ffa5} */
#define NS_IMSGFOLDER_IID_STR "85e39ff0-b248-11d2-b7ef-00805f05ffa5"
#define NS_IMSGFOLDER_IID \
  {0x85e39ff0, 0xb248, 0x11d2, \
    { 0xb7, 0xef, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsIMsgFolder : public nsIFolder {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_IMSGFOLDER_IID;
    return iid;
  }

  /* void AddUnique (in nsISupports element); */
  NS_IMETHOD AddUnique(nsISupports *element) = 0;

  /* void ReplaceElement (in nsISupports element, in nsISupports newElement); */
  NS_IMETHOD ReplaceElement(nsISupports *element, nsISupports *newElement) = 0;

  /* nsIEnumerator GetMessages (); */
  NS_IMETHOD GetMessages(nsIEnumerator **_retval) = 0;

  /* nsIEnumerator GetThreads (); */
  NS_IMETHOD GetThreads(nsIEnumerator **_retval) = 0;

  /* nsIMsgThread GetThreadForMessage (in nsIMessage message); */
  NS_IMETHOD GetThreadForMessage(nsIMessage *message, nsIMsgThread **_retval) = 0;

  /* nsIEnumerator GetVisibleSubFolders (); */
  NS_IMETHOD GetVisibleSubFolders(nsIEnumerator **_retval) = 0;

  /* attribute string prettyName; */
  NS_IMETHOD GetPrettyName(char * *aPrettyName) = 0;
  NS_IMETHOD SetPrettyName(char * aPrettyName) = 0;

  /* readonly attribute string prettiestName; */
  NS_IMETHOD GetPrettiestName(char * *aPrettiestName) = 0;

  /* string BuildFolderURL (); */
  NS_IMETHOD BuildFolderURL(char **_retval) = 0;

  /* readonly attribute boolean deleteIsMoveToTrash; */
  NS_IMETHOD GetDeleteIsMoveToTrash(PRBool *aDeleteIsMoveToTrash) = 0;

  /* readonly attribute boolean showDeletedMessages; */
  NS_IMETHOD GetShowDeletedMessages(PRBool *aShowDeletedMessages) = 0;

  /* void OnCloseFolder (); */
  NS_IMETHOD OnCloseFolder() = 0;

  /* void Delete (); */
  NS_IMETHOD Delete() = 0;

  /* void PropagateDelete (inout nsIMsgFolder folder, in boolean deleteStorage); */
  NS_IMETHOD PropagateDelete(nsIMsgFolder **folder, PRBool deleteStorage) = 0;

  /* void RecursiveDelete (in boolean deleteStorage); */
  NS_IMETHOD RecursiveDelete(PRBool deleteStorage) = 0;

  /* void CreateSubfolder (in string leafNameFromUser, out nsIMsgFolder outFolder, out unsigned long outPos); */
  NS_IMETHOD CreateSubfolder(const char *leafNameFromUser, nsIMsgFolder **outFolder, PRUint32 *outPos) = 0;

  /* void Rename (in string name); */
  NS_IMETHOD Rename(const char *name) = 0;

  /* void Adopt (in nsIMsgFolder srcFolder, out unsigned long outPos); */
  NS_IMETHOD Adopt(nsIMsgFolder *srcFolder, PRUint32 *outPos) = 0;

  /* boolean ContainsChildNamed (in string name); */
  NS_IMETHOD ContainsChildNamed(const char *name, PRBool *_retval) = 0;

  /* nsIMsgFolder FindParentOf (in nsIMsgFolder childFolder); */
  NS_IMETHOD FindParentOf(nsIMsgFolder *childFolder, nsIMsgFolder **_retval) = 0;

  /* boolean IsParentOf (in nsIMsgFolder folder, in boolean deep); */
  NS_IMETHOD IsParentOf(nsIMsgFolder *folder, PRBool deep, PRBool *_retval) = 0;

  /* string GenerateUniqueSubfolderName (in string prefix, in nsIMsgFolder otherFolder); */
  NS_IMETHOD GenerateUniqueSubfolderName(const char *prefix, nsIMsgFolder *otherFolder, char **_retval) = 0;

  /* attribute unsigned long depth; */
  NS_IMETHOD GetDepth(PRUint32 *aDepth) = 0;
  NS_IMETHOD SetDepth(PRUint32 aDepth) = 0;

  /* void UpdateSummaryTotals (); */
  NS_IMETHOD UpdateSummaryTotals() = 0;

  /* void SummaryChanged (); */
  NS_IMETHOD SummaryChanged() = 0;

  /* unsigned long GetNumUnread (in boolean deep); */
  NS_IMETHOD GetNumUnread(PRBool deep, PRUint32 *_retval) = 0;

  /* unsigned long GetTotalMessages (in boolean deep); */
  NS_IMETHOD GetTotalMessages(PRBool deep, PRUint32 *_retval) = 0;

  /* readonly attribute unsigned long expungedBytesCount; */
  NS_IMETHOD GetExpungedBytesCount(PRUint32 *aExpungedBytesCount) = 0;

  /* readonly attribute boolean deletable; */
  NS_IMETHOD GetDeletable(PRBool *aDeletable) = 0;

  /* readonly attribute boolean canCreateChildren; */
  NS_IMETHOD GetCanCreateChildren(PRBool *aCanCreateChildren) = 0;

  /* readonly attribute boolean canBeRenamed; */
  NS_IMETHOD GetCanBeRenamed(PRBool *aCanBeRenamed) = 0;

  /* readonly attribute boolean requiresCleanup; */
  NS_IMETHOD GetRequiresCleanup(PRBool *aRequiresCleanup) = 0;

  /* void ClearRequiresCleanup (); */
  NS_IMETHOD ClearRequiresCleanup() = 0;

  /* readonly attribute boolean knowsSearchNntpExtension; */
  NS_IMETHOD GetKnowsSearchNntpExtension(PRBool *aKnowsSearchNntpExtension) = 0;

  /* readonly attribute boolean allowsPosting; */
  NS_IMETHOD GetAllowsPosting(PRBool *aAllowsPosting) = 0;

  /* boolean DisplayRecipients (); */
  NS_IMETHOD DisplayRecipients(PRBool *_retval) = 0;

  /* void ReadDBFolderInfo (in boolean force); */
  NS_IMETHOD ReadDBFolderInfo(PRBool force) = 0;

  /* readonly attribute string relativePathName; */
  NS_IMETHOD GetRelativePathName(char * *aRelativePathName) = 0;

  /* readonly attribute unsigned long sizeOnDisk; */
  NS_IMETHOD GetSizeOnDisk(PRUint32 *aSizeOnDisk) = 0;

  /* void RememberPassword (in string password); */
  NS_IMETHOD RememberPassword(const char *password) = 0;

  /* string GetRememberedPassword (); */
  NS_IMETHOD GetRememberedPassword(char **_retval) = 0;

  /* boolean UserNeedsToAuthenticateForFolder (in boolean displayOnly); */
  NS_IMETHOD UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *_retval) = 0;

  /* string GetUsersName (); */
  NS_IMETHOD GetUsersName(char **_retval) = 0;

  /* string GetHostName (); */
  NS_IMETHOD GetHostName(char **_retval) = 0;

  /* void SetFlag (in unsigned long flag); */
  NS_IMETHOD SetFlag(PRUint32 flag) = 0;

  /* void ClearFlag (in unsigned long flag); */
  NS_IMETHOD ClearFlag(PRUint32 flag) = 0;

  /* boolean GetFlag (in unsigned long flag); */
  NS_IMETHOD GetFlag(PRUint32 flag, PRBool *_retval) = 0;

  /* void ToggleFlag (in unsigned long flag); */
  NS_IMETHOD ToggleFlag(PRUint32 flag) = 0;

  /* void OnFlagChange (in unsigned long flag); */
  NS_IMETHOD OnFlagChange(PRUint32 flag) = 0;

  /* unsigned long GetFlags (); */
  NS_IMETHOD GetFlags(PRUint32 *_retval) = 0;

  /* void GetFoldersWithFlag (in unsigned long flags, out nsIMsgFolder result, in unsigned long resultsize, out unsigned long numFolders); */
  NS_IMETHOD GetFoldersWithFlag(PRUint32 flags, nsIMsgFolder **result, PRUint32 resultsize, PRUint32 *numFolders) = 0;

  /* void GetExpansionArray (in nsISupportsArray expansionArray); */
  NS_IMETHOD GetExpansionArray(nsISupportsArray *expansionArray) = 0;

  /* void DeleteMessage (in nsIMessage message); */
  NS_IMETHOD DeleteMessage(nsIMessage *message) = 0;

  /* void AcquireSemaphore (in nsISupports semHolder); */
  NS_IMETHOD AcquireSemaphore(nsISupports *semHolder) = 0;

  /* void ReleaseSemaphore (in nsISupports semHolder); */
  NS_IMETHOD ReleaseSemaphore(nsISupports *semHolder) = 0;

  /* boolean TestSemaphore (in nsISupports semHolder); */
  NS_IMETHOD TestSemaphore(nsISupports *semHolder, PRBool *_retval) = 0;

  /* boolean IsLocked (); */
  NS_IMETHOD IsLocked(PRBool *_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgFolder *priv);
#endif
};

/* starting interface nsIMsgLocalMailFolder */

/* {27D2DE40-BAF1-11d2-9578-00805F8AC615} */
#define NS_IMSGLOCALMAILFOLDER_IID_STR "27D2DE40-BAF1-11d2-9578-00805F8AC615"
#define NS_IMSGLOCALMAILFOLDER_IID \
  {0x27D2DE40, 0xBAF1, 0x11d2, \
    { 0x95, 0x78, 0x00, 0x80, 0x5F, 0x8A, 0xC6, 0x15 }}

class nsIMsgLocalMailFolder : public nsISupports {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_IMSGLOCALMAILFOLDER_IID;
    return iid;
  }

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgLocalMailFolder *priv);
#endif
};

/* starting interface nsIMsgImapMailFolder */

/* {FBFEBE79-C1DD-11d2-8A40-0060B0FC04D2} */
#define NS_IMSGIMAPMAILFOLDER_IID_STR "FBFEBE79-C1DD-11d2-8A40-0060B0FC04D2"
#define NS_IMSGIMAPMAILFOLDER_IID \
  {0xFBFEBE79, 0xC1DD, 0x11d2, \
    { 0x8A, 0x40, 0x00, 0x60, 0xB0, 0xFC, 0x04, 0xD2 }}

class nsIMsgImapMailFolder : public nsISupports {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_IMSGIMAPMAILFOLDER_IID;
    return iid;
  }

  /* readonly attribute nsNativeFileSpec pathName; */
  NS_IMETHOD GetPathName(nsNativeFileSpec* *aPathName) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgImapMailFolder *priv);
#endif
};

#endif /* __gen_nsIMsgFolder_h__ */
