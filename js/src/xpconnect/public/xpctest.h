/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM xpctest.idl
 */

#ifndef __gen_xpctest_h__
#define __gen_xpctest_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsITestXPCFoo */

/* {159E36D0-991E-11d2-AC3F-00C09300144B} */
#define NS_ITESTXPCFOO_IID_STR "159E36D0-991E-11d2-AC3F-00C09300144B"
#define NS_ITESTXPCFOO_IID \
  {0x159E36D0, 0x991E, 0x11d2, \
    { 0xAC, 0x3F, 0x00, 0xC0, 0x93, 0x00, 0x14, 0x4B }}

class nsITestXPCFoo : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITESTXPCFOO_IID)

  /* long Test (in long p1, in long p2); */
  NS_IMETHOD Test(PRInt32 p1, PRInt32 p2, PRInt32 *_retval) = 0;

  /* void Test2 (); */
  NS_IMETHOD Test2() = 0;

  enum { one = 1 };

  enum { five = 5 };

  enum { six = 6 };

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsITestXPCFoo *priv);
#endif
};

/* starting interface:    nsITestXPCFoo2 */

/* {5F9D20C0-9B6B-11d2-9FFE-000064657374} */
#define NS_ITESTXPCFOO2_IID_STR "5F9D20C0-9B6B-11d2-9FFE-000064657374"
#define NS_ITESTXPCFOO2_IID \
  {0x5F9D20C0, 0x9B6B, 0x11d2, \
    { 0x9F, 0xFE, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74 }}

class nsITestXPCFoo2 : public nsITestXPCFoo {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITESTXPCFOO2_IID)

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsITestXPCFoo2 *priv);
#endif
};

/* starting interface:    nsIEcho */

/* {CD2F2F40-C5D9-11d2-9838-006008962422} */
#define NS_IECHO_IID_STR "CD2F2F40-C5D9-11d2-9838-006008962422"
#define NS_IECHO_IID \
  {0xCD2F2F40, 0xC5D9, 0x11d2, \
    { 0x98, 0x38, 0x00, 0x60, 0x08, 0x96, 0x24, 0x22 }}

class nsIEcho : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IECHO_IID)

  /* void SetReceiver (in nsIEcho aReceiver); */
  NS_IMETHOD SetReceiver(nsIEcho *aReceiver) = 0;

  /* void SendOneString (in string str); */
  NS_IMETHOD SendOneString(const char *str) = 0;

  /* long In2OutOneInt (in long input); */
  NS_IMETHOD In2OutOneInt(PRInt32 input, PRInt32 *_retval) = 0;

  /* long In2OutAddTwoInts (in long input1, in long input2, out long output1, out long output2); */
  NS_IMETHOD In2OutAddTwoInts(PRInt32 input1, PRInt32 input2, PRInt32 *output1, PRInt32 *output2, PRInt32 *_retval) = 0;

  /* string In2OutOneString (in string input); */
  NS_IMETHOD In2OutOneString(const char *input, char **_retval) = 0;

  /* void SimpleCallNoEcho (); */
  NS_IMETHOD SimpleCallNoEcho() = 0;

  /* void SendManyTypes (in octet p1, in short p2, in long p3, in long long p4, in octet p5, in unsigned short p6, in unsigned long p7, in unsigned long long p8, in float p9, in double p10, in boolean p11, in char p12, in wchar p13, in nsID p14, in string p15, in wstring p16); */
  NS_IMETHOD SendManyTypes(PRUint8 p1, PRInt16 p2, PRInt32 p3, PRInt64 p4, PRUint8 p5, PRUint16 p6, PRUint32 p7, PRUint64 p8, float p9, double p10, PRBool p11, char p12, PRUint16 p13, nsID * p14, const char *p15, const PRUnichar *p16) = 0;

  /* void SendInOutManyTypes (inout octet p1, inout short p2, inout long p3, inout long long p4, inout octet p5, inout unsigned short p6, inout unsigned long p7, inout unsigned long long p8, inout float p9, inout double p10, inout boolean p11, inout char p12, inout wchar p13, inout nsID p14, inout string p15, inout wstring p16); */
  NS_IMETHOD SendInOutManyTypes(PRUint8 *p1, PRInt16 *p2, PRInt32 *p3, PRInt64 *p4, PRUint8 *p5, PRUint16 *p6, PRUint32 *p7, PRUint64 *p8, float *p9, double *p10, PRBool *p11, char *p12, PRUint16 *p13, nsID * *p14, char **p15, PRUnichar **p16) = 0;

  /* void MethodWithNative (in long p1, in voidStar p2); */
  NS_IMETHOD MethodWithNative(PRInt32 p1, void * p2) = 0;

  /* void ReturnCode (in long code); */
  NS_IMETHOD ReturnCode(PRInt32 code) = 0;

  /* void FailInJSTest (in long fail); */
  NS_IMETHOD FailInJSTest(PRInt32 fail) = 0;

  /* void SharedString ([shared, retval] out string str); */
  NS_IMETHOD SharedString(char **str) = 0;

  /* void ReturnCode_NS_OK (); */
  NS_IMETHOD ReturnCode_NS_OK() = 0;

  /* void ReturnCode_NS_COMFALSE (); */
  NS_IMETHOD ReturnCode_NS_COMFALSE() = 0;

  /* void ReturnCode_NS_ERROR_NULL_POINTER (); */
  NS_IMETHOD ReturnCode_NS_ERROR_NULL_POINTER() = 0;

  /* void ReturnCode_NS_ERROR_UNEXPECTED (); */
  NS_IMETHOD ReturnCode_NS_ERROR_UNEXPECTED() = 0;

  /* void ReturnCode_NS_ERROR_OUT_OF_MEMORY (); */
  NS_IMETHOD ReturnCode_NS_ERROR_OUT_OF_MEMORY() = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIEcho *priv);
#endif
};

#endif /* __gen_xpctest_h__ */
