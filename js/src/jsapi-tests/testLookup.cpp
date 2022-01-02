/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/PropertyAndElement.h"  // JS_DefineProperty, JS_DefinePropertyById, JS_GetProperty
#include "jsapi-tests/tests.h"
#include "vm/JSFunction.h"  // for js::IsInternalFunctionObject

#include "vm/JSObject-inl.h"

BEGIN_TEST(testLookup_bug522590) {
  // Define a function that makes method-bearing objects.
  JS::RootedValue x(cx);
  EXEC("function mkobj() { return {f: function () {return 2;}} }");

  // Calling mkobj() multiple times must create multiple functions in ES5.
  EVAL("mkobj().f !== mkobj().f", &x);
  CHECK(x.isTrue());

  // Now make x.f a method.
  EVAL("mkobj()", &x);
  JS::RootedObject xobj(cx, x.toObjectOrNull());

  // This lookup must not return an internal function object.
  JS::RootedValue r(cx);
  CHECK(JS_GetProperty(cx, xobj, "f", &r));
  CHECK(r.isObject());
  JSObject* funobj = &r.toObject();
  CHECK(funobj->is<JSFunction>());
  CHECK(!js::IsInternalFunctionObject(*funobj));

  return true;
}
END_TEST(testLookup_bug522590)

static const JSClass DocumentAllClass = {"DocumentAll",
                                         JSCLASS_EMULATES_UNDEFINED};

bool document_resolve(JSContext* cx, JS::HandleObject obj, JS::HandleId id,
                      bool* resolvedp) {
  // If id is "all", resolve document.all=true.
  JS::RootedValue v(cx);
  if (!JS_IdToValue(cx, id, &v)) {
    return false;
  }

  if (v.isString()) {
    JSString* str = v.toString();
    JSLinearString* linearStr = JS_EnsureLinearString(cx, str);
    if (!linearStr) {
      return false;
    }
    if (JS_LinearStringEqualsLiteral(linearStr, "all")) {
      JS::Rooted<JSObject*> docAll(cx, JS_NewObject(cx, &DocumentAllClass));
      if (!docAll) {
        return false;
      }

      JS::Rooted<JS::Value> allValue(cx, JS::ObjectValue(*docAll));
      if (!JS_DefinePropertyById(cx, obj, id, allValue, JSPROP_RESOLVING)) {
        return false;
      }

      *resolvedp = true;
      return true;
    }
  }

  *resolvedp = false;
  return true;
}

static const JSClassOps document_classOps = {
    nullptr,           // addProperty
    nullptr,           // delProperty
    nullptr,           // enumerate
    nullptr,           // newEnumerate
    document_resolve,  // resolve
    nullptr,           // mayResolve
    nullptr,           // finalize
    nullptr,           // call
    nullptr,           // hasInstance
    nullptr,           // construct
    nullptr,           // trace
};

static const JSClass document_class = {"document", 0, &document_classOps};

BEGIN_TEST(testLookup_bug570195) {
  JS::RootedObject obj(cx, JS_NewObject(cx, &document_class));
  CHECK(obj);
  CHECK(JS_DefineProperty(cx, global, "document", obj, 0));
  JS::RootedValue v(cx);
  EVAL("document.all ? true : false", &v);
  CHECK(v.isFalse());
  EVAL("document.hasOwnProperty('all')", &v);
  CHECK(v.isTrue());
  return true;
}
END_TEST(testLookup_bug570195)
