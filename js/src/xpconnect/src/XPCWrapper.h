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

/* These are used by XPCNativeWrapper polluted code in the common wrapper. */
#define FLAG_DEEP     0x1
#define FLAG_EXPLICIT 0x2
// FLAG_RESOLVING is used to tag an XPCNativeWrapper when while it's calling
// the newResolve hook on the XPCWrappedNative's scriptable info.
#define FLAG_RESOLVING 0x4

#define HAS_FLAGS(_val, _flags) \
  ((PRUint32(JSVAL_TO_INT(_val)) & (_flags)) != 0)

#define NATIVE_HAS_FLAG(_wn, _flag)                \
  ((_wn)->GetScriptableInfo() &&                   \
   (_wn)->GetScriptableInfo()->GetFlags()._flag())

JSBool
XPC_NW_WrapFunction(JSContext* cx, JSObject* funobj, jsval *rval);

JSBool
XPC_NW_RewrapIfDeepWrapper(JSContext *cx, JSObject *obj, jsval v,
                           jsval *rval);

/* These are used by XPC_XOW_* polluted code in the common wrapper. */
JSBool
XPC_XOW_WrapFunction(JSContext *cx, JSObject *wrapperObj, JSObject *funobj,
                     jsval *rval);

JSBool
XPC_XOW_RewrapIfNeeded(JSContext *cx, JSObject *wrapperObj, jsval *vp);

nsresult
IsWrapperSameOrigin(JSContext *cx, JSObject *wrappedObj);

inline JSBool
XPC_XOW_ClassNeedsXOW(const char *name)
{
  // TODO Make a perfect hash of these and use that?
  return !strcmp(name, "Window")            ||
         !strcmp(name, "Location")          ||
         !strcmp(name, "HTMLDocument")      ||
         !strcmp(name, "HTMLIFrameElement") ||
         !strcmp(name, "HTMLFrameElement");
}

extern JSExtendedClass sXPC_XOW_JSClass;

// This class wraps some common functionality between the three existing
// wrappers. Its main purpose is to allow XPCCrossOriginWrapper to act both
// as an XPCSafeSJSObjectWrapper and as an XPCNativeWrapper when required to
// do so (the decision is based on the principals of the wrapper and wrapped
// objects).
class XPCWrapper
{
public:
  /**
   * Used by the cross origin and safe wrappers: the slot that the wrapped
   * object is held in.
   */
  static const PRUint32 sWrappedObjSlot;

  /**
   * Used by the cross origin and safe wrappers: the slot that tells the
   * AddProperty code that we're resolving a property, and therefore to not do
   * a security check.
   */
  static const PRUint32 sResolvingSlot;

  /**
   * The base number of slots needed by code using the above constants.
   */
  static const PRUint32 sNumSlots;

  /**
   * Cross origin wrappers and safe JSObject wrappers both need to know
   * which native is 'eval' for various purposes.
   */
  static JSNative sEvalNative;

  /**
   * Given a context and a global object, fill our eval native.
   */
  static JSBool FindEval(XPCCallContext &ccx, JSObject *obj) {
    if (sEvalNative) {
      return JS_TRUE;
    }

    jsval eval_val;
    if (!::JS_GetProperty(ccx, obj, "eval", &eval_val)) {
      return ThrowException(NS_ERROR_UNEXPECTED, ccx);
    }

    if (JSVAL_IS_PRIMITIVE(eval_val) ||
        !::JS_ObjectIsFunction(ccx, JSVAL_TO_OBJECT(eval_val))) {
      return ThrowException(NS_ERROR_UNEXPECTED, ccx);
    }

    sEvalNative =
      ::JS_GetFunctionNative(ccx, ::JS_ValueToFunction(ccx, eval_val));

    if (!sEvalNative) {
      return ThrowException(NS_ERROR_UNEXPECTED, ccx);
    }

    return JS_TRUE;
  }

  /**
   * A useful function that throws an exception onto cx.
   */
  static JSBool ThrowException(nsresult ex, JSContext *cx) {
    XPCThrower::Throw(ex, cx);
    return JS_FALSE;
  }

