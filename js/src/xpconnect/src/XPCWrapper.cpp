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
#include "XPCNativeWrapper.h"

const PRUint32
XPCWrapper::sWrappedObjSlot = 1;

const PRUint32
XPCWrapper::sFlagsSlot = 0;

const PRUint32
XPCWrapper::sNumSlots = 2;

JSNative
XPCWrapper::sEvalNative = nsnull;

const PRUint32
XPCWrapper::sSecMgrSetProp = nsIXPCSecurityManager::ACCESS_SET_PROPERTY;
const PRUint32
XPCWrapper::sSecMgrGetProp = nsIXPCSecurityManager::ACCESS_GET_PROPERTY;

// static
JSObject *
XPCWrapper::Unwrap(JSContext *cx, JSObject *wrapper)
{
  JSClass *clasp = STOBJ_GET_CLASS(wrapper);
  if (clasp == &sXPC_XOW_JSClass.base) {
    return UnwrapXOW(cx, wrapper);
  }

  if (XPCNativeWrapper::IsNativeWrapperClass(clasp)) {
    XPCWrappedNative *wrappedObj;
    if (!XPCNativeWrapper::GetWrappedNative(cx, wrapper, &wrappedObj) ||
        !wrappedObj) {
      return nsnull;
    }

    return wrappedObj->GetFlatJSObject();
  }

  if (clasp == &sXPC_SJOW_JSClass.base) {
    JSObject *wrappedObj = STOBJ_GET_PARENT(wrapper);

    if (NS_FAILED(CanAccessWrapper(cx, wrappedObj))) {
      JS_ClearPendingException(cx);

      return nsnull;
    }

    return wrappedObj;
  }

  if (clasp == &sXPC_SOW_JSClass.base) {
    return UnwrapSOW(cx, wrapper);
  }
  if (clasp == &sXPC_COW_JSClass.base) {
    return UnwrapCOW(cx, wrapper);
  }

  return nsnull;
}

static void
IteratorFinalize(JSContext *cx, JSObject *obj)
{
  jsval v;
  JS_GetReservedSlot(cx, obj, 0, &v);

  JSIdArray *ida = reinterpret_cast<JSIdArray *>(JSVAL_TO_PRIVATE(v));
  if (ida) {
    JS_DestroyIdArray(cx, ida);
  }
}

static JSBool
IteratorNext(JSContext *cx, uintN argc, jsval *vp)
{
  JSObject *obj;
  jsval v;

  obj = JS_THIS_OBJECT(cx, vp);
  if (!obj)
    return JS_FALSE;

  JS_GetReservedSlot(cx, obj, 0, &v);
  JSIdArray *ida = reinterpret_cast<JSIdArray *>(JSVAL_TO_PRIVATE(v));

  JS_GetReservedSlot(cx, obj, 1, &v);
  jsint idx = JSVAL_TO_INT(v);

  if (idx == ida->length) {
    return JS_ThrowStopIteration(cx);
  }

  JS_GetReservedSlot(cx, obj, 2, &v);
  jsid id = ida->vector[idx++];
  if (JSVAL_TO_BOOLEAN(v)) {
    JSString *str;
    if (!JS_IdToValue(cx, id, &v) ||
        !(str = JS_ValueToString(cx, v))) {
      return JS_FALSE;
    }

    *vp = STRING_TO_JSVAL(str);
  } else {
    // We need to return an [id, value] pair.
    if (!JS_GetPropertyById(cx, STOBJ_GET_PARENT(obj), id, &v)) {
      return JS_FALSE;
    }

    jsval name;
    JSString *str;
    if (!JS_IdToValue(cx, id, &name) ||
        !(str = JS_ValueToString(cx, name))) {
      return JS_FALSE;
    }

    jsval vec[2] = { STRING_TO_JSVAL(str), v };
    JSAutoTempValueRooter tvr(cx, 2, vec);
    JSObject *array = JS_NewArrayObject(cx, 2, vec);
    if (!array) {
      return JS_FALSE;
    }

    *vp = OBJECT_TO_JSVAL(array);
  }

  JS_SetReservedSlot(cx, obj, 1, INT_TO_JSVAL(idx));
  return JS_TRUE;
}

