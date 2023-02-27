/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testBoundFunction) {
  EXEC("function foo() {}");
  JS::RootedValue foo(cx);
  EVAL("foo", &foo);
  JS::RootedValue bound(cx);
  EVAL("foo.bind(1)", &bound);

  JS::Rooted<JSObject*> foofun(cx, &foo.toObject());
  JS::Rooted<JSObject*> boundfun(cx, &bound.toObject());

  CHECK(!JS_ObjectIsBoundFunction(foofun));
  CHECK(JS_ObjectIsBoundFunction(boundfun));

  CHECK(!JS_GetBoundFunctionTarget(foofun));
  JSObject* target = JS_GetBoundFunctionTarget(boundfun);
  CHECK(!!target);
  CHECK(JS_ObjectIsFunction(target));
  JS::RootedValue targetVal(cx, JS::ObjectValue(*target));
  CHECK_SAME(foo, targetVal);

  return true;
}
END_TEST(testBoundFunction)
