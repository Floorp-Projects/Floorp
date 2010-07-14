/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78 sts=2: */
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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Kaplan <mrbkap@gmail.com> (original author)
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

#include "xpcprivate.h"
#include "nsDOMError.h"
#include "jsdbgapi.h"
#include "jscntxt.h"  // For js::AutoValueRooter.
#include "XPCNativeWrapper.h"
#include "XPCWrapper.h"

// This file implements a wrapper around trusted objects that allows them to
// be safely injected into untrusted code.

static JSBool
XPC_SOW_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
XPC_SOW_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
XPC_SOW_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
XPC_SOW_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
XPC_SOW_Enumerate(JSContext *cx, JSObject *obj);

static JSBool
XPC_SOW_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                   JSObject **objp);

static JSBool
XPC_SOW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

static JSBool
XPC_SOW_CheckAccess(JSContext *cx, JSObject *obj, jsval id, JSAccessMode mode,
                    jsval *vp);

static JSBool
XPC_SOW_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

static JSBool
XPC_SOW_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

static JSObject *
XPC_SOW_Iterator(JSContext *cx, JSObject *obj, JSBool keysonly);

static JSObject *
XPC_SOW_WrappedObject(JSContext *cx, JSObject *obj);

using namespace XPCWrapper;

// Throws an exception on context |cx|.
static inline JSBool
ThrowException(nsresult rv, JSContext *cx)
{
  return DoThrowException(rv, cx);
}

static const char prefix[] = "chrome://global/";

