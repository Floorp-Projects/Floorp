/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIAbListener.idl
 */

#ifndef __gen_nsIAbListener_h__
#define __gen_nsIAbListener_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIAbBase.h" /* interface nsIAbBase */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIEnumerator.h" /* interface nsIEnumerator */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
class nsIAbBase; /* forward decl */

/* starting interface:    nsIAbListener */

/* {1920E484-0709-11d3-A2EC-001083003D0C} */
#define NS_IABLISTENER_IID_STR "1920E484-0709-11d3-A2EC-001083003D0C"
#define NS_IABLISTENER_IID \
  {0x1920E484, 0x0709, 0x11d3, \
    { 0xA2, 0xEC, 0x00, 0x10, 0x83, 0x00, 0x3D, 0x0C }}

class nsIAbListener : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IABLISTENER_IID)

  /* void OnItemAdded (in nsIAbBase parentDir, in nsISupports item); */
  NS_IMETHOD OnItemAdded(nsIAbBase *parentDir, nsISupports *item) = 0;

  /* void OnItemRemoved (in nsIAbBase parentDir, in nsISupports item); */
  NS_IMETHOD OnItemRemoved(nsIAbBase *parentDir, nsISupports *item) = 0;

  /* void OnItemPropertyChanged (in nsISupports item, in string property, in string oldValue, in string newValue); */
  NS_IMETHOD OnItemPropertyChanged(nsISupports *item, const char *property, const char *oldValue, const char *newValue) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIAbListener *priv);
#endif
};

#endif /* __gen_nsIAbListener_h__ */
