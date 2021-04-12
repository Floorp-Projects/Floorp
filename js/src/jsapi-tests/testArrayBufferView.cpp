/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */

#include "jsfriendapi.h"

#include "js/ArrayBuffer.h"             // JS::NewArrayBuffer
#include "js/experimental/TypedData.h"  // JS_GetArrayBufferView{Type,ByteLength,Data}, JS_GetObjectAsArrayBufferView, JS_GetObjectAs{{Ui,I}nt{8,16,32},Float{32,64}}Array, JS_IsArrayBufferViewObject, JS_NewDataView, JS_New{{Ui,I}nt{8,16,32},Float{32,64},Uint8Clamped}Array
#include "js/ScalarType.h"              // js::Scalar::Type
#include "jsapi-tests/tests.h"
#include "vm/ProxyObject.h"
#include "vm/Realm.h"

#include "vm/Realm-inl.h"

using namespace js;

BEGIN_TEST(testArrayBufferView_type) {
  CHECK((TestViewType<uint8_t, Create<JS_NewUint8Array, 7>,
                      JS_GetObjectAsUint8Array, js::Scalar::Uint8, 7, 7>(cx)));

  CHECK((TestViewType<int8_t, Create<JS_NewInt8Array, 33>,
                      JS_GetObjectAsInt8Array, js::Scalar::Int8, 33, 33>(cx)));

  CHECK((TestViewType<uint8_t, Create<JS_NewUint8ClampedArray, 7>,
                      JS_GetObjectAsUint8ClampedArray, js::Scalar::Uint8Clamped,
                      7, 7>(cx)));

  CHECK(
      (TestViewType<uint16_t, Create<JS_NewUint16Array, 3>,
                    JS_GetObjectAsUint16Array, js::Scalar::Uint16, 3, 6>(cx)));

  CHECK(
      (TestViewType<int16_t, Create<JS_NewInt16Array, 17>,
                    JS_GetObjectAsInt16Array, js::Scalar::Int16, 17, 34>(cx)));

  CHECK((
      TestViewType<uint32_t, Create<JS_NewUint32Array, 15>,
                   JS_GetObjectAsUint32Array, js::Scalar::Uint32, 15, 60>(cx)));

  CHECK((TestViewType<int32_t, Create<JS_NewInt32Array, 8>,
                      JS_GetObjectAsInt32Array, js::Scalar::Int32, 8, 32>(cx)));

  CHECK((TestViewType<float, Create<JS_NewFloat32Array, 7>,
                      JS_GetObjectAsFloat32Array, js::Scalar::Float32, 7, 28>(
      cx)));

  CHECK((TestViewType<double, Create<JS_NewFloat64Array, 9>,
                      JS_GetObjectAsFloat64Array, js::Scalar::Float64, 9, 72>(
      cx)));

  CHECK((TestViewType<uint8_t, CreateDataView, JS_GetObjectAsArrayBufferView,
                      js::Scalar::MaxTypedArrayViewType, 8, 8>(cx)));

  JS::Rooted<JS::Value> hasTypedObject(cx);
  EVAL("typeof TypedObject !== 'undefined'", &hasTypedObject);
  if (hasTypedObject.isTrue()) {
    JS::Rooted<JS::Value> tval(cx);
    EVAL(
        "var T = new TypedObject.StructType({ x: TypedObject.uint32 });\n"
        "new T(new ArrayBuffer(4));",
        &tval);

    JS::Rooted<JSObject*> tobj(cx, &tval.toObject());
    CHECK(!JS_IsArrayBufferViewObject(tobj));
  }

  return true;
}

static JSObject* CreateDataView(JSContext* cx) {
  JS::Rooted<JSObject*> buffer(cx, JS::NewArrayBuffer(cx, 8));
  if (!buffer) {
    return nullptr;
  }
  return JS_NewDataView(cx, buffer, 0, 8);
}

template <JSObject* CreateTypedArray(JSContext* cx, size_t length),
          size_t Length>
static JSObject* Create(JSContext* cx) {
  return CreateTypedArray(cx, Length);
}

template <typename T, JSObject* CreateViewType(JSContext* cx),
          JSObject* GetObjectAs(JSObject* obj, size_t* length,
                                bool* isSharedMemory, T** data),
          js::Scalar::Type ExpectedType, uint32_t ExpectedLength,
          uint32_t ExpectedByteLength>
bool TestViewType(JSContext* cx) {
  JS::Rooted<JSObject*> obj(cx, CreateViewType(cx));
  CHECK(obj);

  CHECK(JS_IsArrayBufferViewObject(obj));

  CHECK(JS_GetArrayBufferViewType(obj) == ExpectedType);

  CHECK(JS_GetArrayBufferViewByteLength(obj) == ExpectedByteLength);

  {
    JS::AutoCheckCannotGC nogc;
    bool shared1;
    T* data1 = static_cast<T*>(JS_GetArrayBufferViewData(obj, &shared1, nogc));

    T* data2;
    bool shared2;
    size_t len;
    CHECK(obj == GetObjectAs(obj, &len, &shared2, &data2));
    CHECK(data1 == data2);
    CHECK(shared1 == shared2);
    CHECK(len == ExpectedLength);
  }

  JS::RealmOptions options;
  JS::RootedObject otherGlobal(
      cx, JS_NewGlobalObject(cx, basicGlobalClass(), nullptr,
                             JS::DontFireOnNewGlobalHook, options));
  CHECK(otherGlobal);

  JS::Rooted<JSObject*> buffer(cx);
  {
    AutoRealm ar(cx, otherGlobal);
    buffer = JS::NewArrayBuffer(cx, 8);
    CHECK(buffer);
    CHECK(buffer->as<ArrayBufferObject>().byteLength() == 8);
  }
  CHECK(buffer->compartment() == otherGlobal->compartment());
  CHECK(JS_WrapObject(cx, &buffer));
  CHECK(buffer->compartment() == global->compartment());

  JS::Rooted<JSObject*> dataview(cx, JS_NewDataView(cx, buffer, 4, 4));
  CHECK(dataview);
  CHECK(dataview->is<ProxyObject>());

  JS::Rooted<JS::Value> val(cx);

  val = ObjectValue(*dataview);
  CHECK(JS_SetProperty(cx, global, "view", val));

  EVAL("view.buffer", &val);
  CHECK(val.toObject().is<ProxyObject>());

  CHECK(dataview->compartment() == global->compartment());
  JS::Rooted<JSObject*> otherView(cx, js::UncheckedUnwrap(dataview));
  CHECK(otherView->compartment() == otherGlobal->compartment());
  JS::Rooted<JSObject*> otherBuffer(cx, js::UncheckedUnwrap(&val.toObject()));
  CHECK(otherBuffer->compartment() == otherGlobal->compartment());

  EVAL("Object.getPrototypeOf(view) === DataView.prototype", &val);
  CHECK(val.toBoolean() == true);

  return true;
}

END_TEST(testArrayBufferView_type)