namespace SystemOnlyWrapper {

JSExtendedClass SOWClass = {
  // JSClass (JSExtendedClass.base) initialization
  { "SystemOnlyWrapper",
    JSCLASS_NEW_RESOLVE | JSCLASS_IS_EXTENDED |
    JSCLASS_HAS_RESERVED_SLOTS(XPCWrapper::sNumSlots),
    XPC_SOW_AddProperty, XPC_SOW_DelProperty,
    XPC_SOW_GetProperty, XPC_SOW_SetProperty,
    XPC_SOW_Enumerate,   (JSResolveOp)XPC_SOW_NewResolve,
    XPC_SOW_Convert,     nsnull,
    nsnull,              XPC_SOW_CheckAccess,
    nsnull,              nsnull,
    nsnull,              XPC_SOW_HasInstance,
    nsnull,              nsnull
  },

  // JSExtendedClass initialization
  XPC_SOW_Equality,
  nsnull,             // outerObject
  nsnull,             // innerObject
  XPC_SOW_Iterator,
  XPC_SOW_WrappedObject,
  JSCLASS_NO_RESERVED_MEMBERS
};

JSBool
WrapObject(JSContext *cx, JSObject *parent, jsval v, jsval *vp)
{
  // Slim wrappers don't expect to be wrapped, so morph them to fat wrappers
  // if we're about to wrap one.
  JSObject *innerObj = JSVAL_TO_OBJECT(v);
  if (IS_SLIM_WRAPPER(innerObj) && !MorphSlimWrapper(cx, innerObj)) {
    return ThrowException(NS_ERROR_FAILURE, cx);
  }

  JSObject *wrapperObj =
    JS_NewObjectWithGivenProto(cx, &SOWClass.base, NULL, parent);
  if (!wrapperObj) {
    return JS_FALSE;
  }

  *vp = OBJECT_TO_JSVAL(wrapperObj);
  js::AutoValueRooter tvr(cx, *vp);

  if (!JS_SetReservedSlot(cx, wrapperObj, sWrappedObjSlot, v) ||
      !JS_SetReservedSlot(cx, wrapperObj, sFlagsSlot, JSVAL_ZERO)) {
    return JS_FALSE;
  }

  return JS_TRUE;
}

JSBool
MakeSOW(JSContext *cx, JSObject *obj)
{
#ifdef DEBUG
  {
    JSClass *clasp = obj->getClass();
    NS_ASSERTION(clasp != &SystemOnlyWrapper::SOWClass.base &&
                 clasp != &XPCCrossOriginWrapper::XOWClass.base,
                 "bad call");
  }
#endif

  jsval flags;
  return JS_GetReservedSlot(cx, obj, sFlagsSlot, &flags) &&
         JS_SetReservedSlot(cx, obj, sFlagsSlot,
                            INT_TO_JSVAL(JSVAL_TO_INT(flags) | FLAG_SOW));
}


// If you change this code, change also nsContentUtils::CanAccessNativeAnon()!
JSBool
AllowedToAct(JSContext *cx, jsval idval)
{
  // TODO bug 508928: Refactor this with the XOW security checking code.
  nsIScriptSecurityManager *ssm = GetSecurityManager();
  if (!ssm) {
    return JS_TRUE;
  }

  JSStackFrame *fp;
  nsIPrincipal *principal = ssm->GetCxSubjectPrincipalAndFrame(cx, &fp);
  if (!principal) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  if (!fp) {
    if (!JS_FrameIterator(cx, &fp)) {
      // No code at all is running. So we must be arriving here as the result
      // of C++ code asking us to do something. Allow access.
      return JS_TRUE;
    }

    // Some code is running, we can't make the assumption, as above, but we
    // can't use a native frame, so clear fp.
    fp = nsnull;
  } else if (!fp->script) {
    fp = nsnull;
  }

  PRBool privileged;
  if (NS_SUCCEEDED(ssm->IsSystemPrincipal(principal, &privileged)) &&
      privileged) {
    // Chrome things are allowed to touch us.
    return JS_TRUE;
  }

  // XXX HACK EWW! Allow chrome://global/ access to these things, even
  // if they've been cloned into less privileged contexts.
  const char *filename;
  if (fp &&
      (filename = fp->script->filename) &&
      !strncmp(filename, prefix, NS_ARRAY_LENGTH(prefix) - 1)) {
    return JS_TRUE;
  }

  // Before we throw, check for UniversalXPConnect.
  nsresult rv = ssm->IsCapabilityEnabled("UniversalXPConnect", &privileged);
  if (NS_SUCCEEDED(rv) && privileged) {
    return JS_TRUE;
  }

  if (JSVAL_IS_VOID(idval)) {
    ThrowException(NS_ERROR_XPC_SECURITY_MANAGER_VETO, cx);
  } else {
    // TODO Localize me?
    JSString *str = JS_ValueToString(cx, idval);
    if (str) {
      JS_ReportError(cx, "Permission denied to access property '%hs' from a non-chrome context",
                     JS_GetStringChars(str));
    }
  }

  return JS_FALSE;
}

JSBool
CheckFilename(JSContext *cx, jsval idval, JSStackFrame *fp)
{
  const char *filename;
  if (fp &&
      (filename = fp->script->filename) &&
      !strncmp(filename, prefix, NS_ARRAY_LENGTH(prefix) - 1)) {
    return JS_TRUE;
  }

  if (JSVAL_IS_VOID(idval)) {
    ThrowException(NS_ERROR_XPC_SECURITY_MANAGER_VETO, cx);
  } else {
    // TODO Localize me?
    JSString *str = JS_ValueToString(cx, idval);
    if (str) {
      JS_ReportError(cx, "Permission denied to access property '%hs' from a non-chrome context",
                     JS_GetStringChars(str));
    }
  }

  return JS_FALSE;
}

} // namespace SystemOnlyWrapper

using namespace SystemOnlyWrapper;

// Like GetWrappedObject, but works on other types of wrappers, too.
// TODO Move to XPCWrapper?
static inline JSObject *
GetWrappedJSObject(JSContext *cx, JSObject *obj)
{
  JSClass *clasp = obj->getClass();
  if (!(clasp->flags & JSCLASS_IS_EXTENDED)) {
    return obj;
  }

  JSExtendedClass *xclasp = (JSExtendedClass *)clasp;
  if (!xclasp->wrappedObject) {
    return obj;
  }

  return xclasp->wrappedObject(cx, obj);
}

// Get the (possibly nonexistent) SOW off of an object
static inline
JSObject *
GetWrapper(JSObject *obj)
{
  while (obj->getClass() != &SOWClass.base) {
    obj = obj->getProto();
    if (!obj) {
      break;
    }
  }

  return obj;
}

