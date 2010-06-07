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

static JSObject *
XPC_NW_Iterator(JSContext *cx, JSObject *obj, JSBool keysonly);

static JSBool
XPC_NW_FunctionWrapper(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                       jsval *rval);

using namespace XPCWrapper;

// If this flag is set, then this XPCNativeWrapper is *not* the implicit
// wrapper stored in XPCWrappedNative::mWrapperWord. These wrappers may
// be exposed to content script and because they are not shared, they do
// not have expando properties set on implicit native wrappers.
static const PRUint32 FLAG_EXPLICIT = XPCWrapper::LAST_FLAG << 1;

namespace XPCNativeWrapper { namespace internal {

// JS class for XPCNativeWrapper (and this doubles as the constructor
// for XPCNativeWrapper for the moment too...)

JSExtendedClass NW_NoCall_Class = {
  // JSClass (JSExtendedClass.base) initialization
  { "XPCNativeWrapper",
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS |
    // Our one reserved slot holds a jsint of flag bits
    JSCLASS_NEW_RESOLVE | JSCLASS_HAS_RESERVED_SLOTS(1) |
    JSCLASS_MARK_IS_TRACE | JSCLASS_IS_EXTENDED | JSCLASS_CONSTRUCT_PROTOTYPE,
    XPC_NW_AddProperty, XPC_NW_DelProperty,
    XPC_NW_GetProperty, XPC_NW_SetProperty,
    XPC_NW_Enumerate,   (JSResolveOp)XPC_NW_NewResolve,
    XPC_NW_Convert,     XPC_NW_Finalize,
    nsnull,             XPC_NW_CheckAccess,
    nsnull,             XPC_NW_Construct,
    nsnull,             XPC_NW_HasInstance,
    JS_CLASS_TRACE(XPC_NW_Trace), nsnull
  },

  // JSExtendedClass initialization
  XPC_NW_Equality,
  nsnull,             // outerObject
  nsnull,             // innerObject
  XPC_NW_Iterator,
  nsnull,             // wrappedObject
  JSCLASS_NO_RESERVED_MEMBERS
};

JSExtendedClass NW_Call_Class = {
  // JSClass (JSExtendedClass.base) initialization
  { "XPCNativeWrapper",
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS |
    // Our one reserved slot holds a jsint of flag bits
    JSCLASS_NEW_RESOLVE | JSCLASS_HAS_RESERVED_SLOTS(1) |
    JSCLASS_MARK_IS_TRACE | JSCLASS_IS_EXTENDED | JSCLASS_CONSTRUCT_PROTOTYPE,
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
  XPC_NW_Equality,
  nsnull,             // outerObject
  nsnull,             // innerObject
  XPC_NW_Iterator,
  nsnull,             // wrappedObject
  JSCLASS_NO_RESERVED_MEMBERS
};

} // namespace internal

JSBool
GetWrappedNative(JSContext *cx, JSObject *obj,
                 XPCWrappedNative **aWrappedNative)
{
  XPCWrappedNative *wn = static_cast<XPCWrappedNative *>(xpc_GetJSPrivate(obj));
  *aWrappedNative = wn;
  if (!wn) {
    return JS_TRUE;
  }

  nsIScriptSecurityManager *ssm = GetSecurityManager();
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
    PRBool isPrivileged;
    rv = ssm->IsCapabilityEnabled("UniversalXPConnect", &isPrivileged);
    return NS_SUCCEEDED(rv) && isPrivileged;
  }

  return JS_TRUE;
}

JSBool
WrapFunction(JSContext* cx, JSObject* funobj, jsval *rval)
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

  JS_SetReservedSlot(cx, funWrapperObj, eAllAccessSlot, JSVAL_FALSE);

  return JS_TRUE;
}

