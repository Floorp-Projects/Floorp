/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgPost.idl
 */

#ifndef __gen_nsIMsgPost_h__
#define __gen_nsIMsgPost_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgCompFields.h" /* interface nsIMsgCompFields */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIMsgPost */

/* {a6fc080a-f1bb-11d2-84c2-004005263078} */
#define NS_IMSGPOST_IID_STR "a6fc080a-f1bb-11d2-84c2-004005263078"
#define NS_IMSGPOST_IID \
  {0xa6fc080a, 0xf1bb, 0x11d2, \
    { 0x84, 0xc2, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78 }}

class nsIMsgPost : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGPOST_IID)

  /* void PostMessage (in nsIMsgCompFields fields); */
  NS_IMETHOD PostMessage(nsIMsgCompFields *fields) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgPost *priv);
#endif
};

#endif /* __gen_nsIMsgPost_h__ */
