/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsICopyMessageListener.idl
 */

#ifndef __gen_nsICopyMessageListener_h__
#define __gen_nsICopyMessageListener_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIInputStream.h" /* interface nsIInputStream */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface nsICopyMessageListener */

/* {53CA78FE-E231-11d2-8A4D-0060B0FC04D2} */
#define NS_ICOPYMESSAGELISTENER_IID_STR "53CA78FE-E231-11d2-8A4D-0060B0FC04D2"
#define NS_ICOPYMESSAGELISTENER_IID \
  {0x53CA78FE, 0xE231, 0x11d2, \
    { 0x8A, 0x4D, 0x00, 0x60, 0xB0, 0xFC, 0x04, 0xD2 }}

class nsICopyMessageListener : public nsISupports {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_ICOPYMESSAGELISTENER_IID;
    return iid;
  }

  /* void BeginCopy (); */
  NS_IMETHOD BeginCopy() = 0;

  /* void CopyData (in nsIInputStream aIStream, in long aLength); */
  NS_IMETHOD CopyData(nsIInputStream *aIStream, PRInt32 aLength) = 0;

  /* void EndCopy (); */
  NS_IMETHOD EndCopy() = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsICopyMessageListener *priv);
#endif
};

#endif /* __gen_nsICopyMessageListener_h__ */
