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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@mozilla.org> (original author)
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
#include "jsdbgapi.h"
#include "jsscript.h" // for js_ScriptClass
#include "XPCWrapper.h"
#include "jsregexp.h"
#include "nsJSPrincipals.h"

static JSBool
XPC_SJOW_AddProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

static JSBool
XPC_SJOW_DelProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

static JSBool
XPC_SJOW_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

static JSBool
XPC_SJOW_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

static JSBool
XPC_SJOW_Enumerate(JSContext *cx, JSObject *obj);

static JSBool
XPC_SJOW_NewResolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                    JSObject **objp);

static JSBool
XPC_SJOW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

static void
XPC_SJOW_Finalize(JSContext *cx, JSObject *obj);

static JSBool
XPC_SJOW_CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
                     jsval *vp);

static JSBool
XPC_SJOW_Call(JSContext *cx, uintN argc, jsval *vp);

static JSBool
XPC_SJOW_Construct(JSContext *cx, uintN argc, jsval *vp);

static JSBool
XPC_SJOW_Create(JSContext *cx, uintN argc, jsval *vp);

static JSBool
XPC_SJOW_Equality(JSContext *cx, JSObject *obj, const jsval *v, JSBool *bp);

static JSObject *
XPC_SJOW_Iterator(JSContext *cx, JSObject *obj, JSBool keysonly);

static JSObject *
XPC_SJOW_WrappedObject(JSContext *cx, JSObject *obj);

using namespace XPCSafeJSObjectWrapper;
using namespace XPCWrapper;

static inline
JSBool
ThrowException(nsresult ex, JSContext *cx)
{
  DoThrowException(ex, cx);

  return JS_FALSE;
}

// Find the subject and object principal. The argument
// subjectPrincipal can be null if the caller doesn't care about the
// subject principal, and secMgr can also be null if the caller
// doesn't need the security manager.
static nsresult
FindPrincipals(JSContext *cx, JSObject *obj, nsIPrincipal **objectPrincipal,
               nsIPrincipal **subjectPrincipal,
               nsIScriptSecurityManager **secMgr,
               JSStackFrame **fp = nsnull)
{
  XPCCallContext ccx(JS_CALLER, cx);

  if (!ccx.IsValid()) {
    return NS_ERROR_UNEXPECTED;
  }

  nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();

  if (subjectPrincipal) {
    JSStackFrame *fp2;
    NS_IF_ADDREF(*subjectPrincipal = ssm->GetCxSubjectPrincipalAndFrame(cx, &fp2));
    if (fp) {
      *fp = fp2;
    }
  }

  ssm->GetObjectPrincipal(cx, obj, objectPrincipal);

  if (secMgr) {
    NS_ADDREF(*secMgr = ssm);
  }

  return *objectPrincipal ? NS_OK : NS_ERROR_XPC_SECURITY_MANAGER_VETO;
}

static PRBool
CanCallerAccess(JSContext *cx, JSObject *wrapperObj, JSObject *unsafeObj)
{
  // TODO bug 508928: Refactor this with the XOW security checking code.
  nsCOMPtr<nsIPrincipal> subjPrincipal, objPrincipal;
  nsCOMPtr<nsIScriptSecurityManager> ssm;
  JSStackFrame *fp;
  nsresult rv = FindPrincipals(cx, unsafeObj, getter_AddRefs(objPrincipal),
                               getter_AddRefs(subjPrincipal),
                               getter_AddRefs(ssm), &fp);
  if (NS_FAILED(rv)) {
    return ThrowException(rv, cx);
  }

  // Assume that we're trusted if there's no running code.
  if (!subjPrincipal || !fp) {
    return PR_TRUE;
  }

  PRBool subsumes;
  rv = subjPrincipal->Subsumes(objPrincipal, &subsumes);

  if (NS_FAILED(rv) || !subsumes) {
    PRBool enabled = PR_FALSE;
    rv = ssm->IsCapabilityEnabled("UniversalXPConnect", &enabled);
    if (NS_FAILED(rv)) {
      return ThrowException(rv, cx);
    }

    if (!enabled) {
      return ThrowException(NS_ERROR_XPC_SECURITY_MANAGER_VETO, cx);
    }
  }

  if (wrapperObj) {
    jsval flags;
    JS_GetReservedSlot(cx, wrapperObj, sFlagsSlot, &flags);
    if (HAS_FLAGS(flags, FLAG_SOW) &&
        !SystemOnlyWrapper::CheckFilename(cx, JSID_VOID, fp)) {
      return JS_FALSE;
    }
  }

  return PR_TRUE;
}

