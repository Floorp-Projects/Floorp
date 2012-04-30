/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Utils.h"

namespace mozilla {
namespace dom {
namespace bindings {

static bool
DefineConstants(JSContext* cx, JSObject* obj, ConstantSpec* cs)
{
  for (; cs->name; ++cs) {
    JSBool ok =
      JS_DefineProperty(cx, obj, cs->name, cs->value, NULL, NULL,
                        JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
    if (!ok) {
      return false;
    }
  }
  return true;
}

static JSObject*
CreateInterfaceObject(JSContext* cx, JSObject* global, JSObject* receiver,
                      JSClass* constructorClass, JSObject* proto,
                      JSFunctionSpec* staticMethods, ConstantSpec* constants,
                      const char* name)
{
  JSObject* functionProto = JS_GetFunctionPrototype(cx, global);
  if (!functionProto) {
    return NULL;
  }

  JSObject* constructor =
    JS_NewObject(cx, constructorClass, functionProto, global);
  if (!constructor) {
    return NULL;
  }

  if (staticMethods && !JS_DefineFunctions(cx, constructor, staticMethods)) {
    return NULL;
  }

  if (constants && !DefineConstants(cx, constructor, constants)) {
    return NULL;
  }

  if (proto && !JS_LinkConstructorAndPrototype(cx, constructor, proto)) {
    return NULL;
  }

  // This is Enumerable: False per spec.
  if (!JS_DefineProperty(cx, receiver, name, OBJECT_TO_JSVAL(constructor), NULL,
                         NULL, 0)) {
    return NULL;
  }

  return constructor;
}

static JSObject*
CreateInterfacePrototypeObject(JSContext* cx, JSObject* global,
                               JSObject* parentProto, JSClass* protoClass,
                               JSFunctionSpec* methods,
                               JSPropertySpec* properties,
                               ConstantSpec* constants)
{
  JSObject* ourProto = JS_NewObjectWithUniqueType(cx, protoClass, parentProto,
                                                  global);
  if (!ourProto) {
    return NULL;
  }

  if (methods && !JS_DefineFunctions(cx, ourProto, methods)) {
    return NULL;
  }

  if (properties && !JS_DefineProperties(cx, ourProto, properties)) {
    return NULL;
  }

  if (constants && !DefineConstants(cx, ourProto, constants)) {
    return NULL;
  }

  return ourProto;
}

JSObject*
CreateInterfaceObjects(JSContext* cx, JSObject* global, JSObject *receiver,
                       JSObject* protoProto, JSClass* protoClass,
                       JSClass* constructorClass, JSFunctionSpec* methods,
                       JSPropertySpec* properties, ConstantSpec* constants,
                       JSFunctionSpec* staticMethods, const char* name)
{
  MOZ_ASSERT(protoClass || constructorClass, "Need at least one class!");
  MOZ_ASSERT(!(methods || properties) || protoClass,
             "Methods or properties but no protoClass!");
  MOZ_ASSERT(!staticMethods || constructorClass,
             "Static methods but no constructorClass!");
  MOZ_ASSERT(bool(name) == bool(constructorClass),
             "Must have name precisely when we have an interface object");

  JSObject* proto;
  if (protoClass) {
    proto = CreateInterfacePrototypeObject(cx, global, protoProto, protoClass,
                                           methods, properties, constants);
    if (!proto) {
      return NULL;
    }
  }
  else {
    proto = NULL;
  }

  JSObject* interface;
  if (constructorClass) {
    interface = CreateInterfaceObject(cx, global, receiver, constructorClass,
                                      proto, staticMethods, constants, name);
    if (!interface) {
      return NULL;
    }
  }

  return protoClass ? proto : interface;
}

static bool
NativeInterface2JSObjectAndThrowIfFailed(XPCLazyCallContext& aLccx,
                                         JSContext* aCx,
                                         JS::Value* aRetval,
                                         xpcObjectHelper& aHelper,
                                         const nsIID* aIID,
                                         bool aAllowNativeWrapper)
{
  nsresult rv;
  if (!XPCConvert::NativeInterface2JSObject(aLccx, aRetval, NULL, aHelper, aIID,
                                            NULL, aAllowNativeWrapper, &rv)) {
    // I can't tell if NativeInterface2JSObject throws JS exceptions
    // or not.  This is a sloppy stab at the right semantics; the
    // method really ought to be fixed to behave consistently.
    if (!JS_IsExceptionPending(aCx)) {
      Throw<true>(aCx, NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED);
    }
    return false;
  }
  return true;
}

bool
DoHandleNewBindingWrappingFailure(JSContext* cx, JSObject* scope,
                                  nsISupports* value, JS::Value* vp)
{
  if (JS_IsExceptionPending(cx)) {
    return false;
  }

  XPCLazyCallContext lccx(JS_CALLER, cx, scope);

  if (value) {
    xpcObjectHelper helper(value);
    return NativeInterface2JSObjectAndThrowIfFailed(lccx, cx, vp, helper, NULL,
                                                    true);
  }

  return Throw<true>(cx, NS_ERROR_XPC_BAD_CONVERT_JS);
}

// Only set allowNativeWrapper to false if you really know you need it, if in
// doubt use true. Setting it to false disables security wrappers.
bool
XPCOMObjectToJsval(JSContext* cx, JSObject* scope, xpcObjectHelper &helper,
                   const nsIID* iid, bool allowNativeWrapper, JS::Value* rval)
{
  XPCLazyCallContext lccx(JS_CALLER, cx, scope);

  if (!NativeInterface2JSObjectAndThrowIfFailed(lccx, cx, rval, helper, iid,
                                                allowNativeWrapper)) {
    return false;
  }

#ifdef DEBUG
  JSObject* jsobj = JSVAL_TO_OBJECT(*rval);
  if (jsobj && !js::GetObjectParent(jsobj))
    NS_ASSERTION(js::GetObjectClass(jsobj)->flags & JSCLASS_IS_GLOBAL,
                 "Why did we recreate this wrapper?");
#endif

  return true;
}

JSBool
QueryInterface(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::Value thisv = JS_THIS(cx, vp);
  if (thisv == JSVAL_NULL)
    return false;

  JSObject* obj = JSVAL_TO_OBJECT(thisv);
  JSClass* clasp = js::GetObjectJSClass(obj);
  if (!IsDOMClass(clasp)) {
    return Throw<true>(cx, NS_ERROR_FAILURE);
  }

  nsISupports* native = UnwrapDOMObject<nsISupports>(obj, clasp);

  if (argc < 1) {
    return Throw<true>(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
  }

  JS::Value* argv = JS_ARGV(cx, vp);
  if (!argv[0].isObject()) {
    return Throw<true>(cx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }

  nsIJSIID* iid;
  xpc_qsSelfRef iidRef;
  if (NS_FAILED(xpc_qsUnwrapArg<nsIJSIID>(cx, argv[0], &iid, &iidRef.ptr,
                                          &argv[0]))) {
    return Throw<true>(cx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }
  MOZ_ASSERT(iid);

  if (iid->GetID()->Equals(NS_GET_IID(nsIClassInfo))) {
    nsresult rv;
    nsCOMPtr<nsIClassInfo> ci = do_QueryInterface(native, &rv);
    if (NS_FAILED(rv)) {
      return Throw<true>(cx, rv);
    }

    return WrapObject(cx, obj, ci, &NS_GET_IID(nsIClassInfo), vp);
  }
  
  // Lie, otherwise we need to check classinfo or QI
  *vp = thisv;
  return true;
}

} // namespace bindings
} // namespace dom
} // namespace mozilla
