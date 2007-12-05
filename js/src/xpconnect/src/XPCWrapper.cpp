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

const PRUint32
XPCWrapper::sWrappedObjSlot = 1;

const PRUint32
XPCWrapper::sResolvingSlot = 0;

const PRUint32
XPCWrapper::sNumSlots = 2;

JSNative
XPCWrapper::sEvalNative = nsnull;

// static
JSBool
XPCWrapper::AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  if (JSVAL_IS_STRING(id)) {
    JSString *str = JSVAL_TO_STRING(id);
    jschar *chars = ::JS_GetStringChars(str);
    size_t length = ::JS_GetStringLength(str);

    return ::JS_DefineUCProperty(cx, obj, chars, length, *vp, nsnull,
                                 nsnull, JSPROP_ENUMERATE);
  }

  if (!JSVAL_IS_INT(id)) {
    return ThrowException(NS_ERROR_NOT_IMPLEMENTED, cx);
  }

  return ::JS_DefineElement(cx, obj, JSVAL_TO_INT(id), *vp, nsnull,
                            nsnull, JSPROP_ENUMERATE);
}

// static
JSBool
XPCWrapper::DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  if (JSVAL_IS_STRING(id)) {
    JSString *str = JSVAL_TO_STRING(id);
    jschar *chars = ::JS_GetStringChars(str);
    size_t length = ::JS_GetStringLength(str);

    return ::JS_DeleteUCProperty2(cx, obj, chars, length, vp);
  }

  if (!JSVAL_IS_INT(id)) {
    return ThrowException(NS_ERROR_NOT_IMPLEMENTED, cx);
  }

  return ::JS_DeleteElement2(cx, obj, JSVAL_TO_INT(id), vp);
}

// static
JSBool
XPCWrapper::Enumerate(JSContext *cx, JSObject *wrapperObj, JSObject *innerObj)
{
  // We are being notified of a for-in loop or similar operation on
  // this wrapper. Forward to the correct high-level object hook,
  // OBJ_ENUMERATE on the unsafe object, called via the JS_Enumerate API.
  // Then reflect properties named by the enumerated identifiers from the
  // unsafe object to the safe wrapper.

  JSBool ok = JS_TRUE;

  JSIdArray *ida = JS_Enumerate(cx, innerObj);
  if (!ida) {
    return JS_FALSE;
  }

  for (jsint i = 0, n = ida->length; i < n; i++) {
    JSObject *pobj;
    JSProperty *prop;

    // Let OBJ_LOOKUP_PROPERTY, in particular our NewResolve hook,
    // figure out whether this id should be reflected.
    ok = OBJ_LOOKUP_PROPERTY(cx, wrapperObj, ida->vector[i], &pobj, &prop);
    if (!ok) {
      break;
    }

    if (prop) {
      OBJ_DROP_PROPERTY(cx, pobj, prop);
    }

    if (pobj != wrapperObj) {
      ok = OBJ_DEFINE_PROPERTY(cx, wrapperObj, ida->vector[i], JSVAL_VOID,
                               nsnull, nsnull, JSPROP_ENUMERATE | JSPROP_SHARED,
                               nsnull);
    }

    if (!ok) {
      break;
    }
  }

  JS_DestroyIdArray(cx, ida);

  return ok;
}

