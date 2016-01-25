/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */

#ifdef XP_UNIX
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "jsfriendapi.h"
#include "js/StructuredClone.h"
#include "jsapi-tests/tests.h"
#include "vm/ArrayBufferObject.h"

const char test_data[] = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
const char test_filename[] = "temp-bug945152_MappedArrayBuffer";

BEGIN_TEST(testMappedArrayBuffer_bug945152)
{
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

    test_file.remove();

    return true;
}

JSObject* CreateNewObject(const int offset, const int length)
{
    int fd = open(test_filename, O_RDONLY);
    void* ptr = JS_CreateMappedArrayBufferContents(fd, offset, length);
    close(fd);
    if (!ptr)
        return nullptr;
    JSObject* obj = JS_NewMappedArrayBufferWithContents(cx, length, ptr);
    if (!obj) {
        JS_ReleaseMappedArrayBufferContents(ptr, length);
        return nullptr;
    }
    return obj;
}

bool VerifyObject(JS::HandleObject obj, uint32_t offset, uint32_t length, const bool mapped)
{
    JS::AutoCheckCannotGC nogc;

    CHECK(obj);
    CHECK(JS_IsArrayBufferObject(obj));
    CHECK_EQUAL(JS_GetArrayBufferByteLength(obj), length);
    if (mapped)
        CHECK(JS_IsMappedArrayBufferObject(obj));
    else
        CHECK(!JS_IsMappedArrayBufferObject(obj));
    bool sharedDummy;
    const char* data =
        reinterpret_cast<const char*>(JS_GetArrayBufferData(obj, &sharedDummy, nogc));
    CHECK(data);
    CHECK(memcmp(data, test_data + offset, length) == 0);

    return true;
}

bool TestCreateObject(uint32_t offset, uint32_t length)
{
    JS::RootedObject obj(cx, CreateNewObject(offset, length));
    CHECK(VerifyObject(obj, offset, length, true));

    return true;
}

bool TestReleaseContents()
{
    int fd = open(test_filename, O_RDONLY);
    void* ptr = JS_CreateMappedArrayBufferContents(fd, 0, 12);
    close(fd);
    if (!ptr)
        return false;
    JS_ReleaseMappedArrayBufferContents(ptr, 12);

    return true;
}

bool TestDetachObject()
{
    JS::RootedObject obj(cx, CreateNewObject(8, 12));
    CHECK(obj);
    JS_DetachArrayBuffer(cx, obj, ChangeData);
    CHECK(JS_IsDetachedArrayBufferObject(obj));

    return true;
}

bool TestCloneObject()
{
    JS::RootedObject obj1(cx, CreateNewObject(8, 12));
    CHECK(obj1);
    JSAutoStructuredCloneBuffer cloned_buffer;
    JS::RootedValue v1(cx, JS::ObjectValue(*obj1));
    CHECK(cloned_buffer.write(cx, v1, nullptr, nullptr));
    JS::RootedValue v2(cx);
    CHECK(cloned_buffer.read(cx, &v2, nullptr, nullptr));
    JS::RootedObject obj2(cx, v2.toObjectOrNull());
    CHECK(VerifyObject(obj2, 8, 12, false));

    return true;
}

bool TestStealContents()
{
    JS::RootedObject obj(cx, CreateNewObject(8, 12));
    CHECK(obj);
    void* contents = JS_StealArrayBufferContents(cx, obj);
    CHECK(contents);
    CHECK(memcmp(contents, test_data + 8, 12) == 0);
    CHECK(JS_IsDetachedArrayBufferObject(obj));

    return true;
}

bool TestTransferObject()
{
    JS::RootedObject obj1(cx, CreateNewObject(8, 12));
    CHECK(obj1);
    JS::RootedValue v1(cx, JS::ObjectValue(*obj1));

    // Create an Array of transferable values.
    JS::AutoValueVector argv(cx);
    argv.append(v1);
    JS::RootedObject obj(cx, JS_NewArrayObject(cx, JS::HandleValueArray::subarray(argv, 0, 1)));
    CHECK(obj);
    JS::RootedValue transferable(cx, JS::ObjectValue(*obj));

    JSAutoStructuredCloneBuffer cloned_buffer;
    CHECK(cloned_buffer.write(cx, v1, transferable, nullptr, nullptr));
    JS::RootedValue v2(cx);
    CHECK(cloned_buffer.read(cx, &v2, nullptr, nullptr));
    JS::RootedObject obj2(cx, v2.toObjectOrNull());
    CHECK(VerifyObject(obj2, 8, 12, true));
    CHECK(JS_IsDetachedArrayBufferObject(obj1));

    return true;
}

static void GC(JSContext* cx)
{
    JS_GC(JS_GetRuntime(cx));
    // Trigger another to wait for background finalization to end.
    JS_GC(JS_GetRuntime(cx));
}

END_TEST(testMappedArrayBuffer_bug945152)
#endif