// Reserved slot indexes on safe wrappers.

// Slot for holding on to the principal to use if a principal other
// than that of the unsafe object is desired for this wrapper
// (nsIPrincipal, strong reference).
static const PRUint32 sPrincipalSlot = sNumSlots;

// Slot for holding the function that we fill our fake frame with.
static const PRUint32 sScopeFunSlot = sNumSlots + 1;

static const PRUint32 sSJOWSlots = sNumSlots + 2;

// Returns a weak reference.
static nsIPrincipal *
FindObjectPrincipals(JSContext *cx, JSObject *safeObj, JSObject *innerObj)
{
  // Check if we have a cached principal first.
  jsval v;
  if (!JS_GetReservedSlot(cx, safeObj, sPrincipalSlot, &v)) {
    return nsnull;
  }

  if (!JSVAL_IS_VOID(v)) {
    // Found one! No need to do any more refcounting.
    return static_cast<nsIPrincipal *>(JSVAL_TO_PRIVATE(v));
  }

  nsCOMPtr<nsIPrincipal> objPrincipal;
  nsresult rv = FindPrincipals(cx, innerObj, getter_AddRefs(objPrincipal), nsnull,
                               nsnull);
  if (NS_FAILED(rv)) {
    return nsnull;
  }

  if (!JS_SetReservedSlot(cx, safeObj, sPrincipalSlot,
                          PRIVATE_TO_JSVAL(objPrincipal.get()))) {
    return nsnull;
  }

  // The wrapper owns the principal now.
  return objPrincipal.forget().get();
}

static inline JSObject *
FindSafeObject(JSObject *obj)
{
  while (obj->getClass() != &SJOWClass) {
    obj = obj->getProto();

    if (!obj) {
      break;
    }
  }

  return obj;
}

static JSBool
XPC_SJOW_toString(JSContext *cx, uintN argc, jsval *vp);

