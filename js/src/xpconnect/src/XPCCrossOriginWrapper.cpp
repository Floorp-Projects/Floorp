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
#include "jsobj.h"    // For OBJ_GET_PROPERTY.
#include "jscntxt.h"  // For JSAutoTempValueRooter.
#include "XPCWrapper.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowCollection.h"

// This file implements a wrapper around objects that allows them to be
// accessed safely from across origins.

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_Enumerate(JSContext *cx, JSObject *obj);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                   JSObject **objp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

JS_STATIC_DLL_CALLBACK(void)
XPC_XOW_Finalize(JSContext *cx, JSObject *obj);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_CheckAccess(JSContext *cx, JSObject *obj, jsval id, JSAccessMode mode,
                    jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

JS_STATIC_DLL_CALLBACK(JSObject *)
XPC_XOW_Iterator(JSContext *cx, JSObject *obj, JSBool keysonly);

JSExtendedClass sXPC_XOW_JSClass = {
  // JSClass (JSExtendedClass.base) initialization
  { "XPCCrossOriginWrapper",
    JSCLASS_NEW_RESOLVE | JSCLASS_IS_EXTENDED |
    JSCLASS_HAS_RESERVED_SLOTS(XPCWrapper::sNumSlots + 1),
    XPC_XOW_AddProperty, XPC_XOW_DelProperty,
    XPC_XOW_GetProperty, XPC_XOW_SetProperty,
    XPC_XOW_Enumerate,   (JSResolveOp)XPC_XOW_NewResolve,
    XPC_XOW_Convert,     XPC_XOW_Finalize,
    nsnull,              XPC_XOW_CheckAccess,
    XPC_XOW_Call,        XPC_XOW_Construct,
    nsnull,              nsnull,
    nsnull,              nsnull
  },

  // JSExtendedClass initialization
  XPC_XOW_Equality,
  nsnull,             // outerObject
  nsnull,             // innerObject
  XPC_XOW_Iterator,
  JSCLASS_NO_RESERVED_MEMBERS
};

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

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval);

// Throws an exception on context |cx|.
static inline
JSBool
ThrowException(nsresult ex, JSContext *cx)
{
  XPCThrower::Throw(ex, cx);

  return JS_FALSE;
}

