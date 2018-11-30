/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testException_bug860435) {
  JS::RootedValue fun(cx);

  EVAL("ReferenceError", &fun);
  CHECK(fun.isObject());

  JS::RootedValue v(cx);
  CHECK(
      JS_CallFunctionValue(cx, global, fun, JS::HandleValueArray::empty(), &v));
  CHECK(v.isObject());
  JS::RootedObject obj(cx, &v.toObject());

  CHECK(JS_GetProperty(cx, obj, "stack", &v));
  CHECK(v.isString());
  return true;
}
END_TEST(testException_bug860435)
