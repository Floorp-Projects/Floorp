/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINNTPCategory.idl
 */

#ifndef __gen_nsINNTPCategory_h__
#define __gen_nsINNTPCategory_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsINNTPNewsgroup.h" /* interface nsINNTPNewsgroup */
#include "nsID.h" /* interface nsID */
#include "nsIMsgGroupRecord.h" /* interface nsIMsgGroupRecord */
#include "nsINNTPNewsgroupList.h" /* interface nsINNTPNewsgroupList */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include "nsID.h"
#include "nsError.h"


/* starting interface nsINNTPCategory */

/* {203b2120-b256-11d2-b7f0-00805f05ffa5} */
#define NS_INNTPCATEGORY_IID_STR "203b2120-b256-11d2-b7f0-00805f05ffa5"
#define NS_INNTPCATEGORY_IID \
  {0x203b2120, 0xb256, 0x11d2, \
    { 0xb7, 0xf0, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsINNTPCategory : public nsISupports {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_INNTPCATEGORY_IID;
    return iid;
  }

  /* nsINNTPNewsgroup BuildCategoryTree (in nsINNTPNewsgroup parent, in string catName, in nsIMsgGroupRecord group, in short depth); */
  NS_IMETHOD BuildCategoryTree(nsINNTPNewsgroup *parent, char *catName, nsIMsgGroupRecord *group, PRInt16 depth, nsINNTPNewsgroup **_retval) = 0;

  /* nsINNTPNewsgroup AddToCategoryTree (in nsINNTPNewsgroup parent, in string groupName, in nsIMsgGroupRecord group); */
  NS_IMETHOD AddToCategoryTree(nsINNTPNewsgroup *parent, char *groupName, nsIMsgGroupRecord *group, nsINNTPNewsgroup **_retval) = 0;

  /* nsINNTPNewsgroup CloneIntoNewsFolder (); */
  NS_IMETHOD CloneIntoNewsFolder(nsINNTPNewsgroup **_retval) = 0;

  /* nsINNTPNewsgroup GetRootCategory (); */
  NS_IMETHOD GetRootCategory(nsINNTPNewsgroup **_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsINNTPCategory *priv);
#endif
};

#endif /* __gen_nsINNTPCategory_h__ */
