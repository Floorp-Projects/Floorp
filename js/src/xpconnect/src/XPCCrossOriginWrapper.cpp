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
#include "XPCWrapper.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowCollection.h"

// This file implements a wrapper around objects that allows them to be
// accessed safely from across origins.

static JSBool
XPC_XOW_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
XPC_XOW_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
XPC_XOW_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
XPC_XOW_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
XPC_XOW_Enumerate(JSContext *cx, JSObject *obj);

static JSBool
XPC_XOW_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                   JSObject **objp);

static JSBool
XPC_XOW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

static void
XPC_XOW_Finalize(JSContext *cx, JSObject *obj);

static JSBool
XPC_XOW_CheckAccess(JSContext *cx, JSObject *obj, jsval id, JSAccessMode mode,
                    jsval *vp);

static JSBool
XPC_XOW_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static JSBool
XPC_XOW_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval);

static JSBool
XPC_XOW_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

static JSBool
XPC_XOW_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

static JSObject *
XPC_XOW_Iterator(JSContext *cx, JSObject *obj, JSBool keysonly);

static JSObject *
XPC_XOW_WrappedObject(JSContext *cx, JSObject *obj);

// The slot that we stick our scope into.
// This is used in the finalizer to see if we actually need to remove
// ourselves from our scope's map. Because we cannot outlive our scope
// (the parent link ensures this), we know that, when we're being
// finalized, either our scope is still alive (i.e. we became garbage
// due to no more references) or it is being garbage collected right now.
// Therefore, we can look in gDyingScopes, and if our scope is there,
// then the map is about to be destroyed anyway, so we don't need to
// do anything.
static const int XPC_XOW_ScopeSlot = XPCWrapper::sNumSlots;
static const int sUXPCObjectSlot = XPCWrapper::sNumSlots + 1;

using namespace XPCWrapper;

// Throws an exception on context |cx|.
static inline
JSBool
ThrowException(nsresult ex, JSContext *cx)
{
  XPCWrapper::DoThrowException(ex, cx);

  return JS_FALSE;
}

