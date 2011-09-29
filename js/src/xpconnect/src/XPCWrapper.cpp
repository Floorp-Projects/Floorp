/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@mozilla.org> (original author)
 *   Brendan Eich <brendan@mozilla.org>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *   Blake Kaplan <mrbkap@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "XPCWrapper.h"
#include "AccessCheck.h"
#include "WrapperFactory.h"

namespace XPCNativeWrapper {

static inline
JSBool
ThrowException(nsresult ex, JSContext *cx)
{
  XPCThrower::Throw(ex, cx);

  return JS_FALSE;
}

static JSBool
UnwrapNW(JSContext *cx, uintN argc, jsval *vp)
{
  if (argc != 1) {
    return ThrowException(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx);
  }

  jsval v = JS_ARGV(cx, vp)[0];
  if (JSVAL_IS_PRIMITIVE(v)) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  JSObject *obj = JSVAL_TO_OBJECT(v);
  if (!obj->isWrapper()) {
    JS_SET_RVAL(cx, vp, v);
    return JS_TRUE;
  }

  if (xpc::WrapperFactory::IsXrayWrapper(obj) &&
      !xpc::WrapperFactory::IsPartiallyTransparent(obj)) {
    return JS_GetProperty(cx, obj, "wrappedJSObject", vp);
  }

  JS_SET_RVAL(cx, vp, v);
  return JS_TRUE;
}

static JSBool
XrayWrapperConstructor(JSContext *cx, uintN argc, jsval *vp)
{
  if (argc == 0) {
    return ThrowException(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx);
  }

  if (JSVAL_IS_PRIMITIVE(vp[2])) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  JSObject *obj = JSVAL_TO_OBJECT(vp[2]);
  if (!obj->isWrapper()) {
    *vp = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
  }

  obj = obj->unwrap();

  *vp = OBJECT_TO_JSVAL(obj);
  return JS_WrapValue(cx, vp);
}
// static
bool
AttachNewConstructorObject(XPCCallContext &ccx, JSObject *aGlobalObject)
{
  JSObject *xpcnativewrapper =
    JS_DefineFunction(ccx, aGlobalObject, "XPCNativeWrapper",
                      XrayWrapperConstructor, 1,
                      JSPROP_READONLY | JSPROP_PERMANENT | JSFUN_STUB_GSOPS | JSFUN_CONSTRUCTOR);
  if (!xpcnativewrapper) {
    return PR_FALSE;
  }
  return JS_DefineFunction(ccx, xpcnativewrapper, "unwrap", UnwrapNW, 1,
                           JSPROP_READONLY | JSPROP_PERMANENT) != nsnull;
}
}

namespace XPCWrapper {

JSObject *
Unwrap(JSContext *cx, JSObject *wrapper)
{
  if (wrapper->isWrapper()) {
    if (xpc::AccessCheck::isScriptAccessOnly(cx, wrapper))
      return nsnull;
    return wrapper->unwrap();
  }

  return nsnull;
}

JSObject *
UnsafeUnwrapSecurityWrapper(JSObject *obj)
{
  if (obj->isProxy()) {
    return obj->unwrap();
  }

  return obj;
}

}
