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
#include "XPCWrapper.h"
#include "jsdbgapi.h"
#include "jsscope.h"

static JSBool
XPC_NW_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
XPC_NW_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
XPC_NW_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
XPC_NW_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
XPC_NW_Enumerate(JSContext *cx, JSObject *obj);

static JSBool
XPC_NW_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                  JSObject **objp);

static JSBool
XPC_NW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

static void
XPC_NW_Finalize(JSContext *cx, JSObject *obj);

static JSBool
XPC_NW_CheckAccess(JSContext *cx, JSObject *obj, jsval id,
                   JSAccessMode mode, jsval *vp);

static JSBool
XPC_NW_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
            jsval *rval);

static JSBool
XPC_NW_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval);

static JSBool
XPC_NW_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

static void
XPC_NW_Trace(JSTracer *trc, JSObject *obj);

static JSBool
XPC_NW_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

static JSBool
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
    JSCLASS_MARK_IS_TRACE | JSCLASS_IS_EXTENDED,
    XPC_NW_AddProperty, XPC_NW_DelProperty,
    XPC_NW_GetProperty, XPC_NW_SetProperty,
    XPC_NW_Enumerate,   (JSResolveOp)XPC_NW_NewResolve,
    XPC_NW_Convert,     XPC_NW_Finalize,
    nsnull,             XPC_NW_CheckAccess,
    XPC_NW_Call,        XPC_NW_Construct,
    nsnull,             XPC_NW_HasInstance,
    JS_CLASS_TRACE(XPC_NW_Trace), nsnull
  },
  // JSExtendedClass initialization
  XPC_NW_Equality
};

// If one of our class hooks is ever called from a non-system script, bypass
// the hook by calling the same hook on our wrapped native, with obj reset to
// the wrapped native's flat JSObject, so the hook and args macro parameters
// can be simply:
//
//      convert, (cx, obj, type, vp)
//
// in the call from XPC_NW_Convert, for example.

#define XPC_NW_CALL_HOOK(obj, hook, args)                                 \
  return STOBJ_GET_CLASS(obj)->hook args;

#define XPC_NW_CAST_HOOK(obj, type, hook, args)                           \
  return ((type) STOBJ_GET_CLASS(obj)->hook) args;

static JSBool
ShouldBypassNativeWrapper(JSContext *cx, JSObject *obj)
{
  NS_ASSERTION(XPCNativeWrapper::IsNativeWrapper(obj),
               "Unexpected object");
  jsval flags;

  ::JS_GetReservedSlot(cx, obj, 0, &flags);
  if (HAS_FLAGS(flags, FLAG_EXPLICIT))
    return JS_FALSE;

  // Check what the script calling us looks like
  JSStackFrame *fp = JS_GetScriptedCaller(cx, NULL);
  JSScript *script = fp ? fp->script : NULL;

  // If there's no script, bypass for now because that's what the old code did.
  // XXX FIXME: bug 341477 covers figuring out what we _should_ do.
  return !script || !(::JS_GetScriptFilenameFlags(script) & JSFILENAME_SYSTEM);
}

#define XPC_NW_BYPASS_BASE(cx, obj, code)                                     \
  JS_BEGIN_MACRO                                                              \
    if (ShouldBypassNativeWrapper(cx, obj)) {                                 \
      /* Use SafeGetWrappedNative since obj can't be an explicit native       \
         wrapper. */                                                          \
      XPCWrappedNative *wn_ = XPCNativeWrapper::SafeGetWrappedNative(obj);    \
      if (!wn_) {                                                             \
        return JS_TRUE;                                                       \
      }                                                                       \
      obj = wn_->GetFlatJSObject();                                           \
      code                                                                    \
    }                                                                         \
  JS_END_MACRO

#define XPC_NW_BYPASS(cx, obj, hook, args)                                    \
  XPC_NW_BYPASS_BASE(cx, obj, XPC_NW_CALL_HOOK(obj, hook, args))

#define XPC_NW_BYPASS_CAST(cx, obj, type, hook, args)                         \
  XPC_NW_BYPASS_BASE(cx, obj, XPC_NW_CAST_HOOK(obj, type, hook, args))

