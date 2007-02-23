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

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_Enumerate(JSContext *cx, JSObject *obj);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                    JSObject **objp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

JS_STATIC_DLL_CALLBACK(void)
XPC_SJOW_Finalize(JSContext *cx, JSObject *obj);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_CheckAccess(JSContext *cx, JSObject *obj, jsval id, JSAccessMode mode,
                     jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval);

JSBool
XPC_SJOW_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                   jsval *rval);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

static JSNative sEvalNative;

static inline
JSBool
ThrowException(nsresult ex, JSContext *cx)
{
  XPCThrower::Throw(ex, cx);

  return JS_FALSE;
}

// Find the subject and object principal. The argument
// subjectPrincipal can be null if the caller doesn't care about the
// subject principal, and secMgr can also be null if the caller
// doesn't need the security manager.
static nsresult
FindPrincipals(JSContext *cx, JSObject *obj, nsIPrincipal **objectPrincipal,
               nsIPrincipal **subjectPrincipal,
               nsIScriptSecurityManager **secMgr)
{
  XPCCallContext ccx(JS_CALLER, cx);

  if (!ccx.IsValid()) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIXPCSecurityManager> sm = ccx.GetXPCContext()->
    GetAppropriateSecurityManager(nsIXPCSecurityManager::HOOK_CALL_METHOD);

  nsCOMPtr<nsIScriptSecurityManager> ssm(do_QueryInterface(sm));

  if (subjectPrincipal) {
    ssm->GetSubjectPrincipal(subjectPrincipal);

    if (!*subjectPrincipal) {
      return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
    }
  }

  ssm->GetObjectPrincipal(cx, obj, objectPrincipal);

  if (secMgr) {
    *secMgr = nsnull;
    ssm.swap(*secMgr);
  }

  return *objectPrincipal ? NS_OK : NS_ERROR_XPC_SECURITY_MANAGER_VETO;
}

static PRBool
CanCallerAccess(JSContext *cx, JSObject *unsafeObj)
{
  nsCOMPtr<nsIPrincipal> subjPrincipal, objPrincipal;
  nsCOMPtr<nsIScriptSecurityManager> ssm;
  nsresult rv = FindPrincipals(cx, unsafeObj, getter_AddRefs(objPrincipal),
                               getter_AddRefs(subjPrincipal),
                               getter_AddRefs(ssm));
  if (NS_FAILED(rv)) {
    return ThrowException(rv, cx);
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

  return PR_TRUE;
}

static JSPrincipals *
FindObjectPrincipals(JSContext *cx, JSObject *obj)
{
  nsCOMPtr<nsIPrincipal> objPrincipal;
  nsresult rv = FindPrincipals(cx, obj, getter_AddRefs(objPrincipal), nsnull,
                               nsnull);
  if (NS_FAILED(rv)) {
    return nsnull;
  }

  JSPrincipals *jsprin;
  rv = objPrincipal->GetJSPrincipals(cx, &jsprin);
  if (NS_FAILED(rv)) {
    return nsnull;
  }

  return jsprin;
}


// JS class for XPCSafeJSObjectWrapper (and this doubles as the
// constructor for XPCSafeJSObjectWrapper for the moment too...)

JSExtendedClass sXPC_SJOW_JSClass = {
  // JSClass (JSExtendedClass.base) initialization
  { "XPCSafeJSObjectWrapper",
    JSCLASS_NEW_RESOLVE | JSCLASS_IS_EXTENDED | JSCLASS_HAS_RESERVED_SLOTS(5),
    XPC_SJOW_AddProperty, XPC_SJOW_DelProperty,
    XPC_SJOW_GetProperty, XPC_SJOW_SetProperty,
    XPC_SJOW_Enumerate,   (JSResolveOp)XPC_SJOW_NewResolve,
    XPC_SJOW_Convert,     XPC_SJOW_Finalize,
    nsnull,               XPC_SJOW_CheckAccess,
    XPC_SJOW_Call,        XPC_SJOW_Construct,
    nsnull,               nsnull,
    nsnull,               nsnull
  },
  // JSExtendedClass initialization
  XPC_SJOW_Equality
};

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval);

static JSFunctionSpec sXPC_SJOW_JSClass_methods[] = {
  {"toString", XPC_SJOW_toString, 0, 0, 0},
  {0, 0, 0, 0, 0}
};

