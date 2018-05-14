/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */

#include "jsfriendapi.h"

#include "jsapi-tests/tests.h"

BEGIN_TEST(testArrayBuffer_bug720949_steal)
{
    static const unsigned NUM_TEST_BUFFERS  = 2;
    static const unsigned MAGIC_VALUE_1 = 3;
    static const unsigned MAGIC_VALUE_2 = 17;

    JS::RootedObject buf_len1(cx), buf_len200(cx);
    JS::RootedObject tarray_len1(cx), tarray_len200(cx);

    uint32_t sizes[NUM_TEST_BUFFERS] = { sizeof(uint32_t), 200 * sizeof(uint32_t) };
    JS::HandleObject testBuf[NUM_TEST_BUFFERS] = { buf_len1, buf_len200 };
    JS::HandleObject testArray[NUM_TEST_BUFFERS] = { tarray_len1, tarray_len200 };

    // Single-element ArrayBuffer (uses fixed slots for storage)
    CHECK(buf_len1 = JS_NewArrayBuffer(cx, sizes[0]));
    CHECK(tarray_len1 = JS_NewInt32ArrayWithBuffer(cx, testBuf[0], 0, -1));

    CHECK(JS_SetElement(cx, testArray[0], 0, MAGIC_VALUE_1));

    // Many-element ArrayBuffer (uses dynamic storage)
    CHECK(buf_len200 = JS_NewArrayBuffer(cx, 200 * sizeof(uint32_t)));
    CHECK(tarray_len200 = JS_NewInt32ArrayWithBuffer(cx, testBuf[1], 0, -1));

    for (unsigned i = 0; i < NUM_TEST_BUFFERS; i++) {
        JS::HandleObject obj = testBuf[i];
        JS::HandleObject view = testArray[i];
        uint32_t size = sizes[i];
        JS::RootedValue v(cx);

        // Byte lengths should all agree
        CHECK(JS_IsArrayBufferObject(obj));
        CHECK_EQUAL(JS_GetArrayBufferByteLength(obj), size);
        CHECK(JS_GetProperty(cx, obj, "byteLength", &v));
        CHECK(v.isInt32(size));
        CHECK(JS_GetProperty(cx, view, "byteLength", &v));
        CHECK(v.isInt32(size));

        // Modifying the underlying data should update the value returned through the view
        {
            JS::AutoCheckCannotGC nogc;
            bool sharedDummy;
            uint8_t* data = JS_GetArrayBufferData(obj, &sharedDummy, nogc);
            CHECK(data != nullptr);
            *reinterpret_cast<uint32_t*>(data) = MAGIC_VALUE_2;
        }
        CHECK(JS_GetElement(cx, view, 0, &v));
        CHECK(v.isInt32(MAGIC_VALUE_2));

        // Steal the contents
        void* contents = JS_StealArrayBufferContents(cx, obj);
        CHECK(contents != nullptr);

        CHECK(JS_IsDetachedArrayBufferObject(obj));

        // Transfer to a new ArrayBuffer
        JS::RootedObject dst(cx, JS_NewArrayBufferWithContents(cx, size, contents));
        CHECK(JS_IsArrayBufferObject(dst));
        {
            JS::AutoCheckCannotGC nogc;
            bool sharedDummy;
            (void) JS_GetArrayBufferData(obj, &sharedDummy, nogc);
        }

        JS::RootedObject dstview(cx, JS_NewInt32ArrayWithBuffer(cx, dst, 0, -1));
        CHECK(dstview != nullptr);

        CHECK_EQUAL(JS_GetArrayBufferByteLength(dst), size);
        {
            JS::AutoCheckCannotGC nogc;
            bool sharedDummy;
            uint8_t* data = JS_GetArrayBufferData(dst, &sharedDummy, nogc);
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
BEGIN_TEST(testArrayBuffer_bug720949_viewList)
{
    JS::RootedObject buffer(cx);

    // No views
    buffer = JS_NewArrayBuffer(cx, 2000);
    buffer = nullptr;
    GC(cx);

    // One view.
    {
        buffer = JS_NewArrayBuffer(cx, 2000);
        JS::RootedObject view(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, -1));
        void* contents = JS_StealArrayBufferContents(cx, buffer);
        CHECK(contents != nullptr);
        JS_free(nullptr, contents);
        GC(cx);
        CHECK(hasDetachedBuffer(view));
        CHECK(JS_IsDetachedArrayBufferObject(buffer));
        view = nullptr;
        GC(cx);
        buffer = nullptr;
        GC(cx);
    }

    // Two views
    {
        buffer = JS_NewArrayBuffer(cx, 2000);

        JS::RootedObject view1(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, -1));
        JS::RootedObject view2(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 1, 200));

        // Remove, re-add a view
        view2 = nullptr;
        GC(cx);
        view2 = JS_NewUint8ArrayWithBuffer(cx, buffer, 1, 200);

        // Detach
        void* contents = JS_StealArrayBufferContents(cx, buffer);
        CHECK(contents != nullptr);
        JS_free(nullptr, contents);

        CHECK(hasDetachedBuffer(view1));
        CHECK(hasDetachedBuffer(view2));
        CHECK(JS_IsDetachedArrayBufferObject(buffer));

        view1 = nullptr;
        GC(cx);
        view2 = nullptr;
        GC(cx);
        buffer = nullptr;
        GC(cx);
    }

    return true;
}