// static
JSBool
XPCWrapper::NewResolve(JSContext *cx, JSObject *wrapperObj,
                       JSObject *innerObj, jsval id, uintN flags,
                       JSObject **objp, JSBool preserveVal)
{
  jsval v = JSVAL_VOID;

  jsid interned_id;
  if (!::JS_ValueToId(cx, id, &interned_id)) {
    return JS_FALSE;
  }

  JSProperty *prop;
  JSObject *innerObjp;
  if (!OBJ_LOOKUP_PROPERTY(cx, innerObj, interned_id, &innerObjp, &prop)) {
    return JS_FALSE;
  }

  if (!prop) {
    // Nothing to define.
    return JS_TRUE;
  }

  JSBool isXOW = (JS_GET_CLASS(cx, wrapperObj) == &sXPC_XOW_JSClass.base);
  uintN attrs = JSPROP_ENUMERATE;
  JSPropertyOp getter = nsnull;
  JSPropertyOp setter = nsnull;
  if (isXOW && OBJ_IS_NATIVE(innerObjp)) {
    JSScopeProperty *sprop = reinterpret_cast<JSScopeProperty *>(prop);

    attrs = sprop->attrs;
    if (attrs & JSPROP_GETTER) {
      getter =  sprop->getter;
    }
    if (attrs & JSPROP_SETTER) {
      setter = sprop->setter;
    }

    if ((preserveVal || isXOW) &&
        SPROP_HAS_VALID_SLOT(sprop, OBJ_SCOPE(innerObjp))) {
      v = OBJ_GET_SLOT(cx, innerObjp, sprop->slot);
    }
  }

  OBJ_DROP_PROPERTY(cx, innerObjp, prop);

  // Hack alert: we only do this for same-origin calls on XOWs: we want
  // to preserve 'eval' function wrapper on the wrapper object itself
  // to preserve eval's identity.
  if (!preserveVal && isXOW && !JSVAL_IS_PRIMITIVE(v)) {
    JSObject *obj = JSVAL_TO_OBJECT(v);
    if (JS_ObjectIsFunction(cx, obj)) {
      JSFunction *fun = reinterpret_cast<JSFunction *>(JS_GetPrivate(cx, obj));
      if (JS_GetFunctionNative(cx, fun) == sEvalNative &&
          !WrapFunction(cx, wrapperObj, obj, &v, JS_FALSE)) {
        return JS_FALSE;
      }
    }
  }

  jsval oldSlotVal;
  if (!::JS_GetReservedSlot(cx, wrapperObj, sResolvingSlot, &oldSlotVal) ||
      !::JS_SetReservedSlot(cx, wrapperObj, sResolvingSlot, JSVAL_TRUE)) {
    return JS_FALSE;
  }

  const uintN interesting_attrs = isXOW
                                  ? (JSPROP_ENUMERATE |
                                     JSPROP_READONLY  |
                                     JSPROP_PERMANENT |
                                     JSPROP_SHARED    |
                                     JSPROP_GETTER    |
                                     JSPROP_SETTER)
                                  : JSPROP_ENUMERATE;
  JSBool ok = OBJ_DEFINE_PROPERTY(cx, wrapperObj, interned_id, v, getter,
                                  setter, (attrs & interesting_attrs), nsnull);

  if (ok && (ok = ::JS_SetReservedSlot(cx, wrapperObj, sResolvingSlot,
                                       oldSlotVal))) {
    *objp = wrapperObj;
  }

  return ok;
}

