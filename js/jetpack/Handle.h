/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Newman <mozilla@benjamn.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_jetpack_HandleParent_h
#define mozilla_jetpack_HandleParent_h

#include "mozilla/jetpack/PHandleParent.h"
#include "mozilla/jetpack/PHandleChild.h"

#include "jsapi.h"
#include "jsobj.h"
#include "jscntxt.h"

namespace mozilla {
namespace jetpack {

/**
 * BaseType should be one of PHandleParent or PHandleChild; see the
 * HandleParent and HandleChild typedefs at the bottom of this file.
 */
template <class BaseType>
class Handle
  : public BaseType
{
  Handle(Handle* parent)
    : mParent(parent)
    , mObj(NULL)
    , mRuntime(NULL)
  {}

  BaseType* AllocPHandle() {
    return new Handle(this);
  }

  bool DeallocPHandle(BaseType* actor) {
    delete actor;
    return true;
  }

public:

  Handle()
    : mParent(NULL)
    , mObj(NULL)
    , mRuntime(NULL)
  {}

  ~Handle() { TearDown(); }

  static Handle* FromJSObject(JSContext* cx, JSObject* obj) {
    // TODO Convert non-Handles to Handles somehow?
    return Unwrap(cx, obj);
  }

  static Handle* FromJSVal(JSContext* cx, jsval val) {
    if (!JSVAL_IS_OBJECT(val))
      return NULL;
    return Unwrap(cx, JSVAL_TO_OBJECT(val));
  }

  JSObject* ToJSObject(JSContext* cx) const {
    if (!mObj && !mRuntime) {
      JSAutoRequest request(cx);

      JSClass* clasp = const_cast<JSClass*>(&sHandle_JSClass);
      JSObject* obj = JS_NewObject(cx, clasp, NULL, NULL);
      if (!obj)
        return NULL;
      js::AutoObjectRooter root(cx, obj);

      JSPropertySpec* ps = const_cast<JSPropertySpec*>(sHandle_Properties);
      JSFunctionSpec* fs = const_cast<JSFunctionSpec*>(sHandle_Functions);
      JSRuntime* rt;

      if (JS_SetPrivate(cx, obj, (void*)this) &&
          JS_DefineProperties(cx, obj, ps) &&
          JS_DefineFunctions(cx, obj, fs) &&
          (rt = JS_GetRuntime(cx)) &&
          JS_AddObjectRoot(cx, &mObj))
      {
        mObj = obj;
        mRuntime = rt;
      }
    }
    return mObj;
  }

protected:

  void ActorDestroy(typename Handle::ActorDestroyReason why) {
    TearDown();
  }

private:

  static bool IsParent(const PHandleParent* handle) { return true; }
  static bool IsParent(const PHandleChild* handle) { return false; }

  void TearDown() {
    if (mObj) {
      mObj->setPrivate(NULL);
      mObj = NULL;
      // Nulling out mObj effectively unroots the object, but we still
      // need to remove the root, else the JS engine will complain at
      // shutdown.
      NS_ASSERTION(mRuntime, "Should have a JSRuntime if we had an object");
      js_RemoveRoot(mRuntime, (void*)&mObj);
      // By not nulling out mRuntime, we prevent ToJSObject from
      // reviving an invalidated/destroyed handle.
    }
  }

  static const JSClass        sHandle_JSClass;
  static const JSPropertySpec sHandle_Properties[];
  static const JSFunctionSpec sHandle_Functions[];

  Handle* const mParent;

  // Used to cache the JSObject returned by ToJSObject, which is
  // otherwise a const method.
  mutable JSObject*  mObj;
  mutable JSRuntime* mRuntime;

  static Handle*
  Unwrap(JSContext* cx, JSObject* obj) {
    while (obj && obj->getJSClass() != &sHandle_JSClass)
      obj = obj->getProto();

    if (!obj)
      return NULL;

    Handle* self = static_cast<Handle*>(JS_GetPrivate(cx, obj));

    NS_ASSERTION(!self || self->ToJSObject(cx) == obj,
                 "Wrapper and wrapped object disagree?");

    return self;
  }

  static JSBool
  GetParent(JSContext* cx, JSObject* obj, jsid, jsval* vp) {
    JS_SET_RVAL(cx, vp, JSVAL_NULL);

    Handle* self = Unwrap(cx, obj);
    if (!self)
      return JS_TRUE;

    Handle* parent = self->mParent;
    if (!parent)
      return JS_TRUE;

    JSObject* pobj = parent->ToJSObject(cx);
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(pobj));

    return JS_TRUE;
  }