// Reserved slot indexes on safe wrappers.

// Boolean value, initialized to false on object creation and true
// only while we're resolving a property on the object.
#define XPC_SJOW_SLOT_IS_RESOLVING           0

// Slot for caching a compiled scripted function for property
// get/set.
#define XPC_SJOW_SLOT_SCRIPTED_GETSET        1

// Slot for caching a compiled scripted function for function
// calling.
#define XPC_SJOW_SLOT_SCRIPTED_FUN           2

// Slot for caching a compiled scripted function for calling
// toString().
#define XPC_SJOW_SLOT_SCRIPTED_TOSTRING      3

// Slot for holding on to the principal to use if a principal other
// than that of the unsafe object is desired for this wrapper
// (nsIPrincipal, strong reference).
#define XPC_SJOW_SLOT_PRINCIPAL              4


static JSObject *
GetGlobalObject(JSContext *cx, JSObject *obj)
{
  JSObject *parent;

  while ((parent = ::JS_GetParent(cx, obj))) {
    obj = parent;
  }

  return obj;
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
    // Construct a new safe wrapper. Note that it doesn't matter what
    // parent we pass in here, the construct hook will ensure we get
    // the right parent for the wrapper.
    JSObject *safeObj =
      ::JS_ConstructObjectWithArguments(cx, &sXPC_SJOW_JSClass.base, nsnull,
                                        nsnull, 1, &val);
    if (!safeObj) {
      return JS_FALSE;
    }

    // Set *rval to safeObj here to ensure it doesn't get collected in
    // any of the code below.
    *rval = OBJECT_TO_JSVAL(safeObj);

    // If obj and safeObj are from the same scope, propagate cached
    // scripted functions to the new safe object.
    if (GetGlobalObject(cx, obj) == GetGlobalObject(cx, safeObj)) {
      jsval rsval;
      if (!::JS_GetReservedSlot(cx, obj, XPC_SJOW_SLOT_SCRIPTED_GETSET,
                                &rsval) ||
          !::JS_SetReservedSlot(cx, safeObj, XPC_SJOW_SLOT_SCRIPTED_GETSET,
                                rsval) ||
          !::JS_GetReservedSlot(cx, obj, XPC_SJOW_SLOT_SCRIPTED_FUN,
                                &rsval) ||
          !::JS_SetReservedSlot(cx, safeObj, XPC_SJOW_SLOT_SCRIPTED_FUN,
                                rsval)) {
        return JS_FALSE;
      }
    } else {
      // Check to see if the new object we just wrapped is accessible
      // from the unsafe object we got the new object through. If not,
      // force the new wrapper to use the principal of the unsafe
      // object we got the new object from.
      nsCOMPtr<nsIPrincipal> srcObjPrincipal;
      nsCOMPtr<nsIPrincipal> valObjPrincipal;

      nsresult rv = FindPrincipals(cx, obj, getter_AddRefs(srcObjPrincipal),
                                   nsnull, nsnull);
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

      if (!subsumes) {
        // The unsafe object we got the new object from can not access
        // the new object, force the wrapper we just created to use
        // the principal of the unsafe object to prevent users of the
        // new object wrapper from evaluating code through the new
        // wrapper with the principal of the new object.
        if (!::JS_SetReservedSlot(cx, safeObj, XPC_SJOW_SLOT_PRINCIPAL,
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

static inline JSObject *
FindSafeObject(JSContext *cx, JSObject *obj)
{
  while (JS_GET_CLASS(cx, obj) != &sXPC_SJOW_JSClass.base) {
    obj = ::JS_GetPrototype(cx, obj);

    if (!obj) {
      break;
    }
  }

  return obj;
}

PRBool
IsXPCSafeJSObjectWrapper(JSContext *cx, JSObject *obj)
{
  return FindSafeObject(cx, obj) != nsnull;
}

PRBool
IsXPCSafeJSObjectWrapperClass(JSClass *clazz)
{
  return clazz == &sXPC_SJOW_JSClass.base;
}

static inline JSObject *
GetUnsafeObject(JSContext *cx, JSObject *obj)
{
  obj = FindSafeObject(cx, obj);

  if (!obj) {
    return nsnull;
  }

  return ::JS_GetParent(cx, obj);
}

JSObject *
XPC_SJOW_GetUnsafeObject(JSContext *cx, JSObject *obj)
{
  return GetUnsafeObject(cx, obj);
}

static jsval
UnwrapJSValue(JSContext *cx, jsval val)
{
  if (JSVAL_IS_PRIMITIVE(val)) {
    return val;
  }

  JSObject *unsafeObj = GetUnsafeObject(cx, JSVAL_TO_OBJECT(val));
  if (unsafeObj) {
    return OBJECT_TO_JSVAL(unsafeObj);
  }

  return val;
}

// Get a scripted function for use with the safe wrapper (obj) when
// accessing an unsafe object (unsafeObj). If a scripted function
// already exists in the reserved slot slotIndex, use it, otherwise
// create a new one and cache it in that same slot. The source of the
// script is passed in funScript, and the resulting (new or cached)
// scripted function is returned through scriptedFunVal.
static JSBool
GetScriptedFunction(JSContext *cx, JSObject *obj, JSObject *unsafeObj,
                    uint32 slotIndex, const nsAFlatCString& funScript,
                    jsval *scriptedFunVal)
{
  if (!::JS_GetReservedSlot(cx, obj, slotIndex, scriptedFunVal)) {
    return JS_FALSE;
  }

  // If we either have no scripted function in the requested slot yet,
  // or if the scope of the unsafeObj changed since we compiled the
  // scripted function, re-compile to make sure the scripted function
  // is properly scoped etc.
  if (JSVAL_IS_VOID(*scriptedFunVal) ||
      GetGlobalObject(cx, unsafeObj) !=
      GetGlobalObject(cx, JSVAL_TO_OBJECT(*scriptedFunVal))) {
    // Check whether we have a cached principal or not.
    jsval pv;
    if (!::JS_GetReservedSlot(cx, obj, XPC_SJOW_SLOT_PRINCIPAL, &pv)) {
      return JS_FALSE;
    }

    JSPrincipals *jsprin = nsnull;

    if (!JSVAL_IS_VOID(pv)) {
      nsIPrincipal *principal = (nsIPrincipal *)JSVAL_TO_PRIVATE(pv);

      // Found a cached principal, use it rather than looking up the
      // principal of the unsafe object.
      principal->GetJSPrincipals(cx, &jsprin);
    } else {
      // No cached principal found, look up the principal based on the
      // unsafe object.
      jsprin = FindObjectPrincipals(cx, unsafeObj);
    }

    if (!jsprin) {
      return ThrowException(NS_ERROR_UNEXPECTED, cx);
    }

    JSFunction *scriptedFun =
      ::JS_CompileFunctionForPrincipals(cx, GetGlobalObject(cx, unsafeObj),
                                        jsprin, nsnull, 0, nsnull,
                                        funScript.get(), funScript.Length(),
                                        "XPCSafeJSObjectWrapper.cpp",
                                        __LINE__);

    JSPRINCIPALS_DROP(cx, jsprin);

    if (!scriptedFun) {
      return ThrowException(NS_ERROR_FAILURE, cx);
    }

    *scriptedFunVal = OBJECT_TO_JSVAL(::JS_GetFunctionObject(scriptedFun));

    if (*scriptedFunVal == JSVAL_NULL ||
        !::JS_SetReservedSlot(cx, obj, slotIndex, *scriptedFunVal)) {
      return JS_FALSE;
    }
  }

  return JS_TRUE;
}


JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  // The constructor and toString properties needs to live on the safe
  // wrapper.
  if (id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_CONSTRUCTOR) ||
      id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
    return JS_TRUE;
  }

  obj = FindSafeObject(cx, obj);
  NS_ASSERTION(obj != nsnull, "FindSafeObject() returned null in class hook!");

  // Do nothing here if we're in the middle of resolving a property on
  // this safe wrapper.
  jsval isResolving;
  JSBool ok = ::JS_GetReservedSlot(cx, obj, XPC_SJOW_SLOT_IS_RESOLVING,
                                   &isResolving);
  if (!ok || JSVAL_TO_BOOLEAN(isResolving)) {
    return ok;
  }

  JSObject *unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, unsafeObj)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  if (JSVAL_IS_STRING(id)) {
    JSString *str = JSVAL_TO_STRING(id);
    jschar *chars = ::JS_GetStringChars(str);
    size_t length = ::JS_GetStringLength(str);

    return ::JS_DefineUCProperty(cx, unsafeObj, chars, length, *vp, nsnull,
                                 nsnull, JSPROP_ENUMERATE);
  }

  if (!JSVAL_IS_INT(id)) {
    return ThrowException(NS_ERROR_NOT_IMPLEMENTED, cx);
  }

  return ::JS_DefineElement(cx, unsafeObj, JSVAL_TO_INT(id), *vp, nsnull,
                            nsnull, JSPROP_ENUMERATE);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSObject *unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, unsafeObj)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  if (JSVAL_IS_STRING(id)) {
    JSString *str = JSVAL_TO_STRING(id);
    jschar *chars = ::JS_GetStringChars(str);
    size_t length = ::JS_GetStringLength(str);

    return ::JS_DeleteUCProperty2(cx, unsafeObj, chars, length, vp);
  }

  if (!JSVAL_IS_INT(id)) {
    return ThrowException(NS_ERROR_NOT_IMPLEMENTED, cx);
  }

  return ::JS_DeleteElement2(cx, unsafeObj, JSVAL_TO_INT(id), vp);
}

// Call wrapper to help with wrapping calls to functions or callable
// objects in a scripted function (see XPC_SJOW_Call()). The first
// argument passed to this method is the unsafe function to call, the
// rest are the arguments to pass to the function we're calling.
JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_CallWrapper(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                     jsval *rval)
{
  return ::JS_CallFunctionValue(cx, obj, argv[0], argc - 1, argv + 1, rval);
}

static JSBool
XPC_SJOW_GetOrSetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp,
                          JSBool aIsSet)
{
  // We don't deal with the following properties here.
  if (id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_PROTOTYPE) ||
      id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
    return JS_TRUE;
  }

  obj = FindSafeObject(cx, obj);
  NS_ASSERTION(obj != nsnull, "FindSafeObject() returned null in class hook!");

  JSObject *unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, unsafeObj)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  // Function body for wrapping property get/set in a scripted
  // caller. This scripted function's first argument is the property
  // to get/set. If the operation is a get operation, the function is
  // passed one argument. If the operation is a set operation, the
  // function gets two arguments and the second argument will be the
  // value to set the property to.
  NS_NAMED_LITERAL_CSTRING(funScript,
    "if (arguments.length == 1) return this[arguments[0]];"
    "this[arguments[0]] = arguments[1];");

  jsval scriptedFunVal;
  if (!GetScriptedFunction(cx, obj, unsafeObj, XPC_SJOW_SLOT_SCRIPTED_GETSET,
                           funScript, &scriptedFunVal)) {
    return JS_FALSE;
  }

  // Build up our argument array per the comment above.
  jsval args[2];

  args[0] = id;

  if (aIsSet) {
    args[1] = UnwrapJSValue(cx, *vp);
  }

  jsval val;
  JSBool ok = ::JS_CallFunctionValue(cx, unsafeObj, scriptedFunVal,
                                     aIsSet ? 2 : 1, args, &val);

  return ok && WrapJSValue(cx, obj, val, vp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return XPC_SJOW_GetOrSetProperty(cx, obj, id, vp, PR_FALSE);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return XPC_SJOW_GetOrSetProperty(cx, obj, id, vp, PR_TRUE);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_Enumerate(JSContext *cx, JSObject *obj)
{
  obj = FindSafeObject(cx, obj);
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

  // Since we enumerate using JS_Enumerate() on the unsafe object here
  // we don't need to do a security check since JS_Enumerate() will
  // look up unsafeObj.__iterator__ and if we don't have permission to
  // access that, it'll throw and we'll be safe.

  JSIdArray *ida = JS_Enumerate(cx, unsafeObj);
  if (!ida) {
    return JS_FALSE;
  }

  JSBool ok = JS_TRUE;

  for (jsint i = 0, n = ida->length; i < n; i++) {
    JSObject *pobj;
    JSProperty *prop;

    // Let OBJ_LOOKUP_PROPERTY, in particular XPC_SJOW_NewResolve,
    // figure out whether this id should be reflected.
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

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                    JSObject **objp)
{
  // No need to resolve toString as it's a class method.
  if (id == GetRTStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
    return JS_TRUE;
  }

  obj = FindSafeObject(cx, obj);
  NS_ASSERTION(obj != nsnull, "FindSafeObject() returned null in class hook!");

  JSObject *unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    // No unsafe object, nothing to resolve here.

    return JS_TRUE;
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, unsafeObj)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  jschar *chars = nsnull;
  size_t length;
  JSBool hasProp, ok;

  if (JSVAL_IS_STRING(id)) {
    JSString *str = JSVAL_TO_STRING(id);

    chars = ::JS_GetStringChars(str);
    length = ::JS_GetStringLength(str);

    ok = ::JS_HasUCProperty(cx, unsafeObj, chars, length, &hasProp);
  } else if (JSVAL_IS_INT(id)) {
    ok = ::JS_HasElement(cx, unsafeObj, JSVAL_TO_INT(id), &hasProp);
  } else {
    // A non-string and non-int id is being resolved. We don't deal
    // with those yet, return early.

    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  if (!ok || !hasProp) {
    // An error occured, or the property was not found. Return
    // early. This is safe even in the case of a set operation since
    // if the property doesn't exist there's no chance of a setter
    // being called or any other code being run as a result of the
    // set.

    return ok;
  }

  jsval oldSlotVal;
  if (!::JS_GetReservedSlot(cx, obj, XPC_SJOW_SLOT_IS_RESOLVING,
                            &oldSlotVal) ||
      !::JS_SetReservedSlot(cx, obj, XPC_SJOW_SLOT_IS_RESOLVING,
                            BOOLEAN_TO_JSVAL(JS_TRUE))) {
    return JS_FALSE;
  }

  if (chars) {
    ok = ::JS_DefineUCProperty(cx, obj, chars, length, JSVAL_VOID,
                               nsnull, nsnull, JSPROP_ENUMERATE);
  } else {
    ok = ::JS_DefineElement(cx, obj, JSVAL_TO_INT(id), JSVAL_VOID,
                            nsnull, nsnull, JSPROP_ENUMERATE);
  }

  if (ok && (ok = ::JS_SetReservedSlot(cx, obj, XPC_SJOW_SLOT_IS_RESOLVING,
                                       oldSlotVal))) {
    *objp = obj;
  }

  return ok;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(void)
XPC_SJOW_Finalize(JSContext *cx, JSObject *obj)
{
  // Release the reference to the cached principal if we have one.
  jsval v;
  if (::JS_GetReservedSlot(cx, obj, XPC_SJOW_SLOT_PRINCIPAL, &v) &&
      !JSVAL_IS_VOID(v)) {
    nsIPrincipal *principal = (nsIPrincipal *)JSVAL_TO_PRIVATE(v);

    NS_RELEASE(principal);
  }
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_CheckAccess(JSContext *cx, JSObject *obj, jsval id,
                     JSAccessMode mode, jsval *vp)
{
  // Prevent setting __proto__ on an XPCSafeJSObjectWrapper
  if ((mode & JSACC_WATCH) == JSACC_PROTO && (mode & JSACC_WRITE)) {
    return ThrowException(NS_ERROR_XPC_SECURITY_MANAGER_VETO, cx);
  }

  // Forward to the checkObjectAccess hook in the runtime, if any.
  if (cx->runtime->checkObjectAccess &&
      !cx->runtime->checkObjectAccess(cx, obj, id, mode, vp)) {
    return JS_FALSE;
  }

  JSObject *unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    return JS_TRUE;
  }

  // Forward the unsafe object to the checkObjectAccess hook in the
  // runtime too, if any.
  if (cx->runtime->checkObjectAccess &&
      !cx->runtime->checkObjectAccess(cx, unsafeObj, id, mode, vp)) {
    return JS_FALSE;
  }

  JSClass *clazz = JS_GET_CLASS(cx, unsafeObj);
  return !clazz->checkAccess ||
    clazz->checkAccess(cx, unsafeObj, id, mode, vp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
  obj = FindSafeObject(cx, obj);
  if (!obj) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  JSObject *unsafeObj = GetUnsafeObject(cx, obj);
  if (!unsafeObj) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  JSObject *funToCall = GetUnsafeObject(cx, JSVAL_TO_OBJECT(argv[-2]));

  // Check that the caller can access the unsafe object on which the
  // call is being made, and the actual function we're about to call.
  if (!CanCallerAccess(cx, unsafeObj) || !CanCallerAccess(cx, funToCall)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  // Function body for wrapping calls to functions or callable objects
  // in a scripted caller. This scripted function's first argument is
  // a native call wrapper, and the second argument is the unsafe
  // function to call. All but the first argument are passed to the
  // call wrapper.
  NS_NAMED_LITERAL_CSTRING(funScript,
                           "var args = [];"
                           "for (var i = 1; i < arguments.length; i++)"
                           "args.push(arguments[i]);"
                           "return arguments[0].apply(this, args);");

  // Get the scripted function.
  jsval scriptedFunVal;
  if (!GetScriptedFunction(cx, obj, unsafeObj, XPC_SJOW_SLOT_SCRIPTED_FUN,
                           funScript, &scriptedFunVal)) {
    return JS_FALSE;
  }

  JSFunction *callWrapper;
  jsval cwval;

  // Check if we've cached the call wrapper on the scripted function
  // already. If so, use the cached call wrapper.
  if (!::JS_GetReservedSlot(cx, JSVAL_TO_OBJECT(scriptedFunVal), 0, &cwval)) {
    return JS_FALSE;
  }

  if (JSVAL_IS_PRIMITIVE(cwval)) {
    // No cached call wrapper found.
    callWrapper =
      ::JS_NewFunction(cx, XPC_SJOW_CallWrapper, 0, 0, unsafeObj,
                       "XPC_SJOW_CallWrapper");
    if (!callWrapper) {
      return JS_FALSE;
    }

    // Cache the call wrapper function, this will also ensure it
    // doesn't get collected early. We piggy-back on one of the
    // reserved slots in JS functions here, and that's ok since we
    // know the scripted function we're storing it on is a function
    // compiled by the JS engine and the reserved slots are unused.
    JSObject *callWrapperObj = ::JS_GetFunctionObject(callWrapper);
    if (!::JS_SetReservedSlot(cx, JSVAL_TO_OBJECT(scriptedFunVal), 0,
                              OBJECT_TO_JSVAL(callWrapperObj))) {
      return JS_FALSE;
    }
  } else {
    // Found a cached call wrapper, extract the function.
    callWrapper = ::JS_ValueToFunction(cx, cwval);

    if (!callWrapper) {
      return ThrowException(NS_ERROR_UNEXPECTED, cx);
    }
  }

  // Build up our argument array per earlier comment.
  jsval argsBuf[8];
  jsval *args = argsBuf;

  if (argc > 7) {
    args = (jsval *)nsMemory::Alloc((argc + 2) * sizeof(jsval *));
    if (!args) {
      return ThrowException(NS_ERROR_OUT_OF_MEMORY, cx);
    }
  }

  args[0] = OBJECT_TO_JSVAL(::JS_GetFunctionObject(callWrapper));
  args[1] = OBJECT_TO_JSVAL(funToCall);

  if (args[0] == JSVAL_NULL) {
    return JS_FALSE;
  }

  for (uintN i = 0; i < argc; ++i) {
    args[i + 2] = UnwrapJSValue(cx, argv[i]);
  }

  jsval val;
  JSBool ok = ::JS_CallFunctionValue(cx, unsafeObj, scriptedFunVal, argc + 2,
                                     args, &val);

  if (args != argsBuf) {
    nsMemory::Free(args);
  }

  return ok && WrapJSValue(cx, obj, val, rval);
}

JSBool
XPC_SJOW_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                   jsval *rval)
{
  if (argc < 1) {
    return ThrowException(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx);
  }

  // |obj| almost always has the wrong proto and parent so we have to create
  // our own object anyway.  Set |obj| to null so we don't use it by accident.
  obj = nsnull;

  if (JSVAL_IS_PRIMITIVE(argv[0])) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  JSObject *objToWrap = JSVAL_TO_OBJECT(argv[0]);

  // Prevent script created Script objects from ever being wrapped
  // with XPCSafeJSObjectWrapper, and never let the eval function
  // object be directly wrapped.

  if (JS_GET_CLASS(cx, objToWrap) == &js_ScriptClass ||
      (::JS_ObjectIsFunction(cx, objToWrap) &&
       ::JS_GetFunctionNative(cx, ::JS_ValueToFunction(cx, argv[0])) ==
       sEvalNative)) {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, objToWrap)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  JSObject *unsafeObj = GetUnsafeObject(cx, objToWrap);

  if (unsafeObj) {
    // We're asked to wrap an already wrapped object. Re-wrap the
    // object wrapped by the given wrapper.

    objToWrap = unsafeObj;
  }

  // Don't use the object the JS engine created for us, it is in most
  // cases incorectly parented and has a proto from the wrong scope.
  JSObject *wrapperObj = ::JS_NewObject(cx, &sXPC_SJOW_JSClass.base, nsnull,
                                        objToWrap);

  if (!wrapperObj) {
    // JS_NewObject already threw.
    return JS_FALSE;
  }

  if (!::JS_SetReservedSlot(cx, wrapperObj, XPC_SJOW_SLOT_IS_RESOLVING,
                            BOOLEAN_TO_JSVAL(JS_FALSE))) {
    return JS_FALSE;
  }

  *rval = OBJECT_TO_JSVAL(wrapperObj);

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
  if (JSVAL_IS_PRIMITIVE(v)) {
    *bp = JS_FALSE;
  } else {
    JSObject *unsafeObj = GetUnsafeObject(cx, obj);

    JSObject *other = JSVAL_TO_OBJECT(v);
    JSObject *otherUnsafe = GetUnsafeObject(cx, other);

    *bp = (obj == other || unsafeObj == other ||
           (unsafeObj && unsafeObj == otherUnsafe) ||
           XPC_GetIdentityObject(cx, obj) == XPC_GetIdentityObject(cx, other));
  }

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_SJOW_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval)
{
  obj = FindSafeObject(cx, obj);
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

    *rval = STRING_TO_JSVAL(str);

    return JS_TRUE;
  }

  // Check that the caller can access the unsafe object.
  if (!CanCallerAccess(cx, unsafeObj)) {
    // CanCallerAccess() already threw for us.
    return JS_FALSE;
  }

  // Function body for wrapping toString() in a scripted caller.
#ifndef DEBUG
  NS_NAMED_LITERAL_CSTRING(funScript,
    "return '' + this;");
#else
  NS_NAMED_LITERAL_CSTRING(funScript,
    "return '[object XPCSafeJSObjectWrapper (' + this + ')]';");
#endif

  jsval scriptedFunVal;
  if (!GetScriptedFunction(cx, obj, unsafeObj, XPC_SJOW_SLOT_SCRIPTED_TOSTRING,
                           funScript, &scriptedFunVal)) {
    return JS_FALSE;
  }

  jsval val;
  JSBool ok = ::JS_CallFunctionValue(cx, unsafeObj, scriptedFunVal, 0, nsnull,
                                     &val);

  return ok && WrapJSValue(cx, obj, val, rval);
}

PRBool
XPC_SJOW_AttachNewConstructorObject(XPCCallContext &ccx,
                                    JSObject *aGlobalObject)
{
  // Initialize sEvalNative the first time we attach a constructor to
  // a global that is an XPCWrappedNative (i.e. don't bother if the
  // global is the one from the XPConnect safe context, since it
  // doesn't have what it takes to do this)
  if (!sEvalNative &&
      XPCWrappedNative::GetWrappedNativeOfJSObject(ccx, aGlobalObject)) {
    JSObject *obj;
    if (!::JS_GetClassObject(ccx, aGlobalObject, JSProto_Object, &obj) ||
        !obj) {
      return ThrowException(NS_ERROR_UNEXPECTED, ccx);
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
  }

  JSObject *class_obj =
    ::JS_InitClass(ccx, aGlobalObject, nsnull, &sXPC_SJOW_JSClass.base,
                   XPC_SJOW_Construct, 0, nsnull, sXPC_SJOW_JSClass_methods,
                   nsnull, nsnull);
  if (!class_obj) {
    NS_WARNING("can't initialize the XPCSafeJSObjectWrapper class");
    return PR_FALSE;
  }

  // Null out the class object's parent to prevent code in this class
  // from thinking the class object is a wrapper for the global
  // object.
  ::JS_SetParent(ccx, class_obj, nsnull);

  // Make sure our prototype chain is empty and that people can't mess
  // with XPCSafeJSObjectWrapper.prototype.
  ::JS_SetPrototype(ccx, class_obj, nsnull);
  if (!::JS_SealObject(ccx, class_obj, JS_FALSE)) {
    NS_WARNING("Failed to seal XPCSafeJSObjectWrapper.prototype");
    return PR_FALSE;
  }

  JSBool found;
  return ::JS_SetPropertyAttributes(ccx, aGlobalObject,
                                    sXPC_SJOW_JSClass.base.name,
                                    JSPROP_READONLY | JSPROP_PERMANENT,
                                    &found);
}
