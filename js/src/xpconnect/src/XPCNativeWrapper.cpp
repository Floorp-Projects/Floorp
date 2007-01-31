/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
#include "XPCNativeWrapper.h"
#include "jsdbgapi.h"

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_Enumerate(JSContext *cx, JSObject *obj);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                  JSObject **objp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

JS_STATIC_DLL_CALLBACK(void)
XPC_NW_Finalize(JSContext *cx, JSObject *obj);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_CheckAccess(JSContext *cx, JSObject *obj, jsval id,
                   JSAccessMode mode, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
            jsval *rval);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

JS_STATIC_DLL_CALLBACK(uint32)
XPC_NW_Mark(JSContext *cx, JSObject *obj, void *arg);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

static JSBool
RewrapIfDeepWrapper(JSContext *cx, JSObject *obj, jsval v, jsval *rval);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_FunctionWrapper(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                       jsval *rval);

// JS class for XPCNativeWrapper (and this doubles as the constructor
// for XPCNativeWrapper for the moment too...)

JSExtendedClass XPCNativeWrapper::sXPC_NW_JSClass = {
  // JSClass (JSExtendedClass.base) initialization
  { "XPCNativeWrapper",
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS |
    // Our one reserved slot holds a jsint of flag bits
    JSCLASS_NEW_RESOLVE | JSCLASS_HAS_RESERVED_SLOTS(1) |
    JSCLASS_IS_EXTENDED,
    XPC_NW_AddProperty, XPC_NW_DelProperty,
    XPC_NW_GetProperty, XPC_NW_SetProperty,
    XPC_NW_Enumerate,   (JSResolveOp)XPC_NW_NewResolve,
    XPC_NW_Convert,     XPC_NW_Finalize,
    nsnull,             XPC_NW_CheckAccess,
    XPC_NW_Call,        XPC_NW_Construct,
    nsnull,             XPC_NW_HasInstance,
    XPC_NW_Mark,        nsnull
  },
  // JSExtendedClass initialization
  XPC_NW_Equality
};

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

// If one of our class hooks is ever called from a non-system script, bypass
// the hook by calling the same hook on our wrapped native, with obj reset to
// the wrapped native's flat JSObject, so the hook and args macro parameters
// can be simply:
//
//      convert, (cx, obj, type, vp)
//
// in the call from XPC_NW_Convert, for example.

#define XPC_NW_CALL_HOOK(cx, obj, hook, args)                                 \
  return JS_GET_CLASS(cx, obj)->hook args;

#define XPC_NW_CAST_HOOK(cx, obj, type, hook, args)                           \
  return ((type) JS_GET_CLASS(cx, obj)->hook) args;

static JSBool
ShouldBypassNativeWrapper(JSContext *cx, JSObject *obj)
{
  NS_ASSERTION(XPCNativeWrapper::IsNativeWrapper(cx, obj),
               "Unexpected object");
  jsval flags;

  ::JS_GetReservedSlot(cx, obj, 0, &flags);
  if (HAS_FLAGS(flags, FLAG_EXPLICIT))
    return JS_FALSE;

  // Check what the script calling us looks like
  JSScript *script = nsnull;
  JSStackFrame *fp = cx->fp;
  while(!script && fp) {
    script = fp->script;
    fp = fp->down;
  }

  // If there's no script, bypass for now because that's what the old code did.
  // XXX FIXME: bug 341477 covers figuring out what we _should_ do.
  return !script || !(::JS_GetScriptFilenameFlags(script) & JSFILENAME_SYSTEM);
}

#define XPC_NW_BYPASS_BASE(cx, obj, code)                                     \
  JS_BEGIN_MACRO                                                              \
    if (ShouldBypassNativeWrapper(cx, obj)) {                                 \
      XPCWrappedNative *wn_ = XPCNativeWrapper::GetWrappedNative(cx, obj);    \
      if (!wn_) {                                                             \
        return JS_TRUE;                                                       \
      }                                                                       \
      obj = wn_->GetFlatJSObject();                                           \
      code                                                                    \
    }                                                                         \
  JS_END_MACRO

#define XPC_NW_BYPASS(cx, obj, hook, args)                                    \
  XPC_NW_BYPASS_BASE(cx, obj, XPC_NW_CALL_HOOK(cx, obj, hook, args))

#define XPC_NW_BYPASS_CAST(cx, obj, type, hook, args)                         \
  XPC_NW_BYPASS_BASE(cx, obj, XPC_NW_CAST_HOOK(cx, obj, type, hook, args))

#define XPC_NW_BYPASS_TEST(cx, obj, hook, args)                               \
  XPC_NW_BYPASS_BASE(cx, obj,                                                 \
    JSClass *clasp_ = JS_GET_CLASS(cx, obj);                                  \
    return !clasp_->hook || clasp_->hook args;                                \
  )

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval);