namespace XPCSafeJSObjectWrapper {

// JS class for XPCSafeJSObjectWrapper (and this doubles as the
// constructor for XPCSafeJSObjectWrapper for the moment too...)

js::Class SJOWClass = {
    "XPCSafeJSObjectWrapper",
    JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_RESERVED_SLOTS(sSJOWSlots),
    JS_VALUEIFY(js::PropertyOp, XPC_SJOW_AddProperty),
    JS_VALUEIFY(js::PropertyOp, XPC_SJOW_DelProperty),
    JS_VALUEIFY(js::PropertyOp, XPC_SJOW_GetProperty),
    JS_VALUEIFY(js::PropertyOp, XPC_SJOW_SetProperty),
    XPC_SJOW_Enumerate,
    (JSResolveOp)XPC_SJOW_NewResolve,
    JS_VALUEIFY(js::ConvertOp, XPC_SJOW_Convert),
    XPC_SJOW_Finalize,
    nsnull,   // reserved0
    JS_VALUEIFY(js::CheckAccessOp, XPC_SJOW_CheckAccess),
    JS_VALUEIFY(js::CallOp, XPC_SJOW_Call),
    JS_VALUEIFY(js::CallOp, XPC_SJOW_Create),
    nsnull,   // xdrObject
    nsnull,   // hasInstance
    nsnull,   // mark

    // ClassExtension
    {
      JS_VALUEIFY(js::EqualityOp, XPC_SJOW_Equality),
      nsnull, // outerObject
      nsnull, // innerObject
      XPC_SJOW_Iterator,
      XPC_SJOW_WrappedObject
    }
};

JSBool
WrapObject(JSContext *cx, JSObject *scope, jsval v, jsval *vp)
{
  // This might be redundant if called from XPC_SJOW_Construct, but it should
  // be cheap in that case.
  JSObject *objToWrap = UnsafeUnwrapSecurityWrapper(cx, JSVAL_TO_OBJECT(v));
  if (!objToWrap ||
      JS_TypeOfValue(cx, OBJECT_TO_JSVAL(objToWrap)) == JSTYPE_XML) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  // Prevent script created Script objects from ever being wrapped
  // with XPCSafeJSObjectWrapper, and never let the eval function
  // object be directly wrapped.

  if (objToWrap->getClass() == &js_ScriptClass ||
      (JS_ObjectIsFunction(cx, objToWrap) &&
       JS_GetFunctionNative(cx, JS_ValueToFunction(cx, v)) ==
       XPCWrapper::sEvalNative)) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  XPCWrappedNativeScope *xpcscope =
    XPCWrappedNativeScope::FindInJSObjectScope(cx, scope);
  NS_ASSERTION(xpcscope, "what crazy scope are we in?");

  XPCWrappedNative *wrappedNative;
  WrapperType type = xpcscope->GetWrapperFor(cx, objToWrap, SJOW,
                                             &wrappedNative);

  // NB: We allow XOW here because we're as restrictive as it is (and we know
  // we're same origin here).
  if (type != NONE && type != XOW && !(type & SJOW)) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  SLIM_LOG_WILL_MORPH(cx, objToWrap);
  if (IS_SLIM_WRAPPER(objToWrap) && !MorphSlimWrapper(cx, objToWrap)) {
    return ThrowException(NS_ERROR_FAILURE, cx);
  }

  XPCWrappedNative *wn =
    XPCWrappedNative::GetWrappedNativeOfJSObject(cx, objToWrap);
  if (wn) {
    CheckWindow(wn);
  }

  JSObject *wrapperObj =
    JS_NewObjectWithGivenProto(cx, js::Jsvalify(&SJOWClass), nsnull, scope);

  if (!wrapperObj) {
    // JS_NewObjectWithGivenProto already threw.
    return JS_FALSE;
  }

  *vp = OBJECT_TO_JSVAL(wrapperObj);
  if (!JS_SetReservedSlot(cx, wrapperObj, XPCWrapper::sWrappedObjSlot,
                          OBJECT_TO_JSVAL(objToWrap)) ||
      !JS_SetReservedSlot(cx, wrapperObj, XPCWrapper::sFlagsSlot, JSVAL_ZERO)) {
    return JS_FALSE;
  }

  return JS_TRUE;
}

PRBool
AttachNewConstructorObject(XPCCallContext &ccx, JSObject *aGlobalObject)
{
  // Initialize sEvalNative the first time we attach a constructor.
  // NB: This always happens before any cross origin wrappers are
  // created, so it's OK to do this here.
  if (!XPCWrapper::FindEval(ccx, aGlobalObject)) {
    return PR_FALSE;
  }

  JSObject *class_obj =
    ::JS_InitClass(ccx, aGlobalObject, nsnull, js::Jsvalify(&SJOWClass),
                   XPC_SJOW_Construct, 0, nsnull, nsnull, nsnull, nsnull);
  if (!class_obj) {
    NS_WARNING("can't initialize the XPCSafeJSObjectWrapper class");
    return PR_FALSE;
  }

  if (!::JS_DefineFunction(ccx, class_obj, "toString", XPC_SJOW_toString,
                           0, 0)) {
    return PR_FALSE;
  }

  // Make sure our prototype chain is empty and that people can't mess
  // with XPCSafeJSObjectWrapper.prototype.
  ::JS_SetPrototype(ccx, class_obj, nsnull);
  if (!::JS_FreezeObject(ccx, class_obj)) {
    NS_WARNING("Failed to seal XPCSafeJSObjectWrapper.prototype");
    return PR_FALSE;
  }

  JSBool found;
  return ::JS_SetPropertyAttributes(ccx, aGlobalObject,
                                    SJOWClass.name,
                                    JSPROP_READONLY | JSPROP_PERMANENT,
                                    &found);
}

JSObject *
GetUnsafeObject(JSContext *cx, JSObject *obj)
{
  obj = FindSafeObject(obj);

  if (!obj) {
    return nsnull;
  }

  jsval v;
  if (!JS_GetReservedSlot(cx, obj, XPCWrapper::sWrappedObjSlot, &v)) {
    JS_ClearPendingException(cx);
    return nsnull;
  }

  return JSVAL_IS_OBJECT(v) ? JSVAL_TO_OBJECT(v) : nsnull;
}

} // namespace XPCSafeJSObjectWrapper