static inline
JSObject *
GetWrappedObject(JSContext *cx, JSObject *wrapper)
{
  return UnwrapGeneric(cx, &SOWClass, wrapper);
}

static JSBool
XPC_SOW_FunctionWrapper(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                        jsval *rval)
{
  if (!AllowedToAct(cx, JSVAL_VOID)) {
    return JS_FALSE;
  }

  JSObject *wrappedObj;

  // Allow 'this' to be either a SOW, in which case we unwrap it or something
  // that isn't a SOW.  We disallow invalid SOWs that have no wrapped object.
  // We do this so that it's possible to use this function with .call on
  // related objects that are not system only.

  wrappedObj = GetWrapper(obj);
  if (wrappedObj) {
    wrappedObj = GetWrappedObject(cx, wrappedObj);
    if (!wrappedObj) {
      return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
    }
  } else {
    wrappedObj = obj;
  }

  JSObject *funObj = JSVAL_TO_OBJECT(argv[-2]);
  jsval funToCall;
  if (!JS_GetReservedSlot(cx, funObj, eWrappedFunctionSlot, &funToCall)) {
    return JS_FALSE;
  }

  return JS_CallFunctionValue(cx, wrappedObj, funToCall, argc, argv, rval);
}

JSBool
XPC_SOW_WrapFunction(JSContext *cx, JSObject *outerObj, JSObject *funobj,
                     jsval *rval)
{
  jsval funobjVal = OBJECT_TO_JSVAL(funobj);
  JSFunction *wrappedFun =
    reinterpret_cast<JSFunction *>(xpc_GetJSPrivate(funobj));
  JSNative native = JS_GetFunctionNative(cx, wrappedFun);
  if (!native || native == XPC_SOW_FunctionWrapper) {
    *rval = funobjVal;
    return JS_TRUE;
  }

  JSFunction *funWrapper =
    JS_NewFunction(cx, XPC_SOW_FunctionWrapper,
                   JS_GetFunctionArity(wrappedFun), 0,
                   JS_GetGlobalForObject(cx, outerObj),
                   JS_GetFunctionName(wrappedFun));
  if (!funWrapper) {
    return JS_FALSE;
  }

  JSObject *funWrapperObj = JS_GetFunctionObject(funWrapper);
  *rval = OBJECT_TO_JSVAL(funWrapperObj);

  return JS_SetReservedSlot(cx, funWrapperObj,
                            eWrappedFunctionSlot,
                            funobjVal);
}

static JSBool
XPC_SOW_RewrapValue(JSContext *cx, JSObject *wrapperObj, jsval *vp)
{
  jsval v = *vp;
  if (JSVAL_IS_PRIMITIVE(v)) {
    return JS_TRUE;
  }

  JSObject *obj = JSVAL_TO_OBJECT(v);

  if (JS_ObjectIsFunction(cx, obj)) {
    // NB: The JS_ValueToFunction call is guaranteed to succeed.
    JSNative native = JS_GetFunctionNative(cx, JS_ValueToFunction(cx, v));

    // This is really tricky! We need to unwrap this when calling native
    // functions to preserve their assumptions, but *not* when calling
    // scripted functions, since they expect 'this' to be wrapped.
    if (!native) {
     return JS_TRUE;
    }

    if (native == XPC_SOW_FunctionWrapper) {
      // If this is a system function wrapper, make sure its ours, otherwise,
      // its prototype could come from the wrong scope.
      if (wrapperObj->getProto() == obj->getParent()) {
        return JS_TRUE;
      }

      // It isn't ours, rewrap the wrapped function.
      if (!JS_GetReservedSlot(cx, obj, eWrappedFunctionSlot, &v)) {
        return JS_FALSE;
      }
      obj = JSVAL_TO_OBJECT(v);
    }

    return XPC_SOW_WrapFunction(cx, wrapperObj, obj, vp);
  }

  if (obj->getClass() == &SOWClass.base) {
    // We are extra careful about content-polluted wrappers here. I don't know
    // if it's possible to reach them through objects that we wrap, but figuring
    // that out is more expensive (and harder) than simply checking and
    // rewrapping here.
    if (wrapperObj->getParent() == obj->getParent()) {
      // Already wrapped.
      return JS_TRUE;
    }

    obj = GetWrappedObject(cx, obj);
    if (!obj) {
      // XXX Can this happen?
      *vp = JSVAL_NULL;
      return JS_TRUE;
    }
    v = *vp = OBJECT_TO_JSVAL(obj);
  }

  return WrapObject(cx, wrapperObj->getParent(), v, vp);
}