#define XPC_NW_BYPASS_TEST(cx, obj, hook, args)                               \
  XPC_NW_BYPASS_BASE(cx, obj,                                                 \
    JSClass *clasp_ = STOBJ_GET_CLASS(obj);                                  \
    return !clasp_->hook || clasp_->hook args;                                \
  )

static JSBool
XPC_NW_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval);

static inline
JSBool
ThrowException(nsresult ex, JSContext *cx)
{
  XPCThrower::Throw(ex, cx);

  return JS_FALSE;
}

static inline
JSBool
EnsureLegalActivity(JSContext *cx, JSObject *obj)
{
  nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
  if (!ssm) {
    // If there's no security manager, then we're not running in a browser
    // context: allow access.
    return JS_TRUE;
  }

  JSStackFrame *fp;
  nsIPrincipal *subjectPrincipal = ssm->GetCxSubjectPrincipalAndFrame(cx, &fp);
  if (!subjectPrincipal || !fp) {
    // We must allow the access if there is no code running.
    return JS_TRUE;
  }

  // This might be chrome code or content code with UniversalXPConnect.
  void *annotation = JS_GetFrameAnnotation(cx, fp);
  PRBool isPrivileged = PR_FALSE;
  nsresult rv = subjectPrincipal->IsCapabilityEnabled("UniversalXPConnect",
                                                      annotation,
                                                      &isPrivileged);
  if (NS_SUCCEEDED(rv) && isPrivileged) {
    return JS_TRUE;
  }

  // We're in unprivileged code, ensure that we're allowed to access the
  // underlying object.
  XPCWrappedNative *wn = XPCNativeWrapper::SafeGetWrappedNative(obj);
  if (wn) {
    nsIPrincipal *objectPrincipal = wn->GetScope()->GetPrincipal();
    PRBool subsumes;
    if (NS_FAILED(subjectPrincipal->Subsumes(objectPrincipal, &subsumes)) ||
        !subsumes) {
      return ThrowException(NS_ERROR_XPC_SECURITY_MANAGER_VETO, cx);
    }
  }

  // The underlying object is accessible, but this might be the wrong
  // type of wrapper to access it through.
  // TODO This should just be an assertion now.
  jsval flags;

  ::JS_GetReservedSlot(cx, obj, 0, &flags);
  if (HAS_FLAGS(flags, FLAG_EXPLICIT)) {
    // Can't make any assertions about the owner of this wrapper.
    return JS_TRUE;
  }

  JSScript *script = JS_GetFrameScript(cx, fp);
  uint32 fileFlags = JS_GetScriptFilenameFlags(script);
  if (fileFlags == JSFILENAME_NULL || (fileFlags & JSFILENAME_SYSTEM)) {
    // We expect implicit native wrappers in system files.
    return JS_TRUE;
  }

  // Otherwise, we're looking at a non-system file with a handle on an
  // implicit wrapper. This is a bug! Deny access.
  return ThrowException(NS_ERROR_XPC_SECURITY_MANAGER_VETO, cx);
}

// static
JSBool
XPCNativeWrapper::GetWrappedNative(JSContext *cx, JSObject *obj,
                                   XPCWrappedNative **aWrappedNative)
{
  XPCWrappedNative *wn = static_cast<XPCWrappedNative *>(xpc_GetJSPrivate(obj));
  *aWrappedNative = wn;
  if (!wn) {
    return JS_TRUE;
  }

  nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
  if (!ssm) {
    return JS_TRUE;
  }

  nsIPrincipal *subjectPrincipal = ssm->GetCxSubjectPrincipal(cx);
  if (!subjectPrincipal) {
    return JS_TRUE;
  }

  XPCWrappedNativeScope *scope = wn->GetScope();
  nsIPrincipal *objectPrincipal = scope->GetPrincipal();

  PRBool subsumes;
  nsresult rv = subjectPrincipal->Subsumes(objectPrincipal, &subsumes);
  if (NS_FAILED(rv) || !subsumes) {
    return JS_FALSE;
  }

  return JS_TRUE;
}

