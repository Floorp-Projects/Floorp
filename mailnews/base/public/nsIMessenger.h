/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMessenger.idl
 */

#ifndef __gen_nsIMessenger_h__
#define __gen_nsIMessenger_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include "nsID.h"
#include "nsError.h"


/* starting interface nsIMessenger */

/* {01883380-cab9-11d2-b7f6-00805f05ffa5} */
#define NS_IMESSENGER_IID_STR "01883380-cab9-11d2-b7f6-00805f05ffa5"
#define NS_IMESSENGER_IID \
  {0x01883380, 0xcab9, 0x11d2, \
    { 0xb7, 0xf6, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsIMessenger : public nsISupports {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMESSENGER_IID;
    return iid;
  }

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMessenger *priv);
#endif
};

#endif /* __gen_nsIMessenger_h__ */