static JSClass IteratorClass = {
  "XOW iterator", JSCLASS_HAS_RESERVED_SLOTS(3),
  JS_PropertyStub, JS_PropertyStub,
  JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub,
  JS_ConvertStub, IteratorFinalize,

  JSCLASS_NO_OPTIONAL_MEMBERS
};

// static
JSObject *
XPCWrapper::CreateIteratorObj(JSContext *cx, JSObject *tempWrapper,
                              JSObject *wrapperObj, JSObject *innerObj,
                              JSBool keysonly)
{
  // This is rather ugly: we want to use the trick seen in Enumerate,
  // where we use our wrapper's resolve hook to determine if we should
  // enumerate a given property. However, we don't want to pollute the
  // identifiers with a next method, so we create an object that
  // delegates (via the __proto__ link) to the wrapper.

  JSObject *iterObj = JS_NewObject(cx, &IteratorClass, tempWrapper, wrapperObj);
  if (!iterObj) {
    return nsnull;
  }

  JSAutoTempValueRooter tvr(cx, OBJECT_TO_JSVAL(iterObj));

  // Do this sooner rather than later to avoid complications in
  // IteratorFinalize.
  if (!JS_SetReservedSlot(cx, iterObj, 0, PRIVATE_TO_JSVAL(nsnull))) {
    return nsnull;
  }

  // Initialize iterObj.
  if (!JS_DefineFunction(cx, iterObj, "next", (JSNative)IteratorNext, 0,
                         JSFUN_FAST_NATIVE)) {
    return nsnull;
  }

  // Start enumerating over all of our properties.
  do {
    if (!XPCWrapper::Enumerate(cx, iterObj, innerObj)) {
      return nsnull;
    }
  } while ((innerObj = STOBJ_GET_PROTO(innerObj)) != nsnull);

  JSIdArray *ida = JS_Enumerate(cx, iterObj);
  if (!ida) {
    return nsnull;
  }

  if (!JS_SetReservedSlot(cx, iterObj, 0, PRIVATE_TO_JSVAL(ida)) ||
      !JS_SetReservedSlot(cx, iterObj, 1, JSVAL_ZERO) ||
      !JS_SetReservedSlot(cx, iterObj, 2, BOOLEAN_TO_JSVAL(keysonly))) {
    return nsnull;
  }

  if (!JS_SetPrototype(cx, iterObj, nsnull)) {
    return nsnull;
  }

  return iterObj;
}