static JSObject *
GetScopeChainForSafeCall(JSContext *cx, JSObject *outerObj)
{
  JSObject *unsafeObj = GetUnsafeObject(cx, outerObj);
  JSObject *scopeobj = JS_GetGlobalForObject(cx, unsafeObj);
  OBJ_TO_INNER_OBJECT(cx, scopeobj);
  return scopeobj;
}

// Wrap a JS value in a safe wrapper of a function wrapper if
// needed. Note that rval must point to something rooted when calling
// this function.
static JSBool
WrapJSValue(JSContext *cx, JSObject *obj, jsval val, jsval *rval)
{
  JSBool ok = JS_TRUE;

  if (JSVAL_IS_PRIMITIVE(val)) {
    *rval = val;
  } else {
    if (!RewrapObject(cx, obj->getParent(), JSVAL_TO_OBJECT(val), SJOW,
                      rval)) {
      return JS_FALSE;
    }
    // Construct a new safe wrapper. Note that it doesn't matter what
    // parent we pass in here, the construct hook will ensure we get
    // the right parent for the wrapper.
    JSObject *safeObj = JSVAL_TO_OBJECT(*rval);
    if (safeObj->getClass() == &SJOWClass &&
        JS_GetGlobalForObject(cx, obj) != JS_GetGlobalForObject(cx, safeObj)) {
      // Check to see if the new object we just wrapped is accessible
      // from the unsafe object we got the new object through. If not,
      // force the new wrapper to use the principal of the unsafe
      // object we got the new object from.
      nsCOMPtr<nsIPrincipal> srcObjPrincipal;
      nsCOMPtr<nsIPrincipal> subjPrincipal;
      nsCOMPtr<nsIPrincipal> valObjPrincipal;

      nsresult rv = FindPrincipals(cx, obj, getter_AddRefs(srcObjPrincipal),
                                   getter_AddRefs(subjPrincipal), nsnull);
      if (NS_FAILED(rv)) {
        return ThrowException(rv, cx);
      }

      rv = FindPrincipals(cx, JSVAL_TO_OBJECT(val),
                          getter_AddRefs(valObjPrincipal), nsnull, nsnull);
      if (NS_FAILED(rv)) {
        return ThrowException(rv, cx);
      }

      PRBool subsumes = PR_FALSE;
      rv = srcObjPrincipal->Subsumes(valObjPrincipal, &subsumes);
      if (NS_FAILED(rv)) {
        return ThrowException(rv, cx);
      }

      // If the subject can access both the source and object principals, then
      // don't bother forcing the principal below.
      if (!subsumes && subjPrincipal) {
        PRBool subjSubsumes = PR_FALSE;
        rv = subjPrincipal->Subsumes(srcObjPrincipal, &subjSubsumes);
        if (NS_SUCCEEDED(rv) && subjSubsumes) {
          rv = subjPrincipal->Subsumes(valObjPrincipal, &subjSubsumes);
          if (NS_SUCCEEDED(rv) && subjSubsumes) {
            subsumes = PR_TRUE;
          }
        }
      }

      if (!subsumes) {
        // The unsafe object we got the new object from can not access
        // the new object, force the wrapper we just created to use
        // the principal of the unsafe object to prevent users of the
        // new object wrapper from evaluating code through the new
        // wrapper with the principal of the new object.
        if (!::JS_SetReservedSlot(cx, safeObj, sPrincipalSlot,
                                  PRIVATE_TO_JSVAL(srcObjPrincipal.get()))) {
          return JS_FALSE;
        }

        // Pass on ownership of the new object principal to the
        // wrapper.
        nsIPrincipal *tmp = nsnull;
        srcObjPrincipal.swap(tmp);
      }
    }
  }

  return ok;
}