JSBool
RewrapValue(JSContext *cx, JSObject *obj, jsval v, jsval *rval)
{
  NS_ASSERTION(XPCNativeWrapper::IsNativeWrapper(obj),
               "Unexpected object");

  if (JSVAL_IS_PRIMITIVE(v)) {
    *rval = v;
    return JS_TRUE;
  }

  JSObject* nativeObj = JSVAL_TO_OBJECT(v);

  // Wrap function objects specially.
  if (JS_ObjectIsFunction(cx, nativeObj)) {
    return WrapFunction(cx, nativeObj, rval);
  }

  JSObject *scope = JS_GetScopeChain(cx);
  if (!scope) {
    return JS_FALSE;
  }

  jsval flags;
  ::JS_GetReservedSlot(cx, obj, 0, &flags);
  WrapperType type = HAS_FLAGS(flags, FLAG_EXPLICIT)
                     ? XPCNW_EXPLICIT : XPCNW_IMPLICIT;
  return RewrapObject(cx, JS_GetGlobalForObject(cx, scope),
                      nativeObj, type, rval);
}

} // namespace XPCNativeWrapper

using namespace XPCNativeWrapper;

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
EnsureLegalActivity(JSContext *cx, JSObject *obj,
                    jsval id = JSVAL_VOID, PRUint32 accessType = 0)
{
  nsIScriptSecurityManager *ssm = GetSecurityManager();
  if (!ssm) {
    // If there's no security manager, then we're not running in a browser
    // context: allow access.
    return JS_TRUE;
  }

  JSStackFrame *fp;
  nsIPrincipal *subjectPrincipal = ssm->GetCxSubjectPrincipalAndFrame(cx, &fp);
  if (!subjectPrincipal || !fp) {
    // We must allow access if there is no code running.
    return JS_TRUE;
  }

  PRBool isSystem;
  if (NS_SUCCEEDED(ssm->IsSystemPrincipal(subjectPrincipal, &isSystem)) &&
      isSystem) {
    // Chrome code is running.
    return JS_TRUE;
  }

  jsval flags;

  JS_GetReservedSlot(cx, obj, sFlagsSlot, &flags);
  if (HAS_FLAGS(flags, FLAG_SOW) && !SystemOnlyWrapper::CheckFilename(cx, id, fp)) {
    return JS_FALSE;
  }

  // We're in unprivileged code, ensure that we're allowed to access the
  // underlying object.
  XPCWrappedNative *wn = XPCNativeWrapper::SafeGetWrappedNative(obj);
  if (wn) {
    nsIPrincipal *objectPrincipal = wn->GetScope()->GetPrincipal();
    PRBool subsumes;
    if (NS_FAILED(subjectPrincipal->Subsumes(objectPrincipal, &subsumes)) ||
        !subsumes) {
      // This might be chrome code or content code with UniversalXPConnect.
      PRBool isPrivileged = PR_FALSE;
      nsresult rv =
        ssm->IsCapabilityEnabled("UniversalXPConnect", &isPrivileged);
      if (NS_SUCCEEDED(rv) && isPrivileged) {
        return JS_TRUE;
      }

      JSObject* flatObj;
      if (!JSVAL_IS_VOID(id) &&
          (accessType & (sSecMgrSetProp | sSecMgrGetProp)) &&
          (flatObj = wn->GetFlatJSObject())) {
        rv = ssm->CheckPropertyAccess(cx, flatObj,
                                      flatObj->getClass()->name,
                                      id, accessType);
        return NS_SUCCEEDED(rv);
      }

      return ThrowException(NS_ERROR_XPC_SECURITY_MANAGER_VETO, cx);
    }
  }

#ifdef DEBUG
  // The underlying object is accessible, but this might be the wrong
  // type of wrapper to access it through.

  if (HAS_FLAGS(flags, FLAG_EXPLICIT)) {
    // Can't make any assertions about the owner of this wrapper.
    return JS_TRUE;
  }

  JSScript *script = JS_GetFrameScript(cx, fp);
  if (!script) {
    // This is likely a SJOW around an XPCNativeWrapper. We don't know
    // who is accessing us, but given the TODO above, allow access.
    return JS_TRUE;
  }

  uint32 fileFlags = JS_GetScriptFilenameFlags(script);
  if (fileFlags == JSFILENAME_NULL || (fileFlags & JSFILENAME_SYSTEM)) {
    // We expect implicit native wrappers in system files.
    return JS_TRUE;
  }

  // Otherwise, we're looking at a non-system file with a handle on an
  // implicit wrapper. This is a bug! Deny access.
  NS_ERROR("Implicit native wrapper in content code");
  return JS_FALSE;
#else
  return JS_TRUE;
#endif

  // NB: Watch for early returns in the ifdef DEBUG code above.
}

