/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgSend.idl
 */

#ifndef __gen_nsIMsgSend_h__
#define __gen_nsIMsgSend_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgCompFields.h" /* interface nsIMsgCompFields */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include "nsID.h"
#include "nsIID.h"
#include "nsError.h"
#include "nsISupportsUtils.h"


/* starting interface nsIMsgSend */

/* {9E9BD970-C5D6-11d2-8297-000000000000} */
#define NS_IMSGSEND_IID_STR "9E9BD970-C5D6-11d2-8297-000000000000"
#define NS_IMSGSEND_IID \
  {0x9E9BD970, 0xC5D6, 0x11d2, \
    { 0x82, 0x97, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}

class nsIMsgSend : public nsISupports {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGSEND_IID;
    return iid;
  }

  /* void Test (); */
  NS_IMETHOD Test() = 0;

  /* void SendMessage (in nsIMsgCompFields fields); */
  NS_IMETHOD SendMessage(nsIMsgCompFields *fields) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgSend *priv);
#endif
};

#endif /* __gen_nsIMsgSend_h__ */
