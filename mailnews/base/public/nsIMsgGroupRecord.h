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
#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include "nsID.h"
#include "nsError.h"


/* starting interface nsIMsgGroupRecord */

/* {4ed03c60-b256-11d2-b7f0-00805f05ffa5} */
#define NS_IMSGGROUPRECORD_IID_STR "4ed03c60-b256-11d2-b7f0-00805f05ffa5"
#define NS_IMSGGROUPRECORD_IID \
  {0x4ed03c60, 0xb256, 0x11d2, \
    { 0xb7, 0xf0, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsIMsgGroupRecord : public nsISupports {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_IMSGGROUPRECORD_IID;
    return iid;
  }

  /* void InitializeSibling (); */
  NS_IMETHOD InitializeSibling() = 0;

  /* short GroupNameCompare (in string name1, in string name2, in char delimeter, in boolean caseSensitive); */
  NS_IMETHOD GroupNameCompare(char *name1, char *name2, char delimeter, PRBool caseSensitive, PRInt16 *_retval) = 0;

  /* nsIMsgGroupRecord FindDescendant (in string name); */
  NS_IMETHOD FindDescendant(char *name, nsIMsgGroupRecord **_retval) = 0;

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

  /* readonly attribute string fullname; */
  NS_IMETHOD GetFullname(char * *aFullname) = 0;

  /* attribute string prettyName; */
  NS_IMETHOD GetPrettyName(char * *aPrettyName) = 0;
  NS_IMETHOD SetPrettyName(char * aPrettyName) = 0;

  /* readonly attribute long long addTime; */
  NS_IMETHOD GetAddTime(PRInt64 *aAddTime) = 0;

  /* boolean IsCategory (); */
  NS_IMETHOD IsCategory(PRBool *_retval) = 0;

  /* attribute boolean IsCategoryContainer; */
  NS_IMETHOD GetIsCategoryContainer(PRBool *aIsCategoryContainer) = 0;
  NS_IMETHOD SetIsCategoryContainer(PRBool aIsCategoryContainer) = 0;

  /* readonly attribute nsIMsgGroupRecord categoryContainer; */
  NS_IMETHOD GetCategoryContainer(nsIMsgGroupRecord * *aCategoryContainer) = 0;

  /* attribute boolean virtual; */
  NS_IMETHOD GetVirtual(PRBool *aVirtual) = 0;
  NS_IMETHOD SetVirtual(PRBool aVirtual) = 0;

  /* attribute boolean group; */
  NS_IMETHOD GetGroup(PRBool *aGroup) = 0;
  NS_IMETHOD SetGroup(PRBool aGroup) = 0;

  /* attribute boolean expanded; */
  NS_IMETHOD GetExpanded(PRBool *aExpanded) = 0;
  NS_IMETHOD SetExpanded(PRBool aExpanded) = 0;

  /* attribute boolean htmlOk; */
  NS_IMETHOD GetHtmlOk(PRBool *aHtmlOk) = 0;
  NS_IMETHOD SetHtmlOk(PRBool aHtmlOk) = 0;

  /* attribute boolean needsExtraInfo; */
  NS_IMETHOD GetNeedsExtraInfo(PRBool *aNeedsExtraInfo) = 0;
  NS_IMETHOD SetNeedsExtraInfo(PRBool aNeedsExtraInfo) = 0;

  /* attribute boolean onServer; */
  NS_IMETHOD GetOnServer(PRBool *aOnServer) = 0;
  NS_IMETHOD SetOnServer(PRBool aOnServer) = 0;

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
