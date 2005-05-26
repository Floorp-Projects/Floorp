/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

// JS class for XPCNativeWrapper (and this doubles as the constructor
// for XPCNativeWrapper for the moment too...)

JSExtendedClass XPCNativeWrapper::sXPC_NW_JSClass = {
  // JSClass (JSExtendedClass.base) initialization
  { "XPCNativeWrapper",
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS |
    JSCLASS_NEW_RESOLVE | JSCLASS_HAS_RESERVED_SLOTS(1) |
    JSCLASS_IS_EXTENDED,
    JS_PropertyStub,    XPC_NW_DelProperty,
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

// If one of our class hooks is ever called from a non-system script, bypass
// the hook by calling the same hook on our wrapped native, with obj reset to
// the wrapped native's flat JSObject, so the hook and args macro parameters
// can be simply:
//
//      enumerate, (cx, obj)
//
// in the call from XPC_NW_Enumerate, for example.

#define XPC_NW_CALL_HOOK(cx, obj, hook, args)                                 \
  return JS_GET_CLASS(cx, obj)->hook args;

#define XPC_NW_CAST_HOOK(cx, obj, type, hook, args)                           \
  return ((type) JS_GET_CLASS(cx, obj)->hook) args;

static JSBool
ShouldBypassNativeWrapper(JSContext *cx, JSObject *obj)
{
  jsval deep = JS_FALSE;

  ::JS_GetReservedSlot(cx, obj, 0, &deep);
  return deep == JSVAL_TRUE &&
         !(::JS_GetTopScriptFilenameFlags(cx, nsnull) & JSFILENAME_SYSTEM);
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

static inline
JSBool
ThrowException(nsresult ex, JSContext *cx)
{
  XPCThrower::Throw(ex, cx);

  return JS_FALSE;
}

static inline
jsval
GetStringByIndex(JSContext *cx, uintN index)
{
  XPCJSRuntime *rt = nsXPConnect::GetRuntime();

  if (!rt)
    return JSVAL_VOID;

  return ID_TO_VALUE(rt->GetStringID(index));
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
  jsval deep;
  if (!::JS_GetReservedSlot(cx, obj, 0, &deep)) {
    return JS_FALSE;
  }

  // Re-wrap non-primitive values if this is a deep wrapper, i.e.
  // if (deep == JSVAL_TRUE).  Note that just using GetNewOrUsed
  // on the return value of GetWrappedNativeOfJSObject will give
  // the right thing -- the unique deep wrapper associated to
  // wrappedNative.
  if (deep == JSVAL_TRUE && !JSVAL_IS_PRIMITIVE(v)) {
    JSObject *nativeObj = JSVAL_TO_OBJECT(v);
    XPCWrappedNative* wrappedNative =
      XPCWrappedNative::GetWrappedNativeOfJSObject(cx, nativeObj);
    if (!wrappedNative) {
      return ThrowException(NS_ERROR_INVALID_ARG, cx);
    }

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
  if (!::JS_ObjectIsFunction(cx, funObj) ||
      !XPCNativeWrapper::IsNativeWrapper(cx, obj)) {
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
  if (id == GetStringByIndex(cx, XPCJSRuntime::IDX_PROTOTYPE) ||
      id == GetStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
    return JS_TRUE;
  }

  // We can't use XPC_NW_BYPASS here, because we need to do a full
  // OBJ_SET_PROPERTY or OBJ_GET_PROPERTY on the wrapped native's
  // object, in order to trigger reflection done by the underlying
  // OBJ_LOOKUP_PROPERTY done by SET and GET.

  if (ShouldBypassNativeWrapper(cx, obj)) {
    XPCWrappedNative *wn = XPCNativeWrapper::GetWrappedNative(cx, obj);
    jsid interned_id;

    if (!::JS_ValueToId(cx, id, &interned_id)) {
      return JS_FALSE;
    }

    JSObject *wn_obj = wn->GetFlatJSObject();
    return aIsSet
           ? OBJ_SET_PROPERTY(cx, wn_obj, interned_id, vp)
           : OBJ_GET_PROPERTY(cx, wn_obj, interned_id, vp);
  }

  // Be paranoid, don't let people use this as another object's
  // prototype or anything like that.
  if (!XPCNativeWrapper::IsNativeWrapper(cx, obj)) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  XPCWrappedNative *wrappedNative =
    XPCNativeWrapper::GetWrappedNative(cx, obj);

  if (!wrappedNative) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  JSObject *nativeObj = wrappedNative->GetFlatJSObject();

  if (!aIsSet &&
      id == GetStringByIndex(cx, XPCJSRuntime::IDX_WRAPPED_JSOBJECT)) {
    // Return the underlying native object, the XPConnect wrapped
    // object that this additional wrapper layer wraps.

    *vp = OBJECT_TO_JSVAL(nativeObj);

    return JS_TRUE;
  }

  jsval methodName;

  if (JSVAL_IS_STRING(id)) {
    methodName = id;
  } else if (JSVAL_IS_INT(id)) {
    // Map wrapper[n] to wrapper.item(n) for get. For set, throw an
    // error as we don't support that.
    if (aIsSet) {
      return ThrowException(NS_ERROR_UNEXPECTED, cx);
    }

    methodName = GetStringByIndex(cx, XPCJSRuntime::IDX_ITEM);
  } else {
    return ThrowException(NS_ERROR_INVALID_ARG, cx);
  }

  // This will do verification and the method lookup for us.
  XPCCallContext ccx(JS_CALLER, cx, nativeObj, nsnull, methodName);

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
    // No interface, no property -- unless we're getting an indexed
    // property such as frames[0].  Handle that as a special case
    // here, now that we know there's no XPConnected .item() method.

    if (methodName != id) {
      jsid index_id;
      JSObject *pobj;
      JSProperty *prop;

      // Look up the property for the id being indexed in nativeObj.
      if (!::JS_ValueToId(cx, id, &index_id) ||
          !OBJ_LOOKUP_PROPERTY(cx, nativeObj, index_id, &pobj, &prop)) {
        return JS_FALSE;
      }

      if (prop) {
        JSBool safe = JS_FALSE;

        if (OBJ_IS_NATIVE(pobj)) {
          // At this point we know we have a native JS object (one that
          // has a JSScope for its map, containing refs to the runtime's
          // JSScopeProperty tree nodes.  Compare that property's getter
          // to the class default getter to decide whether this element
          // is safe to get -- i.e., it is not overridden, possibly by a
          // user-defined getter.

          JSScopeProperty *sprop = (JSScopeProperty *) prop;
          JSClass *nativeObjClass = JS_GET_CLASS(cx, nativeObj);

          safe = (sprop->getter == nativeObjClass->getProperty);
        }

        OBJ_DROP_PROPERTY(cx, pobj, prop);

        if (safe) {
          return ::JS_GetElement(cx, nativeObj, JSVAL_TO_INT(id), vp);
        }
      }
    }

    *vp = JSVAL_VOID;

    return JS_TRUE;
  }

  // did we find a method/attribute by that name?
  XPCNativeMember* member = ccx.GetMember();
  NS_ASSERTION(member, "not doing IDispatch, how'd this happen?");
  if (!member) {
    // No member, no property.

    *vp = JSVAL_VOID;

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

  if (!member->IsAttribute() && methodName == id) {
    // Getting the value of a method. Just return and let the value
    // from XPC_NW_NewResolve() be used.  Note that if methodName != id
    // then we fully expect that member is not an attribute and we need
    // to keep going and handle this case below.

    return JS_TRUE;
  }

  // Make sure the function we're cloning doesn't go away while
  // we're cloning it.
  AUTO_MARK_JSVAL(ccx, memberval);

  // clone a function we can use for this object
  JSObject* funobj = ::JS_CloneFunctionObject(cx, JSVAL_TO_OBJECT(memberval),
                                              wrapper->GetFlatJSObject());
  if (!funobj) {
    return JS_FALSE;
  }

  jsval *argv = nsnull;
  uintN argc = 0;

  if (methodName != id) {
    // wrapper[n] to wrapper.item(n) mapping case.

    if (member->IsAttribute()) {
      // Uh oh, somehow wrapper.item is an attribute, not a function.
      // Throw an error since we don't know how to deal with this case.

      return ThrowException(NS_ERROR_UNEXPECTED, cx);
    }

#ifdef DEBUG_XPCNativeWrapper
    printf("Mapping wrapper[%d] to wrapper.item(%d)\n", JSVAL_TO_INT(id),
           JSVAL_TO_INT(id));
#endif

    argv = &id;
    argc = 1;
  } else if (aIsSet) {
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

  // Call the getter (or method if an id access was mapped to
  // .item(n)).
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
  XPC_NW_BYPASS(cx, obj, enumerate, (cx, obj));

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                  JSObject **objp)
{
  if (id == GetStringByIndex(cx, XPCJSRuntime::IDX_WRAPPED_JSOBJECT) ||
      id == GetStringByIndex(cx, XPCJSRuntime::IDX_TO_STRING)) {
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

  // Be paranoid, don't let people use this as another object's
  // prototype or anything like that.
  if (!XPCNativeWrapper::IsNativeWrapper(cx, obj)) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  XPCWrappedNative *wrappedNative =
    XPCNativeWrapper::GetWrappedNative(cx, obj);

  if (!wrappedNative) {
    // No wrapped native, no properties.

    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    // An index is being resolved. Define the property and deal with
    // the value in the get/set property hooks.

    if (!::JS_DefineElement(cx, obj, JSVAL_TO_INT(id), JSVAL_VOID, nsnull,
                            nsnull, 0)) {
      return JS_FALSE;
    }

    *objp = obj;

    return JS_TRUE;
  }

  if (!JSVAL_IS_STRING(id)) {
    // A non-int and non-string id is being resolved. Won't be found
    // here, return early.

    return JS_TRUE;
  }

  JSObject *nativeObj = wrappedNative->GetFlatJSObject();

  // This will do verification and the method lookup for us.
  XPCCallContext ccx(JS_CALLER, cx, nativeObj, nsnull, id);

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

    return JS_TRUE;
  }

  // did we find a method/attribute by that name?
  XPCNativeMember* member = ccx.GetMember();
  NS_ASSERTION(member, "not doing IDispatch, how'd this happen?");
  if (!member) {
    // No member, nothing to resolve.

    return JS_TRUE;
  }

  // Get (and perhaps lazily create) the member's value (commonly a
  // cloneable function).
  jsval memberval;
  if (!member->GetValue(ccx, iface, &memberval)) {
    return ThrowException(NS_ERROR_XPC_BAD_CONVERT_JS, cx);
  }

  JSString *str = JSVAL_TO_STRING(id);
  if (!str) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  jsval v;

  if (member->IsConstant()) {
    v = memberval;
  } else if (member->IsAttribute()) {
    // An attribute is being resolved. Define the property, the value
    // will be dealt with in the get/set hooks.

    // XXX: We should really just have getters and setters for
    // properties and not do it the hard and expensive way.

    v = JSVAL_VOID;
  } else {
    // We're dealing with a method member here. Clone a function we can
    // use for this object.  NB: cx's newborn roots will protect funobj
    // and funWrapper and its object from GC.

    JSObject* funobj =
      ::JS_CloneFunctionObject(cx, JSVAL_TO_OBJECT(memberval),
                               wrapper->GetFlatJSObject());
    if (!funobj) {
      return JS_FALSE;
    }

#ifdef DEBUG_XPCNativeWrapper
    printf("Wrapping function object for %s\n",
           ::JS_GetStringBytes(JSVAL_TO_STRING(id)));
#endif

    // Create a new function that'll call the method we got from the
    // member. This new function's parent will be the method to call from
    // the function, and that's how we get the method when this function
    // is called.
    JSFunction *funWrapper =
      ::JS_NewFunction(cx, XPC_NW_FunctionWrapper, 0, 0, funobj,
                       "Components.lookupMethod function wrapper");
    if (!funWrapper) {
      return JS_FALSE;
    }

    v = OBJECT_TO_JSVAL(::JS_GetFunctionObject(funWrapper));
  }

  if (!::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                             ::JS_GetStringLength(str), v, nsnull, nsnull,
                             0)) {
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
  XPC_NW_BYPASS_TEST(cx, obj, checkAccess, (cx, obj, id, mode, vp));

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  XPC_NW_BYPASS_TEST(cx, obj, call, (cx, obj, argc, argv, rval));

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
  XPC_NW_BYPASS_TEST(cx, obj, construct, (cx, obj, argc, argv, rval));

  return JS_TRUE;
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

  jsval native = argv[0];

  if (JSVAL_IS_PRIMITIVE(native)) {
    return ThrowException(NS_ERROR_XPC_BAD_CONVERT_JS, cx);
  }

  JSObject *nativeObj = JSVAL_TO_OBJECT(native);

  XPCWrappedNative *wrappedNative;

  // XXXbz if this is a deep wrap perhaps we shouldn't be using
  // |obj| and should call GetNewOrUsed instead?
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
  if (!::JS_SetReservedSlot(cx, wrapperObj, 0, BOOLEAN_TO_JSVAL(isDeep))) {
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

extern nsISupports *
GetIdentityObject(JSContext *cx, JSObject *obj);

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

  JSObject *other = JSVAL_TO_OBJECT(v);

  *bp = (obj == other ||
         GetIdentityObject(cx, obj) == GetIdentityObject(cx, other));

  return JS_TRUE;
}

extern JSBool JS_DLL_CALLBACK
XPC_WN_Shared_ToString(JSContext *cx, JSObject *obj,
                       uintN argc, jsval *argv, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_NW_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
  // Be paranoid, don't let people use this as another object's
  // prototype or anything like that.
  if (!XPCNativeWrapper::IsNativeWrapper(cx, obj)) {
    return ThrowException(NS_ERROR_UNEXPECTED, cx);
  }

  // Check whether toString was overridden in any object along
  // the wrapped native's object's prototype chain.
  XPCJSRuntime *rt = nsXPConnect::GetRuntime();
  if (!rt)
    return JS_FALSE;

  jsid id = rt->GetStringID(XPCJSRuntime::IDX_TO_STRING);

  XPCWrappedNative *wrappedNative =
    XPCNativeWrapper::GetWrappedNative(cx, obj);

  JSObject *wn_obj = wrappedNative->GetFlatJSObject();
  jsval toStringVal;

  // Check whether toString has been overridden from its XPCWrappedNative
  // default native method.
  if (!OBJ_GET_PROPERTY(cx, wn_obj, id, &toStringVal)) {
    return JS_FALSE;
  }

  JSBool overridden = JS_TypeOfValue(cx, toStringVal) != JSTYPE_FUNCTION;
  if (!overridden) {
    JSObject *toStringFunObj = JSVAL_TO_OBJECT(toStringVal);
    JSFunction *toStringFun = (JSFunction*) ::JS_GetPrivate(cx, toStringFunObj);

    overridden =
      ::JS_GetFunctionNative(cx, toStringFun) != XPC_WN_Shared_ToString;
  }

  JSString* str;
  if (overridden) {
    // Something overrides XPCWrappedNative.prototype.toString, we
    // should defer to it.

    str = ::JS_ValueToString(cx, OBJECT_TO_JSVAL(wn_obj));
  } else {
    // Ok, we do no damage, and add value, by returning our own idea
    // of what toString() should be.

    nsAutoString resultString;
    resultString.AppendLiteral("[object XPCNativeWrapper");

    if (wrappedNative) {
      JSString *str = ::JS_ValueToString(cx, OBJECT_TO_JSVAL(wn_obj));
      if (!str) {
        return JS_FALSE;
      }

      resultString.Append(' ');
      resultString.Append(NS_REINTERPRET_CAST(PRUnichar *,
                                              ::JS_GetStringChars(str)),
                          ::JS_GetStringLength(str));
    }

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
    return NS_ERROR_OUT_OF_MEMORY;
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
      !::JS_SetReservedSlot(cx, obj, 0, JSVAL_TRUE)) {
    return nsnull;
  }

  wrapper->SetNativeWrapper(obj);
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
