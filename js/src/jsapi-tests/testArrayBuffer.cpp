/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"
#include "jsfriendapi.h"

#define NUM_TEST_BUFFERS 2
#define MAGIC_VALUE_1 3
#define MAGIC_VALUE_2 17

BEGIN_TEST(testArrayBuffer_bug720949_steal)
{
    js::RootedObject buf_len1(cx), buf_len200(cx);
    js::RootedObject tarray_len1(cx), tarray_len200(cx);

    uint32_t sizes[NUM_TEST_BUFFERS] = { sizeof(uint32_t), 200 * sizeof(uint32_t) };
    JS::HandleObject testBuf[NUM_TEST_BUFFERS] = { buf_len1, buf_len200 };
    JS::HandleObject testArray[NUM_TEST_BUFFERS] = { tarray_len1, tarray_len200 };

    // Single-element ArrayBuffer (uses fixed slots for storage)
    CHECK(buf_len1 = JS_NewArrayBuffer(cx, sizes[0]));
    CHECK(tarray_len1 = JS_NewInt32ArrayWithBuffer(cx, testBuf[0], 0, -1));

    jsval dummy = INT_TO_JSVAL(MAGIC_VALUE_1);
    JS_SetElement(cx, testArray[0], 0, &dummy);

    // Many-element ArrayBuffer (uses dynamic storage)
    CHECK(buf_len200 = JS_NewArrayBuffer(cx, 200 * sizeof(uint32_t)));
    CHECK(tarray_len200 = JS_NewInt32ArrayWithBuffer(cx, testBuf[1], 0, -1));

    for (unsigned i = 0; i < NUM_TEST_BUFFERS; i++) {
        JS::HandleObject obj = testBuf[i];
        JS::HandleObject view = testArray[i];
        uint32_t size = sizes[i];
        jsval v;

        // Byte lengths should all agree
        CHECK(JS_IsArrayBufferObject(obj, cx));
        CHECK_EQUAL(JS_GetArrayBufferByteLength(obj, cx), size);
        JS_GetProperty(cx, obj, "byteLength", &v);
        CHECK_SAME(v, INT_TO_JSVAL(size));
        JS_GetProperty(cx, view, "byteLength", &v);
        CHECK_SAME(v, INT_TO_JSVAL(size));

        // Modifying the underlying data should update the value returned through the view
        uint8_t *data = JS_GetArrayBufferData(obj, cx);
        CHECK(data != NULL);
        *reinterpret_cast<uint32_t*>(data) = MAGIC_VALUE_2;
        CHECK(JS_GetElement(cx, view, 0, &v));
        CHECK_SAME(v, INT_TO_JSVAL(MAGIC_VALUE_2));

        // Steal the contents
        void *contents;
        CHECK(JS_StealArrayBufferContents(cx, obj, &contents));
        CHECK(contents != NULL);

        // Check that the original ArrayBuffer is neutered
        CHECK_EQUAL(JS_GetArrayBufferByteLength(obj, cx), 0);
        CHECK(JS_GetProperty(cx, obj, "byteLength", &v));
        CHECK_SAME(v, INT_TO_JSVAL(0));
        CHECK(JS_GetProperty(cx, view, "byteLength", &v));
        CHECK_SAME(v, INT_TO_JSVAL(0));
        CHECK(JS_GetProperty(cx, view, "byteOffset", &v));
        CHECK_SAME(v, INT_TO_JSVAL(0));
        CHECK(JS_GetProperty(cx, view, "length", &v));
        CHECK_SAME(v, INT_TO_JSVAL(0));
        CHECK_EQUAL(JS_GetArrayBufferByteLength(obj, cx), 0);
        v = JSVAL_VOID;
        JS_GetElement(cx, obj, 0, &v);
        CHECK_SAME(v, JSVAL_VOID);

        // Transfer to a new ArrayBuffer
        js::RootedObject dst(cx, JS_NewArrayBufferWithContents(cx, contents));
        CHECK(JS_IsArrayBufferObject(dst, cx));
        data = JS_GetArrayBufferData(obj, cx);

        js::RootedObject dstview(cx, JS_NewInt32ArrayWithBuffer(cx, dst, 0, -1));
        CHECK(dstview != NULL);

        CHECK_EQUAL(JS_GetArrayBufferByteLength(dst, cx), size);
        data = JS_GetArrayBufferData(dst, cx);
        CHECK(data != NULL);
        CHECK_EQUAL(*reinterpret_cast<uint32_t*>(data), MAGIC_VALUE_2);
        CHECK(JS_GetElement(cx, dstview, 0, &v));
        CHECK_SAME(v, INT_TO_JSVAL(MAGIC_VALUE_2));
    }

    return true;
}
END_TEST(testArrayBuffer_bug720949_steal)

static void GC(JSContext *cx)
{
    JS_GC(JS_GetRuntime(cx));
    JS_GC(JS_GetRuntime(cx)); // Trigger another to wait for background finalization to end
}

// Varying number of views of a buffer, to test the neutering weak pointers
BEGIN_TEST(testArrayBuffer_bug720949_viewList)
{
    js::RootedObject buffer(cx);

    // No views
    buffer = JS_NewArrayBuffer(cx, 2000);
    buffer = NULL;
    GC(cx);

    // One view.
    {
        buffer = JS_NewArrayBuffer(cx, 2000);
        js::RootedObject view(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, -1));
        void *contents;
        CHECK(JS_StealArrayBufferContents(cx, buffer, &contents));
        CHECK(contents != NULL);
        JS_free(cx, contents);
        GC(cx);
        CHECK(isNeutered(view));
        CHECK(isNeutered(buffer));
        view = NULL;
        GC(cx);
        buffer = NULL;
        GC(cx);
    }

    // Two views
    {
        buffer = JS_NewArrayBuffer(cx, 2000);

        js::RootedObject view1(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, -1));
        js::RootedObject view2(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 1, 200));

        // Remove, re-add a view
        view2 = NULL;
        GC(cx);
        view2 = JS_NewUint8ArrayWithBuffer(cx, buffer, 1, 200);

        // Neuter
        void *contents;
        CHECK(JS_StealArrayBufferContents(cx, buffer, &contents));
        CHECK(contents != NULL);
        JS_free(cx, contents);

        CHECK(isNeutered(view1));
        CHECK(isNeutered(view2));
        CHECK(isNeutered(buffer));

        view1 = NULL;
        GC(cx);
        view2 = NULL;
        GC(cx);
        buffer = NULL;
        GC(cx);
    }

    return true;
}

bool isNeutered(JS::HandleObject obj) {
    JS::Value v;
    return JS_GetProperty(cx, obj, "byteLength", &v) && v.toInt32() == 0;
}

END_TEST(testArrayBuffer_bug720949_viewList)