// Get the (possibly non-existant) XOW off of an object
static inline
JSObject *
GetWrapper(JSContext *cx, JSObject *obj)
{
  while (JS_GET_CLASS(cx, obj) != &sXPC_XOW_JSClass.base) {
    obj = JS_GetPrototype(cx, obj);
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
  if (JS_GET_CLASS(cx, wrapper) != &sXPC_XOW_JSClass.base) {
    return nsnull;
  }

  jsval v;
  if (!JS_GetReservedSlot(cx, wrapper, XPCWrapper::sWrappedObjSlot, &v)) {
    JS_ClearPendingException(cx);
    return nsnull;
  }

  if (!JSVAL_IS_OBJECT(v)) {
    return nsnull;
  }

  return JSVAL_TO_OBJECT(v);
}

static inline
nsIScriptSecurityManager *
GetSecurityManager(JSContext *cx)
{
  XPCCallContext ccx(JS_CALLER, cx);
  NS_ENSURE_TRUE(ccx.IsValid(), nsnull);

  // XXX HOOK_CALL_METHOD seems wrong.
  nsCOMPtr<nsIXPCSecurityManager> sm = ccx.GetXPCContext()->
    GetAppropriateSecurityManager(nsIXPCSecurityManager::HOOK_CALL_METHOD);

  nsCOMPtr<nsIScriptSecurityManager> ssm(do_QueryInterface(sm));

  // This Releases, but that's OK, since XPConnect holds a reference to it.
  return ssm;
}

static JSBool
IsValFrame(JSContext *cx, JSObject *obj, jsval v, XPCWrappedNative *wn)
{
  // Fast path for the common case.
  if (JS_GET_CLASS(cx, obj)->name[0] != 'W') {
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

// Returns whether the currently executing code has the same origin as the
// wrapper. Uses nsIScriptSecurityManager::CheckSameOriginPrincipal.
// |cx| must be the top context on the context stack.
// If the two principals have the same origin, returns NS_OK. If they differ,
// returns NS_ERROR_DOM_PROP_ACCESS_DENIED, returns another error code on
// failure.
nsresult
IsWrapperSameOrigin(JSContext *cx, JSObject *wrappedObj)
{
  nsCOMPtr<nsIPrincipal> subjectPrin, objectPrin;

  // Get the subject principal from the execution stack.
  nsIScriptSecurityManager *ssm = GetSecurityManager(cx);
  if (!ssm) {
    ThrowException(NS_ERROR_NOT_INITIALIZED, cx);
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = ssm->GetSubjectPrincipal(getter_AddRefs(subjectPrin));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!subjectPrin) {
    ThrowException(NS_ERROR_FAILURE, cx);
    return NS_ERROR_FAILURE;
  }

  PRBool isSystem = PR_FALSE;
  rv = ssm->IsSystemPrincipal(subjectPrin, &isSystem);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we somehow end up being called from chrome, just allow full access.
  // This can happen from components with xpcnativewrappers=no.
  if (isSystem) {
    return NS_OK;
  }

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
  return ssm->CheckSameOriginPrincipal(subjectPrin, objectPrin);
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

  wrappedObj = GetWrapper(cx, obj);
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
  if (!JS_GetReservedSlot(cx, funObj, 0, &funToCall)) {
    return JS_FALSE;
  }

  JSFunction *fun = JS_ValueToFunction(cx, funToCall);
  if (!fun) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  nsresult rv = IsWrapperSameOrigin(cx, JSVAL_TO_OBJECT(funToCall));
  if (NS_FAILED(rv) && rv != NS_ERROR_DOM_PROP_ACCESS_DENIED) {
    return ThrowException(rv, cx);
  }

  JSNative native = JS_GetFunctionNative(cx, fun);
  NS_ASSERTION(native, "How'd we get here with a scripted function?");

  // A trick! Calling the native directly doesn't push the native onto the
  // JS stack, so interested onlookers will only see us, meaning that they
  // will compute *our* subject principal.

  argv[-2] = funToCall;
  argv[-1] = OBJECT_TO_JSVAL(wrappedObj);
  if (!native(cx, wrappedObj, argc, argv, rval)) {
    return JS_FALSE;
  }

  if (NS_SUCCEEDED(rv)) {
    return WrapSameOriginProp(cx, outerObj, rval);
  }

  return XPC_XOW_RewrapIfNeeded(cx, obj, rval);
}

static JSBool
WrapSameOriginProp(JSContext *cx, JSObject *outerObj, jsval *vp)
{
  // Don't call XPC_XOW_RewrapIfNeeded for same origin properties. We only
  // need to wrap window, document and location.
  if (JSVAL_IS_PRIMITIVE(*vp)) {
    return JS_TRUE;
  }

  JSObject *wrappedObj = JSVAL_TO_OBJECT(*vp);
  JSClass *clasp = JS_GET_CLASS(cx, wrappedObj);
  if (XPC_XOW_ClassNeedsXOW(clasp->name)) {
    return XPC_XOW_WrapObject(cx, JS_GetGlobalForObject(cx, outerObj), vp);
  }

  // Check if wrappedObj is an XOW. If so, verify that it's from the
  // right scope.
  if (clasp == &sXPC_XOW_JSClass.base &&
      JS_GetParent(cx, wrappedObj) != JS_GetParent(cx, outerObj)) {
    *vp = OBJECT_TO_JSVAL(GetWrappedObject(cx, wrappedObj));
    return XPC_XOW_WrapObject(cx, JS_GetParent(cx, outerObj), vp);
  }

  if (JS_ObjectIsFunction(cx, wrappedObj) &&
      JS_GetFunctionNative(cx, reinterpret_cast<JSFunction *>
                                               (JS_GetPrivate(cx, wrappedObj))) ==
      XPCWrapper::sEvalNative) {
    return XPC_XOW_WrapFunction(cx, outerObj, wrappedObj, vp);
  }

  return JS_TRUE;
}

JSBool
XPC_XOW_WrapFunction(JSContext *cx, JSObject *outerObj, JSObject *funobj,
                     jsval *rval)
{
  jsval funobjVal = OBJECT_TO_JSVAL(funobj);
  JSFunction *wrappedFun = reinterpret_cast<JSFunction *>(JS_GetPrivate(cx, funobj));
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
  if (!JS_SetReservedSlot(cx, funWrapperObj, 0, funobjVal)) {
    return JS_FALSE;
  }

  *rval = OBJECT_TO_JSVAL(funWrapperObj);
  return JS_TRUE;
}

JSBool
XPC_XOW_RewrapIfNeeded(JSContext *cx, JSObject *outerObj, jsval *vp)
{
  // Don't need to wrap primitive values.
  if (JSVAL_IS_PRIMITIVE(*vp)) {
    return JS_TRUE;
  }

  JSObject *obj = JSVAL_TO_OBJECT(*vp);

  if (JS_ObjectIsFunction(cx, obj)) {
    return XPC_XOW_WrapFunction(cx, outerObj, obj, vp);
  }

  if (JS_GET_CLASS(cx, obj) == &sXPC_XOW_JSClass.base &&
      JS_GetParent(cx, outerObj) != JS_GetParent(cx, obj)) {
    *vp = OBJECT_TO_JSVAL(GetWrappedObject(cx, obj));
  } else if (!XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj)) {
    return JS_TRUE;
  }

  return XPC_XOW_WrapObject(cx, JS_GetGlobalForObject(cx, outerObj), vp);
}

JSBool
XPC_XOW_WrapObject(JSContext *cx, JSObject *parent, jsval *vp)
{
  // Our argument should be a wrapped native object.
  JSObject *wrappedObj;
  XPCWrappedNative *wn;
  if (!JSVAL_IS_OBJECT(*vp) ||
      !(wrappedObj = JSVAL_TO_OBJECT(*vp)) ||
      JS_GET_CLASS(cx, wrappedObj) == &sXPC_XOW_JSClass.base ||
      !(wn = XPCWrappedNative::GetWrappedNativeOfJSObject(cx, wrappedObj))) {
    return JS_TRUE;
  }

  XPCJSRuntime *rt = nsXPConnect::GetRuntime();
  XPCCallContext ccx(NATIVE_CALLER, cx);
  NS_ENSURE_TRUE(ccx.IsValid(), JS_FALSE);

  XPCWrappedNativeScope *parentScope =
    XPCWrappedNativeScope::FindInJSObjectScope(ccx, parent);
  XPCWrappedNativeScope *wrapperScope = wn->GetScope();

#ifdef DEBUG_mrbkap
  printf("Wrapping object at %p (%s) [%p %p]\n",
         (void *)wrappedObj, JS_GET_CLASS(cx, wrappedObj)->name,
         (void *)parentScope, (void *)wrapperScope);
#endif

  JSObject *outerObj = nsnull;
  JSBool sameOrigin = (parentScope == wrapperScope);
  WrappedNative2WrapperMap *map =
    sameOrigin ? wrapperScope->GetWrapperMap() : parentScope->GetWrapperMap();

  if (sameOrigin) {
    outerObj = wn->GetWrapper();
    if (outerObj && JS_GET_CLASS(cx, outerObj) == &sXPC_XOW_JSClass.base) {
#ifdef DEBUG_mrbkap
      printf("But found a wrapper already there %p!\n", (void *)outerObj);
#endif
      *vp = OBJECT_TO_JSVAL(outerObj);
      return JS_TRUE;
    }
  }

  { // Scoped lock
    XPCAutoLock al(rt->GetMapLock());

    if (outerObj) {
      outerObj = map->Add(wrappedObj, outerObj);
      if (sameOrigin) {
        wn->SetWrapper(nsnull);
      }
    } else {
      outerObj = map->Find(wrappedObj);
    }
  }

  if (outerObj) {
    NS_ASSERTION(JS_GET_CLASS(cx, outerObj) == &sXPC_XOW_JSClass.base,
                              "What crazy object are we getting here?");
#ifdef DEBUG_mrbkap
    printf("But found a wrapper in the map %p!\n", (void *)outerObj);
#endif
    if (sameOrigin) {
      wn->SetWrapper(outerObj);
    }
    *vp = OBJECT_TO_JSVAL(outerObj);
    return JS_TRUE;
  }

  outerObj = JS_NewObject(cx, &sXPC_XOW_JSClass.base, nsnull, parent);
  if (!outerObj) {
    return JS_FALSE;
  }

  // Sever the prototype link from Object.prototype so we don't
  // accidentally inherit properties like __proto__ and __parent__.
  if (!JS_SetPrototype(cx, outerObj, nsnull)) {
    return JS_FALSE;
  }

  if (!JS_SetReservedSlot(cx, outerObj, XPCWrapper::sWrappedObjSlot, *vp) ||
      !JS_SetReservedSlot(cx, outerObj, XPCWrapper::sResolvingSlot,
                          JSVAL_FALSE) ||
      !JS_SetReservedSlot(cx, outerObj, XPC_XOW_ScopeSlot,
                          PRIVATE_TO_JSVAL(parentScope))) {
    return JS_FALSE;
  }

  *vp = OBJECT_TO_JSVAL(outerObj);
  if (!sameOrigin) {
    XPCAutoLock al(rt->GetMapLock());
    map->Add(wrappedObj, outerObj);
  } else {
#ifdef DEBUG_mrbkap
    printf("Setting wrapper to %p\n", (void *)outerObj);
#endif
    wn->SetWrapper(outerObj);
  }

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  // All AddProperty needs to do is pass on addProperty requests to
  // same-origin objects, and throw for all else.

  obj = GetWrapper(cx, obj);
  jsval resolving;
  if (!JS_GetReservedSlot(cx, obj, XPCWrapper::sResolvingSlot, &resolving)) {
    return JS_FALSE;
  }

  if (JSVAL_TO_BOOLEAN(resolving)) {
    // Allow us to define a property on ourselves.
    return JS_TRUE;
  }

  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }
  nsresult rv = IsWrapperSameOrigin(cx, wrappedObj);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      // Can't override properties on foreign objects.
      return ThrowException(rv, cx);
    }
    return JS_FALSE;
  }

  // Same origin, pass this request along.
  return XPCWrapper::AddProperty(cx, wrappedObj, id, vp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }
  nsresult rv = IsWrapperSameOrigin(cx, wrappedObj);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      // Can't delete properties on foreign objects.
      return ThrowException(rv, cx);
    }
    return JS_FALSE;
  }

  // Same origin, pass this request along.
  return XPCWrapper::DelProperty(cx, wrappedObj, id, vp);
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
  obj = GetWrapper(cx, obj);
  if (!obj) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }

  XPCCallContext ccx(JS_CALLER, cx);
  if (!ccx.IsValid()) {
    return ThrowException(NS_ERROR_FAILURE, cx);
  }

  AUTO_MARK_JSVAL(ccx, vp);

  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    return ThrowException(NS_ERROR_ILLEGAL_VALUE, cx);
  }
  nsresult rv = IsWrapperSameOrigin(cx, wrappedObj);
  if (NS_FAILED(rv)) {
    if (rv != NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      return JS_FALSE;
    }

    // This is a request to get a property across origins. We need to
    // determine if this property is allAccess. If it is, then we need to
    // actually get the property. If not, we simply need to throw an
    // exception.

    XPCWrappedNative *wn =
      XPCWrappedNative::GetWrappedNativeOfJSObject(cx, wrappedObj);
    NS_ASSERTION(wn, "How did we wrap a non-WrappedNative?");
    if (!IsValFrame(cx, wrappedObj, id, wn)) {
      nsIScriptSecurityManager *ssm = GetSecurityManager(cx);
      if (!ssm) {
        return ThrowException(NS_ERROR_NOT_INITIALIZED, cx);
      }

      PRUint32 check = isSet
                       ? (PRUint32)nsIXPCSecurityManager::ACCESS_SET_PROPERTY
                       : (PRUint32)nsIXPCSecurityManager::ACCESS_GET_PROPERTY;
      rv = ssm->CheckPropertyAccess(cx, wrappedObj,
                                    JS_GET_CLASS(cx, wrappedObj)->name,
                                    id, check);
      if (NS_FAILED(rv)) {
        // The security manager threw an exception for us.
        return JS_FALSE;
      }
    }

    if (!XPCWrapper::GetOrSetNativeProperty(cx, obj, wn, id, vp, isSet,
                                            JS_FALSE)) {
      return JS_FALSE;
    }

    return XPC_XOW_RewrapIfNeeded(cx, obj, vp);
  }

  JSObject *proto = nsnull; // Initialize this to quiet GCC.
  JSBool checkProto =
    (isSet && id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_PROTO));
  if (checkProto) {
    proto = JS_GetPrototype(cx, wrappedObj);
  }

  // Same origin, pass this request along as though nothing interesting
  // happened.
  jsid asId;

  if (!JS_ValueToId(cx, id, &asId)) {
    return JS_FALSE;
  }

  JSBool ok = isSet
              ? OBJ_SET_PROPERTY(cx, wrappedObj, asId, vp)
              : OBJ_GET_PROPERTY(cx, wrappedObj, asId, vp);
  if (!ok) {
    return JS_FALSE;
  }

  if (checkProto) {
    JSObject *newProto = JS_GetPrototype(cx, wrappedObj);

    // If code is trying to set obj.__proto__ and we're on obj's
    // prototype chain, then the OBJ_SET_PROPERTY above will do the
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

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return XPC_XOW_GetOrSetProperty(cx, obj, id, vp, JS_FALSE);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return XPC_XOW_GetOrSetProperty(cx, obj, id, vp, JS_TRUE);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_Enumerate(JSContext *cx, JSObject *obj)
{
  obj = GetWrapper(cx, obj);
  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    // Nothing to enumerate.
    return JS_TRUE;
  }
  nsresult rv = IsWrapperSameOrigin(cx, wrappedObj);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      // Can't enumerate on foreign objects.
      return ThrowException(rv, cx);
    }

    return JS_FALSE;
  }

  return XPCWrapper::Enumerate(cx, obj, wrappedObj);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                   JSObject **objp)
{
  obj = GetWrapper(cx, obj);

  if (id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
    *objp = obj;
    return JS_DefineFunction(cx, obj, "toString",
                             XPC_XOW_toString, 0, 0) != nsnull;
  }

  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    // No wrappedObj means that this is probably the prototype.
    *objp = nsnull;
    return JS_TRUE;
  }

  nsresult rv = IsWrapperSameOrigin(cx, wrappedObj);
  if (NS_FAILED(rv)) {
    if (rv != NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      return JS_FALSE;
    }

    // We're dealing with a cross-origin lookup. Ensure that we're allowed to
    // resolve this property and resolve it if so. Otherwise, we deny access
    // and throw a security error. Note that this code does not actually check
    // to see if the property exists, that's dealt with below.

    XPCWrappedNative *wn =
      XPCWrappedNative::GetWrappedNativeOfJSObject(cx, wrappedObj);
    NS_ASSERTION(wn, "How did we wrap a non-WrappedNative?");
    if (!IsValFrame(cx, wrappedObj, id, wn)) {
      nsIScriptSecurityManager *ssm = GetSecurityManager(cx);
      if (!ssm) {
        return ThrowException(NS_ERROR_NOT_INITIALIZED, cx);
      }
      PRUint32 action = (flags & JSRESOLVE_ASSIGNING)
                        ? (PRUint32)nsIXPCSecurityManager::ACCESS_SET_PROPERTY
                        : (PRUint32)nsIXPCSecurityManager::ACCESS_GET_PROPERTY;
      rv = ssm->CheckPropertyAccess(cx, wrappedObj,
                                    JS_GET_CLASS(cx, wrappedObj)->name,
                                    id, action);
      if (NS_FAILED(rv)) {
        // The security manager threw an exception for us.
        return JS_FALSE;
      }
    }

    // We're out! We're allowed to resolve this property.
    return XPCWrapper::ResolveNativeProperty(cx, obj, wrappedObj, wn, id,
                                             flags, objp, JS_FALSE);

  }

  return XPCWrapper::NewResolve(cx, obj, wrappedObj, id, flags, objp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    // Converting the prototype to something.

    if (type == JSTYPE_STRING) {
      return XPC_XOW_toString(cx, obj, 0, nsnull, vp);
    }

    *vp = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
  }

  // Note: JSTYPE_VOID and JSTYPE_STRING are equivalent.
  nsresult rv = IsWrapperSameOrigin(cx, wrappedObj);
  if (NS_FAILED(rv) &&
      (rv != NS_ERROR_DOM_PROP_ACCESS_DENIED ||
       (type != JSTYPE_STRING && type != JSTYPE_VOID))) {
    // Ensure that we report some kind of error.
    if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      ThrowException(rv, cx);
    }
    return JS_FALSE;
  }

  // TODO wrap return value?
  return JS_GET_CLASS(cx, wrappedObj)->convert(cx, wrappedObj, type, vp);
}

