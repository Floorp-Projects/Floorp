/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef ENABLE_RECORD_TUPLE

#  include <string>

#  include "jsfriendapi.h"

#  include "jsapi-tests/tests.h"

static const struct TestTuple {
  const char* expr_string;
  const char* expected;
} tests[] = {
    {"#[]", "#[]"},
    {"#[1, 2, 3]", "#[1, 2, 3]"},
    {"#{}", "#{}"},
    {"#{\"a\": 1, \"b\": \"c\", \"c\": #[]}",
     "#{\"a\": 1, \"b\": \"c\", \"c\": #[]}"},
    {"Object(#[])", "#[]"},
    {"Object(#[1, 2, 3])", "#[1, 2, 3]"},
    {"Object(#{})", "#{}"},
    {"Object(#{\"a\": 1, \"b\": \"c\", \"c\": #[]})",
     "#{\"a\": 1, \"b\": \"c\", \"c\": #[]}"},
};

BEGIN_TEST(testRecordTupleToSource) {
  JS::Rooted<JS::Value> result(cx);
  for (const auto& test : tests) {
    EVAL(test.expr_string, &result);
    JSLinearString* prettyPrinted =
        JS_ASSERT_STRING_IS_LINEAR(JS_ValueToSource(cx, result));
    CHECK(JS_LinearStringEqualsAscii(prettyPrinted, test.expected));
  }

  return true;
}
END_TEST(testRecordTupleToSource)

#endif