static JSFunctionSpec sXPC_NW_JSClass_methods[] = {
  {"toString", XPC_NW_toString, 0, 0, 0},
  {0, 0, 0, 0, 0}
};

JS_STATIC_DLL_CALLBACK(JSBool)
XPCNativeWrapperCtor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                     jsval *rval);

static inline
JSBool
ThrowException(nsresult ex, JSContext *cx)
{
  XPCThrower::Throw(ex, cx);

  return JS_FALSE;
}

static JSBool
WrapFunction(JSContext* cx, JSObject* funobj, jsval *rval)
{
  // If funobj is already a wrapped function, just return it.
  if (JS_GetFunctionNative(cx,
                           JS_ValueToFunction(cx, OBJECT_TO_JSVAL(funobj))) ==
      XPC_NW_FunctionWrapper) {
    *rval = OBJECT_TO_JSVAL(funobj);
    return JS_TRUE;
  }
  
  // Create a new function that'll call our given function.  This new
  // function's parent will be the original function and that's how we
  // get the right thing to call when this function is called.
  JSFunction *funWrapper =
    ::JS_NewFunction(cx, XPC_NW_FunctionWrapper, 0, 0, funobj,
                     "XPCNativeWrapper function wrapper");
  if (!funWrapper) {
    return JS_FALSE;
  }

  *rval = OBJECT_TO_JSVAL(::JS_GetFunctionObject(funWrapper));
  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  jsval flags;
  ::JS_GetReservedSlot(cx, obj, 0, &flags);
  if (!HAS_FLAGS(flags, FLAG_RESOLVING)) {
    return JS_TRUE;
  }

  // Note: no need to protect *vp from GC here, since it's already in the slot
  // on |obj|.
  return RewrapIfDeepWrapper(cx, obj, *vp, vp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  XPC_NW_BYPASS_BASE(cx, obj,
    // We're being notified of a delete operation on id in this
    // XPCNativeWrapper, so forward to the right high-level hook,
    // OBJ_DELETE_PROPERTY, on the XPCWrappedNative's object.
    {
      jsid interned_id;

      if (!::JS_ValueToId(cx, id, &interned_id)) {
        return JS_FALSE;
      }

      return OBJ_DELETE_PROPERTY(cx, obj, interned_id, vp);
    }
  );

  return ThrowException(NS_ERROR_XPC_SECURITY_MANAGER_VETO, cx);
}

static JSBool
RewrapIfDeepWrapper(JSContext *cx, JSObject *obj, jsval v, jsval *rval)
{
  NS_ASSERTION(XPCNativeWrapper::IsNativeWrapper(cx, obj),
               "Unexpected object");

  JSBool primitive = JSVAL_IS_PRIMITIVE(v);
  JSObject* nativeObj = primitive ? nsnull : JSVAL_TO_OBJECT(v);
  
  // We always want to wrap function objects, no matter whether we're deep.
  if (!primitive && JS_ObjectIsFunction(cx, nativeObj)) {
    return WrapFunction(cx, nativeObj, rval);
  }

  jsval flags;
  ::JS_GetReservedSlot(cx, obj, 0, &flags);

  // Re-wrap non-primitive values if this is a deep wrapper, i.e.
  // if (HAS_FLAGS(flags, FLAG_DEEP).
  if (HAS_FLAGS(flags, FLAG_DEEP) && !primitive) {

    XPCWrappedNative* wrappedNative =
      XPCWrappedNative::GetWrappedNativeOfJSObject(cx, nativeObj);
    if (!wrappedNative) {
      // Not something we can protect... just make it JSVAL_NULL
      *rval = JSVAL_NULL;
      return JS_TRUE;
    }

    if (HAS_FLAGS(flags, FLAG_EXPLICIT)) {
#ifdef DEBUG_XPCNativeWrapper
      printf("Rewrapping for deep explicit wrapper\n");
#endif
      if (wrappedNative == XPCNativeWrapper::GetWrappedNative(cx, obj)) {
        // Already wrapped, return the wrapper.
        *rval = OBJECT_TO_JSVAL(obj);
        return JS_TRUE;
      }

      // |obj| is an explicit deep wrapper.  We want to construct another
      // explicit deep wrapper for |v|.  Just call XPCNativeWrapperCtor by hand
      // (passing null as the pre-created object it doesn't use anyway) so we
      // don't have to create an object we never use.

      return XPCNativeWrapperCtor(cx, nsnull, 1, &v, rval);
    }
    
#ifdef DEBUG_XPCNativeWrapper
    printf("Rewrapping for deep implicit wrapper\n");
#endif
    // Just using GetNewOrUsed on the return value of
    // GetWrappedNativeOfJSObject will give the right thing -- the unique deep
    // implicit wrapper associated with wrappedNative.
    JSObject* wrapperObj = XPCNativeWrapper::GetNewOrUsed(cx, wrappedNative);
    if (!wrapperObj) {
      return JS_FALSE;
    }

    *rval = OBJECT_TO_JSVAL(wrapperObj);
  } else {
    *rval = v;
  }

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_FunctionWrapper(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                       jsval *rval)
{
  JSObject *funObj = JSVAL_TO_OBJECT(argv[-2]);
  if (!::JS_ObjectIsFunction(cx, funObj)) {
    obj = nsnull;
  }

  while (obj && !XPCNativeWrapper::IsNativeWrapper(cx, obj)) {
    obj = ::JS_GetPrototype(cx, obj);
  }

  if (!obj) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  // The real method we're going to call is the parent of this
  // function's JSObject.
  JSObject *methodToCallObj = ::JS_GetParent(cx, funObj);
  XPCWrappedNative *wrappedNative =
    XPCNativeWrapper::GetWrappedNative(cx, obj);

  if (!::JS_ObjectIsFunction(cx, methodToCallObj) || !wrappedNative) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  jsval v;
  if (!::JS_CallFunctionValue(cx, wrappedNative->GetFlatJSObject(),
                              OBJECT_TO_JSVAL(methodToCallObj), argc, argv,
                              &v)) {
    return JS_FALSE;
  }

  XPCCallContext ccx(JS_CALLER, cx, obj);

  // Make sure v doesn't get collected while we're re-wrapping it.
  AUTO_MARK_JSVAL(ccx, v);

  return RewrapIfDeepWrapper(cx, obj, v, rval);
}

static JSBool
XPC_NW_GetOrSetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp,
                        JSBool aIsSet)
{
  // We don't deal with the following properties here.
  if (id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_PROTOTYPE) ||
      id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
    return JS_TRUE;
  }

  while (!XPCNativeWrapper::IsNativeWrapper(cx, obj)) {
    obj = ::JS_GetPrototype(cx, obj);
    if (!obj) {
      return ThrowException(NS_ERROR_UNEXPECTED, cx);
    }
  }

  XPCWrappedNative *wrappedNative =
    XPCNativeWrapper::GetWrappedNative(cx, obj);

  if (!wrappedNative) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  JSObject *nativeObj = wrappedNative->GetFlatJSObject();

  // We can't use XPC_NW_BYPASS here, because we need to do a full
  // OBJ_SET_PROPERTY or OBJ_GET_PROPERTY on the wrapped native's
  // object, in order to trigger reflection done by the underlying
  // OBJ_LOOKUP_PROPERTY done by SET and GET.

  if (ShouldBypassNativeWrapper(cx, obj)) {
    jsid interned_id;

    if (!::JS_ValueToId(cx, id, &interned_id)) {
      return JS_FALSE;
    }

    return aIsSet
           ? OBJ_SET_PROPERTY(cx, nativeObj, interned_id, vp)
           : OBJ_GET_PROPERTY(cx, nativeObj, interned_id, vp);
  }

  if (!aIsSet &&
      id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_WRAPPED_JSOBJECT)) {
    // Return a safe wrapper for the underlying native object, the
    // XPConnect wrapped object that this additional wrapper layer
    // wraps.

    jsval nativeVal = OBJECT_TO_JSVAL(nativeObj);
    return XPC_SJOW_Construct(cx, nsnull, 1, &nativeVal, vp);
  }

  // This will do verification and the method lookup for us.
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

      return RewrapIfDeepWrapper(cx, obj, v, vp);
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

    return RewrapIfDeepWrapper(cx, obj, v, vp);
  }
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return XPC_NW_GetOrSetProperty(cx, obj, id, vp, PR_FALSE);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return XPC_NW_GetOrSetProperty(cx, obj, id, vp, PR_TRUE);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_Enumerate(JSContext *cx, JSObject *obj)
{
  // We are being notified of a for-in loop or similar operation on this
  // XPCNativeWrapper, so forward to the correct high-level object hook,
  // OBJ_ENUMERATE on the XPCWrappedNative's object, called via the
  // JS_Enumerate API.  Then reflect properties named by the enumerated
  // identifiers from the wrapped native to the native wrapper.

  XPCWrappedNative *wn = XPCNativeWrapper::GetWrappedNative(cx, obj);
  if (!wn) {
    return JS_TRUE;
  }

  JSIdArray *ida = JS_Enumerate(cx, wn->GetFlatJSObject());
  if (!ida) {
    return JS_FALSE;
  }

  JSBool ok = JS_TRUE;

  for (jsint i = 0, n = ida->length; i < n; i++) {
    JSObject *pobj;
    JSProperty *prop;

    // Let OBJ_LOOKUP_PROPERTY, in particular XPC_NW_NewResolve, figure
    // out whether this id should be bypassed or reflected.
    ok = OBJ_LOOKUP_PROPERTY(cx, obj, ida->vector[i], &pobj, &prop);
    if (!ok) {
      break;
    }
    if (prop) {
      OBJ_DROP_PROPERTY(cx, pobj, prop);
    }
  }

  JS_DestroyIdArray(cx, ida);
  return ok;
}

