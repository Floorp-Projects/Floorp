/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM xpccomponents.idl
 */

#ifndef __gen_xpccomponents_h__
#define __gen_xpccomponents_h__

#include "nsISupports.h" /* interface nsISupports */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface nsIXPCInterfaces */

/* {4b62a640-d26c-11d2-9842-006008962422} */
#define NS_IXPCINTERFACES_IID_STR "4b62a640-d26c-11d2-9842-006008962422"
#define NS_IXPCINTERFACES_IID \
  {0x4b62a640, 0xd26c, 0x11d2, \
    { 0x98, 0x42, 0x00, 0x60, 0x08, 0x96, 0x24, 0x22 }}

class nsIXPCInterfaces : public nsISupports {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_IXPCINTERFACES_IID;
    return iid;
  }

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIXPCInterfaces *priv);
#endif
};

/* starting interface nsIXPCClasses */

/* {978ff520-d26c-11d2-9842-006008962422} */
#define NS_IXPCCLASSES_IID_STR "978ff520-d26c-11d2-9842-006008962422"
#define NS_IXPCCLASSES_IID \
  {0x978ff520, 0xd26c, 0x11d2, \
    { 0x98, 0x42, 0x00, 0x60, 0x08, 0x96, 0x24, 0x22 }}

class nsIXPCClasses : public nsISupports {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_IXPCCLASSES_IID;
    return iid;
  }

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIXPCClasses *priv);
#endif
};

/* starting interface nsIXPCComponents */

/* {42624f80-d26c-11d2-9842-006008962422} */
#define NS_IXPCCOMPONENTS_IID_STR "42624f80-d26c-11d2-9842-006008962422"
#define NS_IXPCCOMPONENTS_IID \
  {0x42624f80, 0xd26c, 0x11d2, \
    { 0x98, 0x42, 0x00, 0x60, 0x08, 0x96, 0x24, 0x22 }}

class nsIXPCComponents : public nsISupports {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_IXPCCOMPONENTS_IID;
    return iid;
  }

  /* readonly attribute nsIXPCInterfaces interfaces; */
  NS_IMETHOD GetInterfaces(nsIXPCInterfaces * *aInterfaces) = 0;

  /* readonly attribute nsIXPCClasses classes; */
  NS_IMETHOD GetClasses(nsIXPCClasses * *aClasses) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIXPCComponents *priv);
#endif
};

#endif /* __gen_xpccomponents_h__ */
