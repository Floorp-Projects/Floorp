/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XPCWrapper.h"
#include "AccessCheck.h"
#include "WrapperFactory.h"
#include "AccessCheck.h"

using namespace xpc;
namespace XPCNativeWrapper {

static inline
JSBool
ThrowException(nsresult ex, JSContext *cx)
{
  XPCThrower::Throw(ex, cx);

  return false;
}

static JSBool
UnwrapNW(JSContext *cx, unsigned argc, jsval *vp)
{
  if (argc != 1) {
    return ThrowException(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx);
  }

  JS::RootedValue v(cx, JS_ARGV(cx, vp)[0]);
  if (JSVAL_IS_PRIMITIVE(v)) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  JS::RootedObject obj(cx, JSVAL_TO_OBJECT(v));
  if (!js::IsWrapper(obj)) {
    JS_SET_RVAL(cx, vp, v);
    return true;
  }

  if (WrapperFactory::IsXrayWrapper(obj) && AccessCheck::wrapperSubsumes(obj)) {
    return JS_GetProperty(cx, obj, "wrappedJSObject", vp);
  }

  JS_SET_RVAL(cx, vp, v);
  return true;
}

static JSBool
XrayWrapperConstructor(JSContext *cx, unsigned argc, jsval *vp)
{
  if (argc == 0) {
    return ThrowException(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx);
  }

  if (JSVAL_IS_PRIMITIVE(vp[2])) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  JSObject *obj = JSVAL_TO_OBJECT(vp[2]);
  if (!js::IsWrapper(obj)) {
    *vp = OBJECT_TO_JSVAL(obj);
    return true;
  }

  obj = js::UnwrapObject(obj);

  *vp = OBJECT_TO_JSVAL(obj);
  return JS_WrapValue(cx, vp);
}
// static
bool
AttachNewConstructorObject(XPCCallContext &ccx, JSObject *aGlobalObject)
{
  JSFunction *xpcnativewrapper =
    JS_DefineFunction(ccx, aGlobalObject, "XPCNativeWrapper",
                      XrayWrapperConstructor, 1,
                      JSPROP_READONLY | JSPROP_PERMANENT | JSFUN_STUB_GSOPS | JSFUN_CONSTRUCTOR);
  if (!xpcnativewrapper) {
    return false;
  }
  return JS_DefineFunction(ccx, JS_GetFunctionObject(xpcnativewrapper), "unwrap", UnwrapNW, 1,
                           JSPROP_READONLY | JSPROP_PERMANENT) != nullptr;
}

} // namespace XPCNativeWrapper

namespace XPCWrapper {

JSObject *
UnsafeUnwrapSecurityWrapper(JSObject *obj)
{
  if (js::IsProxy(obj)) {
    return js::UnwrapObject(obj);
  }

  return obj;
}

} // namespace XPCWrapper
