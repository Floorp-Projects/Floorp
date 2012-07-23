/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "tests.h"
#include "jsfriendapi.h"

using namespace js;

BEGIN_TEST(testTypedArrays)
{
    bool ok = true;

    ok = ok &&
        TestPlainTypedArray<JS_NewInt8Array, int8_t, JS_GetInt8ArrayData>(cx) &&
        TestPlainTypedArray<JS_NewUint8Array, uint8_t, JS_GetUint8ArrayData>(cx) &&
        TestPlainTypedArray<JS_NewUint8ClampedArray, uint8_t, JS_GetUint8ClampedArrayData>(cx) &&
        TestPlainTypedArray<JS_NewInt16Array, int16_t, JS_GetInt16ArrayData>(cx) &&
        TestPlainTypedArray<JS_NewUint16Array, uint16_t, JS_GetUint16ArrayData>(cx) &&
        TestPlainTypedArray<JS_NewInt32Array, int32_t, JS_GetInt32ArrayData>(cx) &&
        TestPlainTypedArray<JS_NewUint32Array, uint32_t, JS_GetUint32ArrayData>(cx) &&
        TestPlainTypedArray<JS_NewFloat32Array, float, JS_GetFloat32ArrayData>(cx) &&
        TestPlainTypedArray<JS_NewFloat64Array, double, JS_GetFloat64ArrayData>(cx);

    size_t nbytes = sizeof(double) * 8;
    RootedObject buffer(cx, JS_NewArrayBuffer(cx, nbytes));
    CHECK(JS_IsArrayBufferObject(buffer, cx));

    RootedObject proto(cx, JS_GetPrototype(buffer));
    CHECK(!JS_IsArrayBufferObject(proto, cx));
    RootedObject dummy(cx, JS_GetParent(proto));
    CHECK(!JS_IsArrayBufferObject(dummy, cx));

    CHECK_EQUAL(JS_GetArrayBufferByteLength(buffer, cx), nbytes);
    memset(JS_GetArrayBufferData(buffer, cx), 1, nbytes);

    ok = ok &&
        TestArrayFromBuffer<JS_NewInt8ArrayWithBuffer, JS_NewInt8ArrayFromArray, int8_t, JS_GetInt8ArrayData>(cx) &&
        TestArrayFromBuffer<JS_NewUint8ArrayWithBuffer, JS_NewUint8ArrayFromArray, uint8_t, JS_GetUint8ArrayData>(cx) &&
        TestArrayFromBuffer<JS_NewUint8ClampedArrayWithBuffer, JS_NewUint8ClampedArrayFromArray, uint8_t, JS_GetUint8ClampedArrayData>(cx) &&
        TestArrayFromBuffer<JS_NewInt16ArrayWithBuffer, JS_NewInt16ArrayFromArray, int16_t, JS_GetInt16ArrayData>(cx) &&
        TestArrayFromBuffer<JS_NewUint16ArrayWithBuffer, JS_NewUint16ArrayFromArray, uint16_t, JS_GetUint16ArrayData>(cx) &&
        TestArrayFromBuffer<JS_NewInt32ArrayWithBuffer, JS_NewInt32ArrayFromArray, int32_t, JS_GetInt32ArrayData>(cx) &&
        TestArrayFromBuffer<JS_NewUint32ArrayWithBuffer, JS_NewUint32ArrayFromArray, uint32_t, JS_GetUint32ArrayData>(cx) &&
        TestArrayFromBuffer<JS_NewFloat32ArrayWithBuffer, JS_NewFloat32ArrayFromArray, float, JS_GetFloat32ArrayData>(cx) &&
        TestArrayFromBuffer<JS_NewFloat64ArrayWithBuffer, JS_NewFloat64ArrayFromArray, double, JS_GetFloat64ArrayData>(cx);

    return ok;
}

template<JSObject *Create(JSContext *, uint32_t),
         typename Element,
         Element *GetData(JSObject *, JSContext *)>
bool
TestPlainTypedArray(JSContext *cx)
{
    RootedObject array(cx, Create(cx, 7));
    CHECK(JS_IsTypedArrayObject(array, cx));
    RootedObject proto(cx, JS_GetPrototype(array));
    CHECK(!JS_IsTypedArrayObject(proto, cx));
    RootedObject dummy(cx, JS_GetParent(proto));
    CHECK(!JS_IsTypedArrayObject(dummy, cx));

    CHECK_EQUAL(JS_GetTypedArrayLength(array, cx), 7);
    CHECK_EQUAL(JS_GetTypedArrayByteOffset(array, cx), 0);
    CHECK_EQUAL(JS_GetTypedArrayByteLength(array, cx), sizeof(Element) * 7);

    Element *data;
    CHECK(data = GetData(array, cx));
    *data = 13;
    jsval v;
    CHECK(JS_GetElement(cx, array, 0, &v));
    CHECK_SAME(v, INT_TO_JSVAL(13));

    return true;
}

