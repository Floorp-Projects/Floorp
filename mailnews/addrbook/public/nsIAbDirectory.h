/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIAbDirectory.idl
 */

#ifndef __gen_nsIAbDirectory_h__
#define __gen_nsIAbDirectory_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIAbBase.h" /* interface nsIAbBase */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsIAbCard.h" /* interface nsIAbCard */
#include "nsIAbListener.h" /* interface nsIAbListener */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIAbDirectory */

/* {1920E485-0709-11d3-A2EC-001083003D0C} */
#define NS_IABDIRECTORY_IID_STR "1920E485-0709-11d3-A2EC-001083003D0C"
#define NS_IABDIRECTORY_IID \
  {0x1920E485, 0x0709, 0x11d3, \
    { 0xA2, 0xEC, 0x00, 0x10, 0x83, 0x00, 0x3D, 0x0C }}

class nsIAbDirectory : public nsIAbBase {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IABDIRECTORY_IID)

  /* nsIEnumerator GetChildCards (); */
  NS_IMETHOD GetChildCards(nsIEnumerator **_retval) = 0;

  /* nsIAbCard CreateCardFromDirectory (); */
  NS_IMETHOD CreateCardFromDirectory(nsIAbCard **_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIAbDirectory *priv);
#endif
};

#endif /* __gen_nsIAbDirectory_h__ */
