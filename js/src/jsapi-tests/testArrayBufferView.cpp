/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */

#include "jsfriendapi.h"

#include "js/ArrayBuffer.h"             // JS::NewArrayBuffer
#include "js/experimental/TypedData.h"  // JS_GetArrayBufferView{Type,ByteLength,Data}, JS_GetObjectAsArrayBufferView, JS_GetObjectAs{{Ui,I}nt{8,16,32},Float{32,64}}Array, JS_IsArrayBufferViewObject, JS_NewDataView, JS_New{{Ui,I}nt{8,16,32},Float{32,64},Uint8Clamped}Array
#include "js/PropertyAndElement.h"      // JS_SetProperty
#include "js/ScalarType.h"              // js::Scalar::Type
#include "jsapi-tests/tests.h"
#include "vm/ProxyObject.h"
#include "vm/Realm.h"

#include "vm/Realm-inl.h"

using namespace js;

BEGIN_TEST(testArrayBufferView_type) {
  CHECK((TestViewType<Scalar::Uint8, 7, 7>(cx)));
  CHECK((TestViewType<Scalar::Int8, 33, 33>(cx)));
  CHECK((TestViewType<Scalar::Uint8Clamped, 7, 7>(cx)));
  CHECK((TestViewType<Scalar::Uint16, 3, 6>(cx)));
  CHECK((TestViewType<Scalar::Int16, 17, 34>(cx)));
  CHECK((TestViewType<Scalar::Uint32, 15, 60>(cx)));
  CHECK((TestViewType<Scalar::Int32, 8, 32>(cx)));
  CHECK((TestViewType<Scalar::Float32, 7, 28>(cx)));
  CHECK((TestViewType<Scalar::Float64, 9, 72>(cx)));
  CHECK((TestViewType<js::Scalar::MaxTypedArrayViewType, 8, 8>(cx)));

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

template <js::Scalar::Type EType>
static JSObject* Create(JSContext* cx, size_t len) {
  return JS::TypedArray<EType>::create(cx, len).asObject();
}
template <>
JSObject* Create<js::Scalar::MaxTypedArrayViewType>(JSContext* cx, size_t len) {
  JS::Rooted<JSObject*> buffer(cx, JS::NewArrayBuffer(cx, len));
  if (!buffer) {
    return nullptr;
  }
  return JS_NewDataView(cx, buffer, 0, len);
}

template <js::Scalar::Type EType>
static uint8_t* GetLengthAndData(JSObject* obj, size_t* len, bool* isShared,
                                 JS::AutoAssertNoGC& nogc) {
  return reinterpret_cast<uint8_t*>(
      JS::TypedArray<EType>::unwrap(obj).getLengthAndData(len, isShared, nogc));
}

template <>
uint8_t* GetLengthAndData<js::Scalar::MaxTypedArrayViewType>(
    JSObject* obj, size_t* len, bool* isShared, JS::AutoAssertNoGC& nogc) {
  uint8_t* data;
  js::GetArrayBufferViewLengthAndData(obj, len, isShared, &data);
  return data;
}

template <Scalar::Type EType, uint32_t ExpectedLength,
          uint32_t ExpectedByteLength>
bool TestViewType(JSContext* cx) {
  JS::Rooted<JSObject*> obj(cx, Create<EType>(cx, ExpectedLength));
  CHECK(obj);

  CHECK(JS_IsArrayBufferViewObject(obj));

  CHECK(JS_GetArrayBufferViewType(obj) == EType);

  CHECK(JS_GetArrayBufferViewByteLength(obj) == ExpectedByteLength);

  {
    JS::AutoCheckCannotGC nogc;
    bool shared1;
    JSObject* view = js::UnwrapArrayBufferView(obj);
    uint8_t* data1 = (uint8_t*)JS_GetArrayBufferViewData(obj, &shared1, nogc);

    bool shared2;
    size_t len;
    uint8_t* data2 = GetLengthAndData<EType>(view, &len, &shared2, nogc);
    CHECK(obj == view);
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
