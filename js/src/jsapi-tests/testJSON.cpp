/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>
#include <string.h>

#include "js/Array.h"  // JS::IsArrayObject
#include "js/Exception.h"
#include "js/friend/ErrorMessages.h"  // JSMSG_*
#include "js/JSON.h"
#include "js/MemoryFunctions.h"
#include "js/Printf.h"
#include "js/PropertyAndElement.h"  // JS_GetProperty
#include "jsapi-tests/tests.h"

using namespace JS;

static bool Cmp(const char16_t* buffer, uint32_t length,
                const char16_t* expected) {
  uint32_t i = 0;
  while (*expected) {
    if (i >= length || *expected != *buffer) {
      return false;
    }
    ++expected;
    ++buffer;
    ++i;
  }
  return i == length;
}

static bool Callback(const char16_t* buffer, uint32_t length, void* data) {
  const char16_t** data_ = static_cast<const char16_t**>(data);
  const char16_t* expected = *data_;
  *data_ = nullptr;
  return Cmp(buffer, length, expected);
}

BEGIN_TEST(testToJSON_same) {
  // Primitives
  Rooted<Value> input(cx);
  input = JS::TrueValue();
  CHECK(TryToJSON(cx, input, u"true"));

  input = JS::FalseValue();
  CHECK(TryToJSON(cx, input, u"false"));

  input = JS::NullValue();
  CHECK(TryToJSON(cx, input, u"null"));

  input.setInt32(0);
  CHECK(TryToJSON(cx, input, u"0"));

  input.setInt32(1);
  CHECK(TryToJSON(cx, input, u"1"));

  input.setInt32(-1);
  CHECK(TryToJSON(cx, input, u"-1"));

  input.setDouble(1);
  CHECK(TryToJSON(cx, input, u"1"));

  input.setDouble(1.75);
  CHECK(TryToJSON(cx, input, u"1.75"));

  input.setDouble(9e9);
  CHECK(TryToJSON(cx, input, u"9000000000"));

  input.setDouble(std::numeric_limits<double>::infinity());
  CHECK(TryToJSON(cx, input, u"null"));
  return true;
}

bool TryToJSON(JSContext* cx, Handle<Value> value, const char16_t* expected) {
  {
    Rooted<Value> v(cx, value);
    const char16_t* expected_ = expected;
    CHECK(JS_Stringify(cx, &v, nullptr, JS::NullHandleValue, Callback,
                       &expected_));
    CHECK_NULL(expected_);
  }
  {
    const char16_t* expected_ = expected;
    CHECK(JS::ToJSON(cx, value, nullptr, JS::NullHandleValue, Callback,
                     &expected_));
    CHECK_NULL(expected_);
  }
  return true;
}
END_TEST(testToJSON_same)

BEGIN_TEST(testToJSON_different) {
  Rooted<Symbol*> symbol(cx, NewSymbol(cx, nullptr));
  Rooted<Value> value(cx, SymbolValue(symbol));

  CHECK(JS::ToJSON(cx, value, nullptr, JS::NullHandleValue, UnreachedCallback,
                   nullptr));
  const char16_t* expected = u"null";
  CHECK(JS_Stringify(cx, &value, nullptr, JS::NullHandleValue, Callback,
                     &expected));
  CHECK_NULL(expected);
  return true;
}

static bool UnreachedCallback(const char16_t*, uint32_t, void*) {
  MOZ_CRASH("Should not call the callback");
}

END_TEST(testToJSON_different)