static JSBool
XPC_NW_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  jsid idAsId;
  JSPropertyDescriptor desc;

  if (!JS_ValueToId(cx, id, &idAsId) ||
      !JS_GetPropertyDescriptorById(cx, obj, idAsId, JSRESOLVE_QUALIFIED,
                                    &desc)) {
    return JS_FALSE;
  }

  // Do not allow scripted getters or setters on XPCNativeWrappers.
  if (desc.attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  jsval flags = JSVAL_VOID;
  JS_GetReservedSlot(cx, obj, 0, &flags);
  // The purpose of XPC_NW_AddProperty is to wrap any object set on the
  // XPCNativeWrapper by the wrapped object's scriptable helper, so bail
  // here if the scriptable helper is not currently adding a property.
  // See comment above #define FLAG_RESOLVING in XPCWrapper.h.
  if (!HAS_FLAGS(flags, FLAG_RESOLVING)) {
    return JS_TRUE;
  }

  // Note: no need to protect *vp from GC here, since it's already in the slot
  // on |obj|.
  return EnsureLegalActivity(cx, obj, id, sSecMgrSetProp) &&
         RewrapValue(cx, obj, *vp, vp);
}

static JSBool
XPC_NW_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return EnsureLegalActivity(cx, obj);
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
    obj = obj->getProto();
  }

  if (!obj) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  // The real method we're going to call is the parent of this
  // function's JSObject.
  JSObject *methodToCallObj = funObj->getParent();
  XPCWrappedNative* wrappedNative = nsnull;

  jsval isAllAccess;
  if (::JS_GetReservedSlot(cx, funObj, eAllAccessSlot, &isAllAccess) &&
      JSVAL_TO_BOOLEAN(isAllAccess)) {
    wrappedNative = XPCNativeWrapper::SafeGetWrappedNative(obj);
  } else if (!XPCNativeWrapper::GetWrappedNative(cx, obj, &wrappedNative)) {
    wrappedNative = nsnull;
  }

  if (!wrappedNative || !::JS_ObjectIsFunction(cx, methodToCallObj)) {
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

  return RewrapValue(cx, obj, v, rval);
}

static JSBool
GetwrappedJSObject(JSContext *cx, JSObject *obj, jsval *vp)
{
  // If we're wrapping an untrusted content wrapper, then we should
  // return a safe wrapper for the underlying native object. Otherwise,
  // such a wrapper would be superfluous.

  nsIScriptSecurityManager *ssm = GetSecurityManager();
  nsCOMPtr<nsIPrincipal> prin;
  nsresult rv = ssm->GetObjectPrincipal(cx, obj, getter_AddRefs(prin));
  if (NS_FAILED(rv)) {
    return ThrowException(rv, cx);
  }

  jsval v = OBJECT_TO_JSVAL(obj);

  PRBool isSystem;
  if (NS_SUCCEEDED(ssm->IsSystemPrincipal(prin, &isSystem)) && isSystem) {
    *vp = v;
    return JS_TRUE;
  }

  return XPCSafeJSObjectWrapper::WrapObject(cx, JS_GetScopeChain(cx), v, vp);
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
    obj = obj->getProto();
    if (!obj) {
      return ThrowException(NS_ERROR_UNEXPECTED, cx);
    }
  }

  if (!EnsureLegalActivity(cx, obj, id,
                           aIsSet ? sSecMgrSetProp : sSecMgrGetProp)) {
    return JS_FALSE;
  }

  // Protected by EnsureLegalActivity.
  XPCWrappedNative *wrappedNative = XPCNativeWrapper::SafeGetWrappedNative(obj);

  if (!wrappedNative) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  JSObject *nativeObj = wrappedNative->GetFlatJSObject();

  if (!aIsSet &&
      id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_WRAPPED_JSOBJECT)) {
    return GetwrappedJSObject(cx, nativeObj, vp);
  }

  return GetOrSetNativeProperty(cx, obj, wrappedNative, id, vp, aIsSet,
                                JS_TRUE);
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

  return Enumerate(cx, obj, wn->GetFlatJSObject());
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

    // See the comment in WrapFunction for why we create this function
    // like this.
    JSFunction *fun = JS_NewFunction(cx, XPC_NW_toString, 0, 0, nsnull,
                                     "toString");
    if (!fun) {
      return JS_FALSE;
    }

    JSObject *funobj = JS_GetFunctionObject(fun);
    funobj->setParent(obj);

    return JS_DefineProperty(cx, obj, "toString", OBJECT_TO_JSVAL(funobj),
                             nsnull, nsnull, 0);
  }

  PRUint32 accessType =
    (flags & JSRESOLVE_ASSIGNING) ? sSecMgrSetProp : sSecMgrGetProp;
  if (!EnsureLegalActivity(cx, obj, id, accessType)) {
    return JS_FALSE;
  }

  while (!XPCNativeWrapper::IsNativeWrapper(obj)) {
    obj = obj->getProto();
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

  return ResolveNativeProperty(cx, obj, wrappedNative->GetFlatJSObject(),
                               wrappedNative, id, flags, objp, JS_TRUE);
}

