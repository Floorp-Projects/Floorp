/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINNTPCategoryContainer.idl
 */

#ifndef __gen_nsINNTPCategoryContainer_h__
#define __gen_nsINNTPCategoryContainer_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsINNTPNewsgroup.h" /* interface nsINNTPNewsgroup */
#include "nsID.h" /* interface nsID */
#include "nsINNTPNewsgroupList.h" /* interface nsINNTPNewsgroupList */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsINNTPCategoryContainer */

/* {cb36d510-b249-11d2-b7ef-00805f05ffa5} */
#define NS_INNTPCATEGORYCONTAINER_IID_STR "cb36d510-b249-11d2-b7ef-00805f05ffa5"
#define NS_INNTPCATEGORYCONTAINER_IID \
  {0xcb36d510, 0xb249, 0x11d2, \
    { 0xb7, 0xef, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsINNTPCategoryContainer : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_INNTPCATEGORYCONTAINER_IID)

  /* attribute nsINNTPNewsgroup rootCategory; */
  NS_IMETHOD GetRootCategory(nsINNTPNewsgroup * *aRootCategory) = 0;
  NS_IMETHOD SetRootCategory(nsINNTPNewsgroup * aRootCategory) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsINNTPCategoryContainer *priv);
#endif
};

#endif /* __gen_nsINNTPCategoryContainer_h__ */