JSBool
XPC_NW_WrapFunction(JSContext* cx, JSObject* funobj, jsval *rval)
{
  // If funobj is already a wrapped function, just return it.
  if (JS_GetFunctionNative(cx,
                           JS_ValueToFunction(cx, OBJECT_TO_JSVAL(funobj))) ==
      XPC_NW_FunctionWrapper) {
    *rval = OBJECT_TO_JSVAL(funobj);
    return JS_TRUE;
  }

  // Ensure that we've been called from JS. Native code should extract
  // the wrapped native and deal with that directly.
  // XXX Can we simply trust |cx| here?
  JSStackFrame *iterator = nsnull;
  if (!::JS_FrameIterator(cx, &iterator)) {
    ::JS_ReportError(cx, "XPCNativeWrappers must be used from script");
    return JS_FALSE;
  }
  
  // Create a new function that'll call our given function.  This new
  // function's parent will be the original function and that's how we
  // get the right thing to call when this function is called.
  // Note that we pass nsnull as the nominal parent so that we'll inherit
  // our caller's Function.prototype.
  JSFunction *funWrapper =
    ::JS_NewFunction(cx, XPC_NW_FunctionWrapper, 0, 0, nsnull,
                     "XPCNativeWrapper function wrapper");
  if (!funWrapper) {
    return JS_FALSE;
  }

  JSObject* funWrapperObj = ::JS_GetFunctionObject(funWrapper);
  ::JS_SetParent(cx, funWrapperObj, funobj);
  *rval = OBJECT_TO_JSVAL(funWrapperObj);
  return JS_TRUE;
}

static JSBool
XPC_NW_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSProperty *prop;
  JSObject *pobj;
  jsid idAsId;

  if (!::JS_ValueToId(cx, id, &idAsId) ||
      !OBJ_LOOKUP_PROPERTY(cx, obj, idAsId, &pobj, &prop)) {
    return JS_FALSE;
  }

  // Do not allow scripted getters or setters on XPCNativeWrappers.
  NS_ASSERTION(prop && pobj == obj, "Wasn't this property just added?");
  JSScopeProperty *sprop = (JSScopeProperty *) prop;
  uint8 attrs = sprop->attrs;

  OBJ_DROP_PROPERTY(cx, pobj, prop);
  if (attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  jsval flags;
  ::JS_GetReservedSlot(cx, obj, 0, &flags);
  if (!HAS_FLAGS(flags, FLAG_RESOLVING)) {
    return JS_TRUE;
  }

  // Note: no need to protect *vp from GC here, since it's already in the slot
  // on |obj|.
  return EnsureLegalActivity(cx, obj) &&
         XPC_NW_RewrapIfDeepWrapper(cx, obj, *vp, vp);
}

static JSBool
XPC_NW_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  if (!EnsureLegalActivity(cx, obj)) {
    return JS_FALSE;
  }

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

