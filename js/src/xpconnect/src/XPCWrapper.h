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

#ifndef XPC_WRAPPER_H
#define XPC_WRAPPER_H 1

#include "xpcprivate.h"

namespace XPCNativeWrapper {

// Given an XPCWrappedNative pointer and the name of the function on
// XPCNativeScriptableFlags corresponding with a flag, returns 'true'
// if the flag is set.
// XXX Convert to using GetFlags() and not a macro.
#define NATIVE_HAS_FLAG(_wn, _flag)                \
  ((_wn)->GetScriptableInfo() &&                   \
   (_wn)->GetScriptableInfo()->GetFlags()._flag())

// Wraps a function in an XPCNativeWrapper function wrapper.
JSBool
WrapFunction(JSContext* cx, JSObject* funobj, jsval *rval);

// Given a value, if the original XPCNativeWrapper is a deep wrapper,
// returns a new XPCNativeWrapper around the value.
JSBool
RewrapIfDeepWrapper(JSContext *cx, JSObject *obj, jsval v, jsval *rval);

} // namespace XPCNativeWrapper

namespace XPCCrossOriginWrapper {

// Wraps an object in an XPCCrossOriginWrapper.
JSBool
WrapObject(JSContext *cx, JSObject *parent, jsval *vp,
           XPCWrappedNative *wn = nsnull);

// Wraps a function in an XPCCrossOriginWrapper function wrapper.
JSBool
WrapFunction(JSContext *cx, JSObject *wrapperObj, JSObject *funobj,
             jsval *rval);

// Wraps a value in a XOW if we need to wrap it (either it's cross-scope
// or ClassNeedsXOW returns true).
JSBool
RewrapIfNeeded(JSContext *cx, JSObject *wrapperObj, jsval *vp);

// Notify a wrapper's XOWs that the wrapper has been reparented.
JSBool
WrapperMoved(JSContext *cx, XPCWrappedNative *innerObj,
             XPCWrappedNativeScope *newScope);

// Returns 'true' if the current context is same-origin to the wrappedObject.
// If we are "same origin" because UniversalXPConnect is enabled and
// privilegeEnabled is non-null, then privilegeEnabled is set to true.
nsresult
CanAccessWrapper(JSContext *cx, JSObject *wrappedObj, JSBool *privilegeEnabled);

// Some elements can change their principal or otherwise need XOWs, even
// if they're same origin. This function returns 'true' if the element's
// JSClass's name matches one such element.
inline JSBool
ClassNeedsXOW(const char *name)
{
  switch (*name) {
    case 'W':
      return strcmp(++name, "indow") == 0;
    case 'L':
      return strcmp(++name, "ocation") == 0;
    case 'H':
      if (strncmp(++name, "TML", 3))
        break;
      name += 3;
      if (*name == 'I')
        ++name;
      return strcmp(name, "FrameElement") == 0;
    default:
      break;
  }

  return JS_FALSE;
}

} // namespace XPCCrossOriginWrapper

namespace ChromeObjectWrapper {

JSBool
WrapObject(JSContext *cx, JSObject *parent, jsval v, jsval *vp);

}

namespace XPCSafeJSObjectWrapper {

JSObject *
GetUnsafeObject(JSContext *cx, JSObject *obj);

JSBool
WrapObject(JSContext *cx, JSObject *scope, jsval v, jsval *vp);

PRBool
AttachNewConstructorObject(XPCCallContext &ccx, JSObject *aGlobalObject);

}

namespace SystemOnlyWrapper {

JSBool
WrapObject(JSContext *cx, JSObject *parent, jsval v, jsval *vp);

// Used by UnwrapSOW below.
JSBool
AllowedToAct(JSContext *cx, jsval idval);

}

namespace ChromeObjectWrapper    { extern JSExtendedClass COWClass; }
namespace XPCSafeJSObjectWrapper { extern JSExtendedClass SJOWClass; }
namespace SystemOnlyWrapper      { extern JSExtendedClass SOWClass; }
namespace XPCCrossOriginWrapper  { extern JSExtendedClass XOWClass; }

extern nsIScriptSecurityManager *gScriptSecurityManager;

