/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgGroupRecord.idl
 */

#ifndef __gen_nsIMsgGroupRecord_h__
#define __gen_nsIMsgGroupRecord_h__

#include "nsISupports.h" /* interface nsISupports */


/* starting interface nsIMsgGroupRecord */
class nsIMsgGroupRecord {
 private:
  void operator delete(void *); // NOT TO BE IMPLEMENTED

 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGGROUPRECORD_IID;
    return iid;
  }

  /*  <IDL>  */
  NS_IMETHOD InitializeSibling() = 0;

  /*  <IDL>  */
  NS_IMETHOD GroupNameCompare(const char *name1, const char *name2, char delimeter, PRBool caseSensitive, PRInt16 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD FindDescendant(const char *name, nsIMsgGroupRecord **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetParent(nsIMsgGroupRecord * *aParent) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetChildren(nsIMsgGroupRecord * *aChildren) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetSibling(nsIMsgGroupRecord * *aSibling) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetSiblingOrAncestorSibling(nsIMsgGroupRecord **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNextAlphabetic(nsIMsgGroupRecord **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNextAlphabeticNoCategories(nsIMsgGroupRecord **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetPartName(char * *aPartName) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFullname(char * *aFullname) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetPrettyName(char * *aPrettyName) = 0;
  NS_IMETHOD SetPrettyName(char * aPrettyName) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetAddTime(PRInt64 *aAddTime) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsCategory(PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsIsCategoryContainer(PRBool *aIsIsCategoryContainer) = 0;
  NS_IMETHOD SetIsCategoryContainer(PRBool aIsCategoryContainer) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetCategoryContainer(nsIMsgGroupRecord * *aCategoryContainer) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsVirtual(PRBool *aIsVirtual) = 0;
  NS_IMETHOD SetVirtual(PRBool aVirtual) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsGroup(PRBool *aIsGroup) = 0;
  NS_IMETHOD SetGroup(PRBool aGroup) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsExpanded(PRBool *aIsExpanded) = 0;
  NS_IMETHOD SetExpanded(PRBool aExpanded) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsHtmlOk(PRBool *aIsHtmlOk) = 0;
  NS_IMETHOD SetHtmlOk(PRBool aHtmlOk) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsNeedsExtraInfo(PRBool *aIsNeedsExtraInfo) = 0;
  NS_IMETHOD SetNeedsExtraInfo(PRBool aNeedsExtraInfo) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsOnServer(PRBool *aIsOnServer) = 0;
  NS_IMETHOD SetOnServer(PRBool aOnServer) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetUniqueID(PRInt32 *aUniqueID) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFileOffset(PRInt32 *aFileOffset) = 0;
  NS_IMETHOD SetFileOffset(PRInt32 aFileOffset) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNumKids(PRInt32 *aNumKids) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetSaveString(char * *aSaveString) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsDirty(PRBool *aIsDirty) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetDepth(PRInt32 *aDepth) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetHierarchySeparator(char *aHierarchySeparator) = 0;
};

#endif /* __gen_nsIMsgGroupRecord_h__ */