// static
JSBool
XPCWrapper::AddProperty(JSContext *cx, JSObject *wrapperObj,
                        JSBool wantGetterSetter, JSObject *innerObj, jsval id,
                        jsval *vp)
{
  jsid interned_id;
  if (!::JS_ValueToId(cx, id, &interned_id)) {
    return JS_FALSE;
  }

  JSPropertyDescriptor desc;
  if (!GetPropertyAttrs(cx, wrapperObj, interned_id, JSRESOLVE_QUALIFIED,
                        wantGetterSetter, &desc)) {
    return JS_FALSE;
  }

  NS_ASSERTION(desc.obj == wrapperObj,
               "What weird wrapper are we using?");

  return JS_DefinePropertyById(cx, innerObj, interned_id, desc.value,
                               desc.getter, desc.setter, desc.attrs);
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

    // Note: v doesn't need to be rooted because it will be read out of a
    // rooted object's slots.
    jsval v = JSVAL_VOID;

    // Let our NewResolve hook figure out whether this id should be reflected.
    ok = JS_LookupPropertyWithFlagsById(cx, wrapperObj, ida->vector[i],
                                        JSRESOLVE_QUALIFIED, &pobj, &v);
    if (!ok) {
      break;
    }

    if (pobj && pobj != wrapperObj) {
      // If the resolution actually happened on a different object, define the
      // property here so that we're sure that enumeration picks it up.
      ok = JS_DefinePropertyById(cx, wrapperObj, ida->vector[i], JSVAL_VOID,
                                 nsnull, nsnull, JSPROP_ENUMERATE | JSPROP_SHARED);
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
                       JSBool wantDetails, JSObject *innerObj, jsval id,
                       uintN flags, JSObject **objp)
{
  jsid interned_id;
  if (!::JS_ValueToId(cx, id, &interned_id)) {
    return JS_FALSE;
  }

  JSPropertyDescriptor desc;
  if (!GetPropertyAttrs(cx, innerObj, interned_id, flags, wantDetails, &desc)) {
    return JS_FALSE;
  }

  if (!desc.obj) {
    // Nothing to define.
    return JS_TRUE;
  }

  desc.value = JSVAL_VOID;

  jsval oldFlags;
  if (!::JS_GetReservedSlot(cx, wrapperObj, sFlagsSlot, &oldFlags) ||
      !::JS_SetReservedSlot(cx, wrapperObj, sFlagsSlot,
                            INT_TO_JSVAL(JSVAL_TO_INT(oldFlags) |
                                         FLAG_RESOLVING))) {
    return JS_FALSE;
  }

  JSBool ok = JS_DefinePropertyById(cx, wrapperObj, interned_id, desc.value,
                                    desc.getter, desc.setter, desc.attrs);

  JS_SetReservedSlot(cx, wrapperObj, sFlagsSlot, oldFlags);

  if (ok) {
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
    if (!::JS_GetReservedSlot(cx, wrapperObj, sFlagsSlot, &oldFlags) ||
        !::JS_SetReservedSlot(cx, wrapperObj, sFlagsSlot,
                              INT_TO_JSVAL(JSVAL_TO_INT(oldFlags) |
                                           FLAG_RESOLVING))) {
      return JS_FALSE;
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

    if (!::JS_SetReservedSlot(cx, wrapperObj, sFlagsSlot, oldFlags)) {
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

      // The scriptable helper resolved this property to a *different* object.
      // We don't know what to do for now (this can't currently happen in
      // Mozilla) so throw.
      // I suspect that we'd need to redo the security check on the new object
      // (if it has a different class than the original object) and then call
      // ResolveNativeProperty with *that* as the inner object.
      return ThrowException(NS_ERROR_NOT_IMPLEMENTED, cx);
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
  if (!member) {
    // No member, nothing to resolve.

    return MaybePreserveWrapper(cx, wn, flags);
  }

  JSString *str = JSVAL_TO_STRING(id);
  if (!str) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  // Get (and perhaps lazily create) the member's value (commonly a
  // cloneable function).
  jsval v;
  uintN attrs = JSPROP_ENUMERATE;
  JSPropertyOp getter = nsnull;
  JSPropertyOp setter = nsnull;

  if (member->IsConstant()) {
    if (!member->GetConstantValue(ccx, iface, &v)) {
      return ThrowException(NS_ERROR_XPC_BAD_CONVERT_JS, cx);
    }
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

    jsval funval;
    if (!member->NewFunctionObject(ccx, iface, wrapper->GetFlatJSObject(),
                                   &funval)) {
      return ThrowException(NS_ERROR_XPC_BAD_CONVERT_JS, cx);
    }

    AUTO_MARK_JSVAL(ccx, funval);

#ifdef DEBUG_XPCNativeWrapper
    printf("Wrapping function object for %s\n",
           ::JS_GetStringBytes(JSVAL_TO_STRING(id)));
#endif

    if (!WrapFunction(cx, wrapperObj, JSVAL_TO_OBJECT(funval), &v,
                      isNativeWrapper)) {
      return JS_FALSE;
    }

    // Functions shouldn't have a getter or a setter. Without the wrappers,
    // they would live on the prototype (and call its getter), since we don't
    // have a prototype, and we need to avoid calling the scriptable helper's
    // GetProperty method for this property, stub out the getters and setters
    // explicitly.
    getter = setter = JS_PropertyStub;

    // Since the XPC_*_NewResolve functions ensure that the method's property
    // name is accessible, we set the eAllAccessSlot bit, which indicates to
    // XPC_NW_FunctionWrapper that the method is safe to unwrap and call, even
    // if XPCNativeWrapper::GetWrappedNative disagrees.
    JS_SetReservedSlot(cx, JSVAL_TO_OBJECT(v), eAllAccessSlot, JSVAL_TRUE);
  }

  // Make sure v doesn't go away while we mess with it.
  AUTO_MARK_JSVAL(ccx, v);

  // XPCNativeWrapper doesn't need to do this.
  jsval oldFlags;
  if (!isNativeWrapper &&
      (!::JS_GetReservedSlot(cx, wrapperObj, sFlagsSlot, &oldFlags) ||
       !::JS_SetReservedSlot(cx, wrapperObj, sFlagsSlot,
                             INT_TO_JSVAL(JSVAL_TO_INT(oldFlags) |
                                          FLAG_RESOLVING)))) {
    return JS_FALSE;
  }

  if (!::JS_DefineUCProperty(cx, wrapperObj, ::JS_GetStringChars(str),
                            ::JS_GetStringLength(str), v, getter, setter,
                            attrs)) {
    return JS_FALSE;
  }

  if (!isNativeWrapper &&
      !::JS_SetReservedSlot(cx, wrapperObj, sFlagsSlot, oldFlags)) {
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
  if (!member) {
    // No member, no IDL property to expose.

    return JS_TRUE;
  }

  if (member->IsConstant()) {
    jsval memberval;
    if (!member->GetConstantValue(ccx, iface, &memberval)) {
      return ThrowException(NS_ERROR_XPC_BAD_CONVERT_JS, cx);
    }

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

  jsval funval;
  if (!member->NewFunctionObject(ccx, iface, wrapper->GetFlatJSObject(),
                                 &funval)) {
    return ThrowException(NS_ERROR_XPC_BAD_CONVERT_JS, cx);
  }

  AUTO_MARK_JSVAL(ccx, funval);

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
  if (!::JS_CallFunctionValue(cx, wrapper->GetFlatJSObject(), funval, argc,
                              argv, &v)) {
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
  XPCJSRuntime *rt = nsXPConnect::GetRuntimeInstance();

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
  JSString* str = nsnull;

  // First, try to see if the object declares a toString in its IDL. If it does,
  // then we need to defer to that.
  if (iface && member && member->IsMethod()) {
    jsval toStringVal;
    if (!member->NewFunctionObject(ccx, iface, wn_obj, &toStringVal)) {
      return JS_FALSE;
    }

    // Defer to the IDL-declared toString.

    AUTO_MARK_JSVAL(ccx, toStringVal);

    jsval v;
    if (!::JS_CallFunctionValue(cx, wn_obj, toStringVal, argc, argv, &v)) {
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

// static
JSBool
XPCWrapper::GetPropertyAttrs(JSContext *cx, JSObject *obj, jsid interned_id,
                             uintN flags, JSBool wantDetails,
                             JSPropertyDescriptor *desc)
{
  if (!JS_GetPropertyDescriptorById(cx, obj, interned_id, flags, desc)) {
    return JS_FALSE;
  }

  const uintN interesting_attrs = wantDetails
                                  ? (JSPROP_ENUMERATE |
                                     JSPROP_READONLY  |
                                     JSPROP_PERMANENT |
                                     JSPROP_SHARED    |
                                     JSPROP_GETTER    |
                                     JSPROP_SETTER)
                                  : JSPROP_ENUMERATE;
  desc->attrs &= interesting_attrs;

  if (wantDetails) {
    // JS_GetPropertyDescriptorById returns non scripted getters and setters.
    // If wantDetails is true, then we need to censor them.
    if (!(desc->attrs & JSPROP_GETTER)) {
      desc->getter = nsnull;
    }
    if (!(desc->attrs & JSPROP_SETTER)) {
      desc->setter = nsnull;
    }
  } else {
    // Clear out all but attrs and obj.
    desc->getter = desc->setter = nsnull;
    desc->value = JSVAL_VOID;
  }

  return JS_TRUE;
}