// Get the (possibly nonexistent) XOW off of an object
static inline
JSObject *
GetWrapper(JSObject *obj)
{
  while (obj->getClass() != &XPCCrossOriginWrapper::XOWClass.base) {
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
  return XPCWrapper::UnwrapGeneric(cx, &XPCCrossOriginWrapper::XOWClass, wrapper);
}

static JSBool
XPC_XOW_FunctionWrapper(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                        jsval *rval);

// This flag is set on objects that were created for UniversalXPConnect-
// enabled code.
static const PRUint32 FLAG_IS_UXPC_OBJECT = XPCWrapper::LAST_FLAG << 1;

// This flag is set on objects that have to clear their wrapped native's XOW
// cache when they get finalized.
static const PRUint32 FLAG_IS_CACHED = XPCWrapper::LAST_FLAG << 2;

namespace XPCCrossOriginWrapper {

JSExtendedClass XOWClass = {
  // JSClass (JSExtendedClass.base) initialization
  { "XPCCrossOriginWrapper",
    JSCLASS_NEW_RESOLVE | JSCLASS_IS_EXTENDED |
    JSCLASS_HAS_RESERVED_SLOTS(XPCWrapper::sNumSlots + 2),
    XPC_XOW_AddProperty, XPC_XOW_DelProperty,
    XPC_XOW_GetProperty, XPC_XOW_SetProperty,
    XPC_XOW_Enumerate,   (JSResolveOp)XPC_XOW_NewResolve,
    XPC_XOW_Convert,     XPC_XOW_Finalize,
    nsnull,              XPC_XOW_CheckAccess,
    XPC_XOW_Call,        XPC_XOW_Construct,
    nsnull,              XPC_XOW_HasInstance,
    nsnull,              nsnull
  },

  // JSExtendedClass initialization
  XPC_XOW_Equality,
  nsnull,             // outerObject
  nsnull,             // innerObject
  XPC_XOW_Iterator,
  XPC_XOW_WrappedObject,
  JSCLASS_NO_RESERVED_MEMBERS
};

JSBool
WrapperMoved(JSContext *cx, XPCWrappedNative *innerObj,
                     XPCWrappedNativeScope *newScope)
{
  typedef WrappedNative2WrapperMap::Link Link;
  XPCJSRuntime *rt = nsXPConnect::GetRuntimeInstance();
  WrappedNative2WrapperMap *map = innerObj->GetScope()->GetWrapperMap();
  Link *link;

  { // Scoped lock
    XPCAutoLock al(rt->GetMapLock());
    link = map->FindLink(innerObj->GetFlatJSObject());
  }

  if (!link) {
    // No link here means that there were no XOWs for this object.
    return JS_TRUE;
  }

  JSObject *xow = link->obj;

  { // Scoped lock.
    XPCAutoLock al(rt->GetMapLock());
    if (!newScope->GetWrapperMap()->AddLink(innerObj->GetFlatJSObject(), link))
      return JS_FALSE;
    map->Remove(innerObj->GetFlatJSObject());
  }

  if (!xow) {
    // Nothing else to do.
    return JS_TRUE;
  }

  return JS_SetReservedSlot(cx, xow, XPC_XOW_ScopeSlot,
                            PRIVATE_TO_JSVAL(newScope)) &&
         JS_SetParent(cx, xow, newScope->GetGlobalJSObject());
}

void
WindowNavigated(JSContext *cx, XPCWrappedNative *innerObj)
{
  NS_ABORT_IF_FALSE(innerObj->NeedsXOW(), "About to write to unowned memory");

  // First, disconnect the old XOW from the XOW cache.
  XPCWrappedNativeWithXOW *wnxow =
    static_cast<XPCWrappedNativeWithXOW *>(innerObj);
  JSObject *oldXOW = wnxow->GetXOW();
  if (oldXOW) {
    jsval flags = GetFlags(cx, oldXOW);
    NS_ASSERTION(HAS_FLAGS(flags, FLAG_IS_CACHED), "Wrapper should be cached");

    SetFlags(cx, oldXOW, RemoveFlags(flags, FLAG_IS_CACHED));

    NS_ASSERTION(wnxow->GetXOW() == oldXOW, "bad XOW in cache");
    wnxow->SetXOW(nsnull);
  }
}

// Returns whether the currently executing code is allowed to access
// the wrapper.  Uses nsIPrincipal::Subsumes.
// |cx| must be the top context on the context stack.
// If the subject is allowed to access the object returns NS_OK. If not,
// returns NS_ERROR_DOM_PROP_ACCESS_DENIED, returns another error code on
// failure.
nsresult
CanAccessWrapper(JSContext *cx, JSObject *outerObj, JSObject *wrappedObj,
                 JSBool *privilegeEnabled)
{
  // Fast path: If the wrapper and the wrapped object have the same global
  // object (or if the wrapped object is a outer for the same inner that
  // the wrapper is parented to), then we don't need to do any more work.

  if (privilegeEnabled) {
    *privilegeEnabled = JS_FALSE;
  }

  if (outerObj) {
    JSObject *outerParent = outerObj->getParent();
    JSObject *innerParent = wrappedObj->getParent();
    if (!innerParent) {
      innerParent = wrappedObj;
      OBJ_TO_INNER_OBJECT(cx, innerParent);
      if (!innerParent) {
        return NS_ERROR_FAILURE;
      }
    } else {
      innerParent = JS_GetGlobalForObject(cx, innerParent);
    }

    if (outerParent == innerParent) {
      return NS_OK;
    }
  }

  // TODO bug 508928: Refactor this with the XOW security checking code.
  // Get the subject principal from the execution stack.
  nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
  if (!ssm) {
    ThrowException(NS_ERROR_NOT_INITIALIZED, cx);
    return NS_ERROR_NOT_INITIALIZED;
  }

  JSStackFrame *fp = nsnull;
  nsIPrincipal *subjectPrin = ssm->GetCxSubjectPrincipalAndFrame(cx, &fp);

  if (!subjectPrin) {
    ThrowException(NS_ERROR_FAILURE, cx);
    return NS_ERROR_FAILURE;
  }

  PRBool isSystem = PR_FALSE;
  nsresult rv = ssm->IsSystemPrincipal(subjectPrin, &isSystem);
  NS_ENSURE_SUCCESS(rv, rv);

  if (privilegeEnabled) {
    *privilegeEnabled = JS_FALSE;
  }

  nsCOMPtr<nsIPrincipal> objectPrin;
  rv = ssm->GetObjectPrincipal(cx, wrappedObj, getter_AddRefs(objectPrin));
  if (NS_FAILED(rv)) {
    return rv;
  }
  NS_ASSERTION(objectPrin, "Object didn't have principals?");

  // Micro-optimization: don't call into caps if we know the answer.
  if (subjectPrin == objectPrin) {
    return NS_OK;
  }

  // Now, we have our two principals, compare them!
  PRBool subsumes;
  rv = subjectPrin->Subsumes(objectPrin, &subsumes);
  if (NS_SUCCEEDED(rv) && !subsumes) {
    // We're about to fail, but make a last effort to see if
    // UniversalXPConnect was enabled anywhere else on the stack.
    rv = ssm->IsCapabilityEnabled("UniversalXPConnect", &isSystem);
    if (NS_SUCCEEDED(rv) && isSystem) {
      rv = NS_OK;
      if (privilegeEnabled) {
        *privilegeEnabled = JS_TRUE;
      }
    } else {
      rv = NS_ERROR_DOM_PROP_ACCESS_DENIED;
    }
  }
  return rv;
}

JSBool
WrapFunction(JSContext *cx, JSObject *outerObj, JSObject *funobj, jsval *rval)
{
  jsval funobjVal = OBJECT_TO_JSVAL(funobj);
  JSFunction *wrappedFun =
    reinterpret_cast<JSFunction *>(xpc_GetJSPrivate(funobj));
  JSNative native = JS_GetFunctionNative(cx, wrappedFun);
  if (!native || native == XPC_XOW_FunctionWrapper) {
    *rval = funobjVal;
    return JS_TRUE;
  }

  JSFunction *funWrapper =
    JS_NewFunction(cx, XPC_XOW_FunctionWrapper,
                   JS_GetFunctionArity(wrappedFun), 0,
                   JS_GetGlobalForObject(cx, outerObj),
                   JS_GetFunctionName(wrappedFun));
  if (!funWrapper) {
    return JS_FALSE;
  }

  JSObject *funWrapperObj = JS_GetFunctionObject(funWrapper);
  *rval = OBJECT_TO_JSVAL(funWrapperObj);

  if (!JS_SetReservedSlot(cx, funWrapperObj, eWrappedFunctionSlot, funobjVal) ||
      !JS_SetReservedSlot(cx, funWrapperObj, eAllAccessSlot, JSVAL_FALSE)) {
    return JS_FALSE;
  }

  return JS_TRUE;
}

JSBool
RewrapIfNeeded(JSContext *cx, JSObject *outerObj, jsval *vp)
{
  // Don't need to wrap primitive values.
  if (JSVAL_IS_PRIMITIVE(*vp)) {
    return JS_TRUE;
  }

  JSObject *obj = JSVAL_TO_OBJECT(*vp);

  if (JS_ObjectIsFunction(cx, obj)) {
    return WrapFunction(cx, outerObj, obj, vp);
  }

  XPCWrappedNative *wn = nsnull;
  if (obj->getClass() == &XOWClass.base &&
      outerObj->getParent() != obj->getParent()) {
    *vp = OBJECT_TO_JSVAL(GetWrappedObject(cx, obj));
  } else if (!(wn = XPCWrappedNative::GetAndMorphWrappedNativeOfJSObject(cx, obj))) {
    return JS_TRUE;
  }

  return WrapObject(cx, JS_GetGlobalForObject(cx, outerObj), vp, wn);
}

JSBool
WrapObject(JSContext *cx, JSObject *parent, jsval *vp, XPCWrappedNative* wn)
{
  NS_ASSERTION(XPCPerThreadData::IsMainThread(cx),
               "Can't do this off the main thread!");

  // Our argument should be a wrapped native object, but the caller may have
  // passed it in as an optimization.
  JSObject *wrappedObj;
  if (JSVAL_IS_PRIMITIVE(*vp) ||
      !(wrappedObj = JSVAL_TO_OBJECT(*vp)) ||
      wrappedObj->getClass() == &XOWClass.base) {
    return JS_TRUE;
  }

  if (!wn &&
      !(wn = XPCWrappedNative::GetAndMorphWrappedNativeOfJSObject(cx, wrappedObj))) {
    return JS_TRUE;
  }

  // The parent must be the inner global object for its scope.
  parent = JS_GetGlobalForObject(cx, parent);
  OBJ_TO_INNER_OBJECT(cx, parent);
  if (!parent) {
    return JS_FALSE;
  }

  XPCWrappedNativeWithXOW *wnxow = nsnull;
  if (wn->NeedsXOW()) {
    JSObject *innerWrappedObj = wrappedObj;
    OBJ_TO_INNER_OBJECT(cx, innerWrappedObj);
    if (!innerWrappedObj) {
      return JS_FALSE;
    }

    if (innerWrappedObj == parent) {
      wnxow = static_cast<XPCWrappedNativeWithXOW *>(wn);
      JSObject *xow = wnxow->GetXOW();
      if (xow) {
        *vp = OBJECT_TO_JSVAL(xow);
        return JS_TRUE;
      }
    }
  }

  XPCWrappedNative *parentwn =
    XPCWrappedNative::GetWrappedNativeOfJSObject(cx, parent);
  XPCWrappedNativeScope *parentScope;
  if (NS_LIKELY(parentwn)) {
    parentScope = parentwn->GetScope();
  } else {
    parentScope = XPCWrappedNativeScope::FindInJSObjectScope(cx, parent);
  }

  JSObject *outerObj = nsnull;
  WrappedNative2WrapperMap *map = parentScope->GetWrapperMap();

  outerObj = map->Find(wrappedObj);
  if (outerObj) {
    NS_ASSERTION(outerObj->getClass() == &XOWClass.base,
                 "What crazy object are we getting here?");
    *vp = OBJECT_TO_JSVAL(outerObj);

    if (wnxow) {
      // NB: wnxow->GetXOW() must have returned false.
      SetFlags(cx, outerObj, AddFlags(GetFlags(cx, outerObj), FLAG_IS_CACHED));
      wnxow->SetXOW(outerObj);
    }

    return JS_TRUE;
  }

  outerObj = JS_NewObjectWithGivenProto(cx, &XOWClass.base, nsnull,
                                        parent);
  if (!outerObj) {
    return JS_FALSE;
  }

  jsval flags = INT_TO_JSVAL(wnxow ? FLAG_IS_CACHED : 0);
  if (!JS_SetReservedSlot(cx, outerObj, sWrappedObjSlot, *vp) ||
      !JS_SetReservedSlot(cx, outerObj, sFlagsSlot, flags) ||
      !JS_SetReservedSlot(cx, outerObj, XPC_XOW_ScopeSlot,
                          PRIVATE_TO_JSVAL(parentScope))) {
    return JS_FALSE;
  }

  *vp = OBJECT_TO_JSVAL(outerObj);

  map->Add(wn->GetScope()->GetWrapperMap(), wrappedObj, outerObj);
  if(wnxow) {
    wnxow->SetXOW(outerObj);
  }

  return JS_TRUE;
}

} // namespace XPCCrossOriginWrapper

using namespace XPCCrossOriginWrapper;

static JSBool
XPC_XOW_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval);