// static
JSBool
XPCWrapper::ResolveNativeProperty(JSContext *cx, JSObject *wrapperObj,
                                  JSObject *innerObj, XPCWrappedNative *wn,
                                  jsval id, uintN flags, JSObject **objp,
                                  JSBool isNativeWrapper)
{
  // This will do verification and the method lookup for us.
  XPCCallContext ccx(JS_CALLER, cx, innerObj, nsnull, id);

  // For "constructor" we don't want to call into the resolve hooks on the
  // wrapped native, since that would give the wrong constructor.
  if (NATIVE_HAS_FLAG(wn, WantNewResolve) &&
      id != GetRTStringByIndex(cx, XPCJSRuntime::IDX_CONSTRUCTOR)) {

    // Mark ourselves as resolving so our AddProperty hook can do the
    // right thing here.
    jsval oldFlags;
    if (isNativeWrapper) {
      if (!::JS_GetReservedSlot(cx, wrapperObj, 0, &oldFlags) ||
          !::JS_SetReservedSlot(cx, wrapperObj, 0,
                                INT_TO_JSVAL(JSVAL_TO_INT(oldFlags) |
                                             FLAG_RESOLVING))) {
        return JS_FALSE;
      }
    } else {
      if (!::JS_GetReservedSlot(cx, wrapperObj, sResolvingSlot, &oldFlags) ||
          !::JS_SetReservedSlot(cx, wrapperObj, sResolvingSlot, JSVAL_TRUE)) {
        return JS_FALSE;
      }
    }

    XPCWrappedNative* oldResolvingWrapper = nsnull;
    JSBool allowPropMods =
      NATIVE_HAS_FLAG(wn, AllowPropModsDuringResolve);
    if (allowPropMods) {
      oldResolvingWrapper = ccx.SetResolvingWrapper(wn);
    }

    JSBool retval = JS_TRUE;
    JSObject* newObj = nsnull;
    nsresult rv = wn->GetScriptableInfo()->
      GetCallback()->NewResolve(wn, cx, wrapperObj, id, flags,
                                &newObj, &retval);

    if (allowPropMods) {
      ccx.SetResolvingWrapper(oldResolvingWrapper);
    }

    if (!::JS_SetReservedSlot(cx, wrapperObj,
                              isNativeWrapper ? 0 : sResolvingSlot,
                              oldFlags)) {
      return JS_FALSE;
    }

    if (NS_FAILED(rv)) {
      return ThrowException(rv, cx);
    }

    if (newObj) {
      if (isNativeWrapper || newObj == wrapperObj) {
#ifdef DEBUG_XPCNativeWrapper
        JSString* strId = ::JS_ValueToString(cx, id);
        if (strId) {
          NS_ConvertUTF16toUTF8 propName((PRUnichar*)::JS_GetStringChars(strId),
                                         ::JS_GetStringLength(strId));
          printf("Resolved via scriptable hooks for '%s'\n", propName.get());
        }
#endif
        // Note that we don't need to preserve the wrapper here, since this is
        // not an "expando" property if the scriptable newResolve hook found it.
        *objp = newObj;
        return retval;
      }

      return NewResolve(cx, wrapperObj, innerObj, id, flags, objp, JS_TRUE);
    }
  }

  if (!JSVAL_IS_STRING(id)) {
    // A non-string id is being resolved. Won't be found here, return
    // early.

    return MaybePreserveWrapper(cx, wn, flags);
  }

  // Verify that our jsobject really is a wrapped native.
  XPCWrappedNative* wrapper = ccx.GetWrapper();
  if (wrapper != wn || !wrapper->IsValid()) {
    NS_ASSERTION(wrapper == wn, "Uh, how did this happen!");
    return ThrowException(NS_ERROR_XPC_BAD_CONVERT_JS, cx);
  }

  // it would be a big surprise if there is a member without an
  // interface :)
  XPCNativeInterface* iface = ccx.GetInterface();
  if (!iface) {
    // No interface, nothing to resolve.

    return MaybePreserveWrapper(cx, wn, flags);
  }

  // did we find a method/attribute by that name?
  XPCNativeMember* member = ccx.GetMember();
  NS_ASSERTION(member, "not doing IDispatch, how'd this happen?");
  if (!member) {
    // No member, nothing to resolve.

    return MaybePreserveWrapper(cx, wn, flags);
  }

  // Get (and perhaps lazily create) the member's value (commonly a
  // cloneable function).
  jsval memberval;
  if (!member->GetValue(ccx, iface, &memberval)) {
    return ThrowException(NS_ERROR_XPC_BAD_CONVERT_JS, cx);
  }

  // Make sure memberval doesn't go away while we mess with it.
  AUTO_MARK_JSVAL(ccx, memberval);

  JSString *str = JSVAL_TO_STRING(id);
  if (!str) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  jsval v;
  uintN attrs = JSPROP_ENUMERATE;

  if (member->IsConstant()) {
    v = memberval;
  } else if (member->IsAttribute()) {
    // An attribute is being resolved. Define the property, the value
    // will be dealt with in the get/set hooks.  Use JSPROP_SHARED to
    // avoid entraining last-got or last-set garbage beyond the life
    // of the value in the getter or setter call site.

    v = JSVAL_VOID;
    attrs |= JSPROP_SHARED;
  } else {
    // We're dealing with a method member here. Clone a function we can
    // use for this object.  NB: cx's newborn roots will protect funobj
    // and funWrapper and its object from GC.

    JSObject* funobj = xpc_CloneJSFunction(ccx, JSVAL_TO_OBJECT(memberval),
                                           wrapper->GetFlatJSObject());
    if (!funobj) {
      return JS_FALSE;
    }

    AUTO_MARK_JSVAL(ccx, OBJECT_TO_JSVAL(funobj));

#ifdef DEBUG_XPCNativeWrapper
    printf("Wrapping function object for %s\n",
           ::JS_GetStringBytes(JSVAL_TO_STRING(id)));
#endif

    if (!WrapFunction(cx, wrapperObj, funobj, &v, isNativeWrapper)) {
      return JS_FALSE;
    }
  }

  // XPCNativeWrapper doesn't need to do this.
  jsval oldFlags;
  if (!isNativeWrapper &&
      (!::JS_GetReservedSlot(cx, wrapperObj, sResolvingSlot, &oldFlags) ||
       !::JS_SetReservedSlot(cx, wrapperObj, sResolvingSlot, JSVAL_TRUE))) {
    return JS_FALSE;
  }

  if (!::JS_DefineUCProperty(cx, wrapperObj, ::JS_GetStringChars(str),
                            ::JS_GetStringLength(str), v, nsnull, nsnull,
                            attrs)) {
    return JS_FALSE;
  }

  if (!isNativeWrapper &&
      !::JS_SetReservedSlot(cx, wrapperObj, sResolvingSlot, oldFlags)) {
    return JS_FALSE;
  }

  *objp = wrapperObj;

  return JS_TRUE;
}

