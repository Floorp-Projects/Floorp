/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINNTPArticleList.idl
 */

#ifndef __gen_nsINNTPArticleList_h__
#define __gen_nsINNTPArticleList_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsINNTPNewsgroup.h" /* interface nsINNTPNewsgroup */
#include "nsID.h" /* interface nsID */
#include "nsINNTPNewsgroupList.h" /* interface nsINNTPNewsgroupList */
#include "nsINNTPHost.h" /* interface nsINNTPHost */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsINNTPArticleList */

/* {921AC214-96B5-11d2-B7EB-00805F05FFA5} */
#define NS_INNTPARTICLELIST_IID_STR "921AC214-96B5-11d2-B7EB-00805F05FFA5"
#define NS_INNTPARTICLELIST_IID \
  {0x921AC214, 0x96B5, 0x11d2, \
    { 0xB7, 0xEB, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsINNTPArticleList : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_INNTPARTICLELIST_IID)

  /* void Init (in nsINNTPHost newsHost, in nsINNTPNewsgroup newsgroup); */
  NS_IMETHOD Init(nsINNTPHost *newsHost, nsINNTPNewsgroup *newsgroup) = 0;

  /* void AddArticleKey (in long key); */
  NS_IMETHOD AddArticleKey(PRInt32 key) = 0;

  /* void FinishAddingArticleKeys (); */
  NS_IMETHOD FinishAddingArticleKeys() = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsINNTPArticleList *priv);
#endif
};

#endif /* __gen_nsINNTPArticleList_h__ */