static JSBool
IsValFrame(JSObject *obj, jsval v, XPCWrappedNative *wn)
{
  // Fast path for the common case.
  if (obj->getClass()->name[0] != 'W') {
    return JS_FALSE;
  }

  nsCOMPtr<nsIDOMWindow> domwin(do_QueryWrappedNative(wn));
  if (!domwin) {
    return JS_FALSE;
  }

  nsCOMPtr<nsIDOMWindowCollection> col;
  domwin->GetFrames(getter_AddRefs(col));
  if (!col) {
    return JS_FALSE;
  }

  if (JSVAL_IS_INT(v)) {
    col->Item(JSVAL_TO_INT(v), getter_AddRefs(domwin));
  } else {
    nsAutoString str(reinterpret_cast<PRUnichar *>
                                     (JS_GetStringChars(JSVAL_TO_STRING(v))));
    col->NamedItem(str, getter_AddRefs(domwin));
  }

  return domwin != nsnull;
}

static JSBool
WrapSameOriginProp(JSContext *cx, JSObject *outerObj, jsval *vp);

static JSBool
XPC_XOW_FunctionWrapper(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                        jsval *rval)
{
  JSObject *wrappedObj, *outerObj = obj;

  // Allow 'this' to be either an XOW, in which case we unwrap it.
  // We disallow invalid XOWs that have no wrapped object. Otherwise,
  // if it isn't an XOW, then pass it through as-is.

  outerObj = wrappedObj = GetWrapper(obj);
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

  JSFunction *fun = JS_ValueToFunction(cx, funToCall);
  if (!fun) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  XPCCallContext ccx(JS_CALLER, cx);
  if (!ccx.IsValid()) {
    return ThrowException(NS_ERROR_FAILURE, cx);
  }

  nsresult rv = CanAccessWrapper(cx, outerObj, JSVAL_TO_OBJECT(funToCall), nsnull);
  if (NS_FAILED(rv) && rv != NS_ERROR_DOM_PROP_ACCESS_DENIED) {
    return ThrowException(rv, cx);
  }

#ifdef DEBUG
  JSNative native = JS_GetFunctionNative(cx, fun);
  NS_ASSERTION(native, "How'd we get here with a scripted function?");
#endif

  if (!JS_CallFunctionValue(cx, wrappedObj, funToCall, argc, argv, rval)) {
    return JS_FALSE;
  }

  if (NS_SUCCEEDED(rv)) {
    return WrapSameOriginProp(cx, outerObj, rval);
  }

  return RewrapIfNeeded(cx, obj, rval);
}