static JSBool
XPC_NW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
  return EnsureLegalActivity(cx, obj);
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

  JSClass *clazz = wrapperJSObject->getClass();
  return !clazz->checkAccess ||
    clazz->checkAccess(cx, wrapperJSObject, id, mode, vp);
}

static JSBool
XPC_NW_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#ifdef DEBUG
  if (!XPCNativeWrapper::IsNativeWrapper(obj) &&
      !JS_ObjectIsFunction(cx, obj)) {
    NS_WARNING("Ignoring a call for a weird object");
  }
#endif
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

  return RewrapValue(cx, obj, *rval, rval);
}

static JSBool
XPC_NW_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
  return JS_TRUE;
}

static JSBool
MirrorWrappedNativeParent(JSContext *cx, XPCWrappedNative *wrapper,
                          JSObject **result NS_OUTPARAM)
{
  JSObject *wn_parent = wrapper->GetFlatJSObject()->getParent();
  if (!wn_parent) {
    *result = nsnull;
  } else {
    XPCWrappedNative *parent_wrapper =
      XPCWrappedNative::GetAndMorphWrappedNativeOfJSObject(cx, wn_parent);

    // parent_wrapper can be null if we're in a Components.utils.evalInSandbox
    // scope. In that case, the best we can do is just use the
    // non-native-wrapped sandbox global object for our parent.
    if (parent_wrapper) {
      *result = XPCNativeWrapper::GetNewOrUsed(cx, parent_wrapper, nsnull,
                                               nsnull);
      if (!*result)
        return JS_FALSE;
    } else {
      *result = nsnull;
    }
  }
  return JS_TRUE;
}

static JSBool
XPCNativeWrapperCtor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                     jsval *rval)
{
  JSStackFrame *fp = nsnull;
  JSBool constructing = JS_FALSE;
  if (JS_FrameIterator(cx, &fp) && JS_IsConstructorFrame(cx, fp)) {
    constructing = JS_TRUE;

    JSObject *proto = obj->getProto();
    if (proto && !XPCNativeWrapper::IsNativeWrapper(proto)) {
      // Deal with our prototype object specially.

      JS_ASSERT(XPCNativeWrapper::IsNativeWrapper(obj));
      return JS_SetPrivate(cx, obj, nsnull) &&
             JS_SetReservedSlot(cx, obj, 0, JSVAL_ZERO);
    }
  }

  if (argc < 1) {
    return ThrowException(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx);
  }

  // |obj| almost always has the wrong proto and parent so we have to create
  // our own object anyway.  Set |obj| to null so we don't use it by accident.
  obj = nsnull;

  jsval native = argv[0];

  if (JSVAL_IS_PRIMITIVE(native)) {
    if (constructing) {
      return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
    }

    *rval = native;
    return JS_TRUE;
  }

  JSObject *nativeObj = JSVAL_TO_OBJECT(native);

  // First, if this is another type of security wrapper, unwrap it to see what
  // we're really dealing with.
  nativeObj = UnsafeUnwrapSecurityWrapper(cx, nativeObj);
  if (!nativeObj) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }
  native = OBJECT_TO_JSVAL(nativeObj);

  // Now, figure out if we're allowed to create an XPCNativeWrapper around it.
  JSObject *scope = JS_GetScopeChain(cx);
  if (!scope) {
    return JS_FALSE;
  }

  XPCWrappedNativeScope *xpcscope =
    XPCWrappedNativeScope::FindInJSObjectScope(cx, scope);
  NS_ASSERTION(xpcscope, "what crazy scope are we in?");

  XPCWrappedNative *wrappedNative;
  WrapperType type = xpcscope->GetWrapperFor(cx, nativeObj, XPCNW_EXPLICIT,
                                             &wrappedNative);

  if (type != NONE && !(type & XPCNW_EXPLICIT)) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  // We might have to morph.
  if (!wrappedNative) {
    wrappedNative =
      XPCWrappedNative::GetAndMorphWrappedNativeOfJSObject(cx, nativeObj);

    if (!wrappedNative) {
      return ThrowException(NS_ERROR_INVALID_ARG, cx);
    }
  }

  // Prevent wrapping a double-wrapped JS object in an
  // XPCNativeWrapper!
  nsCOMPtr<nsIXPConnectWrappedJS> xpcwrappedjs =
    do_QueryWrappedNative(wrappedNative);

  if (xpcwrappedjs) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  if (argc == 2 && !JSVAL_IS_PRIMITIVE(argv[1])) {
    // An object was passed as the second argument to the
    // constructor. In this case we check that the object we're
    // wrapping is an instance of the assumed constructor that we
    // got. If not, throw an exception.
    JSBool hasInstance;
    if (!JS_HasInstance(cx, JSVAL_TO_OBJECT(argv[1]), native, &hasInstance)) {
      return ThrowException(NS_ERROR_UNEXPECTED, cx);
    }

    if (!hasInstance) {
      return ThrowException(NS_ERROR_INVALID_ARG, cx);
    }
  }

  if (!XPCNativeWrapper::CreateExplicitWrapper(cx, wrappedNative, rval)) {
    return JS_FALSE;
  }

  if (!(type & SOW)) {
    return JS_TRUE;
  }

  return SystemOnlyWrapper::MakeSOW(cx, JSVAL_TO_OBJECT(*rval));
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

