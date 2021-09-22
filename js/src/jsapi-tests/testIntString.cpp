/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testIntString_bug515273) {
  JS::RootedValue v(cx);

  EVAL("'1';", &v);
  JSString* str = v.toString();
  CHECK(JS_StringHasBeenPinned(cx, str));
  CHECK(JS_LinearStringEqualsLiteral(JS_ASSERT_STRING_IS_LINEAR(str), "1"));

  EVAL("'42';", &v);
  str = v.toString();
  CHECK(JS_StringHasBeenPinned(cx, str));
  CHECK(JS_LinearStringEqualsLiteral(JS_ASSERT_STRING_IS_LINEAR(str), "42"));

  // Short string literal should use atom.
  EVAL("'111';", &v);
  str = v.toString();
  CHECK(JS_StringHasBeenPinned(cx, str));
  CHECK(JS_LinearStringEqualsLiteral(JS_ASSERT_STRING_IS_LINEAR(str), "111"));

  // Long string literal shouldn't use atom, but just linear string.
  EVAL("'111222333';", &v);
  str = v.toString();
  CHECK(!JS_StringHasBeenPinned(cx, str));
  CHECK(JS_LinearStringEqualsLiteral(JS_ASSERT_STRING_IS_LINEAR(str),
                                     "111222333"));

  /* Test other types of static strings. */
  EVAL("'a';", &v);
  str = v.toString();
  CHECK(JS_StringHasBeenPinned(cx, str));
  CHECK(JS_LinearStringEqualsLiteral(JS_ASSERT_STRING_IS_LINEAR(str), "a"));

  EVAL("'bc';", &v);
  str = v.toString();
  CHECK(JS_StringHasBeenPinned(cx, str));
  CHECK(JS_LinearStringEqualsLiteral(JS_ASSERT_STRING_IS_LINEAR(str), "bc"));

  return true;
}
END_TEST(testIntString_bug515273)
