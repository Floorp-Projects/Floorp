/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMessageView.idl
 */

#ifndef __gen_nsIMessageView_h__
#define __gen_nsIMessageView_h__

#include "nsISupports.h" /* interface nsISupports */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface nsIMessageView */

/* {14495572-E945-11d2-8A52-0060B0FC04D2} */
#define NS_IMESSAGEVIEW_IID_STR "14495572-E945-11d2-8A52-0060B0FC04D2"
#define NS_IMESSAGEVIEW_IID \
  {0x14495572, 0xE945, 0x11d2, \
    { 0x8A, 0x52, 0x00, 0x60, 0xB0, 0xFC, 0x04, 0xD2 }}

class nsIMessageView : public nsISupports {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_IMESSAGEVIEW_IID;
    return iid;
  }

  /* void SetShowAll (); */
  NS_IMETHOD SetShowAll() = 0;

  /* void SetShowUnread (); */
  NS_IMETHOD SetShowUnread() = 0;

  /* void SetShowRead (); */
  NS_IMETHOD SetShowRead() = 0;

  /* void SetShowWatched (); */
  NS_IMETHOD SetShowWatched() = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMessageView *priv);
#endif
};

#endif /* __gen_nsIMessageView_h__ */
