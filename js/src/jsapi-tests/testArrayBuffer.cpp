/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */

#include "jsfriendapi.h"

#include "builtin/TestingFunctions.h"
#include "js/Array.h"        // JS::NewArrayObject
#include "js/ArrayBuffer.h"  // JS::{GetArrayBuffer{ByteLength,Data},IsArrayBufferObject,NewArrayBuffer{,WithContents},StealArrayBufferContents}
#include "js/Exception.h"
#include "js/MemoryFunctions.h"
#include "jsapi-tests/tests.h"

BEGIN_TEST(testArrayBuffer_bug720949_steal) {
  static const unsigned NUM_TEST_BUFFERS = 2;
  static const unsigned MAGIC_VALUE_1 = 3;
  static const unsigned MAGIC_VALUE_2 = 17;

  JS::RootedObject buf_len1(cx), buf_len200(cx);
  JS::RootedObject tarray_len1(cx), tarray_len200(cx);

  uint32_t sizes[NUM_TEST_BUFFERS] = {sizeof(uint32_t), 200 * sizeof(uint32_t)};
  JS::HandleObject testBuf[NUM_TEST_BUFFERS] = {buf_len1, buf_len200};
  JS::HandleObject testArray[NUM_TEST_BUFFERS] = {tarray_len1, tarray_len200};

  // Single-element ArrayBuffer (uses fixed slots for storage)
  CHECK(buf_len1 = JS::NewArrayBuffer(cx, sizes[0]));
  CHECK(tarray_len1 = JS_NewInt32ArrayWithBuffer(cx, testBuf[0], 0, -1));

  CHECK(JS_SetElement(cx, testArray[0], 0, MAGIC_VALUE_1));

  // Many-element ArrayBuffer (uses dynamic storage)
  CHECK(buf_len200 = JS::NewArrayBuffer(cx, 200 * sizeof(uint32_t)));
  CHECK(tarray_len200 = JS_NewInt32ArrayWithBuffer(cx, testBuf[1], 0, -1));

  for (unsigned i = 0; i < NUM_TEST_BUFFERS; i++) {
    JS::HandleObject obj = testBuf[i];
    JS::HandleObject view = testArray[i];
    uint32_t size = sizes[i];
    JS::RootedValue v(cx);

    // Byte lengths should all agree
    CHECK(JS::IsArrayBufferObject(obj));
    CHECK_EQUAL(JS::GetArrayBufferByteLength(obj), size);
    CHECK(JS_GetProperty(cx, obj, "byteLength", &v));
    CHECK(v.isInt32(size));
    CHECK(JS_GetProperty(cx, view, "byteLength", &v));
    CHECK(v.isInt32(size));

    // Modifying the underlying data should update the value returned through
    // the view
    {
      JS::AutoCheckCannotGC nogc;
      bool sharedDummy;
      uint8_t* data = JS::GetArrayBufferData(obj, &sharedDummy, nogc);
      CHECK(data != nullptr);
      *reinterpret_cast<uint32_t*>(data) = MAGIC_VALUE_2;
    }
    CHECK(JS_GetElement(cx, view, 0, &v));
    CHECK(v.isInt32(MAGIC_VALUE_2));

    // Steal the contents
    void* contents = JS::StealArrayBufferContents(cx, obj);
    CHECK(contents != nullptr);

    CHECK(JS::IsDetachedArrayBufferObject(obj));

    // Transfer to a new ArrayBuffer
    JS::RootedObject dst(cx,
                         JS::NewArrayBufferWithContents(cx, size, contents));
    CHECK(JS::IsArrayBufferObject(dst));
    {
      JS::AutoCheckCannotGC nogc;
      bool sharedDummy;
      (void)JS::GetArrayBufferData(obj, &sharedDummy, nogc);
    }

    JS::RootedObject dstview(cx, JS_NewInt32ArrayWithBuffer(cx, dst, 0, -1));
    CHECK(dstview != nullptr);

    CHECK_EQUAL(JS::GetArrayBufferByteLength(dst), size);
    {
      JS::AutoCheckCannotGC nogc;
      bool sharedDummy;
      uint8_t* data = JS::GetArrayBufferData(dst, &sharedDummy, nogc);
      CHECK(data != nullptr);
      CHECK_EQUAL(*reinterpret_cast<uint32_t*>(data), MAGIC_VALUE_2);
    }
    CHECK(JS_GetElement(cx, dstview, 0, &v));
    CHECK(v.isInt32(MAGIC_VALUE_2));
  }

  return true;
}
END_TEST(testArrayBuffer_bug720949_steal)

