/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIComposer.idl
 */
#include "jsapi.h"
#include "nsIComposer.h"

static char XXXnsresult2string_fmt[] = "XPCOM error %#x";
#define XXXnsresult2string(res) XXXnsresult2string_fmt, res

static void
nsIComposer_Finalize(JSContext *cx, JSObject *obj)
{
  nsIComposer *priv = (nsIComposer *)JS_GetPrivate(cx, obj);
  if (!priv)
    return;
  JSObject *globj = JS_GetGlobalObject(cx);
  if (globj)
    (void) JS_DeleteElement(cx, globj, (jsint)priv >> 1);
  NS_RELEASE(priv);
}

static JSBool
nsIComposer_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSClass nsIComposer_class = {
  "nsIComposer",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsIComposer_Finalize
};
#ifdef XPIDL_JS_STUBS

JSObject *
nsIComposer::InitJSClass(JSContext *cx)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  JSObject *proto = JS_InitClass(cx, globj, 0, &nsIComposer_class, nsIComposer_ctor, 0,
                                 0, 0, 0, 0);
  return proto;
}

JSObject *
nsIComposer::GetJSObject(JSContext *cx, nsIComposer *priv)
{
  JSObject *globj = JS_GetGlobalObject(cx);
  if (!globj)
    return 0;
  jsval v;
  if (!JS_LookupElement(cx, globj, (jsint)priv >> 1, &v))
    return 0;
  if (JSVAL_IS_VOID(v)) {
    JSObject *obj = JS_NewObject(cx, &nsIComposer_class, 0, 0);
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
