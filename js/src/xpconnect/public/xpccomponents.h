/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM xpccomponents.idl
 */

#ifndef __gen_xpccomponents_h__
#define __gen_xpccomponents_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIXPCInterfaces */

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

/* starting interface:    nsIXPCClasses */

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

/* starting interface:    nsIXPCComponents */

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
  enum { RESULT_NS_OK = 0 };
  enum { RESULT_NS_COMFALSE = 1 };
  enum { RESULT_NS_ERROR_NOT_IMPLEMENTED = -2147467263 };
  enum { RESULT_NS_NOINTERFACE = -2147467262 };
  enum { RESULT_NS_ERROR_NO_INTERFACE = -2147467262 };
  enum { RESULT_NS_ERROR_INVALID_POINTER = -2147467261 };
  enum { RESULT_NS_ERROR_NULL_POINTER = -2147467261 };
  enum { RESULT_NS_ERROR_ABORT = -2147467260 };
  enum { RESULT_NS_ERROR_FAILURE = -2147467259 };
  enum { RESULT_NS_ERROR_UNEXPECTED = -2147418113 };
  enum { RESULT_NS_ERROR_OUT_OF_MEMORY = -2147024882 };
  enum { RESULT_NS_ERROR_ILLEGAL_VALUE = -2147024809 };
  enum { RESULT_NS_ERROR_INVALID_ARG = -2147024809 };
  enum { RESULT_NS_ERROR_NO_AGGREGATION = -2147221232 };
  enum { RESULT_NS_ERROR_NOT_AVAILABLE = -2147221231 };
  enum { RESULT_NS_ERROR_FACTORY_NOT_REGISTERED = -2147221164 };
  enum { RESULT_NS_ERROR_FACTORY_NOT_LOADED = -2147221000 };

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIXPCComponents *priv);
#endif
};

#endif /* __gen_xpccomponents_h__ */