JSBool
XPC_NW_RewrapIfDeepWrapper(JSContext *cx, JSObject *obj, jsval v, jsval *rval)
{
  NS_ASSERTION(XPCNativeWrapper::IsNativeWrapper(obj),
               "Unexpected object");

  JSBool primitive = JSVAL_IS_PRIMITIVE(v);
  JSObject* nativeObj = primitive ? nsnull : JSVAL_TO_OBJECT(v);
  
  // We always want to wrap function objects, no matter whether we're deep.
  if (!primitive && JS_ObjectIsFunction(cx, nativeObj)) {
    return XPC_NW_WrapFunction(cx, nativeObj, rval);
  }

  jsval flags;
  ::JS_GetReservedSlot(cx, obj, 0, &flags);

  // Re-wrap non-primitive values if this is a deep wrapper, i.e.
  // if (HAS_FLAGS(flags, FLAG_DEEP).
  if (HAS_FLAGS(flags, FLAG_DEEP) && !primitive) {
    // Unwrap a cross origin wrapper, since we're more restrictive.
    if (STOBJ_GET_CLASS(nativeObj) == &sXPC_XOW_JSClass.base) {
      if (!::JS_GetReservedSlot(cx, nativeObj, XPCWrapper::sWrappedObjSlot,
                                &v)) {
        return JS_FALSE;
      }

      // If v is primitive, allow nativeObj to remain a cross origin wrapper,
      // which will fail below (since it isn't a wrapped native).
      if (!JSVAL_IS_PRIMITIVE(v)) {
        nativeObj = JSVAL_TO_OBJECT(v);
      }
    }

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
      if (wrappedNative == XPCNativeWrapper::SafeGetWrappedNative(obj)) {
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
    JSObject* wrapperObj = XPCNativeWrapper::GetNewOrUsed(cx, wrappedNative,
                                                          nsnull);
    if (!wrapperObj) {
      return JS_FALSE;
    }

    *rval = OBJECT_TO_JSVAL(wrapperObj);
  } else {
    *rval = v;
  }

  return JS_TRUE;
}

static JSBool
XPC_NW_FunctionWrapper(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                       jsval *rval)
{
  JSObject *funObj = JSVAL_TO_OBJECT(argv[-2]);
  if (!::JS_ObjectIsFunction(cx, funObj)) {
    obj = nsnull;
  }

  while (obj && !XPCNativeWrapper::IsNativeWrapper(obj)) {
    obj = STOBJ_GET_PROTO(obj);
  }

  if (!obj) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  // The real method we're going to call is the parent of this
  // function's JSObject.
  JSObject *methodToCallObj = STOBJ_GET_PARENT(funObj);
  XPCWrappedNative *wrappedNative;

  if (!XPCNativeWrapper::GetWrappedNative(cx, obj, &wrappedNative) ||
      !::JS_ObjectIsFunction(cx, methodToCallObj) ||
      !wrappedNative) {
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

  return XPC_NW_RewrapIfDeepWrapper(cx, obj, v, rval);
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

  while (!XPCNativeWrapper::IsNativeWrapper(obj)) {
    obj = STOBJ_GET_PROTO(obj);
    if (!obj) {
      return ThrowException(NS_ERROR_UNEXPECTED, cx);
    }
  }

  if (!EnsureLegalActivity(cx, obj)) {
    return JS_FALSE;
  }

  // Protected by EnsureLegalActivity.
  XPCWrappedNative *wrappedNative = XPCNativeWrapper::SafeGetWrappedNative(obj);

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
    // If we're wrapping an untrusted content wrapper, then we should
    // return a safe wrapper for the underlying native object. Otherwise,
    // such a wrapper would be superfluous.

    jsval nativeVal = OBJECT_TO_JSVAL(nativeObj);

    nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
    nsCOMPtr<nsIPrincipal> prin;
    nsresult rv = ssm->GetObjectPrincipal(cx, nativeObj,
                                          getter_AddRefs(prin));
    if (NS_FAILED(rv)) {
      return ThrowException(rv, cx);
    }

    PRBool isSystem;
    if (NS_SUCCEEDED(ssm->IsSystemPrincipal(prin, &isSystem)) && isSystem) {
      *vp = nativeVal;
      return JS_TRUE;
    }

    return XPC_SJOW_Construct(cx, nsnull, 1, &nativeVal, vp);
  }

  return XPCWrapper::GetOrSetNativeProperty(cx, obj, wrappedNative, id, vp,
                                            aIsSet, JS_TRUE);
}

static JSBool
XPC_NW_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return XPC_NW_GetOrSetProperty(cx, obj, id, vp, PR_FALSE);
}

static JSBool
XPC_NW_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return XPC_NW_GetOrSetProperty(cx, obj, id, vp, PR_TRUE);
}

static JSBool
XPC_NW_Enumerate(JSContext *cx, JSObject *obj)
{
  // We are being notified of a for-in loop or similar operation on this
  // XPCNativeWrapper, so forward to the correct high-level object hook,
  // OBJ_ENUMERATE on the XPCWrappedNative's object, called via the
  // JS_Enumerate API.  Then reflect properties named by the enumerated
  // identifiers from the wrapped native to the native wrapper.

  if (!EnsureLegalActivity(cx, obj)) {
    return JS_FALSE;
  }

  // Protected by EnsureLegalActivity.
  XPCWrappedNative *wn = XPCNativeWrapper::SafeGetWrappedNative(obj);
  if (!wn) {
    return JS_TRUE;
  }

  return XPCWrapper::Enumerate(cx, obj, wn->GetFlatJSObject());
}

