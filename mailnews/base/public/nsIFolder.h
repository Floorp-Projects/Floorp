/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIFolder.idl
 */

#ifndef __gen_nsIFolder_h__
#define __gen_nsIFolder_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIFolderListener.h" /* interface nsIFolderListener */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
class nsIFolderListener; /* forward decl */

/* starting interface:    nsIFolder */

/* {361c89b0-c481-11d2-8614-000000000001} */
#define NS_IFOLDER_IID_STR "361c89b0-c481-11d2-8614-000000000001"
#define NS_IFOLDER_IID \
  {0x361c89b0, 0xc481, 0x11d2, \
    { 0x86, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }}

class nsIFolder : public nsICollection {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFOLDER_IID)

  /* readonly attribute string URI; */
  NS_IMETHOD GetURI(char * *aURI) = 0;

  /* attribute string name; */
  NS_IMETHOD GetName(char * *aName) = 0;
  NS_IMETHOD SetName(char * aName) = 0;

  /* nsISupports GetChildNamed (in string name); */
  NS_IMETHOD GetChildNamed(const char *name, nsISupports **_retval) = 0;

  /* nsIFolder GetParent (); */
  NS_IMETHOD GetParent(nsIFolder **_retval) = 0;

  /* nsIEnumerator GetSubFolders (); */
  NS_IMETHOD GetSubFolders(nsIEnumerator **_retval) = 0;

  /* void AddFolderListener (in nsIFolderListener listener); */
  NS_IMETHOD AddFolderListener(nsIFolderListener *listener) = 0;

  /* void RemoveFolderListener (in nsIFolderListener listener); */
  NS_IMETHOD RemoveFolderListener(nsIFolderListener *listener) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIFolder *priv);
#endif
};

#endif /* __gen_nsIFolder_h__ */
