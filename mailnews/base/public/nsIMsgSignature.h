/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgSignature.idl
 */

#ifndef __gen_nsIMsgSignature_h__
#define __gen_nsIMsgSignature_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIMsgSignature */

/* {7e1531b0-e3df-11d2-b7fc-00805f05ffa5} */
#define NS_IMSGSIGNATURE_IID_STR "7e1531b0-e3df-11d2-b7fc-00805f05ffa5"
#define NS_IMSGSIGNATURE_IID \
  {0x7e1531b0, 0xe3df, 0x11d2, \
    { 0xb7, 0xfc, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsIMsgSignature : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGSIGNATURE_IID)

  /* attribute string signatureName; */
  NS_IMETHOD GetSignatureName(char * *aSignatureName) = 0;
  NS_IMETHOD SetSignatureName(char * aSignatureName) = 0;

  /* attribute string signature; */
  NS_IMETHOD GetSignature(char * *aSignature) = 0;
  NS_IMETHOD SetSignature(char * aSignature) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgSignature *priv);
#endif
};

#endif /* __gen_nsIMsgSignature_h__ */