static JSBool
XPC_SJOW_AddProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  // The constructor and toString properties needs to live on the safe
  // wrapper.
  if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_CONSTRUCTOR) ||
      id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
    return JS_TRUE;
  }

  obj = FindSafeObject(obj);
  NS_ASSERTION(obj != nsnull, "FindSafeObject() returned null in class hook!");

  // Do nothing here if we're in the middle of resolving a property on
  // this safe wrapper.
  jsval isResolving;
  JSBool ok = ::JS_GetReservedSlot(cx, obj, sFlagsSlot, &isResolving);
  if (!ok || HAS_FLAGS(isResolving, FLAG_RESOLVING)) {
    return ok;
  }

  JSObject *unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, obj, unsafeObj)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  if (!JSVAL_IS_PRIMITIVE(*vp)) {
    // Adding an object of some type to the content object, make sure it's
    // properly wrapped.
    JSObject *added = JSVAL_TO_OBJECT(*vp);
    if (!RewrapObject(cx, JS_GetGlobalForObject(cx, unsafeObj), added,
                      UNKNOWN, vp)) {
      return JS_FALSE;
    }
  }

  return XPCWrapper::AddProperty(cx, obj, JS_FALSE, unsafeObj, id, vp);
}

static JSBool
XPC_SJOW_DelProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  JSObject *unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, obj, unsafeObj)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  return XPCWrapper::DelProperty(cx, unsafeObj, id, vp);
}

NS_STACK_CLASS class SafeCallGuard {
public:
  SafeCallGuard(JSContext *cx, nsIPrincipal *principal)
    : cx(cx) {
    nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
    if (ssm) {
      // Note: We pass null as the target frame pointer because we know that
      // we're about to set aside the frame chain.
      nsresult rv = ssm->PushContextPrincipal(cx, nsnull, principal);
      if (NS_FAILED(rv)) {
        NS_WARNING("Not allowing call because we're out of memory");
        JS_ReportOutOfMemory(cx);
        this->cx = nsnull;
        return;
      }
    }

    fp = JS_SaveFrameChain(cx);
    options =
      JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_DONT_REPORT_UNCAUGHT);
  }

  JSBool ready() {
    return cx != nsnull;
  }

  ~SafeCallGuard() {
    if (cx) {
      JS_SetOptions(cx, options);
      JS_RestoreFrameChain(cx, fp);
      nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
      if (ssm) {
        ssm->PopContextPrincipal(cx);
      }
    }
  }

private:
  JSContext *cx;
  uint32 options;
  JSStackFrame *fp;
};

static JSBool
XPC_SJOW_GetOrSetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp,
                          JSBool aIsSet)
{
  // We resolve toString to a function in our resolve hook.
  if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
    return JS_TRUE;
  }

  obj = FindSafeObject(obj);
  NS_ASSERTION(obj != nsnull, "FindSafeObject() returned null in class hook!");

  JSObject *unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, obj, unsafeObj)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  JSObject *scopeChain = GetScopeChainForSafeCall(cx, obj);
  if (!scopeChain) {
    return JS_FALSE;
  }

  {
    SafeCallGuard guard(cx, FindObjectPrincipals(cx, obj, unsafeObj));
    if (!guard.ready()) {
      return JS_FALSE;
    }

    if (aIsSet &&
        !JSVAL_IS_PRIMITIVE(*vp) &&
        !RewrapObject(cx, JS_GetGlobalForObject(cx, unsafeObj),
                      JSVAL_TO_OBJECT(*vp), UNKNOWN, vp)) {
      return JS_FALSE;
    }

    JSBool ok = aIsSet
                ? js_SetPropertyByIdWithFakeFrame(cx, unsafeObj, scopeChain, id, vp)
                : js_GetPropertyByIdWithFakeFrame(cx, unsafeObj, scopeChain, id, vp);
    if (!ok) {
      return JS_FALSE;
    }
  }

  return WrapJSValue(cx, obj, *vp, vp);
}

static JSBool
XPC_SJOW_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  return XPC_SJOW_GetOrSetProperty(cx, obj, id, vp, PR_FALSE);
}

static JSBool
XPC_SJOW_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  return XPC_SJOW_GetOrSetProperty(cx, obj, id, vp, PR_TRUE);
}

