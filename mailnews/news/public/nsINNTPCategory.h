/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINNTPCategory.idl
 */

#ifndef __gen_nsINNTPCategory_h__
#define __gen_nsINNTPCategory_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsINNTPNewsgroup.h" /* interface nsINNTPNewsgroup */
#include "nsIMsgGroupRecord.h" /* interface nsIMsgGroupRecord */


/* starting interface nsINNTPCategory */
class nsINNTPCategory : public nsISupports {
 private:
  void operator delete(void *); // NOT TO BE IMPLEMENTED

 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_INNTPCATEGORY_IID;
    return iid;
  }

  /*  <IDL>  */
  NS_IMETHOD BuildCategoryTree(const nsINNTPNewsgroup *parent, const char *catName, const nsIMsgGroupRecord *group, PRInt16 depth, nsINNTPNewsgroup **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddToCategoryTree(const nsINNTPNewsgroup *parent, const char *groupName, const nsIMsgGroupRecord *group, nsINNTPNewsgroup **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD CloneIntoNewsFolder(nsINNTPNewsgroup **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetRootCategory(nsINNTPNewsgroup **_retval) = 0;
};

#endif /* __gen_nsINNTPCategory_h__ */