static JSBool
XPC_NW_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                  JSObject **objp)
{
  // No need to preserve on sets of wrappedJSObject or toString, since callers
  // couldn't get at those values anyway.  Also, we always deal with
  // wrappedJSObject and toString before looking at our scriptable hooks, so no
  // need to mess with our flags yet.
  if (id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_WRAPPED_JSOBJECT)) {
    return JS_TRUE;
  }

  if (id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
    *objp = obj;
    return JS_DefineFunction(cx, obj, "toString",
                             XPC_NW_toString, 0, 0) != nsnull;
  }

  if (!EnsureLegalActivity(cx, obj)) {
    return JS_FALSE;
  }

  // We can't use XPC_NW_BYPASS here, because we need to do a full
  // OBJ_LOOKUP_PROPERTY on the wrapped native's object, in order to
  // trigger reflection along the wrapped native prototype chain.
  // All we need to do is define the property in obj if it exists in
  // the wrapped native's object.

  if (ShouldBypassNativeWrapper(cx, obj)) {
    // Protected by EnsureLegalActivity.
    XPCWrappedNative *wn = XPCNativeWrapper::SafeGetWrappedNative(obj);
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

  while (!XPCNativeWrapper::IsNativeWrapper(obj)) {
    obj = STOBJ_GET_PROTO(obj);
    if (!obj) {
      return ThrowException(NS_ERROR_UNEXPECTED, cx);
    }
  }

  // Protected by EnsureLegalActivity.
  XPCWrappedNative *wrappedNative = XPCNativeWrapper::SafeGetWrappedNative(obj);

  if (!wrappedNative) {
    // No wrapped native, no properties.

    return JS_TRUE;
  }

  return XPCWrapper::ResolveNativeProperty(cx, obj,
                                           wrappedNative->GetFlatJSObject(),
                                           wrappedNative, id, flags, objp,
                                           JS_TRUE);
}

static JSBool
XPC_NW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
  if (!EnsureLegalActivity(cx, obj)) {
    return JS_FALSE;
  }

  XPC_NW_BYPASS(cx, obj, convert, (cx, obj, type, vp));
  return JS_TRUE;
}

static void
XPC_NW_Finalize(JSContext *cx, JSObject *obj)
{
  // We must not use obj's private data here since it's likely that it
  // has already been finalized.
  XPCJSRuntime *rt = nsXPConnect::GetRuntimeInstance();

  {
    // scoped lock
    XPCAutoLock lock(rt->GetMapLock());
    rt->GetExplicitNativeWrapperMap()->Remove(obj);
  }
}

static JSBool
XPC_NW_CheckAccess(JSContext *cx, JSObject *obj, jsval id,
                   JSAccessMode mode, jsval *vp)
{
  // Prevent setting __proto__ on an XPCNativeWrapper
  if ((mode & JSACC_WATCH) == JSACC_PROTO && (mode & JSACC_WRITE)) {
    return ThrowException(NS_ERROR_XPC_SECURITY_MANAGER_VETO, cx);
  }

  // Forward to the checkObjectAccess hook in the JSContext, if any.
  JSSecurityCallbacks *callbacks = JS_GetSecurityCallbacks(cx);
  if (callbacks && callbacks->checkObjectAccess &&
      !callbacks->checkObjectAccess(cx, obj, id, mode, vp)) {
    return JS_FALSE;
  }

  // This function does its own security checks.
  XPCWrappedNative *wrappedNative = XPCNativeWrapper::SafeGetWrappedNative(obj);
  if (!wrappedNative) {
    return JS_TRUE;
  }

  JSObject *wrapperJSObject = wrappedNative->GetFlatJSObject();

  JSClass *clazz = STOBJ_GET_CLASS(wrapperJSObject);
  return !clazz->checkAccess ||
    clazz->checkAccess(cx, wrapperJSObject, id, mode, vp);
}