  /**
   * Used to ensure that an XPCWrappedNative stays alive when its scriptable
   * helper defines an "expando" property on it.
   */
  static JSBool MaybePreserveWrapper(JSContext *cx, XPCWrappedNative *wn,
                                     uintN flags) {
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

  /**
   * Unwraps an XPCSafeJSObjectWrapper or an XPCCrossOriginWrapper into its
   * wrapped native.
   */
  static JSObject *Unwrap(JSContext *cx, JSObject *wrapper) {
    if (JS_GET_CLASS(cx, wrapper) != &sXPC_XOW_JSClass.base) {
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

    JSObject *wrappedObj = JSVAL_TO_OBJECT(v);
    nsresult rv = IsWrapperSameOrigin(cx, wrappedObj);
    if (NS_FAILED(rv)) {
      JS_ClearPendingException(cx);
      return nsnull;
    }

    return wrappedObj;
  }

  /**
   * Rewraps a property if it needs to be rewrapped. Used by
   * GetOrSetNativeProperty to rewrap the return value.
   */
  static JSBool RewrapIfDeepWrapper(JSContext *cx, JSObject *obj, jsval v,
                                    jsval *rval, JSBool isNativeWrapper) {
    *rval = v;
    return isNativeWrapper
           ? XPC_NW_RewrapIfDeepWrapper(cx, obj, v, rval)
           : XPC_XOW_RewrapIfNeeded(cx, obj, rval);
  }

  /**
   * Creates a wrapper around a JSObject function object. Note
   * XPCSafeJSObjectWrapper code doesn't have special function wrappers,
   * obviating the need for this function. Instead, this is used by
   * XPCNativeWrapper and the cross origin wrapper.
   */
  static inline JSBool WrapFunction(JSContext *cx, JSObject *wrapperObj,
                                    JSObject *funobj, jsval *v,
                                    JSBool isNativeWrapper) {
    return isNativeWrapper
           ? XPC_NW_WrapFunction(cx, funobj, v)
           : XPC_XOW_WrapFunction(cx, wrapperObj, funobj, v);
  }

  /**
   * Called for the common part of adding a property to obj.
   */
  static JSBool AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

  /**
   * Called for the common part of deleting a property from obj.
   */
  static JSBool DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

  /**
   * Called to enumerate the properties of |innerObj| onto |wrapperObj|.
   */
  static JSBool Enumerate(JSContext *cx, JSObject *wrapperObj,
                          JSObject *innerObj);

  /**
   * Resolves a property (that may be) defined on |innerObj| onto
   * |wrapperObj|. This will also resolve random, page-defined objects
   * and is therefore unsuitable for cross-origin resolution.
   */
  static JSBool NewResolve(JSContext *cx, JSObject *wrapperObj,
                           JSObject *innerObj, jsval id, uintN flags,
                           JSObject **objp, JSBool preserveVal = JS_FALSE);

  /**
   * Resolve a native property named id from innerObj onto wrapperObj. The
   * native wrapper will be preserved if necessary. Note that if we resolve
   * an attribute here, we don't deal with the value until later.
   */
  static JSBool ResolveNativeProperty(JSContext *cx, JSObject *wrapperObj,
                                      JSObject *innerObj, XPCWrappedNative *wn,
                                      jsval id, uintN flags, JSObject **objp,
                                      JSBool isNativeWrapper);

  /**
   * Gets a native property from obj. This goes directly through XPConnect, it
   * does not look at Javascript-defined getters or setters. This ensures that
   * the caller gets a real answer.
   */
  static JSBool GetOrSetNativeProperty(JSContext *cx, JSObject *obj,
                                       XPCWrappedNative *wrappedNative,
                                       jsval id, jsval *vp, JSBool aIsSet,
                                       JSBool isNativeWrapper);

  /**
   * Gets a string representation of wrappedNative, going through IDL.
   */
  static JSBool NativeToString(JSContext *cx, XPCWrappedNative *wrappedNative,
                               uintN argc, jsval *argv, jsval *rval,
                               JSBool isNativeWrapper);
};


#endif
