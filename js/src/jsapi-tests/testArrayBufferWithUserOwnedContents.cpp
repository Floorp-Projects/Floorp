/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */

#include "jsfriendapi.h"
#include "jsapi-tests/tests.h"
#include "vm/ArrayBufferObject.h"

char testData[] =
    "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

constexpr size_t testDataLength = sizeof(testData);

static void GC(JSContext* cx) {
  JS_GC(cx);
  // Trigger another to wait for background finalization to end.
  JS_GC(cx);
}

BEGIN_TEST(testArrayBufferWithUserOwnedContents) {
  JS::RootedObject obj(
      cx, JS_NewArrayBufferWithUserOwnedContents(cx, testDataLength, testData));
  GC(cx);
  CHECK(VerifyObject(obj, testDataLength));
  GC(cx);
  JS_DetachArrayBuffer(cx, obj);
  GC(cx);
  CHECK(VerifyObject(obj, 0));

  return true;
}

bool VerifyObject(JS::HandleObject obj, uint32_t length) {
  JS::AutoCheckCannotGC nogc;

  CHECK(obj);
  CHECK(JS_IsArrayBufferObject(obj));
  CHECK_EQUAL(JS_GetArrayBufferByteLength(obj), length);
  bool sharedDummy;
  const char* data = reinterpret_cast<const char*>(
      JS_GetArrayBufferData(obj, &sharedDummy, nogc));
  if (length == testDataLength) {
    CHECK(data);
    CHECK(testData == data);
  }

  return true;
}

END_TEST(testArrayBufferWithUserOwnedContents)