static JSBool
XPC_SJOW_Enumerate(JSContext *cx, JSObject *obj)
{
  obj = FindSafeObject(obj);
  NS_ASSERTION(obj != nsnull, "FindSafeObject() returned null in class hook!");

  // We are being notified of a for-in loop or similar operation on
  // this XPCSafeJSObjectWrapper. Forward to the correct high-level
  // object hook, OBJ_ENUMERATE on the unsafe object, called via the
  // JS_Enumerate API.  Then reflect properties named by the
  // enumerated identifiers from the unsafe object to the safe
  // wrapper.

  JSObject *unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    return JS_TRUE;
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, obj, unsafeObj)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  // Since we enumerate using JS_Enumerate() on the unsafe object here
  // we don't need to do a security check since JS_Enumerate() will
  // look up unsafeObj.__iterator__ and if we don't have permission to
  // access that, it'll throw and we'll be safe.

  return XPCWrapper::Enumerate(cx, obj, unsafeObj);
}

static JSBool
XPC_SJOW_NewResolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                    JSObject **objp)
{
  obj = FindSafeObject(obj);
  NS_ASSERTION(obj != nsnull, "FindSafeObject() returned null in class hook!");

  JSObject *unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    // No unsafe object, nothing to resolve here.

    return JS_TRUE;
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, obj, unsafeObj)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  // Resolve toString specially.
  if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
    *objp = obj;
    return JS_DefineFunction(cx, obj, "toString",
                             XPC_SJOW_toString, 0, 0) != nsnull;
  }

  return XPCWrapper::NewResolve(cx, obj, JS_FALSE, unsafeObj, id, flags,
                                objp);
}

static JSBool
XPC_SJOW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
  NS_ASSERTION(type != JSTYPE_STRING, "toString failed us");
  return JS_TRUE;
}

static void
XPC_SJOW_Finalize(JSContext *cx, JSObject *obj)
{
  // Release the reference to the cached principal if we have one.
  jsval v;
  if (::JS_GetReservedSlot(cx, obj, sPrincipalSlot, &v) && !JSVAL_IS_VOID(v)) {
    nsIPrincipal *principal = (nsIPrincipal *)JSVAL_TO_PRIVATE(v);

    NS_RELEASE(principal);
  }
}

static JSBool
XPC_SJOW_CheckAccess(JSContext *cx, JSObject *obj, jsid id,
                     JSAccessMode mode, jsval *vp)
{
  // Prevent setting __proto__ on an XPCSafeJSObjectWrapper
  if ((mode & JSACC_WATCH) == JSACC_PROTO && (mode & JSACC_WRITE)) {
    return ThrowException(NS_ERROR_XPC_SECURITY_MANAGER_VETO, cx);
  }

  // Forward to the checkObjectAccess hook in the runtime, if any.
  JSSecurityCallbacks *callbacks = JS_GetSecurityCallbacks(cx);
  if (callbacks && callbacks->checkObjectAccess &&
      !callbacks->checkObjectAccess(cx, obj, id, mode, vp)) {
    return JS_FALSE;
  }

  JSObject *unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    return JS_TRUE;
  }

  // Forward the unsafe object to the checkObjectAccess hook in the
  // runtime too, if any.
  if (callbacks && callbacks->checkObjectAccess &&
      !callbacks->checkObjectAccess(cx, unsafeObj, id, mode, vp)) {
    return JS_FALSE;
  }

  JSClass *clazz = unsafeObj->getJSClass();
  return !clazz->checkAccess ||
    clazz->checkAccess(cx, unsafeObj, id, mode, vp);
}