static JSBool
WrapSameOriginProp(JSContext *cx, JSObject *outerObj, jsval *vp)
{
  // Don't call RewrapIfNeeded for same origin properties. We only
  // need to wrap window, document and location.
  if (JSVAL_IS_PRIMITIVE(*vp)) {
    return JS_TRUE;
  }

  JSObject *wrappedObj = JSVAL_TO_OBJECT(*vp);
  JSClass *clasp = wrappedObj->getClass();
  if (ClassNeedsXOW(clasp->name)) {
    return WrapObject(cx, JS_GetGlobalForObject(cx, outerObj), vp);
  }

  // Check if wrappedObj is an XOW. If so, verify that it's from the
  // right scope.
  if (clasp == &XOWClass.base &&
      wrappedObj->getParent() != outerObj->getParent()) {
    *vp = OBJECT_TO_JSVAL(GetWrappedObject(cx, wrappedObj));
    return WrapObject(cx, outerObj->getParent(), vp);
  }

  return JS_TRUE;
}

static JSBool
XPC_XOW_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  // All AddProperty needs to do is pass on addProperty requests to
  // same-origin objects, and throw for all else.

  obj = GetWrapper(obj);
  jsval resolving;
  if (!JS_GetReservedSlot(cx, obj, sFlagsSlot, &resolving)) {
    return JS_FALSE;
  }

  if (!JSVAL_IS_PRIMITIVE(*vp)) {
    JSObject *addedObj = JSVAL_TO_OBJECT(*vp);
    if (addedObj->getClass() == &XOWClass.base &&
        addedObj->getParent() != obj->getParent()) {
      *vp = OBJECT_TO_JSVAL(GetWrappedObject(cx, addedObj));
      if (!WrapObject(cx, obj->getParent(), vp, nsnull)) {
        return JS_FALSE;
      }
    }
  }

  if (HAS_FLAGS(resolving, FLAG_RESOLVING)) {
    // Allow us to define a property on ourselves.
    return JS_TRUE;
  }

  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  JSBool privilegeEnabled = JS_FALSE;
  nsresult rv = CanAccessWrapper(cx, obj, wrappedObj, &privilegeEnabled);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      // Can't override properties on foreign objects.
      return ThrowException(rv, cx);
    }
    return JS_FALSE;
  }

  // Same origin, pass this request along.
  return AddProperty(cx, obj, JS_TRUE, wrappedObj, id, vp);
}

