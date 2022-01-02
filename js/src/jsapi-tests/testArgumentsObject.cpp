/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

#include "vm/ArgumentsObject-inl.h"
#include "vm/JSObject-inl.h"

using namespace js;

static const char NORMAL_ZERO[] = "function f() { return arguments; }";
static const char NORMAL_ONE[] = "function f(a) { return arguments; }";
static const char NORMAL_TWO[] = "function f(a, b) { return arguments; }";
static const char NORMAL_THREE[] = "function f(a, b, c) { return arguments; }";

static const char STRICT_ZERO[] =
    "function f() { 'use strict'; return arguments; }";
static const char STRICT_ONE[] =
    "function f() { 'use strict'; return arguments; }";
static const char STRICT_TWO[] =
    "function f() { 'use strict'; return arguments; }";
static const char STRICT_THREE[] =
    "function f() { 'use strict'; return arguments; }";

static const char* const CALL_CODES[] = {"f()",           "f(0)",
                                         "f(0, 1)",       "f(0, 1, 2)",
                                         "f(0, 1, 2, 3)", "f(0, 1, 2, 3, 4)"};

BEGIN_TEST(testArgumentsObject) {
  return ExhaustiveTest<0>(NORMAL_ZERO) && ExhaustiveTest<1>(NORMAL_ZERO) &&
         ExhaustiveTest<2>(NORMAL_ZERO) && ExhaustiveTest<0>(NORMAL_ONE) &&
         ExhaustiveTest<1>(NORMAL_ONE) && ExhaustiveTest<2>(NORMAL_ONE) &&
         ExhaustiveTest<3>(NORMAL_ONE) && ExhaustiveTest<0>(NORMAL_TWO) &&
         ExhaustiveTest<1>(NORMAL_TWO) && ExhaustiveTest<2>(NORMAL_TWO) &&
         ExhaustiveTest<3>(NORMAL_TWO) && ExhaustiveTest<4>(NORMAL_TWO) &&
         ExhaustiveTest<0>(NORMAL_THREE) && ExhaustiveTest<1>(NORMAL_THREE) &&
         ExhaustiveTest<2>(NORMAL_THREE) && ExhaustiveTest<3>(NORMAL_THREE) &&
         ExhaustiveTest<4>(NORMAL_THREE) && ExhaustiveTest<5>(NORMAL_THREE) &&
         ExhaustiveTest<0>(STRICT_ZERO) && ExhaustiveTest<1>(STRICT_ZERO) &&
         ExhaustiveTest<2>(STRICT_ZERO) && ExhaustiveTest<0>(STRICT_ONE) &&
         ExhaustiveTest<1>(STRICT_ONE) && ExhaustiveTest<2>(STRICT_ONE) &&
         ExhaustiveTest<3>(STRICT_ONE) && ExhaustiveTest<0>(STRICT_TWO) &&
         ExhaustiveTest<1>(STRICT_TWO) && ExhaustiveTest<2>(STRICT_TWO) &&
         ExhaustiveTest<3>(STRICT_TWO) && ExhaustiveTest<4>(STRICT_TWO) &&
         ExhaustiveTest<0>(STRICT_THREE) && ExhaustiveTest<1>(STRICT_THREE) &&
         ExhaustiveTest<2>(STRICT_THREE) && ExhaustiveTest<3>(STRICT_THREE) &&
         ExhaustiveTest<4>(STRICT_THREE) && ExhaustiveTest<5>(STRICT_THREE);
}

static const size_t MAX_ELEMS = 6;

template <size_t ArgCount>
bool ExhaustiveTest(const char funcode[]) {
  RootedValue v(cx);
  EVAL(funcode, &v);

  EVAL(CALL_CODES[ArgCount], &v);
  Rooted<ArgumentsObject*> argsobj(cx,
                                   &v.toObjectOrNull()->as<ArgumentsObject>());

  JS::RootedValueArray<MAX_ELEMS> elems(cx);

  for (size_t i = 0; i <= ArgCount; i++) {
    for (size_t j = 0; j <= ArgCount - i; j++) {
      ClearElements(elems);
      CHECK(argsobj->maybeGetElements(i, j, elems.begin()));
      for (size_t k = 0; k < j; k++) {
        CHECK(elems[k].isInt32(i + k));
      }
      for (size_t k = j; k < MAX_ELEMS - 1; k++) {
        CHECK(elems[k].isNull());
      }
      CHECK(elems[MAX_ELEMS - 1].isInt32(42));
    }
  }

  return true;
}

template <size_t N>
static void ClearElements(JS::RootedValueArray<N>& elems) {
  for (size_t i = 0; i < elems.length() - 1; i++) {
    elems[i].setNull();
  }
  elems[elems.length() - 1].setInt32(42);
}
END_TEST(testArgumentsObject)