static JSBool
XPC_SJOW_Call(JSContext *cx, uintN argc, jsval *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj)
    return JS_FALSE;

  JSObject *tmp = FindSafeObject(obj);
  JSObject *unsafeObj, *callThisObj = nsnull;

  if (tmp) {
    // A function wrapped in an XPCSafeJSObjectWrapper is being called
    // directly (i.e. safeObj.fun()), set obj to be the safe object
    // wrapper. In this case, the "this" object used when calling the
    // function will be the unsafe object gotten off of the safe
    // object.
    obj = tmp;
  } else {
    // A function wrapped in an XPCSafeJSObjectWrapper is being called
    // indirectly off of an object that's not a safe wrapper
    // (i.e. foo.bar = safeObj.fun; foo.bar()), set obj to be the safe
    // wrapper for the function, and use the object passed in as the
    // "this" object when calling the function.
    callThisObj = obj;

    // Check that the caller can access the object we're about to pass
    // in as "this" for the call we're about to make.
    if (!CanCallerAccess(cx, nsnull, callThisObj)) {
      // CanCallerAccess() already threw for us.
      return JS_FALSE;
    }

    obj = FindSafeObject(JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));

    if (!obj) {
      return ThrowException(NS_ERROR_INVALID_ARG, cx);
    }
  }

  unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  if (!callThisObj) {
    callThisObj = unsafeObj;
  }

  JSObject *safeObj = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
  JSObject *funToCall = GetUnsafeObject(cx, safeObj);

  if (!funToCall) {
    // Someone has called XPCSafeJSObjectWrapper.prototype() causing
    // us to find a safe object wrapper without an unsafeObject as
    // its parent. That call shouldn't do anything, so bail here.
    return JS_TRUE;
  }

  // Check that the caller can access the unsafe object on which the
  // call is being made, and the actual function we're about to call.
  if (!CanCallerAccess(cx, safeObj, unsafeObj) ||
      !CanCallerAccess(cx, nsnull, funToCall)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  JSObject *scopeChain = GetScopeChainForSafeCall(cx, safeObj);
  if (!scopeChain) {
    return JS_FALSE;
  }

  {
    SafeCallGuard guard(cx, FindObjectPrincipals(cx, safeObj, funToCall));

    JSObject *scope = JS_GetGlobalForObject(cx, funToCall);
    jsval *argv = JS_ARGV(cx, vp);
    for (uintN i = 0; i < argc; ++i) {
      // NB: Passing NONE for a hint here.
      if (!JSVAL_IS_PRIMITIVE(argv[i]) &&
          !RewrapObject(cx, scope, JSVAL_TO_OBJECT(argv[i]), NONE, &argv[i])) {
        return JS_FALSE;
      }
    }

    jsval v;
    if (!RewrapObject(cx, scope, callThisObj, NONE, &v)) {
      return JS_FALSE;
    }

    if (!js_CallFunctionValueWithFakeFrame(cx, JSVAL_TO_OBJECT(v), scopeChain,
                                           OBJECT_TO_JSVAL(funToCall),
                                           argc, argv, vp)) {
      return JS_FALSE;
    }
  }

  return WrapJSValue(cx, safeObj, *vp, vp);
}

static JSBool
XPC_SJOW_Construct(JSContext *cx, uintN argc, jsval *vp)
{
  if (argc < 1) {
    return ThrowException(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx);
  }

  JSObject *scope = JS_GetGlobalForObject(cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));

  jsval *argv = JS_ARGV(cx, vp);
  if (JSVAL_IS_PRIMITIVE(argv[0])) {
    if (JS_IsConstructing(cx, vp)) {
      return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
    }

    JS_SET_RVAL(cx, vp, argv[0]);
    return JS_TRUE;
  }

  JSObject *objToWrap = UnsafeUnwrapSecurityWrapper(cx, JSVAL_TO_OBJECT(argv[0]));
  if (!objToWrap) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, nsnull, objToWrap)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  return WrapObject(cx, scope, OBJECT_TO_JSVAL(objToWrap), vp);
}

static JSBool
XPC_SJOW_Create(JSContext *cx, uintN argc, jsval *vp)
{
  JSObject *callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
  NS_ASSERTION(GetUnsafeObject(cx, callee), "How'd we get here?");
  JSObject *unsafeObj = GetUnsafeObject(cx, callee);

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, callee, unsafeObj)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  JSObject *scopeChain = GetScopeChainForSafeCall(cx, callee);
  if (!scopeChain) {
    return JS_FALSE;
  }

  {
    SafeCallGuard guard(cx, FindObjectPrincipals(cx, callee, unsafeObj));
    if (!guard.ready()) {
      return JS_FALSE;
    }

    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, unsafeObj)) {
      return JS_FALSE;
    }

    JSObject *scope = JS_GetGlobalForObject(cx, unsafeObj);
    jsval *argv = JS_ARGV(cx, vp);
    for (uintN i = 0; i < argc; ++i) {
      // NB: Passing NONE for a hint here.
      if (!JSVAL_IS_PRIMITIVE(argv[i]) &&
          !RewrapObject(cx, scope, JSVAL_TO_OBJECT(argv[i]), NONE, &argv[i])) {
        return JS_FALSE;
      }
    }

    JSObject *obj = JS_New(cx, unsafeObj, argc, argv);
    if (!obj)
      return JS_FALSE;

    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
  }

  return WrapJSValue(cx, callee, *vp, vp);
}