static JSBool
XPC_XOW_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  nsresult rv = CanAccessWrapper(cx, obj, wrappedObj, nsnull);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      // Can't delete properties on foreign objects.
      return ThrowException(rv, cx);
    }
    return JS_FALSE;
  }

  // Same origin, pass this request along.
  return DelProperty(cx, wrappedObj, id, vp);
}

static JSBool
XPC_XOW_GetOrSetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp,
                         JSBool isSet)
{
  if (id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
    return JS_TRUE;
  }

  // Don't do anything if we already resolved to a wrapped function in
  // NewResolve. In practice, this means that this is a wrapped eval
  // function.
  jsval v = *vp;
  if (!JSVAL_IS_PRIMITIVE(v) &&
      JS_ObjectIsFunction(cx, JSVAL_TO_OBJECT(v)) &&
      JS_GetFunctionNative(cx, JS_ValueToFunction(cx, v)) ==
      XPC_XOW_FunctionWrapper) {
    return JS_TRUE;
  }

  JSObject *origObj = obj;
  obj = GetWrapper(obj);
  if (!obj) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  js::AutoArrayRooter rooter(cx, 1, vp);

  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  JSBool privilegeEnabled;
  nsresult rv = CanAccessWrapper(cx, obj, wrappedObj, &privilegeEnabled);
  if (NS_FAILED(rv)) {
    if (rv != NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      return JS_FALSE;
    }

    XPCCallContext ccx(JS_CALLER, cx);
    if (!ccx.IsValid()) {
      return ThrowException(NS_ERROR_FAILURE, cx);
    }

    // This is a request to get a property across origins. We need to
    // determine if this property is allAccess. If it is, then we need to
    // actually get the property. If not, we simply need to throw an
    // exception.

    XPCWrappedNative *wn =
      XPCWrappedNative::GetWrappedNativeOfJSObject(cx, wrappedObj);
    NS_ASSERTION(wn, "How did we wrap a non-WrappedNative?");
    if (!IsValFrame(wrappedObj, id, wn)) {
      nsIScriptSecurityManager *ssm = GetSecurityManager();
      if (!ssm) {
        return ThrowException(NS_ERROR_NOT_INITIALIZED, cx);
      }
      rv = ssm->CheckPropertyAccess(cx, wrappedObj,
                                    wrappedObj->getClass()->name,
                                    id, isSet ? sSecMgrSetProp
                                              : sSecMgrGetProp);
      if (NS_FAILED(rv)) {
        // The security manager threw an exception for us.
        return JS_FALSE;
      }
    }

    return GetOrSetNativeProperty(cx, obj, wn, id, vp, isSet, JS_FALSE);
  }

  JSObject *proto = nsnull; // Initialize this to quiet GCC.
  JSBool checkProto =
    (isSet && id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_PROTO));
  if (checkProto) {
    proto = wrappedObj->getProto();
  }

  // Same origin, pass this request along as though nothing interesting
  // happened.
  jsid asId;

  if (!JS_ValueToId(cx, id, &asId)) {
    return JS_FALSE;
  }

  JSBool ok = isSet
              ? JS_SetPropertyById(cx, wrappedObj, asId, vp)
              : JS_GetPropertyById(cx, wrappedObj, asId, vp);
  if (!ok) {
    return JS_FALSE;
  }

  if (checkProto) {
    JSObject *newProto = wrappedObj->getProto();

    // If code is trying to set obj.__proto__ and we're on obj's
    // prototype chain, then the JS_GetPropertyById above will do the
    // wrong thing if wrappedObj still delegates to Object.prototype.
    // However, it's hard to figure out if wrappedObj still does
    // delegate to Object.prototype so check to see if proto changed as a
    // result of setting __proto__.

    if (origObj != obj) {
      // Undo the damage.
      if (!JS_SetPrototype(cx, wrappedObj, proto) ||
          !JS_SetPrototype(cx, origObj, newProto)) {
        return JS_FALSE;
      }
    } else if (newProto) {
      // __proto__ setting is a bad hack, people shouldn't do it. In
      // this case we're setting the direct prototype of a XOW object,
      // in the interests of sanity only allow it to be set to null in
      // this case.

      JS_SetPrototype(cx, wrappedObj, proto);
      JS_ReportError(cx, "invalid __proto__ value (can only be set to null)");
      return JS_FALSE;
    }
  }

  return WrapSameOriginProp(cx, obj, vp);
}

static JSBool
XPC_XOW_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return XPC_XOW_GetOrSetProperty(cx, obj, id, vp, JS_FALSE);
}

static JSBool
XPC_XOW_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return XPC_XOW_GetOrSetProperty(cx, obj, id, vp, JS_TRUE);
}

static JSBool
XPC_XOW_Enumerate(JSContext *cx, JSObject *obj)
{
  obj = GetWrapper(obj);
  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    // Nothing to enumerate.
    return JS_TRUE;
  }

  nsresult rv = CanAccessWrapper(cx, obj, wrappedObj, nsnull);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      // Can't enumerate on foreign objects.
      return ThrowException(rv, cx);
    }

    return JS_FALSE;
  }

  return Enumerate(cx, obj, wrappedObj);
}