static
JSBool MaybePreserveWrapper(JSContext* cx, XPCWrappedNative *wn, uintN flags)
{
  if ((flags & JSRESOLVE_ASSIGNING) &&
      (::JS_GetOptions(cx) & JSOPTION_PRIVATE_IS_NSISUPPORTS)) {
    nsCOMPtr<nsIXPCScriptNotify> scriptNotify = 
      do_QueryInterface(NS_STATIC_CAST(nsISupports*,
                                       JS_GetContextPrivate(cx)));
    if (scriptNotify) {
      return NS_SUCCEEDED(scriptNotify->PreserveWrapper(wn));
    }
  }
  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                  JSObject **objp)
{
  // No need to preserve on sets of wrappedJSObject or toString, since callers
  // couldn't get at those values anyway.  Also, we always deal with
  // wrappedJSObject and toString before looking at our scriptable hooks, so no
  // need to mess with our flags yet.
  if (id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_WRAPPED_JSOBJECT) ||
      id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
    return JS_TRUE;
  }

  // We can't use XPC_NW_BYPASS here, because we need to do a full
  // OBJ_LOOKUP_PROPERTY on the wrapped native's object, in order to
  // trigger reflection along the wrapped native prototype chain.
  // All we need to do is define the property in obj if it exists in
  // the wrapped native's object.

  if (ShouldBypassNativeWrapper(cx, obj)) {
    XPCWrappedNative *wn = XPCNativeWrapper::GetWrappedNative(cx, obj);
    if (!wn) {
      return JS_TRUE;
    }

    JSAutoRequest ar(cx);

    jsid interned_id;
    JSObject *pobj;
    JSProperty *prop;

    if (!::JS_ValueToId(cx, id, &interned_id) ||
        !OBJ_LOOKUP_PROPERTY(cx, wn->GetFlatJSObject(), interned_id,
                             &pobj, &prop)) {
      return JS_FALSE;
    }

    if (prop) {
      OBJ_DROP_PROPERTY(cx, pobj, prop);

      if (!OBJ_DEFINE_PROPERTY(cx, obj, interned_id, JSVAL_VOID,
                               nsnull, nsnull, 0, nsnull)) {
        return JS_FALSE;
      }

      *objp = obj;
    }
    return JS_TRUE;
  }

  while (!XPCNativeWrapper::IsNativeWrapper(cx, obj)) {
    obj = ::JS_GetPrototype(cx, obj);
    if (!obj) {
      return ThrowException(NS_ERROR_UNEXPECTED, cx);
    }
  }

  XPCWrappedNative *wrappedNative =
    XPCNativeWrapper::GetWrappedNative(cx, obj);

  if (!wrappedNative) {
    // No wrapped native, no properties.

    return JS_TRUE;
  }

  JSObject *nativeObj = wrappedNative->GetFlatJSObject();

  // This will do verification and the method lookup for us.
  XPCCallContext ccx(JS_CALLER, cx, nativeObj, nsnull, id);

  // For "constructor" we don't want to call into the resolve hooks on the
  // wrapped native, since that would give the wrong constructor.
  if (NATIVE_HAS_FLAG(wrappedNative, WantNewResolve) &&
      id != GetRTStringByIndex(cx, XPCJSRuntime::IDX_CONSTRUCTOR)) {

    // Mark ourselves as resolving so our AddProperty hook can do the
    // right thing here.
    jsval oldFlags;
    ::JS_GetReservedSlot(cx, obj, 0, &oldFlags);
    if (!::JS_SetReservedSlot(cx, obj, 0,
                              INT_TO_JSVAL(JSVAL_TO_INT(oldFlags) |
                                           FLAG_RESOLVING))) {
      return JS_FALSE;
    }        
    
    XPCWrappedNative* oldResolvingWrapper = nsnull;
    JSBool allowPropMods =
      NATIVE_HAS_FLAG(wrappedNative, AllowPropModsDuringResolve);
    if (allowPropMods) {
      oldResolvingWrapper = ccx.SetResolvingWrapper(wrappedNative);
    }
      
    JSBool retval = JS_TRUE;
    JSObject* newObj = nsnull;
    nsresult rv = wrappedNative->GetScriptableInfo()->
      GetCallback()->NewResolve(wrappedNative, cx, obj, id, flags,
                                &newObj, &retval);

    if (allowPropMods) {
      ccx.SetResolvingWrapper(oldResolvingWrapper);
    }

    if (!::JS_SetReservedSlot(cx, obj, 0, oldFlags)) {
      return JS_FALSE;
    }
    
    if (NS_FAILED(rv)) {
      return ThrowException(rv, cx);
    }

    if (newObj) {
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
  }

  if (!JSVAL_IS_STRING(id)) {
    // A non-string id is being resolved. Won't be found here, return
    // early.

    return MaybePreserveWrapper(cx, wrappedNative, flags);
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
    // No interface, nothing to resolve.

    return MaybePreserveWrapper(cx, wrappedNative, flags);
  }

  // did we find a method/attribute by that name?
  XPCNativeMember* member = ccx.GetMember();
  NS_ASSERTION(member, "not doing IDispatch, how'd this happen?");
  if (!member) {
    // No member, nothing to resolve.

    return MaybePreserveWrapper(cx, wrappedNative, flags);
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

    if (!WrapFunction(cx, funobj, &v)) {
      return JS_FALSE;
    }
  }

  if (!::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                            ::JS_GetStringLength(str), v, nsnull, nsnull,
                            attrs)) {
    return JS_FALSE;
  }

  *objp = obj;

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
  XPC_NW_BYPASS(cx, obj, convert, (cx, obj, type, vp));

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(void)
XPC_NW_Finalize(JSContext *cx, JSObject *obj)
{
  // We must not use obj's private data here since it's likely that it
  // has already been finalized.
  XPCJSRuntime *rt = nsXPConnect::GetRuntime();

  {
    // scoped lock
    XPCAutoLock lock(rt->GetMapLock());
    rt->GetExplicitNativeWrapperMap()->Remove(obj);
  }
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_CheckAccess(JSContext *cx, JSObject *obj, jsval id,
                   JSAccessMode mode, jsval *vp)
{
  // Prevent setting __proto__ on an XPCNativeWrapper
  if ((mode & JSACC_WATCH) == JSACC_PROTO && (mode & JSACC_WRITE)) {
    return ThrowException(NS_ERROR_XPC_SECURITY_MANAGER_VETO, cx);
  }

  // Forward to the checkObjectAccess hook in the JSContext, if any.
  if (cx->runtime->checkObjectAccess &&
      !cx->runtime->checkObjectAccess(cx, obj, id, mode, vp)) {
    return JS_FALSE;
  }

  XPCWrappedNative *wrappedNative =
    XPCNativeWrapper::GetWrappedNative(cx, obj);
  if (!wrappedNative) {
    return JS_TRUE;
  }

  JSObject *wrapperJSObject = wrappedNative->GetFlatJSObject();

  JSClass *clazz = JS_GET_CLASS(cx, wrapperJSObject);
  return !clazz->checkAccess ||
    clazz->checkAccess(cx, wrapperJSObject, id, mode, vp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  if (!XPCNativeWrapper::IsNativeWrapper(cx, obj)) {
    // If obj is not an XPCNativeWrapper, then someone's probably trying to call
    // our prototype (i.e., XPCNativeWrapper.prototype()). In this case, it is
    // safe to simply ignore the call, since that's what would happen anyway.

#ifdef DEBUG
    if (!JS_ObjectIsFunction(cx, obj)) {
      NS_WARNING("Ignoring a call for a weird object");
    }
#endif
    return JS_TRUE;
  }

  XPC_NW_BYPASS_TEST(cx, obj, call, (cx, obj, argc, argv, rval));

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
  // The object given to us by the JS engine is actually a stub object (the
  // "new" object). This isn't any help to us, so instead use the function
  // object of the constructor that we're calling (which is the native
  // wrapper).
  obj = JSVAL_TO_OBJECT(argv[-2]);

  XPC_NW_BYPASS_TEST(cx, obj, construct, (cx, obj, argc, argv, rval));

  XPCWrappedNative *wrappedNative =
    XPCNativeWrapper::GetWrappedNative(cx, obj);
  if (!wrappedNative) {
    return JS_TRUE;
  }

  JSBool retval = JS_TRUE;

  if (!NATIVE_HAS_FLAG(wrappedNative, WantConstruct)) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  nsresult rv = wrappedNative->GetScriptableInfo()->
    GetCallback()->Construct(wrappedNative, cx, obj, argc, argv, rval,
                             &retval);
  if (NS_FAILED(rv)) {
    return ThrowException(rv, cx);
  }

  if (!retval) {
    return JS_FALSE;
  }

  if (JSVAL_IS_PRIMITIVE(*rval)) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  return RewrapIfDeepWrapper(cx, obj, *rval, rval);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
  XPC_NW_BYPASS_TEST(cx, obj, hasInstance, (cx, obj, v, bp));

  return JS_TRUE;
}

static JSBool
MirrorWrappedNativeParent(JSContext *cx, XPCWrappedNative *wrapper,
                          JSObject **result)
{
  JSObject *wn_parent = ::JS_GetParent(cx, wrapper->GetFlatJSObject());
  if (!wn_parent) {
    *result = nsnull;
  } else {
    XPCWrappedNative *parent_wrapper =
      XPCWrappedNative::GetWrappedNativeOfJSObject(cx, wn_parent);

    *result = XPCNativeWrapper::GetNewOrUsed(cx, parent_wrapper);
    if (!*result)
      return JS_FALSE;
  }
  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPCNativeWrapperCtor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                     jsval *rval)
{
  if (argc < 1) {
    return ThrowException(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx);
  }

  // |obj| almost always has the wrong proto and parent so we have to create
  // our own object anyway.  Set |obj| to null so we don't use it by accident.
  obj = nsnull;
  
  jsval native = argv[0];

  if (JSVAL_IS_PRIMITIVE(native)) {
    return ThrowException(NS_ERROR_XPC_BAD_CONVERT_JS, cx);
  }

  JSObject *nativeObj = JSVAL_TO_OBJECT(native);

  XPCWrappedNative *wrappedNative;

  if (XPCNativeWrapper::IsNativeWrapper(cx, nativeObj)) {
    // We're asked to wrap an already wrapped object. Re-wrap the
    // object wrapped by the given wrapper.

#ifdef DEBUG_XPCNativeWrapper
    printf("Wrapping already wrapped object\n");
#endif

    wrappedNative = XPCNativeWrapper::GetWrappedNative(cx, nativeObj);

    if (!wrappedNative) {
      return ThrowException(NS_ERROR_INVALID_ARG, cx);
    }

    nativeObj = wrappedNative->GetFlatJSObject();
    native = OBJECT_TO_JSVAL(nativeObj);
  } else {
    wrappedNative
      = XPCWrappedNative::GetWrappedNativeOfJSObject(cx, nativeObj);

    if (!wrappedNative) {
      return ThrowException(NS_ERROR_INVALID_ARG, cx);
    }

    // Prevent wrapping a double-wrapped JS object in an
    // XPCNativeWrapper!
    nsCOMPtr<nsIXPConnectWrappedJS> xpcwrappedjs =
      do_QueryWrappedNative(wrappedNative);

    if (xpcwrappedjs) {
      return ThrowException(NS_ERROR_INVALID_ARG, cx);
    }
  }

  JSObject *wrapperObj;

  // Don't use the object the JS engine created for us, it is in most
  // cases incorectly parented and has a proto from the wrong scope.
#ifdef DEBUG_XPCNativeWrapper
  printf("Creating new JSObject\n");
#endif
  wrapperObj = ::JS_NewObject(cx, XPCNativeWrapper::GetJSClass(), nsnull,
                              wrappedNative->GetScope()->GetGlobalJSObject());

  if (!wrapperObj) {
    // JS_NewObject already threw (or reported OOM).
    return JS_FALSE;
  }

  PRBool hasStringArgs = PR_FALSE;
  for (uintN i = 1; i < argc; ++i) {
    if (!JSVAL_IS_STRING(argv[i])) {
      hasStringArgs = PR_FALSE;

      break;
    }

    if (i == 1) {
#ifdef DEBUG_XPCNativeWrapper
      printf("Constructing XPCNativeWrapper() with string args\n");
#endif
    }

#ifdef DEBUG_XPCNativeWrapper
    printf("  %s\n", ::JS_GetStringBytes(JSVAL_TO_STRING(argv[i])));
#endif

    hasStringArgs = PR_TRUE;
  }

  JSBool isDeep = !hasStringArgs;
  jsuint flags = isDeep ? FLAG_DEEP | FLAG_EXPLICIT : FLAG_EXPLICIT;
  if (!::JS_SetReservedSlot(cx, wrapperObj, 0, INT_TO_JSVAL(flags))) {
    return JS_FALSE;
  }

  JSObject *parent = nsnull;

  if (isDeep) {
    // Make sure wrapperObj doesn't get collected while we're wrapping
    // parents for it.
    ::JS_LockGCThing(cx, wrapperObj);

    // A deep XPCNativeWrapper has a __parent__ chain that mirrors its
    // XPCWrappedNative's chain.
    if (!MirrorWrappedNativeParent(cx, wrappedNative, &parent))
      return JS_FALSE;

    ::JS_UnlockGCThing(cx, wrapperObj);

    if (argc == 2 && !JSVAL_IS_PRIMITIVE(argv[1])) {
      // An object was passed as the second argument to the
      // constructor. In this case we check that the object we're
      // wrapping is an instance of the assumed constructor that we
      // got. If not, throw an exception.
      JSBool hasInstance;
      if (!::JS_HasInstance(cx, JSVAL_TO_OBJECT(argv[1]), native,
                            &hasInstance)) {
        return ThrowException(NS_ERROR_UNEXPECTED, cx);
      }

      if (!hasInstance) {
        return ThrowException(NS_ERROR_INVALID_ARG, cx);
      }
    }
  }

  if (!parent) {
    parent = wrappedNative->GetScope()->GetGlobalJSObject();
  }
    
  if (!::JS_SetParent(cx, wrapperObj, parent))
    return JS_FALSE;

  // Set the XPCWrappedNative as private data in the native wrapper.
  if (!::JS_SetPrivate(cx, wrapperObj, wrappedNative)) {
    return JS_FALSE;
  }

#if defined(DEBUG_XPCNativeWrapper) || defined(DEBUG_xpc_leaks)
  {
    XPCCallContext ccx(JS_CALLER, cx);

    // Keep wrapperObj alive while we mess with strings
    AUTO_MARK_JSVAL(ccx, OBJECT_TO_JSVAL(wrapperObj));

    char *s = wrappedNative->ToString(ccx);
    printf("Created new XPCNativeWrapper %p for wrapped native %s\n",
           (void*)wrapperObj, s);
    if (s)
      JS_smprintf_free(s);
  }
#endif
  
  *rval = OBJECT_TO_JSVAL(wrapperObj);

  {
    XPCJSRuntime *rt = wrappedNative->GetRuntime();

    // scoped lock
    XPCAutoLock lock(rt->GetMapLock());
    rt->GetExplicitNativeWrapperMap()->Add(wrapperObj);
  }

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(uint32)
XPC_NW_Mark(JSContext *cx, JSObject *obj, void *arg)
{
  XPCWrappedNative *wrappedNative =
    XPCNativeWrapper::GetWrappedNative(cx, obj);

  if (wrappedNative && wrappedNative->IsValid()) {
    ::JS_MarkGCThing(cx, wrappedNative->GetFlatJSObject(),
                     "XPCNativeWrapper wrapped native", arg);
  }

  return 0;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
  NS_ASSERTION(XPCNativeWrapper::IsNativeWrapper(cx, obj),
               "Uh, we should only ever be called for XPCNativeWrapper "
               "objects!");

  if (JSVAL_IS_PRIMITIVE(v)) {
    *bp = JS_FALSE;

    return JS_TRUE;
  }

  XPCWrappedNative *wrappedNative =
    XPCNativeWrapper::GetWrappedNative(cx, obj);

  if (wrappedNative && wrappedNative->IsValid() &&
      NATIVE_HAS_FLAG(wrappedNative, WantEquality)) {
    // Forward the call to the wrapped native's Equality() hook.
    nsresult rv = wrappedNative->GetScriptableCallback()->
      Equality(wrappedNative, cx, obj, v, bp);

    if (NS_FAILED(rv)) {
      return ThrowException(rv, cx);
    }
  } else {
    JSObject *other = JSVAL_TO_OBJECT(v);

    *bp = (obj == other ||
           XPC_GetIdentityObject(cx, obj) == XPC_GetIdentityObject(cx, other));
  }

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
  while (!XPCNativeWrapper::IsNativeWrapper(cx, obj)) {
    obj = ::JS_GetPrototype(cx, obj);
    if (!obj) {
      return ThrowException(NS_ERROR_UNEXPECTED, cx);
    }
  }

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

  XPCWrappedNative *wrappedNative =
    XPCNativeWrapper::GetWrappedNative(cx, obj);

  if (!wrappedNative) {
    // toString() called on XPCNativeWrapper.prototype
    NS_NAMED_LITERAL_STRING(protoString, "[object XPCNativeWrapper]");
    JSString *str =
      ::JS_NewUCStringCopyN(cx, NS_REINTERPRET_CAST(const jschar*,
                                                    protoString.get()),
                            protoString.Length());
    NS_ENSURE_TRUE(str, JS_FALSE);
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
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

    nsAutoString resultString;
    resultString.AppendLiteral("[object XPCNativeWrapper");

    char *wrapperStr = wrappedNative->ToString(ccx);
    if (!wrapperStr) {
      return JS_FALSE;
    }

    resultString.Append(' ');
    resultString.AppendASCII(wrapperStr);
    JS_smprintf_free(wrapperStr);

    resultString.Append(']');

    str = ::JS_NewUCStringCopyN(cx, NS_REINTERPRET_CAST(const jschar *,
                                                        resultString.get()),
                                resultString.Length());
  }

  NS_ENSURE_TRUE(str, JS_FALSE);

  *rval = STRING_TO_JSVAL(str);
  return JS_TRUE;
}

// static
PRBool
XPCNativeWrapper::AttachNewConstructorObject(XPCCallContext &ccx,
                                             JSObject *aGlobalObject)
{
  JSObject *class_obj =
    ::JS_InitClass(ccx, aGlobalObject, nsnull, &sXPC_NW_JSClass.base,
                   XPCNativeWrapperCtor, 0, nsnull, sXPC_NW_JSClass_methods,
                   nsnull, nsnull);
  if (!class_obj) {
    NS_WARNING("can't initialize the XPCNativeWrapper class");
    return PR_FALSE;
  }
  
  // Make sure our prototype chain is empty and that people can't mess
  // with XPCNativeWrapper.prototype.
  ::JS_SetPrototype(ccx, class_obj, nsnull);
  if (!::JS_SealObject(ccx, class_obj, JS_FALSE)) {
    NS_WARNING("Failed to seal XPCNativeWrapper.prototype");
    return PR_FALSE;
  }

  JSBool found;
  return ::JS_SetPropertyAttributes(ccx, aGlobalObject,
                                    sXPC_NW_JSClass.base.name,
                                    JSPROP_READONLY | JSPROP_PERMANENT,
                                    &found);
}

// static
JSObject *
XPCNativeWrapper::GetNewOrUsed(JSContext *cx, XPCWrappedNative *wrapper)
{
  // Prevent wrapping a double-wrapped JS object in an
  // XPCNativeWrapper!
  nsCOMPtr<nsIXPConnectWrappedJS> xpcwrappedjs(do_QueryWrappedNative(wrapper));

  if (xpcwrappedjs) {
    XPCThrower::Throw(NS_ERROR_INVALID_ARG, cx);

    return nsnull;
  }

  JSObject *obj = wrapper->GetNativeWrapper();
  if (obj) {
    return obj;
  }

  JSObject *nw_parent;
  if (!MirrorWrappedNativeParent(cx, wrapper, &nw_parent)) {
    return nsnull;
  }

  PRBool lock;

  if (!nw_parent) {
    nw_parent = wrapper->GetScope()->GetGlobalJSObject();

    lock = PR_FALSE;
  } else {
    lock = PR_TRUE;
  }

  if (lock) {
    // Make sure nw_parent doesn't get collected while we're creating
    // the new wrapper.
    ::JS_LockGCThing(cx, nw_parent);
  }

  obj = ::JS_NewObject(cx, GetJSClass(), nsnull, nw_parent);

  if (lock) {
    ::JS_UnlockGCThing(cx, nw_parent);
  }

  if (!obj ||
      !::JS_SetPrivate(cx, obj, wrapper) ||
      !::JS_SetReservedSlot(cx, obj, 0, INT_TO_JSVAL(FLAG_DEEP))) {
    return nsnull;
  }

  wrapper->SetNativeWrapper(obj);

#if defined(DEBUG_XPCNativeWrapper) || defined(DEBUG_xpc_leaks)
  {
    XPCCallContext ccx(NATIVE_CALLER, cx);

    // Keep obj alive while we mess with strings
    AUTO_MARK_JSVAL(ccx, OBJECT_TO_JSVAL(obj));

    char *s = wrapper->ToString(ccx);
    printf("Created new XPCNativeWrapper %p for wrapped native %s\n",
           (void*)obj, s);
    if (s)
      JS_smprintf_free(s);
  }
#endif
  
  return obj;
}

struct WrapperAndCxHolder
{
    XPCWrappedNative* wrapper;
    JSContext* cx;
};

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
ClearNativeWrapperScope(JSDHashTable *table, JSDHashEntryHdr *hdr,
                        uint32 number, void *arg)
{
    JSDHashEntryStub* entry = (JSDHashEntryStub*)hdr;
    WrapperAndCxHolder* d = (WrapperAndCxHolder*)arg;

    if (d->wrapper->GetNativeWrapper() == (JSObject*)entry->key)
    {
        ::JS_ClearScope(d->cx, (JSObject*)entry->key);
    }

    return JS_DHASH_NEXT;
}

// static
void
XPCNativeWrapper::ClearWrappedNativeScopes(JSContext* cx,
                                           XPCWrappedNative* wrapper)
{
  JSObject *nativeWrapper = wrapper->GetNativeWrapper();

  if (nativeWrapper) {
    ::JS_ClearScope(cx, nativeWrapper);
  }

  WrapperAndCxHolder d =
    {
      wrapper,
      cx
    };

  wrapper->GetRuntime()->GetExplicitNativeWrapperMap()->
    Enumerate(ClearNativeWrapperScope, &d);
}