// Varying number of views of a buffer, to test the detachment weak pointers
BEGIN_TEST(testArrayBuffer_bug720949_viewList) {
  JS::RootedObject buffer(cx);

  // No views
  buffer = JS::NewArrayBuffer(cx, 2000);
  buffer = nullptr;
  GC(cx);

  // One view.
  {
    buffer = JS::NewArrayBuffer(cx, 2000);
    JS::RootedObject view(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, -1));
    void* contents = JS::StealArrayBufferContents(cx, buffer);
    CHECK(contents != nullptr);
    JS_free(nullptr, contents);
    GC(cx);
    CHECK(hasDetachedBuffer(view));
    CHECK(JS::IsDetachedArrayBufferObject(buffer));
    view = nullptr;
    GC(cx);
    buffer = nullptr;
    GC(cx);
  }

  // Two views
  {
    buffer = JS::NewArrayBuffer(cx, 2000);

    JS::RootedObject view1(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, -1));
    JS::RootedObject view2(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 1, 200));

    // Remove, re-add a view
    view2 = nullptr;
    GC(cx);
    view2 = JS_NewUint8ArrayWithBuffer(cx, buffer, 1, 200);

    // Detach
    void* contents = JS::StealArrayBufferContents(cx, buffer);
    CHECK(contents != nullptr);
    JS_free(nullptr, contents);

    CHECK(hasDetachedBuffer(view1));
    CHECK(hasDetachedBuffer(view2));
    CHECK(JS::IsDetachedArrayBufferObject(buffer));

    view1 = nullptr;
    GC(cx);
    view2 = nullptr;
    GC(cx);
    buffer = nullptr;
    GC(cx);
  }

  return true;
}

static void GC(JSContext* cx) {
  JS_GC(cx);
  JS_GC(cx);  // Trigger another to wait for background finalization to end
}

bool hasDetachedBuffer(JS::HandleObject obj) {
  JS::RootedValue v(cx);
  return JS_GetProperty(cx, obj, "byteLength", &v) && v.toInt32() == 0;
}

END_TEST(testArrayBuffer_bug720949_viewList)

BEGIN_TEST(testArrayBuffer_customFreeFunc) {
  ExternalData data("One two three four");

  // The buffer takes ownership of the data.
  JS::RootedObject buffer(
      cx, JS::NewExternalArrayBuffer(cx, data.len(), data.contents(),
                                     &ExternalData::freeCallback, &data));
  CHECK(buffer);
  CHECK(!data.wasFreed());

  uint32_t len;
  bool isShared;
  uint8_t* bufferData;
  JS::GetArrayBufferLengthAndData(buffer, &len, &isShared, &bufferData);
  CHECK_EQUAL(len, data.len());
  CHECK(bufferData == data.contents());
  CHECK(strcmp(reinterpret_cast<char*>(bufferData), data.asString()) == 0);

  buffer = nullptr;
  JS_GC(cx);
  JS_GC(cx);
  CHECK(data.wasFreed());

  return true;
}
END_TEST(testArrayBuffer_customFreeFunc)

