/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIFolderListener.idl
 */

#ifndef __gen_nsIFolderListener_h__
#define __gen_nsIFolderListener_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsID.h" /* interface nsID */
#include "nsIFolder.h" /* interface nsIFolder */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
class nsIFolder; /* forward decl */

/* starting interface nsIFolderListener */

/* {1c5ef9f0-d1c0-11d2-94CA-006097222B83} */
#define NS_IFOLDERLISTENER_IID_STR "1c5ef9f0-d1c0-11d2-94CA-006097222B83"
#define NS_IFOLDERLISTENER_IID \
  {0x1c5ef9f0, 0xd1c0, 0x11d2, \
    { 0x94, 0xCA, 0x00, 0x60, 0x97, 0x22, 0x2B, 0x83 }}

class nsIFolderListener : public nsISupports {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_IFOLDERLISTENER_IID;
    return iid;
  }

  /* void OnItemAdded (in nsIFolder parentFolder, in nsISupports item); */
  NS_IMETHOD OnItemAdded(nsIFolder *parentFolder, nsISupports *item) = 0;

  /* void OnItemRemoved (in nsIFolder parentFolder, in nsISupports item); */
  NS_IMETHOD OnItemRemoved(nsIFolder *parentFolder, nsISupports *item) = 0;

  /* void OnItemPropertyChanged (in nsISupports item, in string property, in string value); */
  NS_IMETHOD OnItemPropertyChanged(nsISupports *item, const char *property, const char *value) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIFolderListener *priv);
#endif
};

#endif /* __gen_nsIFolderListener_h__ */
