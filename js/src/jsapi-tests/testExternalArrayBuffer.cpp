/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */

#include "jsfriendapi.h"
#include "jsapi-tests/tests.h"
#include "vm/ArrayBufferObject.h"

char test_data[] =
    "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void GC(JSContext* cx) {
  JS_GC(cx);
  // Trigger another to wait for background finalization to end.
  JS_GC(cx);
}

BEGIN_TEST(testExternalArrayBuffer) {
  size_t length = sizeof(test_data);
  JS::RootedObject obj(
      cx, JS_NewArrayBufferWithExternalContents(cx, length, test_data));
  GC(cx);
  CHECK(VerifyObject(obj, length));
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
  CHECK(data);
  CHECK(test_data == data);

  return true;
}

END_TEST(testExternalArrayBuffer)
