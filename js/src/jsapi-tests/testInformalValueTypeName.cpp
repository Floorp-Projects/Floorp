/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Array.h"  // JS::NewArrayObject
#include "js/Symbol.h"
#include "jsapi-tests/tests.h"

BEGIN_TEST(testInformalValueTypeName) {
  JS::RootedValue v1(cx, JS::ObjectOrNullValue(JS_NewPlainObject(cx)));
  CHECK(strcmp(JS::InformalValueTypeName(v1), "Object") == 0);

  JS::RootedValue v2(cx, JS::ObjectOrNullValue(JS::NewArrayObject(cx, 0)));
  CHECK(strcmp(JS::InformalValueTypeName(v2), "Array") == 0);

  JS::RootedValue v3(cx, JS::StringValue(JS_GetEmptyString(cx)));
  CHECK(strcmp(JS::InformalValueTypeName(v3), "string") == 0);

  JS::RootedValue v4(cx, JS::SymbolValue(JS::NewSymbol(cx, nullptr)));
  CHECK(strcmp(JS::InformalValueTypeName(v4), "symbol") == 0);

  JS::RootedValue v5(cx, JS::NumberValue(50.5));
  CHECK(strcmp(JS::InformalValueTypeName(v5), "number") == 0);
  JS::RootedValue v6(cx, JS::Int32Value(50));
  CHECK(strcmp(JS::InformalValueTypeName(v6), "number") == 0);
  JS::RootedValue v7(cx, JS::Float32Value(50.5));
  CHECK(strcmp(JS::InformalValueTypeName(v7), "number") == 0);

  JS::RootedValue v8(cx, JS::TrueValue());
  CHECK(strcmp(JS::InformalValueTypeName(v8), "boolean") == 0);

  JS::RootedValue v9(cx, JS::NullValue());
  CHECK(strcmp(JS::InformalValueTypeName(v9), "null") == 0);

  JS::RootedValue v10(cx, JS::UndefinedValue());
  CHECK(strcmp(JS::InformalValueTypeName(v10), "undefined") == 0);

  return true;
}
END_TEST(testInformalValueTypeName)
