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
    FILE *test_stream = test_file.open(test_filename);
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

#ifdef JSGC_USE_EXACT_ROOTING
    // Ensure that fd is closed after object been GCed.
    // Check the fd returned from object created in a function,
    // then do the GC, in order to guarantee the object is freed when
    // exact rooting is not on.
    int fd = GetNewObjectFD();
    GC(cx);
    CHECK(!fd_is_valid(fd));
#endif

    // Neuter mapped array buffer.
    CHECK(TestNeuterObject());

    // Clone mapped array buffer.
    CHECK(TestCloneObject());

    test_file.remove();

    return true;
}

JSObject *CreateNewObject(const int offset, const int length)
{
    int fd = open(test_filename, O_RDONLY);
    void *ptr;
    int new_fd;
    if (!JS_CreateMappedArrayBufferContents(fd, &new_fd, offset, length, &ptr))
        return nullptr;
    JSObject *obj = JS_NewArrayBufferWithContents(cx, ptr);
    close(fd);

    return obj;
}

// Return the fd from object created in the stack.
int GetNewObjectFD()
{
    JS::RootedObject obj(cx, CreateNewObject(0, 12));
    int fd = getFD(obj);
    CHECK(fd_is_valid(fd));

    return fd;
}

bool VerifyObject(JS::HandleObject obj, const int offset, const int length)
{
    CHECK(obj != nullptr);
    CHECK(JS_IsArrayBufferObject(obj));
    CHECK_EQUAL(JS_GetArrayBufferByteLength(obj), length);
    js::ArrayBufferObject *buf = &obj->as<js::ArrayBufferObject>();
    CHECK(buf->isMappedArrayBuffer());
    const char *data = reinterpret_cast<const char *>(JS_GetArrayBufferData(obj));
    CHECK(data != nullptr);
    CHECK(memcmp(data, test_data + offset, length) == 0);

    return true;
}

bool TestCreateObject(const int offset, const int length)
{
    JS::RootedObject obj(cx, CreateNewObject(offset, length));
    CHECK(VerifyObject(obj, offset, length));

    return true;
}

bool TestReleaseContents()
{
    int fd = open(test_filename, O_RDONLY);
    void *ptr;
    int new_fd;
    if (!JS_CreateMappedArrayBufferContents(fd, &new_fd, 0, 12, &ptr))
        return false;
    CHECK(fd_is_valid(new_fd));
    JS_ReleaseMappedArrayBufferContents(new_fd, ptr, 12);
    CHECK(!fd_is_valid(new_fd));
    close(fd);

    return true;
}

bool TestNeuterObject()
{
    JS::RootedObject obj(cx, CreateNewObject(8, 12));
    CHECK(obj != nullptr);
    int fd = getFD(obj);
    CHECK(fd_is_valid(fd));
    JS_NeuterArrayBuffer(cx, obj);
    CHECK(isNeutered(obj));
    CHECK(!fd_is_valid(fd));

    return true;
}

bool TestCloneObject()
{
    JS::RootedObject obj1(cx, CreateNewObject(8, 12));
    CHECK(obj1 != nullptr);
    JSAutoStructuredCloneBuffer cloned_buffer;
    JS::RootedValue v1(cx, OBJECT_TO_JSVAL(obj1));
    const JSStructuredCloneCallbacks *callbacks = js::GetContextStructuredCloneCallbacks(cx);
    CHECK(cloned_buffer.write(cx, v1, callbacks, nullptr));
    JS::RootedValue v2(cx);
    CHECK(cloned_buffer.read(cx, &v2, callbacks, nullptr));
    JS::RootedObject obj2(cx, JSVAL_TO_OBJECT(v2));
    CHECK(VerifyObject(obj2, 8, 12));

    return true;
}

bool isNeutered(JS::HandleObject obj)
{
    JS::RootedValue v(cx);
    return JS_GetProperty(cx, obj, "byteLength", &v) && v.toInt32() == 0;
}

int getFD(JS::HandleObject obj)
{
    CHECK(obj != nullptr);
    js::ArrayBufferObject *buf = &obj->as<js::ArrayBufferObject>();
    return buf->getMappingFD();
}

static bool fd_is_valid(int fd)
{
     return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

static void GC(JSContext *cx)
{
    JS_GC(JS_GetRuntime(cx));
    // Trigger another to wait for background finalization to end.
    JS_GC(JS_GetRuntime(cx));
}

END_TEST(testMappedArrayBuffer_bug945152)
#endif