template<JSObject *CreateWithBuffer(JSContext *, JSObject *, uint32_t, int32_t),
         JSObject *CreateFromArray(JSContext *, JSObject *),
         typename Element,
         Element *GetData(JSObject *, JSContext *)>
bool
TestArrayFromBuffer(JSContext *cx)
{
    size_t elts = 8;
    size_t nbytes = elts * sizeof(Element);
    RootedObject buffer(cx, JS_NewArrayBuffer(cx, nbytes));
    uint8_t *bufdata;
    CHECK(bufdata = JS_GetArrayBufferData(buffer, cx));
    memset(bufdata, 1, nbytes);

    RootedObject array(cx, CreateWithBuffer(cx, buffer, 0, -1));
    CHECK_EQUAL(JS_GetTypedArrayLength(array, cx), elts);
    CHECK_EQUAL(JS_GetTypedArrayByteOffset(array, cx), 0);
    CHECK_EQUAL(JS_GetTypedArrayByteLength(array, cx), nbytes);

    Element *data;
    CHECK(data = GetData(array, cx));
    CHECK(bufdata = JS_GetArrayBufferData(buffer, cx));
    CHECK_EQUAL((void*) data, (void*) bufdata);

    CHECK_EQUAL(*bufdata, 1);
    CHECK_EQUAL(*reinterpret_cast<uint8_t*>(data), 1);

    RootedObject shortArray(cx, CreateWithBuffer(cx, buffer, 0, elts / 2));
    CHECK_EQUAL(JS_GetTypedArrayLength(shortArray, cx), elts / 2);
    CHECK_EQUAL(JS_GetTypedArrayByteOffset(shortArray, cx), 0);
    CHECK_EQUAL(JS_GetTypedArrayByteLength(shortArray, cx), nbytes / 2);

    RootedObject ofsArray(cx, CreateWithBuffer(cx, buffer, nbytes / 2, -1));
    CHECK_EQUAL(JS_GetTypedArrayLength(ofsArray, cx), elts / 2);
    CHECK_EQUAL(JS_GetTypedArrayByteOffset(ofsArray, cx), nbytes / 2);
    CHECK_EQUAL(JS_GetTypedArrayByteLength(ofsArray, cx), nbytes / 2);

    // Make sure all 3 views reflect the same buffer at the expected locations
    jsval v = INT_TO_JSVAL(39);
    JS_SetElement(cx, array, 0, &v);
    jsval v2;
    CHECK(JS_GetElement(cx, array, 0, &v2));
    CHECK_SAME(v, v2);
    CHECK(JS_GetElement(cx, shortArray, 0, &v2));
    CHECK_SAME(v, v2);
    CHECK_EQUAL(long(JSVAL_TO_INT(v)), long(reinterpret_cast<Element*>(data)[0]));

    v = INT_TO_JSVAL(40);
    JS_SetElement(cx, array, elts / 2, &v);
    CHECK(JS_GetElement(cx, array, elts / 2, &v2));
    CHECK_SAME(v, v2);
    CHECK(JS_GetElement(cx, ofsArray, 0, &v2));
    CHECK_SAME(v, v2);
    CHECK_EQUAL(long(JSVAL_TO_INT(v)), long(reinterpret_cast<Element*>(data)[elts / 2]));

    v = INT_TO_JSVAL(41);
    JS_SetElement(cx, array, elts - 1, &v);
    CHECK(JS_GetElement(cx, array, elts - 1, &v2));
    CHECK_SAME(v, v2);
    CHECK(JS_GetElement(cx, ofsArray, elts / 2 - 1, &v2));
    CHECK_SAME(v, v2);
    CHECK_EQUAL(long(JSVAL_TO_INT(v)), long(reinterpret_cast<Element*>(data)[elts - 1]));

    RootedObject copy(cx, CreateFromArray(cx, array));
    CHECK(JS_GetElement(cx, array, 0, &v));
    CHECK(JS_GetElement(cx, copy, 0, &v2));
    CHECK_SAME(v, v2);

    /* The copy should not see changes in the original */
    v2 = INT_TO_JSVAL(42);
    JS_SetElement(cx, array, 0, &v2);
    CHECK(JS_GetElement(cx, copy, 0, &v2));
    CHECK_SAME(v2, v); /* v is still the original value from 'array' */

    return true;
}

END_TEST(testTypedArrays)
