/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIAbBase.idl
 */

#ifndef __gen_nsIAbBase_h__
#define __gen_nsIAbBase_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsIAbListener.h" /* interface nsIAbListener */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
class nsIAbListener; /* forward decl */

/* starting interface:    nsIAbBase */

/* {013DD009-F73B-11d2-A2DA-001083003D0C} */
#define NS_IABBASE_IID_STR "013DD009-F73B-11d2-A2DA-001083003D0C"
#define NS_IABBASE_IID \
  {0x013DD009, 0xF73B, 0x11d2, \
    { 0xA2, 0xDA, 0x00, 0x10, 0x83, 0x00, 0x3D, 0x0C }}

class nsIAbBase : public nsICollection {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IABBASE_IID)

  /* readonly attribute string URI; */
  NS_IMETHOD GetURI(char * *aURI) = 0;

  /* attribute string name; */
  NS_IMETHOD GetName(char * *aName) = 0;
  NS_IMETHOD SetName(char * aName) = 0;

  /* nsISupports GetChildNamed (in string name); */
  NS_IMETHOD GetChildNamed(const char *name, nsISupports **_retval) = 0;

  /* attribute nsIAbBase parent; */
  NS_IMETHOD GetParent(nsIAbBase * *aParent) = 0;
  NS_IMETHOD SetParent(nsIAbBase * aParent) = 0;

  /* nsIEnumerator GetChildNodes (); */
  NS_IMETHOD GetChildNodes(nsIEnumerator **_retval) = 0;

  /* void AddAddrBookListener (in nsIAbListener listener); */
  NS_IMETHOD AddAddrBookListener(nsIAbListener *listener) = 0;

  /* void RemoveAddrBookListener (in nsIAbListener listener); */
  NS_IMETHOD RemoveAddrBookListener(nsIAbListener *listener) = 0;

  /* void AddUnique (in nsISupports element); */
  NS_IMETHOD AddUnique(nsISupports *element) = 0;

  /* void ReplaceElement (in nsISupports element, in nsISupports newElement); */
  NS_IMETHOD ReplaceElement(nsISupports *element, nsISupports *newElement) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIAbBase *priv);
#endif
};

#endif /* __gen_nsIAbBase_h__ */