  static JSBool
  GetIsValid(JSContext* cx, JSObject* obj, jsid, jsval* vp) {
    Handle* self = Unwrap(cx, obj);
    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(!!self));
    return JS_TRUE;
  }

  static JSBool
  Invalidate(JSContext* cx, uintN argc, jsval* vp) {
    if (argc > 0) {
      JS_ReportError(cx, "invalidate takes zero arguments");
      return JS_FALSE;
    }

    Handle* self = Unwrap(cx, JS_THIS_OBJECT(cx, vp));
    if (self) {
      JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_TRUE));
      if (!Send__delete__(self)) {
        JS_ReportError(cx, "Failed to send __delete__ while invalidating");
        return JS_FALSE;
      }
    } else {
      JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_FALSE));
    }

    return JS_TRUE;
  }

  static JSBool
  CreateHandle(JSContext* cx, uintN argc, jsval* vp) {
    if (argc > 0) {
      JS_ReportError(cx, "createHandle takes zero arguments");
      return JS_FALSE;
    }

    Handle* self = Unwrap(cx, JS_THIS_OBJECT(cx, vp));
    if (!self) {
      JS_ReportError(cx, "Tried to create child from invalid handle");
      return JS_FALSE;
    }

    BaseType* child = self->SendPHandleConstructor();
    if (!child) {
      JS_ReportError(cx, "Failed to construct child");
      return JS_FALSE;
    }

    JSObject* obj = static_cast<Handle*>(child)->ToJSObject(cx);
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));

    return JS_TRUE;
  }

  static void
  Finalize(JSContext* cx, JSObject* obj) {
    Handle* self = Unwrap(cx, obj);
    // Avoid warnings about unused return values:
    self && Send__delete__(self);
  }

};

template <class BaseType>
const JSClass
Handle<BaseType>::sHandle_JSClass = {
  "IPDL Handle", JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub,
  JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub,
  JS_ConvertStub, Handle::Finalize,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

#define HANDLE_PROP_FLAGS (JSPROP_READONLY | JSPROP_PERMANENT)

template <class BaseType>
const JSPropertySpec
Handle<BaseType>::sHandle_Properties[] = {
  { "parent",  0, HANDLE_PROP_FLAGS, GetParent,  NULL },
  { "isValid", 0, HANDLE_PROP_FLAGS, GetIsValid, NULL },
  { 0, 0, 0, NULL, NULL }
};

#undef HANDLE_PROP_FLAGS

#define HANDLE_FUN_FLAGS (JSFUN_FAST_NATIVE |   \
                          JSPROP_READONLY |     \
                          JSPROP_PERMANENT)

template <class BaseType>
const JSFunctionSpec
Handle<BaseType>::sHandle_Functions[] = {
  JS_FN("invalidate",   Invalidate,   0, HANDLE_FUN_FLAGS),
  JS_FN("createHandle", CreateHandle, 0, HANDLE_FUN_FLAGS),
  JS_FS_END
};

#undef HANDLE_FUN_FLAGS

// The payoff for using templates is that these two implementations are
// guaranteed to be perfectly symmetric:
typedef Handle<PHandleParent> HandleParent;
typedef Handle<PHandleChild> HandleChild;

} // namespace jetpack
} // namespace mozilla

#endif