JS_STATIC_DLL_CALLBACK(void)
XPC_XOW_Finalize(JSContext *cx, JSObject *obj)
{
  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    return;
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
  if (!scope || XPCWrappedNativeScope::IsDyingScope(scope)) {
    return;
  }

  // Remove ourselves from the map.
  scope->GetWrapperMap()->Remove(wrappedObj);
}

JS_STATIC_DLL_CALLBACK(JSBool)
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

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSObject *wrappedObj = GetWrappedObject(cx, obj);
  if (!wrappedObj) {
    // Nothing to call.
    return JS_TRUE;
  }
  nsresult rv = IsWrapperSameOrigin(cx, wrappedObj);
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

  return XPC_XOW_RewrapIfNeeded(cx, callee, rval);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval)
{
  JSObject *realObj = GetWrapper(cx, JSVAL_TO_OBJECT(argv[-2]));
  JSObject *wrappedObj = GetWrappedObject(cx, realObj);
  if (!wrappedObj) {
    // Nothing to construct.
    return JS_TRUE;
  }
  nsresult rv = IsWrapperSameOrigin(cx, wrappedObj);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
      // Can't construct.
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

  return XPC_XOW_RewrapIfNeeded(cx, callee, rval);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
  // Convert both sides to XPCWrappedNative and see if they match.
  if (JSVAL_IS_PRIMITIVE(v)) {
    *bp = JS_FALSE;
    return JS_TRUE;
  }

  JSObject *test = JSVAL_TO_OBJECT(v);
  if (JS_GET_CLASS(cx, test) == &sXPC_XOW_JSClass.base) {
    if (!JS_GetReservedSlot(cx, test, XPCWrapper::sWrappedObjSlot, &v)) {
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
  return ((JSExtendedClass *)JS_GET_CLASS(cx, obj))->
    equality(cx, obj, OBJECT_TO_JSVAL(test), bp);
}

JS_STATIC_DLL_CALLBACK(void)
IteratorFinalize(JSContext *cx, JSObject *obj)
{
  jsval v;
  JS_GetReservedSlot(cx, obj, 0, &v);

  JSIdArray *ida = reinterpret_cast<JSIdArray *>(JSVAL_TO_PRIVATE(v));
  if (ida) {
    JS_DestroyIdArray(cx, ida);
  }
}

JS_STATIC_DLL_CALLBACK(JSBool)
IteratorNext(JSContext *cx, uintN argc, jsval *vp)
{
  JSObject *obj = JSVAL_TO_OBJECT(vp[1]);
  jsval v;

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
    if (!JS_IdToValue(cx, id, &v)) {
      return JS_FALSE;
    }

    *vp = v;
  } else {
    // We need to return an [id, value] pair.
    if (!OBJ_GET_PROPERTY(cx, JS_GetParent(cx, obj), id, &v)) {
      return JS_FALSE;
    }

    jsval name;
    if (!JS_IdToValue(cx, id, &name)) {
      return JS_FALSE;
    }

    jsval vec[2] = { name, v };
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


JS_STATIC_DLL_CALLBACK(JSObject *)
XPC_XOW_Iterator(JSContext *cx, JSObject *obj, JSBool keysonly)
{
  // This is rather ugly: we want to use the trick seen in Enumerate,
  // where we use our wrapper's resolve hook to determine if we should
  // enumerate a given property. However, we don't want to pollute the
  // identifiers with a next method, so we create an object that
  // delegates (via the __proto__ link) to a XOW.

  jsval root = JSVAL_NULL;

  // Root v's address so we can set it and have the right value rooted.
  JSAutoTempValueRooter tvr(cx, 1, &root);

  JSObject *wrapperIter = JS_NewObject(cx, &sXPC_XOW_JSClass.base, nsnull,
                                       JS_GetGlobalForObject(cx, obj));
  if (!wrapperIter) {
    return nsnull;
  }

  root = OBJECT_TO_JSVAL(wrapperIter);

  JSObject *iterObj = JS_NewObject(cx, &IteratorClass, wrapperIter, obj);
  if (!iterObj) {
    return nsnull;
  }

  root = OBJECT_TO_JSVAL(iterObj);

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

  // Initialize our XOW.
  JSObject *innerObj = GetWrappedObject(cx, obj);
  if (!innerObj) {
    ThrowException(NS_ERROR_INVALID_ARG, cx);
    return nsnull;
  }

  jsval v = OBJECT_TO_JSVAL(innerObj);
  if (!JS_SetReservedSlot(cx, wrapperIter, XPCWrapper::sWrappedObjSlot, v) ||
      !JS_SetReservedSlot(cx, wrapperIter, XPCWrapper::sResolvingSlot,
                          JSVAL_FALSE) ||
      !JS_SetReservedSlot(cx, wrapperIter, XPCWrapper::sNumSlots,
                          PRIVATE_TO_JSVAL(nsnull))) {
    return nsnull;
  }

  // Start enumerating over all of our properties.
  do {
    if (!XPCWrapper::Enumerate(cx, iterObj, innerObj)) {
      return nsnull;
    }
  } while ((innerObj = JS_GetPrototype(cx, innerObj)) != nsnull);

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

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_XOW_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
  obj = GetWrapper(cx, obj);
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

  nsresult rv = IsWrapperSameOrigin(cx, wrappedObj);
  if (rv == NS_ERROR_DOM_PROP_ACCESS_DENIED) {
    nsIScriptSecurityManager *ssm = GetSecurityManager(cx);
    if (!ssm) {
      return ThrowException(NS_ERROR_NOT_INITIALIZED, cx);
    }
    rv = ssm->CheckPropertyAccess(cx, wrappedObj,
                                  JS_GET_CLASS(cx, wrappedObj)->name,
                                  GetRTStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING),
                                  nsIXPCSecurityManager::ACCESS_GET_PROPERTY);
  }
  if (NS_FAILED(rv)) {
    return JS_FALSE;
  }

  XPCWrappedNative *wn =
    XPCWrappedNative::GetWrappedNativeOfJSObject(cx, wrappedObj);
  return XPCWrapper::NativeToString(cx, wn, argc, argv, rval, JS_FALSE);
}