static JSBool
XPC_NW_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  if (!XPCNativeWrapper::IsNativeWrapper(obj)) {
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

static JSBool
XPC_NW_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
  // The object given to us by the JS engine is actually a stub object (the
  // "new" object). This isn't any help to us, so instead use the function
  // object of the constructor that we're calling (which is the native
  // wrapper).
  obj = JSVAL_TO_OBJECT(argv[-2]);

  XPC_NW_BYPASS_TEST(cx, obj, construct, (cx, obj, argc, argv, rval));

  if (!EnsureLegalActivity(cx, obj)) {
    return JS_FALSE;
  }

  // Protected by EnsureLegalActivity.
  XPCWrappedNative *wrappedNative = XPCNativeWrapper::SafeGetWrappedNative(obj);
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

  return XPC_NW_RewrapIfDeepWrapper(cx, obj, *rval, rval);
}

static JSBool
XPC_NW_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
  XPC_NW_BYPASS_TEST(cx, obj, hasInstance, (cx, obj, v, bp));

  return JS_TRUE;
}

static JSBool
MirrorWrappedNativeParent(JSContext *cx, XPCWrappedNative *wrapper,
                          JSObject **result)
{
  JSObject *wn_parent = STOBJ_GET_PARENT(wrapper->GetFlatJSObject());
  if (!wn_parent) {
    *result = nsnull;
  } else {
    XPCWrappedNative *parent_wrapper =
      XPCWrappedNative::GetWrappedNativeOfJSObject(cx, wn_parent);

    // parent_wrapper can be null if we're in a Components.utils.evalInSandbox
    // scope. In that case, the best we can do is just use the
    // non-native-wrapped sandbox global object for our parent.
    if (parent_wrapper) {
      *result = XPCNativeWrapper::GetNewOrUsed(cx, parent_wrapper, nsnull);
      if (!*result)
        return JS_FALSE;
    }
  }
  return JS_TRUE;
}

JSBool
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

  // Unwrap a cross origin wrapper, since we're more restrictive than it is.
  if (STOBJ_GET_CLASS(nativeObj) == &sXPC_XOW_JSClass.base) {
    jsval v;
    if (!::JS_GetReservedSlot(cx, nativeObj, XPCWrapper::sWrappedObjSlot, &v)) {
      return JS_FALSE;
    }
    // If v is primitive, allow nativeObj to remain a cross origin wrapper,
    // which will fail below (since it isn't a wrapped native).
    if (!JSVAL_IS_PRIMITIVE(v)) {
      nativeObj = JSVAL_TO_OBJECT(v);
    }
  } else if (STOBJ_GET_CLASS(nativeObj) == &sXPC_SJOW_JSClass.base) {
    // Also unwrap SJOWs.
    nativeObj = JS_GetParent(cx, nativeObj);
    if (!nativeObj) {
      return ThrowException(NS_ERROR_XPC_BAD_CONVERT_JS, cx);
    }
  }

  XPCWrappedNative *wrappedNative;

  if (XPCNativeWrapper::IsNativeWrapper(nativeObj)) {
    // We're asked to wrap an already wrapped object. Re-wrap the
    // object wrapped by the given wrapper.

#ifdef DEBUG_XPCNativeWrapper
    printf("Wrapping already wrapped object\n");
#endif

    // It's always safe to re-wrap an object.
    wrappedNative = XPCNativeWrapper::SafeGetWrappedNative(nativeObj);

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
  wrapperObj = ::JS_NewObjectWithGivenProto(cx, XPCNativeWrapper::GetJSClass(),
                                            nsnull,
                                            wrappedNative->GetScope()
                                                         ->GetGlobalJSObject());

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

static void
XPC_NW_Trace(JSTracer *trc, JSObject *obj)
{
  // Untrusted code can't trigger this.
  XPCWrappedNative *wrappedNative = XPCNativeWrapper::SafeGetWrappedNative(obj);

  if (wrappedNative && wrappedNative->IsValid()) {
    JS_CALL_OBJECT_TRACER(trc, wrappedNative->GetFlatJSObject(),
                          "wrappedNative.flatJSObject");
  }
}

static JSBool
XPC_NW_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
  NS_ASSERTION(XPCNativeWrapper::IsNativeWrapper(obj),
               "Uh, we should only ever be called for XPCNativeWrapper "
               "objects!");

  if (!EnsureLegalActivity(cx, obj)) {
    return JS_FALSE;
  }

  if (JSVAL_IS_PRIMITIVE(v)) {
    *bp = JS_FALSE;

    return JS_TRUE;
  }

  // Protected by EnsureLegalActivity.
  XPCWrappedNative *wrappedNative = XPCNativeWrapper::SafeGetWrappedNative(obj);

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

static JSBool
XPC_NW_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
  while (!XPCNativeWrapper::IsNativeWrapper(obj)) {
    obj = STOBJ_GET_PROTO(obj);
    if (!obj) {
      return ThrowException(NS_ERROR_UNEXPECTED, cx);
    }
  }

  if (!EnsureLegalActivity(cx, obj)) {
    return JS_FALSE;
  }

  // Protected by EnsureLegalActivity.
  XPCWrappedNative *wrappedNative = XPCNativeWrapper::SafeGetWrappedNative(obj);

  if (!wrappedNative) {
    // toString() called on XPCNativeWrapper.prototype
    NS_NAMED_LITERAL_STRING(protoString, "[object XPCNativeWrapper]");
    JSString *str =
      ::JS_NewUCStringCopyN(cx, reinterpret_cast<const jschar*>
                                                (protoString.get()),
                            protoString.Length());
    NS_ENSURE_TRUE(str, JS_FALSE);
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
  }

  return XPCWrapper::NativeToString(cx, wrappedNative, argc, argv, rval,
                                    JS_TRUE);
}