static void GC(JSContext* cx)
{
    JS_GC(cx);
    JS_GC(cx); // Trigger another to wait for background finalization to end
}

bool hasDetachedBuffer(JS::HandleObject obj) {
    JS::RootedValue v(cx);
    return JS_GetProperty(cx, obj, "byteLength", &v) && v.toInt32() == 0;
}

END_TEST(testArrayBuffer_bug720949_viewList)

BEGIN_TEST(testArrayBuffer_externalize)
{
    if (!testWithSize(cx, 2))    // ArrayBuffer data stored inline in the object.
        return false;
    if (!testWithSize(cx, 2000)) // ArrayBuffer data stored out-of-line in a separate heap allocation.
        return false;

    return true;
}

bool testWithSize(JSContext* cx, size_t n)
{
    JS::RootedObject buffer(cx, JS_NewArrayBuffer(cx, n));
    CHECK(buffer != nullptr);

    JS::RootedObject view(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, -1));
    CHECK(view != nullptr);

    void* contents = JS_ExternalizeArrayBufferContents(cx, buffer);
    CHECK(contents != nullptr);
    uint32_t actualLength;
    CHECK(hasExpectedLength(cx, view, &actualLength));
    CHECK(actualLength == n);
    CHECK(!JS_IsDetachedArrayBufferObject(buffer));
    CHECK(JS_GetArrayBufferByteLength(buffer) == uint32_t(n));

    uint8_t* uint8Contents = static_cast<uint8_t*>(contents);
    CHECK(*uint8Contents == 0);
    uint8_t randomByte(rand() % UINT8_MAX);
    *uint8Contents = randomByte;

    JS::RootedValue v(cx);
    CHECK(JS_GetElement(cx, view, 0, &v));
    CHECK(v.toInt32() == randomByte);

    view = nullptr;
    GC(cx);

    CHECK(JS_DetachArrayBuffer(cx, buffer));
    GC(cx);
    CHECK(*uint8Contents == randomByte);
    JS_free(cx, contents);
    GC(cx);
    buffer = nullptr;
    GC(cx);

    return true;
}

static void GC(JSContext* cx)
{
    JS_GC(cx);
    JS_GC(cx); // Trigger another to wait for background finalization to end
}

static bool
hasExpectedLength(JSContext* cx, JS::HandleObject obj, uint32_t* len)
{
    JS::RootedValue v(cx);
    if (!JS_GetProperty(cx, obj, "byteLength", &v))
        return false;
    *len = v.toInt32();
    return true;
}

END_TEST(testArrayBuffer_externalize)

BEGIN_TEST(testArrayBuffer_customFreeFunc)
{
    ExternalData data("One two three four");

    // The buffer takes ownership of the data.
    JS::RootedObject buffer(cx, JS_NewExternalArrayBuffer(cx, data.len(), data.contents(),
        &ExternalData::freeCallback, &data));
    CHECK(buffer);
    CHECK(!data.wasFreed());

    uint32_t len;
    bool isShared;
    uint8_t* bufferData;
    js::GetArrayBufferLengthAndData(buffer, &len, &isShared, &bufferData);
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

BEGIN_TEST(testArrayBuffer_staticContents)
{
    ExternalData data("One two three four");

    // When not passing a free function, the buffer doesn't own the data.
    JS::RootedObject buffer(cx, JS_NewExternalArrayBuffer(cx, data.len(), data.contents(),
        nullptr));
    CHECK(buffer);
    CHECK(!data.wasFreed());

    uint32_t len;
    bool isShared;
    uint8_t* bufferData;
    js::GetArrayBufferLengthAndData(buffer, &len, &isShared, &bufferData);
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

BEGIN_TEST(testArrayBuffer_stealDetachExternal)
{
    ExternalData data("One two three four");
    JS::RootedObject buffer(cx, JS_NewExternalArrayBuffer(cx, data.len(), data.contents(),
        &ExternalData::freeCallback, &data));
    CHECK(buffer);
    CHECK(!data.wasFreed());

    void* stolenContents = JS_StealArrayBufferContents(cx, buffer);
    // External buffers are currently not stealable, since stealing only
    // gives you a pointer with no indication how to free it. So this should
    // copy the data.
    CHECK(stolenContents != data.contents());
    CHECK(strcmp(reinterpret_cast<char*>(stolenContents), data.asString()) == 0);
    // External buffers are currently not stealable, so this should keep the
    // reference to the data and just mark the buffer as detached.
    CHECK(JS_IsDetachedArrayBufferObject(buffer));
    CHECK(!data.wasFreed());

    buffer = nullptr;
    JS_GC(cx);
    JS_GC(cx);
    CHECK(data.wasFreed());

    return true;
}
END_TEST(testArrayBuffer_stealDetachExternal)