// This namespace wraps some common functionality between the three existing
// wrappers. Its main purpose is to allow XPCCrossOriginWrapper to act both
// as an XPCSafeJSObjectWrapper and as an XPCNativeWrapper when required to
// do so (the decision is based on the principals of the wrapper and wrapped
// objects).
namespace XPCWrapper {

// FLAG_RESOLVING is used to tag a wrapper when while it's calling
// the newResolve. It tells the addProperty hook to not worry about
// what's being defined.
extern const PRUint32 FLAG_RESOLVING;

// This is used by individual wrappers as a starting point to stick
// per-wrapper flags into the flags slot. This is guaranteed to only
// have one bit set.
extern const PRUint32 LAST_FLAG;

inline JSBool
HAS_FLAGS(jsval v, PRUint32 flags)
{
  return (PRUint32(JSVAL_TO_INT(v)) & flags) != 0;
}

/**
 * Used by the cross origin and safe wrappers: the slot that the wrapped
 * object is held in.
 */
extern const PRUint32 sWrappedObjSlot;

/**
 * Used by all wrappers to store flags about their state. For example,
 * it is used when resolving a property to tell to the addProperty hook
 * that it shouldn't perform any security checks.
 */
extern const PRUint32 sFlagsSlot;

/**
 * The base number of slots needed by code using the above constants.
 */
extern const PRUint32 sNumSlots;

/**
 * Cross origin wrappers and safe JSObject wrappers both need to know
 * which native is 'eval' for various purposes.
 */
extern JSNative sEvalNative;

enum FunctionObjectSlot {
  eWrappedFunctionSlot = 0,
  eAllAccessSlot = 1
};

// Helpful for keeping lines short:
extern const PRUint32 sSecMgrSetProp, sSecMgrGetProp;

/**
 * A useful function that throws an exception onto cx.
 */
inline JSBool
DoThrowException(nsresult ex, JSContext *cx)
{
  XPCThrower::Throw(ex, cx);
  return JS_FALSE;
}

/**
 * Given a context and a global object, fill our eval native.
 */
inline JSBool
FindEval(XPCCallContext &ccx, JSObject *obj)
{
  if (sEvalNative) {
    return JS_TRUE;
  }

  jsval eval_val;
  if (!::JS_GetProperty(ccx, obj, "eval", &eval_val)) {
    return DoThrowException(NS_ERROR_UNEXPECTED, ccx);
  }

  if (JSVAL_IS_PRIMITIVE(eval_val) ||
      !::JS_ObjectIsFunction(ccx, JSVAL_TO_OBJECT(eval_val))) {
    return DoThrowException(NS_ERROR_UNEXPECTED, ccx);
  }

  sEvalNative =
    ::JS_GetFunctionNative(ccx, ::JS_ValueToFunction(ccx, eval_val));

  if (!sEvalNative) {
    return DoThrowException(NS_ERROR_UNEXPECTED, ccx);
  }

  return JS_TRUE;
}

/**
 * Returns the script security manager used by XPConnect.
 */
inline nsIScriptSecurityManager *
GetSecurityManager()
{
  return ::gScriptSecurityManager;
}

/**
 * Used to ensure that an XPCWrappedNative stays alive when its scriptable
 * helper defines an "expando" property on it.
 */
inline JSBool
MaybePreserveWrapper(JSContext *cx, XPCWrappedNative *wn, uintN flags)
{
  if ((flags & JSRESOLVE_ASSIGNING) &&
      (::JS_GetOptions(cx) & JSOPTION_PRIVATE_IS_NSISUPPORTS)) {
    nsCOMPtr<nsIXPCScriptNotify> scriptNotify = 
      do_QueryInterface(static_cast<nsISupports*>
                                   (JS_GetContextPrivate(cx)));
    if (scriptNotify) {
      return NS_SUCCEEDED(scriptNotify->PreserveWrapper(wn));
    }
  }
  return JS_TRUE;
}

inline JSBool
IsSecurityWrapper(JSObject *wrapper)
{
  JSClass *clasp = STOBJ_GET_CLASS(wrapper);
  return (clasp->flags & JSCLASS_IS_EXTENDED) &&
    ((JSExtendedClass*)clasp)->wrappedObject;
}

/**
 * Given an arbitrary object, Unwrap will return the wrapped object if the
 * passed-in object is a wrapper that Unwrap knows about *and* the
 * currently running code has permission to access both the wrapper and
 * wrapped object.
 *
 * Since this is meant to be called from functions like
 * XPCWrappedNative::GetWrappedNativeOfJSObject, it does not set an
 * exception on |cx|.
 */
JSObject *
Unwrap(JSContext *cx, JSObject *wrapper);

/**
 * Unwraps objects whose class is |xclasp|.
 */
inline JSObject *
UnwrapGeneric(JSContext *cx, const JSExtendedClass *xclasp, JSObject *wrapper)
{
  if (STOBJ_GET_CLASS(wrapper) != &xclasp->base) {
    return nsnull;
  }

  jsval v;
  if (!JS_GetReservedSlot(cx, wrapper, XPCWrapper::sWrappedObjSlot, &v)) {
    JS_ClearPendingException(cx);
    return nsnull;
  }

  if (JSVAL_IS_PRIMITIVE(v)) {
    return nsnull;
  }

  return JSVAL_TO_OBJECT(v);
}

inline JSObject *
UnwrapSOW(JSContext *cx, JSObject *wrapper)
{
  wrapper = UnwrapGeneric(cx, &SystemOnlyWrapper::SOWClass, wrapper);
  if (!wrapper) {
    return nsnull;
  }

  if (!SystemOnlyWrapper::AllowedToAct(cx, JSVAL_VOID)) {
    JS_ClearPendingException(cx);
    wrapper = nsnull;
  }

  return wrapper;
}

/**
 * Unwraps a XOW into its wrapped native.
 */
inline JSObject *
UnwrapXOW(JSContext *cx, JSObject *wrapper)
{
  wrapper = UnwrapGeneric(cx, &XPCCrossOriginWrapper::XOWClass, wrapper);
  if (!wrapper) {
    return nsnull;
  }

  nsresult rv = XPCCrossOriginWrapper::CanAccessWrapper(cx, wrapper, nsnull);
  if (NS_FAILED(rv)) {
    JS_ClearPendingException(cx);
    wrapper = nsnull;
  }

  return wrapper;
}

inline JSObject *
UnwrapCOW(JSContext *cx, JSObject *wrapper)
{
  wrapper = UnwrapGeneric(cx, &ChromeObjectWrapper::COWClass, wrapper);
  if (!wrapper) {
    return nsnull;
  }

  nsresult rv = XPCCrossOriginWrapper::CanAccessWrapper(cx, wrapper, nsnull);
  if (NS_FAILED(rv)) {
    JS_ClearPendingException(cx);
    wrapper = nsnull;
  }

  return wrapper;
}

/**
 * Rewraps a property if it needs to be rewrapped. Used by
 * GetOrSetNativeProperty to rewrap the return value.
 */
inline JSBool
RewrapIfDeepWrapper(JSContext *cx, JSObject *obj, jsval v, jsval *rval,
                    JSBool isNativeWrapper)
{
  *rval = v;
  return isNativeWrapper
         ? XPCNativeWrapper::RewrapIfDeepWrapper(cx, obj, v, rval)
         : XPCCrossOriginWrapper::RewrapIfNeeded(cx, obj, rval);
}

/**
 * Creates a wrapper around a JSObject function object. Note
 * XPCSafeJSObjectWrapper code doesn't have special function wrappers,
 * obviating the need for this function. Instead, this is used by
 * XPCNativeWrapper and the cross origin wrapper.
 */
inline JSBool
WrapFunction(JSContext *cx, JSObject *wrapperObj, JSObject *funobj, jsval *v,
             JSBool isNativeWrapper)
{
  return isNativeWrapper
         ? XPCNativeWrapper::WrapFunction(cx, funobj, v)
         : XPCCrossOriginWrapper::WrapFunction(cx, wrapperObj, funobj, v);
}

/**
 * Creates an iterator object that walks up the prototype of
 * wrappedObj. This is suitable for for-in loops over a wrapper. If
 * a property is not supposed to be reflected, the resolve hook
 * is expected to censor it. tempWrapper must be rooted already.
 */
JSObject *
CreateIteratorObj(JSContext *cx, JSObject *tempWrapper,
                  JSObject *wrapperObj, JSObject *innerObj,
                  JSBool keysonly);

/**
 * Called for the common part of adding a property to obj.
 */
JSBool
AddProperty(JSContext *cx, JSObject *wrapperObj,
            JSBool wantGetterSetter, JSObject *innerObj,
            jsval id, jsval *vp);

/**
 * Called for the common part of deleting a property from obj.
 */
JSBool
DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

/**
 * Called to enumerate the properties of |innerObj| onto |wrapperObj|.
 */
JSBool
Enumerate(JSContext *cx, JSObject *wrapperObj, JSObject *innerObj);

/**
 * Resolves a property (that may be) defined on |innerObj| onto
 * |wrapperObj|. This will also resolve random, page-defined objects
 * and is therefore unsuitable for cross-origin resolution.
 */
JSBool
NewResolve(JSContext *cx, JSObject *wrapperObj,
           JSBool preserveVal, JSObject *innerObj,
           jsval id, uintN flags, JSObject **objp);

/**
 * Resolve a native property named id from innerObj onto wrapperObj. The
 * native wrapper will be preserved if necessary. Note that if we resolve
 * an attribute here, we don't deal with the value until later.
 */
JSBool
ResolveNativeProperty(JSContext *cx, JSObject *wrapperObj,
                      JSObject *innerObj, XPCWrappedNative *wn,
                      jsval id, uintN flags, JSObject **objp,
                      JSBool isNativeWrapper);

/**
 * Gets a native property from obj. This goes directly through XPConnect, it
 * does not look at Javascript-defined getters or setters. This ensures that
 * the caller gets a real answer.
 */
JSBool
GetOrSetNativeProperty(JSContext *cx, JSObject *obj,
                       XPCWrappedNative *wrappedNative,
                       jsval id, jsval *vp, JSBool aIsSet,
                       JSBool isNativeWrapper);

/**
 * Gets a string representation of wrappedNative, going through IDL.
 */
JSBool
NativeToString(JSContext *cx, XPCWrappedNative *wrappedNative,
               uintN argc, jsval *argv, jsval *rval,
               JSBool isNativeWrapper);

/**
 * Looks up a property on obj. If it exists, then the parameters are filled
 * in with useful values.
 *
 * NB: All parameters must be initialized before the call.
 */
JSBool
GetPropertyAttrs(JSContext *cx, JSObject *obj, jsid interned_id, uintN flags,
                 JSBool wantDetails, JSPropertyDescriptor *desc);

} // namespace XPCWrapper


#endif
