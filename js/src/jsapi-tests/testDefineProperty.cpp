/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/PropertyAndElement.h"  // JS_DefineProperty
#include "jsapi-tests/tests.h"

BEGIN_TEST(testDefineProperty_bug564344) {
  JS::RootedValue x(cx);
  EVAL(
      "function f() {}\n"
      "var x = {p: f};\n"
      "x.p();  // brand x's scope\n"
      "x;",
      &x);

  JS::RootedObject obj(cx, x.toObjectOrNull());
  for (int i = 0; i < 2; i++) {
    CHECK(JS_DefineProperty(cx, obj, "q", JS::UndefinedHandleValue, 0));
  }
  return true;
}
END_TEST(testDefineProperty_bug564344)
