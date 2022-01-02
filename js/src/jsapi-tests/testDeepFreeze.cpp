/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testDeepFreeze_bug535703) {
  JS::RootedValue v(cx);
  EVAL("var x = {}; x;", &v);
  JS::RootedObject obj(cx, v.toObjectOrNull());
  CHECK(JS_DeepFreezeObject(cx, obj));  // don't crash
  EVAL("Object.isFrozen(x)", &v);
  CHECK(v.isTrue());
  return true;
}
END_TEST(testDeepFreeze_bug535703)

BEGIN_TEST(testDeepFreeze_deep) {
  JS::RootedValue a(cx), o(cx);
  EXEC(
      "var a = {}, o = a;\n"
      "for (var i = 0; i < 5000; i++)\n"
      "    a = {x: a, y: a};\n");
  EVAL("a", &a);
  EVAL("o", &o);

  JS::RootedObject aobj(cx, a.toObjectOrNull());
  CHECK(JS_DeepFreezeObject(cx, aobj));

  JS::RootedValue b(cx);
  EVAL("Object.isFrozen(a)", &b);
  CHECK(b.isTrue());
  EVAL("Object.isFrozen(o)", &b);
  CHECK(b.isTrue());
  return true;
}
END_TEST(testDeepFreeze_deep)

BEGIN_TEST(testDeepFreeze_loop) {
  JS::RootedValue x(cx), y(cx);
  EXEC("var x = [], y = {x: x}; y.y = y; x.push(x, y);");
  EVAL("x", &x);
  EVAL("y", &y);

  JS::RootedObject xobj(cx, x.toObjectOrNull());
  CHECK(JS_DeepFreezeObject(cx, xobj));

  JS::RootedValue b(cx);
  EVAL("Object.isFrozen(x)", &b);
  CHECK(b.isTrue());
  EVAL("Object.isFrozen(y)", &b);
  CHECK(b.isTrue());
  return true;
}
END_TEST(testDeepFreeze_loop)
