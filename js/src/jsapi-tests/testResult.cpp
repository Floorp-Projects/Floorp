/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"      // JS_NewPlainObject
#include "js/Result.h"  // Error, Ok, Result
#include "js/String.h"  // JS_NewStringCopyZ
#include "jsapi-tests/tests.h"

JS::Result<> SimpleSuccess(JSContext*) { return JS::Ok(); }

JS::Result<> SimpleFailure(JSContext*) { return JS::Result<>{JS::Error()}; }

JS::Result<int> PODSuccess(JSContext*) { return 42; }

JS::Result<int> PODFailure(JSContext*) { return JS::Result<int>{JS::Error()}; }

JS::Result<JSObject*> ObjectSuccess(JSContext* cx) {
  JS::RootedObject obj{cx, JS_NewPlainObject(cx)};
  MOZ_RELEASE_ASSERT(obj);
  return obj.get();
}

JS::Result<JSObject*> ObjectFailure(JSContext*) {
  return JS::Result<JSObject*>{JS::Error()};
}

JS::Result<JSString*> StringSuccess(JSContext* cx) {
  JS::RootedString str{cx, JS_NewStringCopyZ(cx, "foo")};
  MOZ_RELEASE_ASSERT(str);
  return str.get();
}

JS::Result<JSString*> StringFailure(JSContext*) {
  return JS::Result<JSString*>{JS::Error()};
}

BEGIN_TEST(testResult_SimpleSuccess) {
  JS::Result<> result = SimpleSuccess(cx);
  CHECK(result.isOk());
  return true;
}
END_TEST(testResult_SimpleSuccess)

BEGIN_TEST(testResult_SimpleFailure) {
  JS::Result<> result = SimpleFailure(cx);
  CHECK(result.isErr());
  return true;
}
END_TEST(testResult_SimpleFailure)

BEGIN_TEST(testResult_PODSuccess) {
  JS::Result<int> result = PODSuccess(cx);
  CHECK(result.isOk());
  CHECK_EQUAL(result.unwrap(), 42);
  CHECK_EQUAL(result.inspect(), 42);
  return true;
}
END_TEST(testResult_PODSuccess)

BEGIN_TEST(testResult_PODFailure) {
  JS::Result<int> result = PODFailure(cx);
  CHECK(result.isErr());
  return true;
}
END_TEST(testResult_PODFailure)

BEGIN_TEST(testResult_ObjectSuccess) {
  JS::Result<JSObject*> result = ObjectSuccess(cx);
  CHECK(result.isOk());
  CHECK(result.inspect() != nullptr);
  CHECK(result.unwrap() != nullptr);
  return true;
}
END_TEST(testResult_ObjectSuccess)

BEGIN_TEST(testResult_ObjectFailure) {
  JS::Result<JSObject*> result = ObjectFailure(cx);
  CHECK(result.isErr());
  return true;
}
END_TEST(testResult_ObjectFailure)

BEGIN_TEST(testResult_StringSuccess) {
  JS::Result<JSString*> result = StringSuccess(cx);
  CHECK(result.isOk());
  CHECK(result.inspect() != nullptr);
  CHECK(result.unwrap() != nullptr);
  return true;
}
END_TEST(testResult_StringSuccess)

BEGIN_TEST(testResult_StringFailure) {
  JS::Result<JSString*> result = StringFailure(cx);
  CHECK(result.isErr());
  return true;
}
END_TEST(testResult_StringFailure)