BEGIN_TEST(testArrayBuffer_staticContents) {
  ExternalData data("One two three four");

  // When not passing a free function, the buffer doesn't own the data.
  JS::RootedObject buffer(
      cx, JS::NewExternalArrayBuffer(cx, data.len(), data.contents(), nullptr));
  CHECK(buffer);
  CHECK(!data.wasFreed());

  uint32_t len;
  bool isShared;
  uint8_t* bufferData;
  JS::GetArrayBufferLengthAndData(buffer, &len, &isShared, &bufferData);
  CHECK_EQUAL(len, data.len());
  CHECK(bufferData == data.contents());
  CHECK(strcmp(reinterpret_cast<char*>(bufferData), data.asString()) == 0);

  buffer = nullptr;
  JS_GC(cx);
  JS_GC(cx);
  CHECK(!data.wasFreed());

  data.free();
  return true;
}
END_TEST(testArrayBuffer_staticContents)

BEGIN_TEST(testArrayBuffer_stealDetachExternal) {
  static const char dataBytes[] = "One two three four";
  ExternalData data(dataBytes);
  JS::RootedObject buffer(
      cx, JS::NewExternalArrayBuffer(cx, data.len(), data.contents(),
                                     &ExternalData::freeCallback, &data));
  CHECK(buffer);
  CHECK(!data.wasFreed());

  void* stolenContents = JS::StealArrayBufferContents(cx, buffer);

  // External buffers are stealable: the data is copied into freshly allocated
  // memory, and the buffer's data pointer is cleared (immediately freeing the
  // data) and the buffer is marked as detached.
  CHECK(stolenContents != data.contents());
  CHECK(strcmp(reinterpret_cast<char*>(stolenContents), dataBytes) == 0);
  CHECK(data.wasFreed());
  CHECK(JS::IsDetachedArrayBufferObject(buffer));

  JS_free(cx, stolenContents);
  return true;
}
END_TEST(testArrayBuffer_stealDetachExternal)

BEGIN_TEST(testArrayBuffer_serializeExternal) {
  JS::RootedValue serializeValue(cx);

  {
    JS::RootedFunction serialize(cx);
    serialize =
        JS_NewFunction(cx, js::testingFunc_serialize, 1, 0, "serialize");
    CHECK(serialize);

    serializeValue.setObject(*JS_GetFunctionObject(serialize));
  }

  ExternalData data("One two three four");
  JS::RootedObject externalBuffer(
      cx, JS::NewExternalArrayBuffer(cx, data.len(), data.contents(),
                                     &ExternalData::freeCallback, &data));
  CHECK(externalBuffer);
  CHECK(!data.wasFreed());

  JS::RootedValue v(cx, JS::ObjectValue(*externalBuffer));
  JS::RootedObject transferMap(cx,
                               JS::NewArrayObject(cx, JS::HandleValueArray(v)));
  CHECK(transferMap);

  JS::RootedValueArray<2> args(cx);
  args[0].setObject(*externalBuffer);
  args[1].setObject(*transferMap);

  // serialize(externalBuffer, [externalBuffer]) should throw for an unhandled
  // BufferContents kind.
  CHECK(!JS::Call(cx, JS::UndefinedHandleValue, serializeValue,
                  JS::HandleValueArray(args), &v));

  JS::ExceptionStack exnStack(cx);
  CHECK(JS::StealPendingExceptionStack(cx, &exnStack));

  js::ErrorReport report(cx);
  CHECK(report.init(cx, exnStack, js::ErrorReport::NoSideEffects));

  CHECK_EQUAL(report.report()->errorNumber,
              static_cast<unsigned int>(JSMSG_SC_NOT_TRANSFERABLE));

  // Data should have been left alone.
  CHECK(!data.wasFreed());

  v.setNull();
  transferMap = nullptr;
  args[0].setNull();
  args[1].setNull();
  externalBuffer = nullptr;

  JS_GC(cx);
  JS_GC(cx);
  CHECK(data.wasFreed());

  return true;
}
END_TEST(testArrayBuffer_serializeExternal)
