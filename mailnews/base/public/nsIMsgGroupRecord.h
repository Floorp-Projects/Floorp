/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgGroupRecord.idl
 */

#ifndef __gen_nsIMsgGroupRecord_h__
#define __gen_nsIMsgGroupRecord_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIMsgGroupRecord */

/* {4ed03c60-b256-11d2-b7f0-00805f05ffa5} */
#define NS_IMSGGROUPRECORD_IID_STR "4ed03c60-b256-11d2-b7f0-00805f05ffa5"
#define NS_IMSGGROUPRECORD_IID \
  {0x4ed03c60, 0xb256, 0x11d2, \
    { 0xb7, 0xf0, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsIMsgGroupRecord : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGGROUPRECORD_IID)

  /* void Init (in voidStar parent, in string partname, in long long time, in long uniqueid, in long fileoffset, in char delimiter); */
  NS_IMETHOD Init(void* parent, const char *partname, PRInt64 time, PRInt32 uniqueid, PRInt32 fileoffset, char delimiter) = 0;

  /* void InitFromParent (in nsIMsgGroupRecord parent, in string saveline, in long savelinelength, in long fileoffset); */
  NS_IMETHOD InitFromParent(nsIMsgGroupRecord *parent, const char *saveline, PRInt32 savelinelength, PRInt32 fileoffset) = 0;

  /* void InitFromFile (in nsIMsgGroupRecord parent, in string partname, in long long time, in long uniqueid, in long fileoffset); */
  NS_IMETHOD InitFromFile(nsIMsgGroupRecord *parent, const char *partname, PRInt64 time, PRInt32 uniqueid, PRInt32 fileoffset) = 0;

  /* void InitializeSibling (); */
  NS_IMETHOD InitializeSibling() = 0;

  /* short GroupNameCompare (in string name1, in string name2, in char delimeter, in boolean caseSensitive); */
  NS_IMETHOD GroupNameCompare(const char *name1, const char *name2, char delimeter, PRBool caseSensitive, PRInt16 *_retval) = 0;

  /* nsIMsgGroupRecord FindDescendent (in string name); */
  NS_IMETHOD FindDescendent(const char *name, nsIMsgGroupRecord **_retval) = 0;

  /* readonly attribute nsIMsgGroupRecord parent; */
  NS_IMETHOD GetParent(nsIMsgGroupRecord * *aParent) = 0;

  /* readonly attribute nsIMsgGroupRecord children; */
  NS_IMETHOD GetChildren(nsIMsgGroupRecord * *aChildren) = 0;

  /* readonly attribute nsIMsgGroupRecord sibling; */
  NS_IMETHOD GetSibling(nsIMsgGroupRecord * *aSibling) = 0;

  /* nsIMsgGroupRecord GetSiblingOrAncestorSibling (); */
  NS_IMETHOD GetSiblingOrAncestorSibling(nsIMsgGroupRecord **_retval) = 0;

  /* nsIMsgGroupRecord GetNextAlphabetic (); */
  NS_IMETHOD GetNextAlphabetic(nsIMsgGroupRecord **_retval) = 0;

  /* nsIMsgGroupRecord GetNextAlphabeticNoCategories (); */
  NS_IMETHOD GetNextAlphabeticNoCategories(nsIMsgGroupRecord **_retval) = 0;

  /* readonly attribute string partName; */
  NS_IMETHOD GetPartName(char * *aPartName) = 0;

  /* readonly attribute string fullName; */
  NS_IMETHOD GetFullName(char * *aFullName) = 0;

  /* attribute string prettyName; */
  NS_IMETHOD GetPrettyName(char * *aPrettyName) = 0;
  NS_IMETHOD SetPrettyName(char * aPrettyName) = 0;

  /* readonly attribute long long addTime; */
  NS_IMETHOD GetAddTime(PRInt64 *aAddTime) = 0;

  /* readonly attribute boolean isCategory; */
  NS_IMETHOD GetIsCategory(PRBool *aIsCategory) = 0;

  /* attribute boolean isCategoryContainer; */
  NS_IMETHOD GetIsCategoryContainer(PRBool *aIsCategoryContainer) = 0;
  NS_IMETHOD SetIsCategoryContainer(PRBool aIsCategoryContainer) = 0;

  /* readonly attribute nsIMsgGroupRecord categoryContainer; */
  NS_IMETHOD GetCategoryContainer(nsIMsgGroupRecord * *aCategoryContainer) = 0;

  /* attribute boolean isVirtual; */
  NS_IMETHOD GetIsVirtual(PRBool *aIsVirtual) = 0;
  NS_IMETHOD SetIsVirtual(PRBool aIsVirtual) = 0;

  /* attribute boolean isGroup; */
  NS_IMETHOD GetIsGroup(PRBool *aIsGroup) = 0;
  NS_IMETHOD SetIsGroup(PRBool aIsGroup) = 0;

  /* attribute boolean isExpanded; */
  NS_IMETHOD GetIsExpanded(PRBool *aIsExpanded) = 0;
  NS_IMETHOD SetIsExpanded(PRBool aIsExpanded) = 0;

  /* attribute boolean isDescendentsLoaded; */
  NS_IMETHOD GetIsDescendentsLoaded(PRBool *aIsDescendentsLoaded) = 0;
  NS_IMETHOD SetIsDescendentsLoaded(PRBool aIsDescendentsLoaded) = 0;

  /* attribute boolean HTMLOkGroup; */
  NS_IMETHOD GetHTMLOkGroup(PRBool *aHTMLOkGroup) = 0;
  NS_IMETHOD SetHTMLOkGroup(PRBool aHTMLOkGroup) = 0;

  /* attribute boolean HTMLOkTree; */
  NS_IMETHOD GetHTMLOkTree(PRBool *aHTMLOkTree) = 0;
  NS_IMETHOD SetHTMLOkTree(PRBool aHTMLOkTree) = 0;

  /* attribute boolean needsExtraInfo; */
  NS_IMETHOD GetNeedsExtraInfo(PRBool *aNeedsExtraInfo) = 0;
  NS_IMETHOD SetNeedsExtraInfo(PRBool aNeedsExtraInfo) = 0;

  /* attribute boolean doesNotExistOnServer; */
  NS_IMETHOD GetDoesNotExistOnServer(PRBool *aDoesNotExistOnServer) = 0;
  NS_IMETHOD SetDoesNotExistOnServer(PRBool aDoesNotExistOnServer) = 0;

  /* readonly attribute long uniqueID; */
  NS_IMETHOD GetUniqueID(PRInt32 *aUniqueID) = 0;

  /* attribute long fileOffset; */
  NS_IMETHOD GetFileOffset(PRInt32 *aFileOffset) = 0;
  NS_IMETHOD SetFileOffset(PRInt32 aFileOffset) = 0;

  /* readonly attribute long numKids; */
  NS_IMETHOD GetNumKids(PRInt32 *aNumKids) = 0;

  /* readonly attribute string saveString; */
  NS_IMETHOD GetSaveString(char * *aSaveString) = 0;

  /* readonly attribute boolean dirty; */
  NS_IMETHOD GetDirty(PRBool *aDirty) = 0;

  /* readonly attribute long depth; */
  NS_IMETHOD GetDepth(PRInt32 *aDepth) = 0;

  /* readonly attribute char hierarchySeparator; */
  NS_IMETHOD GetHierarchySeparator(char *aHierarchySeparator) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgGroupRecord *priv);
#endif
};

#endif /* __gen_nsIMsgGroupRecord_h__ */