// Because of the drastically different ways that same- and cross-origin XOWs
// work, we have to call JS_ClearScope when a XOW changes from being same-
// origin to cross-origin. Normally, there are defined places in Gecko where
// this happens and they notify us. However, UniversalXPConnect causes the
// same transition without any notifications. We could try to detect when this
// happens, but doing so would require calling JS_ClearScope from random
// hooks, which is bad.
//
// The compromise is the UXPCObject. When resolving a property on a XOW as
// same-origin because of UniversalXPConnect, we actually resolve it on the
// UXPCObject (which is just a XOW for the same object). This causes the JS
// engine to do all of its work on another object, not polluting the main
// object. However, if the get results in calling a setter, the engine still
// uses the regular object as 'this', ensuring that the UXPCObject doesn't
// leak to script.
static JSObject *
GetUXPCObject(JSContext *cx, JSObject *obj)
{
  NS_ASSERTION(obj->getClass() == &XOWClass.base, "wrong object");

  jsval v;
  if (!JS_GetReservedSlot(cx, obj, sFlagsSlot, &v)) {
    return nsnull;
  }

  if (HAS_FLAGS(v, FLAG_IS_UXPC_OBJECT)) {
    return obj;
  }

  if (!JS_GetReservedSlot(cx, obj, sUXPCObjectSlot, &v)) {
    return nsnull;
  }

  if (JSVAL_IS_OBJECT(v)) {
    return JSVAL_TO_OBJECT(v);
  }

  JSObject *uxpco =
    JS_NewObjectWithGivenProto(cx, &XOWClass.base, nsnull, obj->getParent());
  if (!uxpco) {
    return nsnull;
  }

  js::AutoValueRooter tvr(cx, uxpco);

  jsval wrappedObj, parentScope;
  if (!JS_GetReservedSlot(cx, obj, sWrappedObjSlot, &wrappedObj) ||
      !JS_GetReservedSlot(cx, obj, XPC_XOW_ScopeSlot, &parentScope)) {
    return nsnull;
  }

  if (!JS_SetReservedSlot(cx, uxpco, sWrappedObjSlot, wrappedObj) ||
      !JS_SetReservedSlot(cx, uxpco, sFlagsSlot,
                          INT_TO_JSVAL(FLAG_IS_UXPC_OBJECT)) ||
      !JS_SetReservedSlot(cx, uxpco, XPC_XOW_ScopeSlot, parentScope)) {
    return nsnull;
  }

  if (!JS_SetReservedSlot(cx, obj, sUXPCObjectSlot, OBJECT_TO_JSVAL(uxpco))) {
    return nsnull;
  }

  return uxpco;
}

static JSBool
XPC_XOW_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                   JSObject **objp)
{
  obj = GetWrapper(obj);

  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    // No wrappedObj means that this is probably the prototype.
    *objp = nsnull;
    return JS_TRUE;
  }

  JSBool privilegeEnabled;
  nsresult rv = CanAccessWrapper(cx, obj, wrappedObj, &privilegeEnabled);
  if (NS_FAILED(rv)) {
    if (rv != NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      return JS_FALSE;
    }

    XPCCallContext ccx(JS_CALLER, cx);
    if (!ccx.IsValid()) {
      return ThrowException(NS_ERROR_FAILURE, cx);
    }

    // We're dealing with a cross-origin lookup. Ensure that we're allowed to
    // resolve this property and resolve it if so. Otherwise, we deny access
    // and throw a security error. Note that this code does not actually check
    // to see if the property exists, that's dealt with below.

    XPCWrappedNative *wn =
      XPCWrappedNative::GetWrappedNativeOfJSObject(cx, wrappedObj);
    NS_ASSERTION(wn, "How did we wrap a non-WrappedNative?");
    if (!IsValFrame(wrappedObj, id, wn)) {
      nsIScriptSecurityManager *ssm = GetSecurityManager();
      if (!ssm) {
        return ThrowException(NS_ERROR_NOT_INITIALIZED, cx);
      }

      PRUint32 action = (flags & JSRESOLVE_ASSIGNING)
                        ? sSecMgrSetProp
                        : sSecMgrGetProp;
      rv = ssm->CheckPropertyAccess(cx, wrappedObj,
                                    wrappedObj->getClass()->name,
                                    id, action);
      if (NS_FAILED(rv)) {
        // The security manager threw an exception for us.
        return JS_FALSE;
      }
    }

    // We're out! We're allowed to resolve this property.
    return ResolveNativeProperty(cx, obj, wrappedObj, wn, id,
                                 flags, objp, JS_FALSE);

  }

  if (privilegeEnabled && !(obj = GetUXPCObject(cx, obj))) {
    return JS_FALSE;
  }

  if (id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
    jsval oldSlotVal;
    if (!JS_GetReservedSlot(cx, obj, sFlagsSlot, &oldSlotVal) ||
        !JS_SetReservedSlot(cx, obj, sFlagsSlot,
                            INT_TO_JSVAL(JSVAL_TO_INT(oldSlotVal) |
                                         FLAG_RESOLVING))) {
      return JS_FALSE;
    }

    JSBool ok = JS_DefineFunction(cx, obj, "toString",
                                  XPC_XOW_toString, 0, 0) != nsnull;

    JS_SetReservedSlot(cx, obj, sFlagsSlot, oldSlotVal);

    if (ok) {
      *objp = obj;
    }

    return ok;
  }

  return NewResolve(cx, obj, JS_TRUE, wrappedObj, id, flags, objp);
}