static JSBool
XPC_SOW_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  NS_ASSERTION(obj->getClass() == &SOWClass.base, "Wrong object");

  jsval resolving;
  if (!JS_GetReservedSlot(cx, obj, sFlagsSlot, &resolving)) {
    return JS_FALSE;
  }

  if (HAS_FLAGS(resolving, FLAG_RESOLVING)) {
    // Allow us to define a property on ourselves.
    return JS_TRUE;
  }

  if (!AllowedToAct(cx, id)) {
    return JS_FALSE;
  }

  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    return JS_TRUE;
  }

  return AddProperty(cx, obj, JS_TRUE, wrappedObj, id, vp);
}

static JSBool
XPC_SOW_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  if (!AllowedToAct(cx, id)) {
    return JS_FALSE;
  }

  return DelProperty(cx, wrappedObj, id, vp);
}

static JSBool
XPC_SOW_GetOrSetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp,
                         JSBool isSet)
{
  obj = GetWrapper(obj);
  if (!obj) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  if (!AllowedToAct(cx, id)) {
    return JS_FALSE;
  }

  js::AutoArrayRooter tvr(cx, 1, vp);

  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  if (isSet && id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_PROTO)) {
    // No setting __proto__ on my object.
    return ThrowException(NS_ERROR_INVALID_ARG, cx); // XXX better error message
  }

  jsid interned_id;
  if (!JS_ValueToId(cx, id, &interned_id)) {
    return JS_FALSE;
  }

  JSBool ok = isSet
              ? JS_SetPropertyById(cx, wrappedObj, interned_id, vp)
              : JS_GetPropertyById(cx, wrappedObj, interned_id, vp);
  if (!ok) {
    return JS_FALSE;
  }

  return XPC_SOW_RewrapValue(cx, obj, vp);
}

static JSBool
XPC_SOW_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return XPC_SOW_GetOrSetProperty(cx, obj, id, vp, JS_FALSE);
}

static JSBool
XPC_SOW_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return XPC_SOW_GetOrSetProperty(cx, obj, id, vp, JS_TRUE);
}

static JSBool
XPC_SOW_Enumerate(JSContext *cx, JSObject *obj)
{
  obj = GetWrapper(obj);
  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    // Nothing to enumerate.
    return JS_TRUE;
  }

  if (!AllowedToAct(cx, JSVAL_VOID)) {
    return JS_FALSE;
  }

  return Enumerate(cx, obj, wrappedObj);
}

static JSBool
XPC_SOW_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                   JSObject **objp)
{
  obj = GetWrapper(obj);

  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    // No wrappedObj means that this is probably the prototype.
    *objp = nsnull;
    return JS_TRUE;
  }

  if (!AllowedToAct(cx, id)) {
    return JS_FALSE;
  }

  return NewResolve(cx, obj, JS_FALSE, wrappedObj, id, flags, objp);
}

static JSBool
XPC_SOW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
  if (!AllowedToAct(cx, JSVAL_VOID)) {
    return JS_FALSE;
  }

  // Don't do any work to convert to object.
  if (type == JSTYPE_OBJECT) {
    *vp = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
  }

  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    // Converting the prototype to something.
    // XXX Can this happen?
    return JS_TRUE;
  }

  return wrappedObj->getClass()->convert(cx, wrappedObj, type, vp);
}

