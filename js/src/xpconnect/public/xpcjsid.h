/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM xpcjsid.idl
 */

#ifndef __gen_xpcjsid_h__
#define __gen_xpcjsid_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIJSID */

/* {C86AE131-D101-11d2-9841-006008962422} */
#define NS_IJSID_IID_STR "C86AE131-D101-11d2-9841-006008962422"
#define NS_IJSID_IID \
  {0xC86AE131, 0xD101, 0x11d2, \
    { 0x98, 0x41, 0x00, 0x60, 0x08, 0x96, 0x24, 0x22 }}

class nsIJSID : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IJSID_IID)

  /* readonly attribute string name; */
  NS_IMETHOD GetName(char * *aName) = 0;

  /* readonly attribute string number; */
  NS_IMETHOD GetNumber(char * *aNumber) = 0;

  /* readonly attribute nsID id; */
  NS_IMETHOD GetId(nsID* *aId) = 0;

  /* readonly attribute boolean valid; */
  NS_IMETHOD GetValid(PRBool *aValid) = 0;

  /* boolean equals (in nsIJSID other); */
  NS_IMETHOD equals(nsIJSID *other, PRBool *_retval) = 0;

  /* boolean init (in string idString); */
  NS_IMETHOD init(const char *idString, PRBool *_retval) = 0;

  /* string toString (); */
  NS_IMETHOD toString(char **_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIJSID *priv);
#endif
};

/* starting interface:    nsIJSIID */

/* {e08dcda0-d651-11d2-9843-006008962422} */
#define NS_IJSIID_IID_STR "e08dcda0-d651-11d2-9843-006008962422"
#define NS_IJSIID_IID \
  {0xe08dcda0, 0xd651, 0x11d2, \
    { 0x98, 0x43, 0x00, 0x60, 0x08, 0x96, 0x24, 0x22 }}

class nsIJSIID : public nsIJSID {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IJSIID_IID)

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIJSIID *priv);
#endif
};

/* starting interface:    nsIJSCID */

/* {e3a24a60-d651-11d2-9843-006008962422} */
#define NS_IJSCID_IID_STR "e3a24a60-d651-11d2-9843-006008962422"
#define NS_IJSCID_IID \
  {0xe3a24a60, 0xd651, 0x11d2, \
    { 0x98, 0x43, 0x00, 0x60, 0x08, 0x96, 0x24, 0x22 }}

class nsIJSCID : public nsIJSID {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IJSCID_IID)

  /* readonly attribute nsISupports createInstance; */
  NS_IMETHOD GetCreateInstance(nsISupports * *aCreateInstance) = 0;

  /* readonly attribute nsISupports getService; */
  NS_IMETHOD GetGetService(nsISupports * *aGetService) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIJSCID *priv);
#endif
};
/********************************************************/
// {1D17BFF0-D58D-11d2-9843-006008962422}
#define NS_JS_IID_CID  \
{ 0x1d17bff0, 0xd58d, 0x11d2, \
 { 0x98, 0x43, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }
// {8628BFA0-D652-11d2-9843-006008962422}
#define NS_JS_CID_CID  \
{ 0x8628bfa0, 0xd652, 0x11d2, \
 { 0x98, 0x43, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }


#endif /* __gen_xpcjsid_h__ */