static JSBool
XPC_XOW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
  // Don't do any work to convert to object.
  if (type == JSTYPE_OBJECT) {
    *vp = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
  }

  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    // Converting the prototype to something.

    if (type == JSTYPE_STRING || type == JSTYPE_VOID) {
      return XPC_XOW_toString(cx, obj, 0, nsnull, vp);
    }

    *vp = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
  }

  XPCCallContext ccx(JS_CALLER, cx);
  if (!ccx.IsValid()) {
    return ThrowException(NS_ERROR_FAILURE, cx);
  }

  // Note: JSTYPE_VOID and JSTYPE_STRING are equivalent.
  nsresult rv = CanAccessWrapper(cx, obj, wrappedObj, nsnull);
  if (NS_FAILED(rv) &&
      (rv != NS_ERROR_DOM_PROP_ACCESS_DENIED ||
       (type != JSTYPE_STRING && type != JSTYPE_VOID))) {
    // Ensure that we report some kind of error.
    if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      ThrowException(rv, cx);
    }
    return JS_FALSE;
  }

  if (!wrappedObj->getClass()->convert(cx, wrappedObj, type, vp)) {
    return JS_FALSE;
  }

  return NS_SUCCEEDED(rv)
         ? WrapSameOriginProp(cx, obj, vp)
         : RewrapIfNeeded(cx, obj, vp);
}

static void
XPC_XOW_Finalize(JSContext *cx, JSObject *obj)
{
  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    return;
  }

  jsval flags = GetFlags(cx, obj);
  if (HAS_FLAGS(flags, FLAG_IS_CACHED)) {
    // We know that it's safe to access our wrapped object.
    XPCWrappedNativeWithXOW *wnxow =
      static_cast<XPCWrappedNativeWithXOW *>(xpc_GetJSPrivate(wrappedObj));
    wnxow->SetXOW(nsnull);

    JS_SetReservedSlot(cx, obj, sWrappedObjSlot, JSVAL_VOID);
    SetFlags(cx, obj, RemoveFlags(flags, FLAG_IS_CACHED));
  }

  // Get our scope.
  jsval scopeVal;
  if (!JS_GetReservedSlot(cx, obj, XPC_XOW_ScopeSlot, &scopeVal)) {
    return;
  }

  // Now that we have our scope, see if it's going away. If it is,
  // then our work here is going to be done when we destroy the scope
  // entirely. Scope can be null if we're an enumerating XOW.
  XPCWrappedNativeScope *scope = reinterpret_cast<XPCWrappedNativeScope *>
                                                 (JSVAL_TO_PRIVATE(scopeVal));
  if (!scope) {
    return;
  }

  // Remove ourselves from the map.
  scope->GetWrapperMap()->Remove(wrappedObj);
}

static JSBool
XPC_XOW_CheckAccess(JSContext *cx, JSObject *obj, jsval prop, JSAccessMode mode,
                    jsval *vp)
{
  // Simply forward checkAccess to our wrapped object. It's already expecting
  // untrusted things to ask it about accesses.

  uintN junk;
  jsid id;
  return JS_ValueToId(cx, prop, &id) &&
         JS_CheckAccess(cx, GetWrappedObject(cx, obj), id, mode, vp, &junk);
}

static JSBool
XPC_XOW_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    // Nothing to call.
    return JS_TRUE;
  }

  nsresult rv = CanAccessWrapper(cx, obj, wrappedObj, nsnull);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      // Can't call.
      return ThrowException(rv, cx);
    }

    return JS_FALSE;
  }

  JSObject *callee = JSVAL_TO_OBJECT(argv[-2]);
  NS_ASSERTION(GetWrappedObject(cx, callee), "How'd we get here?");
  callee = GetWrappedObject(cx, callee);
  if (!JS_CallFunctionValue(cx, obj, OBJECT_TO_JSVAL(callee), argc, argv,
                            rval)) {
    return JS_FALSE;
  }

  return RewrapIfNeeded(cx, callee, rval);
}

static JSBool
XPC_XOW_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval)
{
  JSObject *realObj = GetWrapper(JSVAL_TO_OBJECT(argv[-2]));
  JSObject *wrappedObj = GetWrappedObject(cx, realObj);
  if (!wrappedObj) {
    // Nothing to construct.
    return JS_TRUE;
  }

  nsresult rv = CanAccessWrapper(cx, realObj, wrappedObj, nsnull);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      // Can't construct.
      return ThrowException(rv, cx);
    }
    return JS_FALSE;
  }

  if (!JS_CallFunctionValue(cx, obj, OBJECT_TO_JSVAL(wrappedObj), argc, argv,
                            rval)) {
    return JS_FALSE;
  }

  return RewrapIfNeeded(cx, wrappedObj, rval);
}