// static
PRBool
XPCNativeWrapper::AttachNewConstructorObject(XPCCallContext &ccx,
                                             JSObject *aGlobalObject)
{
  JSObject *class_obj =
    ::JS_InitClass(ccx, aGlobalObject, nsnull, &sXPC_NW_JSClass.base,
                   XPCNativeWrapperCtor, 0, nsnull, nsnull,
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
XPCNativeWrapper::GetNewOrUsed(JSContext *cx, XPCWrappedNative *wrapper,
                               nsIPrincipal *aObjectPrincipal)
{
  if (aObjectPrincipal) {
    nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();

    PRBool isSystem;
    nsresult rv = ssm->IsSystemPrincipal(aObjectPrincipal, &isSystem);
    if (NS_SUCCEEDED(rv) && !isSystem) {
      jsval v = OBJECT_TO_JSVAL(wrapper->GetFlatJSObject());
      if (!XPCNativeWrapperCtor(cx, JSVAL_TO_OBJECT(v), 1, &v, &v))
        return nsnull;
      return JSVAL_TO_OBJECT(v);
    }
  }

  // Prevent wrapping a double-wrapped JS object in an
  // XPCNativeWrapper!
  nsCOMPtr<nsIXPConnectWrappedJS> xpcwrappedjs(do_QueryWrappedNative(wrapper));

  if (xpcwrappedjs) {
    XPCThrower::Throw(NS_ERROR_INVALID_ARG, cx);

    return nsnull;
  }

  JSObject *obj = wrapper->GetWrapper();
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

  obj = ::JS_NewObjectWithGivenProto(cx, GetJSClass(), nsnull, nw_parent);

  if (lock) {
    ::JS_UnlockGCThing(cx, nw_parent);
  }

  if (!obj ||
      !::JS_SetPrivate(cx, obj, wrapper) ||
      !::JS_SetReservedSlot(cx, obj, 0, INT_TO_JSVAL(FLAG_DEEP))) {
    return nsnull;
  }

  wrapper->SetWrapper(obj);

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

static JSDHashOperator
ClearNativeWrapperScope(JSDHashTable *table, JSDHashEntryHdr *hdr,
                        uint32 number, void *arg)
{
    JSDHashEntryStub* entry = (JSDHashEntryStub*)hdr;
    WrapperAndCxHolder* d = (WrapperAndCxHolder*)arg;

    if (d->wrapper->GetWrapper() == (JSObject*)entry->key)
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
  JSObject *nativeWrapper = wrapper->GetWrapper();

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
