/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/PropertyAndElement.h"  // JS_DefineObject
#include "jsapi-tests/tests.h"

static int g_counter;

static bool CounterAdd(JSContext* cx, JS::HandleObject obj, JS::HandleId id,
                       JS::HandleValue v) {
  g_counter++;
  return true;
}

static const JSClassOps CounterClassOps = {
    CounterAdd,  // addProperty
    nullptr,     // delProperty
    nullptr,     // enumerate
    nullptr,     // newEnumerate
    nullptr,     // resolve
    nullptr,     // mayResolve
    nullptr,     // finalize
    nullptr,     // call
    nullptr,     // construct
    nullptr,     // trace
};

static const JSClass CounterClass = {"Counter", /* name */
                                     0,         /* flags */
                                     &CounterClassOps};

BEGIN_TEST(testPropCache_bug505798) {
  g_counter = 0;
  EXEC("var x = {};");
  CHECK(JS_DefineObject(cx, global, "y", &CounterClass, JSPROP_ENUMERATE));
  EXEC(
      "var arr = [x, y];\n"
      "for (var i = 0; i < arr.length; i++)\n"
      "    arr[i].p = 1;\n");
  CHECK_EQUAL(g_counter, 1);
  return true;
}
END_TEST(testPropCache_bug505798)