static JSBool
XPC_XOW_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
  JSObject *iface = GetWrappedObject(cx, obj);

  XPCCallContext ccx(JS_CALLER, cx);
  if (!ccx.IsValid()) {
    return ThrowException(NS_ERROR_FAILURE, cx);
  }

  nsresult rv = CanAccessWrapper(cx, obj, iface, nsnull);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      // Don't do this test across origins.
      return ThrowException(rv, cx);
    }
    return JS_FALSE;
  }

  JSClass *clasp = iface->getClass();

  *bp = JS_FALSE;
  if (!clasp->hasInstance) {
    return JS_TRUE;
  }

  // Prematurely unwrap the left hand side.
  if (!JSVAL_IS_PRIMITIVE(v)) {
    JSObject *test = JSVAL_TO_OBJECT(v);

    // GetWrappedObject does an instanceof check.
    test = GetWrappedObject(cx, test);
    if (test) {
      v = OBJECT_TO_JSVAL(test);
    }
  }

  return clasp->hasInstance(cx, iface, v, bp);
}

static JSBool
XPC_XOW_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
  // Convert both sides to XPCWrappedNative and see if they match.
  if (JSVAL_IS_PRIMITIVE(v)) {
    *bp = JS_FALSE;
    return JS_TRUE;
  }

  JSObject *test = JSVAL_TO_OBJECT(v);
  if (test->getClass() == &XOWClass.base) {
    if (!JS_GetReservedSlot(cx, test, sWrappedObjSlot, &v)) {
      return JS_FALSE;
    }

    if (JSVAL_IS_PRIMITIVE(v)) {
      *bp = JS_FALSE;
      return JS_TRUE;
    }

    test = JSVAL_TO_OBJECT(v);
  }

  obj = GetWrappedObject(cx, obj);
  if (!obj) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }
  XPCWrappedNative *other =
    XPCWrappedNative::GetWrappedNativeOfJSObject(cx, test);
  if (!other) {
    *bp = JS_FALSE;
    return JS_TRUE;
  }

  XPCWrappedNative *me = XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);
  obj = me->GetFlatJSObject();
  test = other->GetFlatJSObject();
  return ((JSExtendedClass *)obj->getClass())->
    equality(cx, obj, OBJECT_TO_JSVAL(test), bp);
}

static JSObject *
XPC_XOW_Iterator(JSContext *cx, JSObject *obj, JSBool keysonly)
{
  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    ThrowException(NS_ERROR_INVALID_ARG, cx);
    return nsnull;
  }

  XPCCallContext ccx(JS_CALLER, cx);
  if (!ccx.IsValid()) {
    ThrowException(NS_ERROR_FAILURE, cx);
    return nsnull;
  }

  nsresult rv = CanAccessWrapper(cx, obj, wrappedObj, nsnull);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      // Can't create iterators for foreign objects.
      ThrowException(rv, cx);
      return nsnull;
    }

    ThrowException(NS_ERROR_FAILURE, cx);
    return nsnull;
  }

  JSObject *wrapperIter = JS_NewObject(cx, &XOWClass.base, nsnull,
                                       JS_GetGlobalForObject(cx, obj));
  if (!wrapperIter) {
    return nsnull;
  }

  js::AutoObjectRooter tvr(cx, wrapperIter);

  // Initialize our XOW.
  jsval v = OBJECT_TO_JSVAL(wrappedObj);
  if (!JS_SetReservedSlot(cx, wrapperIter, sWrappedObjSlot, v) ||
      !JS_SetReservedSlot(cx, wrapperIter, sFlagsSlot, JSVAL_ZERO) ||
      !JS_SetReservedSlot(cx, wrapperIter, XPC_XOW_ScopeSlot,
                          PRIVATE_TO_JSVAL(nsnull))) {
    return nsnull;
  }

  return CreateIteratorObj(cx, wrapperIter, obj, wrappedObj, keysonly);
}

static JSObject *
XPC_XOW_WrappedObject(JSContext *cx, JSObject *obj)
{
  return GetWrappedObject(cx, obj);
}

static JSBool
XPC_XOW_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
  obj = GetWrapper(obj);
  if (!obj) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    // Someone's calling toString on our prototype.
    NS_NAMED_LITERAL_CSTRING(protoString, "[object XPCCrossOriginWrapper]");
    JSString *str =
      JS_NewStringCopyN(cx, protoString.get(), protoString.Length());
    if (!str) {
      return JS_FALSE;
    }
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
  }

  XPCCallContext ccx(JS_CALLER, cx);
  if (!ccx.IsValid()) {
    return ThrowException(NS_ERROR_FAILURE, cx);
  }

  nsresult rv = CanAccessWrapper(cx, obj, wrappedObj, nsnull);
  if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
    nsIScriptSecurityManager *ssm = GetSecurityManager();
    if (!ssm) {
      return ThrowException(NS_ERROR_NOT_INITIALIZED, cx);
    }
    rv = ssm->CheckPropertyAccess(cx, wrappedObj,
                                  wrappedObj->getClass()->name,
                                  GetRTStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING),
                                  nsIXPCSecurityManager::ACCESS_GET_PROPERTY);
  }
  if (NS_FAILED(rv)) {
    return JS_FALSE;
  }

  XPCWrappedNative *wn =
    XPCWrappedNative::GetWrappedNativeOfJSObject(cx, wrappedObj);
  return NativeToString(cx, wn, argc, argv, rval, JS_FALSE);
}