static JSObject *
XPC_NW_Iterator(JSContext *cx, JSObject *obj, JSBool keysonly)
{
  XPCCallContext ccx(JS_CALLER, cx);
  if (!ccx.IsValid()) {
    ThrowException(NS_ERROR_FAILURE, cx);
    return nsnull;
  }

  JSObject *wrapperIter =
    JS_NewObjectWithGivenProto(cx, XPCNativeWrapper::GetJSClass(false), nsnull,
                               obj->getParent());
  if (!wrapperIter) {
    return nsnull;
  }

  js::AutoObjectRooter tvr(cx, wrapperIter);

  // Initialize our native wrapper.
  XPCWrappedNative *wn = static_cast<XPCWrappedNative *>(JS_GetPrivate(cx, obj));
  JS_SetPrivate(cx, wrapperIter, wn);
  if (!JS_SetReservedSlot(cx, wrapperIter, 0, INT_TO_JSVAL(FLAG_EXPLICIT))) {
    return nsnull;
  }

  return CreateIteratorObj(cx, wrapperIter, obj, wn->GetFlatJSObject(),
                           keysonly);
}

static JSBool
XPC_NW_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
  while (!XPCNativeWrapper::IsNativeWrapper(obj)) {
    obj = obj->getProto();
    if (!obj) {
      return ThrowException(NS_ERROR_UNEXPECTED, cx);
    }
  }

  if (!EnsureLegalActivity(cx, obj,
                           GetRTStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING),
                           sSecMgrGetProp)) {
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

  return NativeToString(cx, wrappedNative, argc, argv, rval, JS_TRUE);
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

  if (!IsNativeWrapper(JSVAL_TO_OBJECT(v))) {
    JS_SET_RVAL(cx, vp, v);
    return JS_TRUE;
  }

  XPCWrappedNative *wn;
  if (!XPCNativeWrapper::GetWrappedNative(cx, JSVAL_TO_OBJECT(v), &wn)) {
    return JS_FALSE;
  }

  if (!wn) {
    JS_SET_RVAL(cx, vp, JSVAL_NULL);
    return JS_TRUE;
  }

  return GetwrappedJSObject(cx, wn->GetFlatJSObject(), vp);
}

static JSFunctionSpec static_functions[] = {
  JS_FN("unwrap", UnwrapNW, 1, 0),
  JS_FS_END
};