// static
JSBool
XPCWrapper::GetOrSetNativeProperty(JSContext *cx, JSObject *obj,
                                   XPCWrappedNative *wrappedNative,
                                   jsval id, jsval *vp, JSBool aIsSet,
                                   JSBool isNativeWrapper)
{
  // This will do verification and the method lookup for us.
  JSObject *nativeObj = wrappedNative->GetFlatJSObject();
  XPCCallContext ccx(JS_CALLER, cx, nativeObj, nsnull, id);

  if (aIsSet ? NATIVE_HAS_FLAG(wrappedNative, WantSetProperty) :
               NATIVE_HAS_FLAG(wrappedNative, WantGetProperty)) {

    jsval v = *vp;
    // Note that some sets return random DOM objects (setting
    // document.location, say), so we want to rewrap for sets too if v != *vp.
    JSBool retval = JS_TRUE;
    nsresult rv;
    if (aIsSet) {
      rv = wrappedNative->GetScriptableCallback()->
        SetProperty(wrappedNative, cx, obj, id, &v, &retval);
    } else {
      rv = wrappedNative->GetScriptableCallback()->
        GetProperty(wrappedNative, cx, obj, id, &v, &retval);
    }

    if (NS_FAILED(rv)) {
      return ThrowException(rv, cx);
    }
    if (!retval) {
      return JS_FALSE;
    }

    if (rv == NS_SUCCESS_I_DID_SOMETHING) {
      // Make sure v doesn't get collected while we're re-wrapping it.
      AUTO_MARK_JSVAL(ccx, v);

#ifdef DEBUG_XPCNativeWrapper
      JSString* strId = ::JS_ValueToString(cx, id);
      if (strId) {
        NS_ConvertUTF16toUTF8 propName((PRUnichar*)::JS_GetStringChars(strId),
                                       ::JS_GetStringLength(strId));
        printf("%s via scriptable hooks for '%s'\n",
               aIsSet ? "Set" : "Got", propName.get());
      }
#endif

      return RewrapIfDeepWrapper(cx, obj, v, vp, isNativeWrapper);
    }
  }

  if (!JSVAL_IS_STRING(id)) {
    // Not going to be found here
    return JS_TRUE;
  }

  // Verify that our jsobject really is a wrapped native.
  XPCWrappedNative* wrapper = ccx.GetWrapper();
  if (wrapper != wrappedNative || !wrapper->IsValid()) {
    NS_ASSERTION(wrapper == wrappedNative, "Uh, how did this happen!");
    return ThrowException(NS_ERROR_XPC_BAD_CONVERT_JS, cx);
  }

  // it would be a big surprise if there is a member without an
  // interface :)
  XPCNativeInterface* iface = ccx.GetInterface();
  if (!iface) {

    return JS_TRUE;
  }

  // did we find a method/attribute by that name?
  XPCNativeMember* member = ccx.GetMember();
  NS_ASSERTION(member, "not doing IDispatch, how'd this happen?");
  if (!member) {
    // No member, no IDL property to expose.

    return JS_TRUE;
  }

  // Get (and perhaps lazily create) the member's value (commonly a
  // cloneable function).
  jsval memberval;
  if (!member->GetValue(ccx, iface, &memberval)) {
    return ThrowException(NS_ERROR_XPC_BAD_CONVERT_JS, cx);
  }

  if (member->IsConstant()) {
    // Getting the value of constants is easy, just return the
    // value. Setting is not supported (obviously).
    if (aIsSet) {
      return ThrowException(NS_ERROR_XPC_BAD_CONVERT_JS, cx);
    }

    *vp = memberval;

    return JS_TRUE;
  }

  if (!member->IsAttribute()) {
    // Getting the value of a method. Just return and let the value
    // from XPC_NW_NewResolve() be used.

    return JS_TRUE;
  }

  // Make sure the function we're cloning doesn't go away while
  // we're cloning it.
  AUTO_MARK_JSVAL(ccx, memberval);

  // clone a function we can use for this object
  JSObject* funobj = xpc_CloneJSFunction(ccx, JSVAL_TO_OBJECT(memberval),
                                         wrapper->GetFlatJSObject());
  if (!funobj) {
    return JS_FALSE;
  }

  jsval *argv = nsnull;
  uintN argc = 0;

  if (aIsSet) {
    if (member->IsReadOnlyAttribute()) {
      // Trying to set a property for which there is no setter!
      return ThrowException(NS_ERROR_NOT_AVAILABLE, cx);
    }

#ifdef DEBUG_XPCNativeWrapper
    printf("Calling setter for %s\n",
           ::JS_GetStringBytes(JSVAL_TO_STRING(id)));
#endif

    argv = vp;
    argc = 1;
  } else {
#ifdef DEBUG_XPCNativeWrapper
    printf("Calling getter for %s\n",
           ::JS_GetStringBytes(JSVAL_TO_STRING(id)));
#endif
  }

  // Call the getter
  jsval v;
  if (!::JS_CallFunctionValue(cx, wrapper->GetFlatJSObject(),
                              OBJECT_TO_JSVAL(funobj), argc, argv, &v)) {
    return JS_FALSE;
  }

  if (aIsSet) {
    return JS_TRUE;
  }

  {
    // Make sure v doesn't get collected while we're re-wrapping it.
    AUTO_MARK_JSVAL(ccx, v);

    return RewrapIfDeepWrapper(cx, obj, v, vp, isNativeWrapper);
  }
}