static JSBool
XPC_SJOW_Equality(JSContext *cx, JSObject *obj, const jsval *valp, JSBool *bp)
{
  if (JSVAL_IS_PRIMITIVE(*valp)) {
    *bp = JS_FALSE;
  } else {
    JSObject *unsafeObj = GetUnsafeObject(cx, obj);

    JSObject *other = JSVAL_TO_OBJECT(*valp);
    JSObject *otherUnsafe = GetUnsafeObject(cx, other);

    // An object is equal to a SJOW if:
    //   - The other object is the same SJOW.
    //   - The other object is the object that this SJOW is wrapping.
    //   - The other object is a SJOW wrapping the same object as this one.
    // or
    //   - Both objects somehow wrap the same native object.
    if (obj == other || unsafeObj == other ||
        (unsafeObj && unsafeObj == otherUnsafe)) {
      *bp = JS_TRUE;
    } else {
      nsISupports *objIdentity = XPC_GetIdentityObject(cx, obj);
      nsISupports *otherIdentity = XPC_GetIdentityObject(cx, other);

      *bp = objIdentity && objIdentity == otherIdentity;
    }
  }

  return JS_TRUE;
}

static JSObject *
XPC_SJOW_Iterator(JSContext *cx, JSObject *obj, JSBool keysonly)
{
  obj = FindSafeObject(obj);
  NS_ASSERTION(obj != nsnull, "FindSafeObject() returned null in class hook!");

  JSObject *unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    ThrowException(NS_ERROR_INVALID_ARG, cx);

    return nsnull;
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, obj, unsafeObj)) {
    // CanCallerAccess() already threw for us.
    return nsnull;
  }

  // Create our dummy SJOW.
  JSObject *wrapperIter =
    JS_NewObjectWithGivenProto(cx, js::Jsvalify(&SJOWClass), nsnull,
                               JS_GetGlobalForObject(cx, obj));
  if (!wrapperIter) {
    return nsnull;
  }

  if (!JS_SetReservedSlot(cx, wrapperIter, XPCWrapper::sWrappedObjSlot,
                          OBJECT_TO_JSVAL(unsafeObj)) ||
      !JS_SetReservedSlot(cx, wrapperIter, XPCWrapper::sFlagsSlot,
                          JSVAL_ZERO)) {
    return nsnull;
  }

  js::AutoObjectRooter tvr(cx, wrapperIter);

  // Initialize the wrapper.
  return XPCWrapper::CreateIteratorObj(cx, wrapperIter, obj, unsafeObj,
                                       keysonly);
}

static JSObject *
XPC_SJOW_WrappedObject(JSContext *cx, JSObject *obj)
{
  return GetUnsafeObject(cx, obj);
}

static JSBool
XPC_SJOW_toString(JSContext *cx, uintN argc, jsval *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    return JS_FALSE;
  }

  obj = FindSafeObject(obj);
  if (!obj) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  JSObject *unsafeObj = GetUnsafeObject(cx, obj);

  if (!unsafeObj) {
    // No unsafe object, nothing to stringify here, return "[object
    // XPCSafeJSObjectWrapper]" so callers know what they're looking
    // at.

    JSString *str = JS_NewStringCopyZ(cx, "[object XPCSafeJSObjectWrapper]");
    if (!str) {
      return JS_FALSE;
    }

    JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(str));
    return JS_TRUE;
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, obj, unsafeObj)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  {
    SafeCallGuard guard(cx, FindObjectPrincipals(cx, obj, unsafeObj));
    if (!guard.ready()) {
      return JS_FALSE;
    }

    JSString *str = JS_ValueToString(cx, OBJECT_TO_JSVAL(unsafeObj));
    if (!str) {
      return JS_FALSE;
    }
    JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(str));
  }
  return JS_TRUE;
}