// static
PRBool
XPCNativeWrapper::AttachNewConstructorObject(XPCCallContext &ccx,
                                             JSObject *aGlobalObject)
{
  JSObject *class_obj =
    ::JS_InitClass(ccx, aGlobalObject, nsnull, &internal::NW_Call_Class.base,
                   XPCNativeWrapperCtor, 0, nsnull, nsnull,
                   nsnull, static_functions);
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
                                    internal::NW_Call_Class.base.name,
                                    JSPROP_READONLY | JSPROP_PERMANENT,
                                    &found);
}

// static
JSObject *
XPCNativeWrapper::GetNewOrUsed(JSContext *cx, XPCWrappedNative *wrapper,
                               JSObject *scope, nsIPrincipal *aObjectPrincipal)
{
  if (aObjectPrincipal) {
    nsIScriptSecurityManager *ssm = GetSecurityManager();

    PRBool isSystem;
    nsresult rv = ssm->IsSystemPrincipal(aObjectPrincipal, &isSystem);
    if (NS_SUCCEEDED(rv) && !isSystem) {
      jsval v = OBJECT_TO_JSVAL(wrapper->GetFlatJSObject());
      if (!CreateExplicitWrapper(cx, wrapper, &v)) {
        return nsnull;
      }
      return JSVAL_TO_OBJECT(v);
    }
  }

  // Prevent wrapping a double-wrapped JS object in an
  // XPCNativeWrapper!
  nsCOMPtr<nsIXPConnectWrappedJS> xpcwrappedjs(do_QueryWrappedNative(wrapper));

  if (xpcwrappedjs) {
    JSObject *flat = wrapper->GetFlatJSObject();
    jsval v = OBJECT_TO_JSVAL(flat);

    XPCCallContext ccx(JS_CALLER, cx);

    // Make sure v doesn't get collected while we're re-wrapping it.
    AUTO_MARK_JSVAL(ccx, v);

    if (XPCSafeJSObjectWrapper::WrapObject(cx, scope, v, &v))
        return JSVAL_TO_OBJECT(v);

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

  bool call = NATIVE_HAS_FLAG(wrapper, WantCall) ||
              NATIVE_HAS_FLAG(wrapper, WantConstruct);
  obj = ::JS_NewObjectWithGivenProto(cx, GetJSClass(call), nsnull, nw_parent);

  if (lock) {
    ::JS_UnlockGCThing(cx, nw_parent);
  }

  if (!obj ||
      !::JS_SetPrivate(cx, obj, wrapper) ||
      !::JS_SetReservedSlot(cx, obj, 0, JSVAL_ZERO)) {
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

// static
JSBool
XPCNativeWrapper::CreateExplicitWrapper(JSContext *cx,
                                        XPCWrappedNative *wrappedNative,
                                        jsval *rval)
{
#ifdef DEBUG_XPCNativeWrapper
  printf("Creating new JSObject\n");
#endif

  bool call = NATIVE_HAS_FLAG(wrappedNative, WantCall) ||
              NATIVE_HAS_FLAG(wrappedNative, WantConstruct);
  JSObject *wrapperObj =
    JS_NewObjectWithGivenProto(cx, XPCNativeWrapper::GetJSClass(call), nsnull,
                               wrappedNative->GetScope()->GetGlobalJSObject());

  if (!wrapperObj) {
    // JS_NewObject already threw (or reported OOM).
    return JS_FALSE;
  }

  if (!::JS_SetReservedSlot(cx, wrapperObj, 0, INT_TO_JSVAL(FLAG_EXPLICIT))) {
    return JS_FALSE;
  }

  JSObject *parent = nsnull;

  // Make sure wrapperObj doesn't get collected while we're wrapping
  // parents for it.
  JS_LockGCThing(cx, wrapperObj);

  // A deep XPCNativeWrapper has a parent chain that mirrors its
  // XPCWrappedNative's chain.
  if (!MirrorWrappedNativeParent(cx, wrappedNative, &parent))
    return JS_FALSE;

  JS_UnlockGCThing(cx, wrapperObj);

  if (!parent) {
    parent = wrappedNative->GetScope()->GetGlobalJSObject();
  }

  if (!JS_SetParent(cx, wrapperObj, parent))
    return JS_FALSE;

  // Set the XPCWrappedNative as private data in the native wrapper.
  if (!JS_SetPrivate(cx, wrapperObj, wrappedNative)) {
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
