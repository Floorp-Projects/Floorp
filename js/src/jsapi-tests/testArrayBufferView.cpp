/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */

#include "jsfriendapi.h"

#include "jsapi-tests/tests.h"

BEGIN_TEST(testArrayBufferView_type)
{
    CHECK((TestViewType<uint8_t,
                        Create<JS_NewUint8Array, 7>,
                        JS_GetObjectAsUint8Array,
                        js::Scalar::Uint8,
                        7, 7>(cx)));

    CHECK((TestViewType<int8_t,
                        Create<JS_NewInt8Array, 33>,
                        JS_GetObjectAsInt8Array,
                        js::Scalar::Int8,
                        33, 33>(cx)));

    CHECK((TestViewType<uint8_t,
                        Create<JS_NewUint8ClampedArray, 7>,
                        JS_GetObjectAsUint8ClampedArray,
                        js::Scalar::Uint8Clamped,
                        7, 7>(cx)));

    CHECK((TestViewType<uint16_t,
                        Create<JS_NewUint16Array, 3>,
                        JS_GetObjectAsUint16Array,
                        js::Scalar::Uint16,
                        3, 6>(cx)));

    CHECK((TestViewType<int16_t,
                        Create<JS_NewInt16Array, 17>,
                        JS_GetObjectAsInt16Array,
                        js::Scalar::Int16,
                        17, 34>(cx)));

    CHECK((TestViewType<uint32_t,
                        Create<JS_NewUint32Array, 15>,
                        JS_GetObjectAsUint32Array,
                        js::Scalar::Uint32,
                        15, 60>(cx)));

    CHECK((TestViewType<int32_t,
                        Create<JS_NewInt32Array, 8>,
                        JS_GetObjectAsInt32Array,
                        js::Scalar::Int32,
                        8, 32>(cx)));

    CHECK((TestViewType<float,
                        Create<JS_NewFloat32Array, 7>,
                        JS_GetObjectAsFloat32Array,
                        js::Scalar::Float32,
                        7, 28>(cx)));

    CHECK((TestViewType<double,
                        Create<JS_NewFloat64Array, 9>,
                        JS_GetObjectAsFloat64Array,
                        js::Scalar::Float64,
                        9, 72>(cx)));

    CHECK((TestViewType<uint8_t,
                        CreateDataView,
                        JS_GetObjectAsArrayBufferView,
                        js::Scalar::TypeMax,
                        8, 8>(cx)));

    JS::Rooted<JS::Value> hasTypedObject(cx);
    EVAL("typeof TypedObject !== 'undefined'", &hasTypedObject);
    if (hasTypedObject.isTrue()) {
        JS::Rooted<JS::Value> tval(cx);
        EVAL("var T = new TypedObject.StructType({ x: TypedObject.uint32 });\n"
             "new T(new ArrayBuffer(4));",
             &tval);

        JS::Rooted<JSObject*> tobj(cx, &tval.toObject());
        CHECK(!JS_IsArrayBufferViewObject(tobj));
    }

    return true;
}

static JSObject *
CreateDataView(JSContext *cx)
{
    JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
    if (!global)
        return nullptr;

    static const char code[] = "new DataView(new ArrayBuffer(8))";

    JS::Rooted<JS::Value> val(cx);
    if (!JS_EvaluateScript(cx, global, code, strlen(code), __FILE__, __LINE__, &val))
        return nullptr;

    JS::Rooted<JSObject*> dv(cx, &val.toObject());
    if (!JS_IsDataViewObject(dv))
        return nullptr;

    return dv;
}

template<JSObject * CreateTypedArray(JSContext *cx, uint32_t length),
         size_t Length>
static JSObject *
Create(JSContext *cx)
{
    return CreateTypedArray(cx, Length);
}

template<typename T,
         JSObject * CreateViewType(JSContext *cx),
         JSObject * GetObjectAs(JSObject *obj, uint32_t *length, T **data),
         js::Scalar::Type ExpectedType,
         uint32_t ExpectedLength,
         uint32_t ExpectedByteLength>
bool TestViewType(JSContext *cx)
{
    JS::Rooted<JSObject*> obj(cx, CreateViewType(cx));
    CHECK(obj);

    CHECK(JS_IsArrayBufferViewObject(obj));

    CHECK(JS_GetArrayBufferViewType(obj) == ExpectedType);

    CHECK(JS_GetArrayBufferViewByteLength(obj) == ExpectedByteLength);

    {
        JS::AutoCheckCannotGC nogc;
        T *data1 = static_cast<T*>(JS_GetArrayBufferViewData(obj, nogc));

        T *data2;
        uint32_t len;
        CHECK(obj == GetObjectAs(obj, &len, &data2));
        CHECK(data1 == data2);
        CHECK(len == ExpectedLength);
    }

    return true;
}

END_TEST(testArrayBufferView_type)
