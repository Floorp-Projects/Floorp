/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIFolder.idl
 */

#ifndef __gen_nsIFolder_h__
#define __gen_nsIFolder_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include "nsID.h"
#include "nsError.h"


/* starting interface nsIFolder */

/* {361c89b0-c481-11d2-8614-000000000001} */
#define NS_IFOLDER_IID_STR "361c89b0-c481-11d2-8614-000000000001"
#define NS_IFOLDER_IID \
  {0x361c89b0, 0xc481, 0x11d2, \
    { 0x86, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }}

class nsIFolder : public nsICollection {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IFOLDER_IID;
    return iid;
  }

  /* readonly attribute string URI; */
  NS_IMETHOD GetURI(char * *aURI) = 0;

  /* attribute string name; */
  NS_IMETHOD GetName(char * *aName) = 0;
  NS_IMETHOD SetName(char * aName) = 0;

  /* nsISupports GetChildNamed (in string name); */
  NS_IMETHOD GetChildNamed(char *name, nsISupports **_retval) = 0;

  /* nsIFolder GetParent (); */
  NS_IMETHOD GetParent(nsIFolder **_retval) = 0;

  /* nsIEnumerator GetSubFolders (); */
  NS_IMETHOD GetSubFolders(nsIEnumerator **_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIFolder *priv);
#endif
};

#endif /* __gen_nsIFolder_h__ */