// static
JSBool
XPCWrapper::NativeToString(JSContext *cx, XPCWrappedNative *wrappedNative,
                           uintN argc, jsval *argv, jsval *rval,
                           JSBool isNativeWrapper)
{
  // Check whether toString was overridden in any object along
  // the wrapped native's object's prototype chain.
  XPCJSRuntime *rt = nsXPConnect::GetRuntime();
  if (!rt)
    return JS_FALSE;

  jsid id = rt->GetStringID(XPCJSRuntime::IDX_TO_STRING);
  jsval idAsVal;
  if (!::JS_IdToValue(cx, id, &idAsVal)) {
    return JS_FALSE;
  }

  // Someone is trying to call toString on our wrapped object.
  JSObject *wn_obj = wrappedNative->GetFlatJSObject();
  XPCCallContext ccx(JS_CALLER, cx, wn_obj, nsnull, idAsVal);
  if (!ccx.IsValid()) {
    // Shouldn't really happen.
    return ThrowException(NS_ERROR_FAILURE, cx);
  }

  XPCNativeInterface *iface = ccx.GetInterface();
  XPCNativeMember *member = ccx.GetMember();
  JSBool overridden = JS_FALSE;
  jsval toStringVal;

  // First, try to see if the object declares a toString in its IDL. If it does,
  // then we need to defer to that.
  if (iface && member) {
    if (!member->GetValue(ccx, iface, &toStringVal)) {
      return JS_FALSE;
    }

    overridden = member->IsMethod();
  }

  JSString* str = nsnull;
  if (overridden) {
    // Defer to the IDL-declared toString.

    AUTO_MARK_JSVAL(ccx, toStringVal);

    JSObject *funobj = xpc_CloneJSFunction(ccx, JSVAL_TO_OBJECT(toStringVal),
                                           wn_obj);
    if (!funobj) {
      return JS_FALSE;
    }

    jsval v;
    if (!::JS_CallFunctionValue(cx, wn_obj, OBJECT_TO_JSVAL(funobj), argc, argv,
                                &v)) {
      return JS_FALSE;
    }

    if (JSVAL_IS_STRING(v)) {
      str = JSVAL_TO_STRING(v);
    }
  }

  if (!str) {
    // Ok, we do no damage, and add value, by returning our own idea
    // of what toString() should be.
    // Note: We can't just call JS_ValueToString on the wrapped object. Instead,
    // we need to call the wrapper's ToString in order to safely convert our
    // object to a string.

    char *wrapperStr = nsnull;
    nsAutoString resultString;
    if (isNativeWrapper) {
      resultString.AppendLiteral("[object XPCNativeWrapper ");

      wrapperStr = wrappedNative->ToString(ccx);
      if (!wrapperStr) {
        return JS_FALSE;
      }
    } else {
      wrapperStr = wrappedNative->ToString(ccx);
      if (!wrapperStr) {
        return JS_FALSE;
      }
    }

    resultString.AppendASCII(wrapperStr);
    JS_smprintf_free(wrapperStr);

    if (isNativeWrapper) {
      resultString.Append(']');
    }

    str = ::JS_NewUCStringCopyN(cx, reinterpret_cast<const jschar *>
                                                    (resultString.get()),
                                resultString.Length());
  }

  NS_ENSURE_TRUE(str, JS_FALSE);

  *rval = STRING_TO_JSVAL(str);
  return JS_TRUE;
}
