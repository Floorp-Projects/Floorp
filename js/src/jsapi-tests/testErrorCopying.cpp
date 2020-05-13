/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Tests that the column number of error reports is properly copied over from
 * other reports when invoked from the C++ api.
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Exception.h"
#include "jsapi-tests/tests.h"

BEGIN_TEST(testErrorCopying_columnCopied) {
  // 0        1         2
  // 1234567890123456789012345678
  EXEC("function check() { Object; foo; }");

  JS::RootedValue rval(cx);
  CHECK(!JS_CallFunctionName(cx, global, "check", JS::HandleValueArray::empty(),
                             &rval));
  JS::ExceptionStack exnStack(cx);
  CHECK(JS::StealPendingExceptionStack(cx, &exnStack));

  js::ErrorReport report(cx);
  CHECK(report.init(cx, exnStack, js::ErrorReport::WithSideEffects));

  CHECK_EQUAL(report.report()->column, 28u);
  return true;
}

END_TEST(testErrorCopying_columnCopied)
