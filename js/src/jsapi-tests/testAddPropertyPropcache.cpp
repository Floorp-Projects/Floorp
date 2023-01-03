/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Array.h"               // JS::NewArrayObject
#include "js/PropertyAndElement.h"  // JS_DefineElement, JS_DefineProperty
#include "jsapi-tests/tests.h"

static int callCount = 0;

static bool AddProperty(JSContext* cx, JS::HandleObject obj, JS::HandleId id,
                        JS::HandleValue v) {
  callCount++;
  return true;
}

static const JSClassOps AddPropertyClassOps = {
    AddProperty,  // addProperty
    nullptr,      // delProperty
    nullptr,      // enumerate
    nullptr,      // newEnumerate
    nullptr,      // resolve
    nullptr,      // mayResolve
    nullptr,      // finalize
    nullptr,      // call
    nullptr,      // construct
    nullptr,      // trace
};

static const JSClass AddPropertyClass = {"AddPropertyTester", 0,
                                         &AddPropertyClassOps};

BEGIN_TEST(testAddPropertyHook) {
  /*
   * Do the test a bunch of times, because sometimes we seem to randomly
   * miss the propcache.
   */
  static const int ExpectedCount = 100;

  JS::RootedObject obj(cx, JS::NewArrayObject(cx, 0));
  CHECK(obj);
  JS::RootedValue arr(cx, JS::ObjectValue(*obj));

  CHECK(JS_DefineProperty(cx, global, "arr", arr, JSPROP_ENUMERATE));

  JS::RootedObject arrObj(cx, &arr.toObject());
  for (int i = 0; i < ExpectedCount; ++i) {
    obj = JS_NewObject(cx, &AddPropertyClass);
    CHECK(obj);
    CHECK(JS_DefineElement(cx, arrObj, i, obj, JSPROP_ENUMERATE));
  }

  // Now add a prop to each of the objects, but make sure to do
  // so at the same bytecode location so we can hit the propcache.
  EXEC(
      "'use strict';                                     \n"
      "for (var i = 0; i < arr.length; ++i)              \n"
      "  arr[i].prop = 42;                               \n");

  CHECK(callCount == ExpectedCount);

  return true;
}
END_TEST(testAddPropertyHook)
