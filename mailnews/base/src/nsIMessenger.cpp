/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMessenger.idl
 */
#include "jsapi.h"
#include "nsIMessenger.h"

static char XXXnsresult2string_fmt[] = "XPCOM error %#x";
#define XXXnsresult2string(res) XXXnsresult2string_fmt, res

static void
nsIMessenger_Finalize(JSContext *cx, JSObject *obj)
{
  nsIMessenger *priv = (nsIMessenger *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIMessenger_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIMessenger_class = {
  "nsIMessenger",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIMessenger_Finalize
};

#ifdef XPIDL_JS_STUBS
JSObject *
nsIMessenger::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIMessenger_class, nsIMessenger_ctor, 0,
                                 0, 0, 0, 0);
  return proto;
}

JSObject *
nsIMessenger::GetJSObject(JSContext *cx, nsIMessenger *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIMessenger_class, 0, 0);
    if (!obj || !JS_SetPrivate(cx, obj, priv))
      return 0;
    NS_ADDREF(priv);
    v = PRIVATE_TO_JSVAL(obj);
    if (!JS_DefineElement(cx, globj, (jsint)priv >> 1, v, 0, 0,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
      return 0;
    }
  }
  return (JSObject *)JSVAL_TO_PRIVATE(v);
}
#endif /* XPIDL_JS_STUBS */
