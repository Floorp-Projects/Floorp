/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */

#include <fcntl.h>
#include <stdio.h>

#include "js/Array.h"        // JS::NewArrayObject
#include "js/ArrayBuffer.h"  // JS::{{Create,Release}MappedArrayBufferContents,DetachArrayBuffer,GetArrayBuffer{ByteLength,Data},Is{,Detached,Mapped}ArrayBufferObject,NewMappedArrayBufferWithContents,StealArrayBufferContents}
#include "js/StructuredClone.h"
#include "jsapi-tests/tests.h"
#include "vm/ArrayBufferObject.h"

#ifdef XP_WIN
#  include <io.h>
#  define GET_OS_FD(a) int(_get_osfhandle(a))
#else
#  include <unistd.h>
#  define GET_OS_FD(a) (a)
#endif

const char test_data[] =
    "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
const char test_filename[] = "temp-bug945152_MappedArrayBuffer";

BEGIN_TEST(testMappedArrayBuffer_bug945152) {
  TempFile test_file;
  FILE* test_stream = test_file.open(test_filename);
  CHECK(fputs(test_data, test_stream) != EOF);
  test_file.close();

  // Offset 0.
  CHECK(TestCreateObject(0, 12));

  // Aligned offset.
  CHECK(TestCreateObject(8, 12));

  // Unaligned offset.
  CHECK(CreateNewObject(11, 12) == nullptr);

  // Offset + length greater than file size.
  CHECK(CreateNewObject(8, sizeof(test_data) - 7) == nullptr);

  // Release the mapped content.
  CHECK(TestReleaseContents());

  // Detach mapped array buffer.
  CHECK(TestDetachObject());

  // Clone mapped array buffer.
  CHECK(TestCloneObject());

  // Steal mapped array buffer contents.
  CHECK(TestStealContents());

  // Transfer mapped array buffer contents.
  CHECK(TestTransferObject());

  // GC so we can remove the file we created.
  GC(cx);

  test_file.remove();

  return true;
}

JSObject* CreateNewObject(const int offset, const int length) {
  int fd = open(test_filename, O_RDONLY);
  void* ptr =
      JS::CreateMappedArrayBufferContents(GET_OS_FD(fd), offset, length);
  close(fd);
  if (!ptr) {
    return nullptr;
  }
  JSObject* obj = JS::NewMappedArrayBufferWithContents(cx, length, ptr);
  if (!obj) {
    JS::ReleaseMappedArrayBufferContents(ptr, length);
    return nullptr;
  }
  return obj;
}

bool VerifyObject(JS::HandleObject obj, uint32_t offset, uint32_t length,
                  const bool mapped) {
  JS::AutoCheckCannotGC nogc;

  CHECK(obj);
  CHECK(JS::IsArrayBufferObject(obj));
  CHECK_EQUAL(JS::GetArrayBufferByteLength(obj), length);
  if (mapped) {
    CHECK(JS::IsMappedArrayBufferObject(obj));
  } else {
    CHECK(!JS::IsMappedArrayBufferObject(obj));
  }
  bool sharedDummy;
  const char* data = reinterpret_cast<const char*>(
      JS::GetArrayBufferData(obj, &sharedDummy, nogc));
  CHECK(data);
  CHECK(memcmp(data, test_data + offset, length) == 0);

  return true;
}

bool TestCreateObject(uint32_t offset, uint32_t length) {
  JS::RootedObject obj(cx, CreateNewObject(offset, length));
  CHECK(VerifyObject(obj, offset, length, true));

  return true;
}

bool TestReleaseContents() {
  int fd = open(test_filename, O_RDONLY);
  void* ptr = JS::CreateMappedArrayBufferContents(GET_OS_FD(fd), 0, 12);
  close(fd);
  if (!ptr) {
    return false;
  }
  JS::ReleaseMappedArrayBufferContents(ptr, 12);

  return true;
}

bool TestDetachObject() {
  JS::RootedObject obj(cx, CreateNewObject(8, 12));
  CHECK(obj);
  JS::DetachArrayBuffer(cx, obj);
  CHECK(JS::IsDetachedArrayBufferObject(obj));

  return true;
}

bool TestCloneObject() {
  JS::RootedObject obj1(cx, CreateNewObject(8, 12));
  CHECK(obj1);
  JSAutoStructuredCloneBuffer cloned_buffer(
      JS::StructuredCloneScope::SameProcess, nullptr, nullptr);
  JS::RootedValue v1(cx, JS::ObjectValue(*obj1));
  CHECK(cloned_buffer.write(cx, v1, nullptr, nullptr));
  JS::RootedValue v2(cx);
  CHECK(cloned_buffer.read(cx, &v2, JS::CloneDataPolicy(), nullptr, nullptr));
  JS::RootedObject obj2(cx, v2.toObjectOrNull());
  CHECK(VerifyObject(obj2, 8, 12, false));

  return true;
}

bool TestStealContents() {
  JS::RootedObject obj(cx, CreateNewObject(8, 12));
  CHECK(obj);
  void* contents = JS::StealArrayBufferContents(cx, obj);
  CHECK(contents);
  CHECK(memcmp(contents, test_data + 8, 12) == 0);
  CHECK(JS::IsDetachedArrayBufferObject(obj));

  return true;
}

bool TestTransferObject() {
  JS::RootedObject obj1(cx, CreateNewObject(8, 12));
  CHECK(obj1);
  JS::RootedValue v1(cx, JS::ObjectValue(*obj1));

  // Create an Array of transferable values.
  JS::RootedValueVector argv(cx);
  if (!argv.append(v1)) {
    return false;
  }

  JS::RootedObject obj(
      cx, JS::NewArrayObject(cx, JS::HandleValueArray::subarray(argv, 0, 1)));
  CHECK(obj);
  JS::RootedValue transferable(cx, JS::ObjectValue(*obj));

  JSAutoStructuredCloneBuffer cloned_buffer(
      JS::StructuredCloneScope::SameProcess, nullptr, nullptr);
  JS::CloneDataPolicy policy;
  CHECK(cloned_buffer.write(cx, v1, transferable, policy, nullptr, nullptr));
  JS::RootedValue v2(cx);
  CHECK(cloned_buffer.read(cx, &v2, policy, nullptr, nullptr));
  JS::RootedObject obj2(cx, v2.toObjectOrNull());
  CHECK(VerifyObject(obj2, 8, 12, true));
  CHECK(JS::IsDetachedArrayBufferObject(obj1));

  return true;
}

static void GC(JSContext* cx) {
  JS_GC(cx);
  // Trigger another to wait for background finalization to end.
  JS_GC(cx);
}

END_TEST(testMappedArrayBuffer_bug945152)

#undef GET_OS_FD
