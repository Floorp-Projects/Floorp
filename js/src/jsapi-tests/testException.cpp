/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/CallAndConstruct.h"    // JS_CallFunctionValue
#include "js/ColumnNumber.h"        // JS::ColumnNumberOneOrigin
#include "js/PropertyAndElement.h"  // JS_GetProperty
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

BEGIN_TEST(testException_getCause) {
  JS::RootedValue err(cx);
  EVAL("new Error('message', { cause: new Error('message 2') })", &err);
  CHECK(err.isObject());

  JS::RootedString msg(cx, JS::ToString(cx, err));
  CHECK(msg);
  // Check that we have the outer error
  bool match;
  CHECK(JS_StringEqualsLiteral(cx, msg, "Error: message", &match));
  CHECK(match);

  JS::Rooted<mozilla::Maybe<JS::Value>> maybeCause(
      cx, JS::GetExceptionCause(&err.toObject()));
  CHECK(maybeCause.isSome());
  JS::RootedValue cause(cx, *maybeCause);
  CHECK(cause.isObject());

  msg = JS::ToString(cx, cause);
  CHECK(msg);
  // Check that we have the inner error
  CHECK(JS_StringEqualsLiteral(cx, msg, "Error: message 2", &match));
  CHECK(match);

  maybeCause = JS::GetExceptionCause(&cause.toObject());
  CHECK(maybeCause.isNothing());

  return true;
}
END_TEST(testException_getCause)

BEGIN_TEST(testException_getCausePlainObject) {
  JS::RootedObject plain(cx, JS_NewPlainObject(cx));
  CHECK(plain);
  JS::Rooted<mozilla::Maybe<JS::Value>> maybeCause(
      cx, JS::GetExceptionCause(plain));
  CHECK(maybeCause.isNothing());
  return true;
}
END_TEST(testException_getCausePlainObject)

BEGIN_TEST(testException_createErrorWithCause) {
  JS::RootedString empty(cx, JS_GetEmptyString(cx));
  JS::Rooted<mozilla::Maybe<JS::Value>> cause(
      cx, mozilla::Some(JS::Int32Value(-1)));
  JS::RootedValue err(cx);
  CHECK(JS::CreateError(cx, JSEXN_ERR, nullptr, empty, 1,
                        JS::ColumnNumberOneOrigin(1), nullptr, empty, cause,
                        &err));
  CHECK(err.isObject());
  JS::Rooted<mozilla::Maybe<JS::Value>> maybeCause(
      cx, JS::GetExceptionCause(&err.toObject()));
  CHECK(maybeCause.isSome());
  CHECK_SAME(*cause, *maybeCause);

  CHECK(JS::CreateError(cx, JSEXN_ERR, nullptr, empty, 1,
                        JS::ColumnNumberOneOrigin(1), nullptr, empty,
                        JS::NothingHandleValue, &err));
  CHECK(err.isObject());
  maybeCause = JS::GetExceptionCause(&err.toObject());
  CHECK(maybeCause.isNothing());

  return true;
}
END_TEST(testException_createErrorWithCause)
