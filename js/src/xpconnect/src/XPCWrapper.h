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

JSBool
XPC_XOW_WrapperMoved(JSContext *cx, XPCWrappedNative *innerObj,
                     XPCWrappedNativeScope *newScope);

nsresult
CanAccessWrapper(JSContext *cx, JSObject *wrappedObj);

// Used by UnwrapSOW below.
JSBool
AllowedToAct(JSContext *cx, jsval idval);

inline JSBool
XPC_XOW_ClassNeedsXOW(const char *name)
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

extern JSExtendedClass sXPC_COW_JSClass;
extern JSExtendedClass sXPC_SJOW_JSClass;
extern JSExtendedClass sXPC_SOW_JSClass;
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
   * Used by all wrappers to store flags about their state. For example,
   * it is used when resolving a property to tell to the addProperty hook
   * that it shouldn't perform any security checks.
   */
  static const PRUint32 sFlagsSlot;

  /**
   * The base number of slots needed by code using the above constants.
   */
  static const PRUint32 sNumSlots;

  /**
   * Cross origin wrappers and safe JSObject wrappers both need to know
   * which native is 'eval' for various purposes.
   */
  static JSNative sEvalNative;

  enum FunctionObjectSlot {
    eWrappedFunctionSlot = 0,
    eAllAccessSlot = 1
  };

  // Helpful for keeping lines short:
  static const PRUint32 sSecMgrSetProp, sSecMgrGetProp;

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
   * Returns the script security manager used by XPConnect.
   */
  static nsIScriptSecurityManager *GetSecurityManager() {
    extern nsIScriptSecurityManager *gScriptSecurityManager;

    return gScriptSecurityManager;
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

  static JSBool IsSecurityWrapper(JSObject *wrapper)
  {
    JSClass *clasp = STOBJ_GET_CLASS(wrapper);
    return clasp == &sXPC_COW_JSClass.base ||
           clasp == &sXPC_SJOW_JSClass.base ||
           clasp == &sXPC_SOW_JSClass.base ||
           clasp == &sXPC_XOW_JSClass.base;
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
  static JSObject *Unwrap(JSContext *cx, JSObject *wrapper);

  /**
   * Unwraps objects whose class is |xclasp|.
   */
  static JSObject *UnwrapGeneric(JSContext *cx, const JSExtendedClass *xclasp,
                                 JSObject *wrapper)
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

  static JSObject *UnwrapSOW(JSContext *cx, JSObject *wrapper) {
    wrapper = UnwrapGeneric(cx, &sXPC_SOW_JSClass, wrapper);
    if (!wrapper) {
      return nsnull;
    }

    if (!AllowedToAct(cx, JSVAL_VOID)) {
      JS_ClearPendingException(cx);
      wrapper = nsnull;
    }

    return wrapper;
  }

  /**
   * Unwraps a XOW into its wrapped native.
   */
  static JSObject *UnwrapXOW(JSContext *cx, JSObject *wrapper) {
    wrapper = UnwrapGeneric(cx, &sXPC_XOW_JSClass, wrapper);
    if (!wrapper) {
      return nsnull;
    }

    nsresult rv = CanAccessWrapper(cx, wrapper);
    if (NS_FAILED(rv)) {
      JS_ClearPendingException(cx);
      wrapper = nsnull;
    }

    return wrapper;
  }

  static JSObject *UnwrapCOW(JSContext *cx, JSObject *wrapper) {
    wrapper = UnwrapGeneric(cx, &sXPC_COW_JSClass, wrapper);
    if (!wrapper) {
      return nsnull;
    }

    nsresult rv = CanAccessWrapper(cx, wrapper);
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
   * Creates an iterator object that walks up the prototype of
   * wrappedObj. This is suitable for for-in loops over a wrapper. If
   * a property is not supposed to be reflected, the resolve hook
   * is expected to censor it. tempWrapper must be rooted already.
   */
  static JSObject *CreateIteratorObj(JSContext *cx,
                                     JSObject *tempWrapper,
                                     JSObject *wrapperObj,
                                     JSObject *innerObj,
                                     JSBool keysonly);

  /**
   * Called for the common part of adding a property to obj.
   */
  static JSBool AddProperty(JSContext *cx, JSObject *wrapperObj,
                            JSBool wantGetterSetter, JSObject *innerObj,
                            jsval id, jsval *vp);

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
                           JSBool preserveVal, JSObject *innerObj,
                           jsval id, uintN flags, JSObject **objp);

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

  /**
   * Looks up a property on obj. If it exists, then the parameters are filled
   * in with useful values.
   *
   * NB: All parameters must be initialized before the call.
   */
  static JSBool GetPropertyAttrs(JSContext *cx, JSObject *obj,
                                 jsid interned_id, uintN flags,
                                 JSBool wantDetails,
                                 JSPropertyDescriptor *desc);
};


#endif
