/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgVCard.idl
 */

#ifndef __gen_nsIMsgVCard_h__
#define __gen_nsIMsgVCard_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIMsgVCard */

/* {e0e67ec0-e3df-11d2-b7fc-00805f05ffa5} */
#define NS_IMSGVCARD_IID_STR "e0e67ec0-e3df-11d2-b7fc-00805f05ffa5"
#define NS_IMSGVCARD_IID \
  {0xe0e67ec0, 0xe3df, 0x11d2, \
    { 0xb7, 0xfc, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsIMsgVCard : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGVCARD_IID)

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgVCard *priv);
#endif
};

#endif /* __gen_nsIMsgVCard_h__ */