static JSBool
XPC_SOW_CheckAccess(JSContext *cx, JSObject *obj, jsval prop, JSAccessMode mode,
                    jsval *vp)
{
  // Simply forward checkAccess to our wrapped object. It's already expecting
  // untrusted things to ask it about accesses.

  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    *vp = JSVAL_VOID;
    return JS_TRUE;
  }

  uintN junk;
  jsid id;
  return JS_ValueToId(cx, prop, &id) &&
         JS_CheckAccess(cx, wrappedObj, id, mode, vp, &junk);
}

static JSBool
XPC_SOW_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
  if (!AllowedToAct(cx, JSVAL_VOID)) {
    return JS_FALSE;
  }

  JSObject *iface = GetWrappedObject(cx, obj);
  if (!iface) {
    *bp = JS_FALSE;
    return JS_TRUE;
  }

  JSClass *clasp = iface->getClass();

  *bp = JS_FALSE;
  if (!clasp->hasInstance) {
    return JS_TRUE;
  }

  // Prematurely unwrap the left hand side. This isn't necessary, but could be
  // faster than waiting until XPCWrappedNative::GetWrappedNativeOfJSObject to
  // do it.
  if (!JSVAL_IS_PRIMITIVE(v)) {
    JSObject *test = JSVAL_TO_OBJECT(v);

    // GetWrappedObject does a class check.
    test = GetWrappedObject(cx, test);
    if (test) {
      v = OBJECT_TO_JSVAL(test);
    }
  }

  return clasp->hasInstance(cx, iface, v, bp);
}

static JSBool
XPC_SOW_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
  // Delegate to our wrapped object.
  if (JSVAL_IS_PRIMITIVE(v)) {
    *bp = JS_FALSE;
    return JS_TRUE;
  }

  if (obj == JSVAL_TO_OBJECT(v)) {
    *bp = JS_TRUE;
    return JS_TRUE;
  }

  JSObject *lhs = GetWrappedObject(cx, obj);
  JSObject *rhs = GetWrappedJSObject(cx, JSVAL_TO_OBJECT(v));
  if (lhs == rhs) {
    *bp = JS_TRUE;
    return JS_TRUE;
  }

  if (lhs) {
    // Delegate to our wrapped object if we can.
    JSClass *clasp = lhs->getClass();
    if (clasp->flags & JSCLASS_IS_EXTENDED) {
      JSExtendedClass *xclasp = (JSExtendedClass *) clasp;
      // NB: JSExtendedClass.equality is a required field.
      return xclasp->equality(cx, lhs, OBJECT_TO_JSVAL(rhs), bp);
    }
  }

  // We know rhs is non-null.
  JSClass *clasp = rhs->getClass();
  if (clasp->flags & JSCLASS_IS_EXTENDED) {
    JSExtendedClass *xclasp = (JSExtendedClass *) clasp;
    // NB: JSExtendedClass.equality is a required field.
    return xclasp->equality(cx, rhs, OBJECT_TO_JSVAL(lhs), bp);
  }

  *bp = JS_FALSE;
  return JS_TRUE;
}

static JSObject *
XPC_SOW_Iterator(JSContext *cx, JSObject *obj, JSBool keysonly)
{
  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    ThrowException(NS_ERROR_INVALID_ARG, cx);
    return nsnull;
  }

  JSObject *wrapperIter = JS_NewObject(cx, &SOWClass.base, nsnull,
                                       JS_GetGlobalForObject(cx, obj));
  if (!wrapperIter) {
    return nsnull;
  }

  js::AutoValueRooter tvr(cx, OBJECT_TO_JSVAL(wrapperIter));

  // Initialize our SOW.
  jsval v = OBJECT_TO_JSVAL(wrappedObj);
  if (!JS_SetReservedSlot(cx, wrapperIter, sWrappedObjSlot, v) ||
      !JS_SetReservedSlot(cx, wrapperIter, sFlagsSlot, JSVAL_ZERO)) {
    return nsnull;
  }

  return CreateIteratorObj(cx, wrapperIter, obj, wrappedObj, keysonly);
}

static JSObject *
XPC_SOW_WrappedObject(JSContext *cx, JSObject *obj)
{
  return GetWrappedObject(cx, obj);
}
