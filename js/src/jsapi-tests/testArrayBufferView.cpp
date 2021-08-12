/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsfriendapi.h"

#include "js/ArrayBuffer.h"             // JS::NewArrayBuffer
#include "js/experimental/TypedData.h"  // JS_GetArrayBufferView{Type,ByteLength,Data}, JS_GetObjectAsArrayBufferView, JS_GetObjectAs{{Ui,I}nt{8,16,32},Float{32,64}}Array, JS_IsArrayBufferViewObject, JS_NewDataView, JS_New{{Ui,I}nt{8,16,32},Float{32,64},Uint8Clamped}Array
#include "js/PropertyAndElement.h"      // JS_SetProperty
#include "js/ScalarType.h"              // js::Scalar::Type
#include "jsapi-tests/tests.h"
#include "vm/ProxyObject.h"
#include "vm/Realm.h"
#include "vm/Uint8Clamped.h"  // js::uint8_clamped_t

#include "vm/JSObject-inl.h"
#include "vm/Realm-inl.h"
#include "vm/TypedArrayObject-inl.h"  // TypeIDOfType

using namespace js;

template <class ViewType>
static JSObject* Create(JSContext* cx, size_t len) {
  return ViewType::create(cx, len).asObject();
}

template <>
JSObject* Create<JS::DataView>(JSContext* cx, size_t len) {
  JS::Rooted<JSObject*> buffer(cx, JS::NewArrayBuffer(cx, len));
  if (!buffer) {
    return nullptr;
  }
  return JS_NewDataView(cx, buffer, 0, len);
}

template <class T>
struct InternalType {
  using Type = uint8_t;
};

#define INT_TYPE(ExternalType, NativeType, Name)          \
  template <>                                             \
  struct InternalType<JS::TypedArray<js::Scalar::Name>> { \
    using Type = NativeType;                              \
  };
JS_FOR_EACH_TYPED_ARRAY(INT_TYPE)
#undef INT_TYPE

BEGIN_TEST(testArrayBufferView_type) {
  CHECK((TestViewType<JS::TypedArray<Scalar::Uint8>, 7, 7>(cx)));
  CHECK((TestViewType<JS::TypedArray<Scalar::Int8>, 33, 33>(cx)));
  CHECK((TestViewType<JS::TypedArray<Scalar::Uint8Clamped>, 7, 7>(cx)));
  CHECK((TestViewType<JS::TypedArray<Scalar::Uint16>, 3, 6>(cx)));
  CHECK((TestViewType<JS::TypedArray<Scalar::Int16>, 17, 34>(cx)));
  CHECK((TestViewType<JS::TypedArray<Scalar::Uint32>, 15, 60>(cx)));
  CHECK((TestViewType<JS::TypedArray<Scalar::Int32>, 8, 32>(cx)));
  CHECK((TestViewType<JS::TypedArray<Scalar::Float32>, 7, 28>(cx)));
  CHECK((TestViewType<JS::TypedArray<Scalar::Float64>, 9, 72>(cx)));
  CHECK((TestViewType<JS::DataView, 8, 8>(cx)));

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

template <class T>
struct ScalarTypeOf {
  static constexpr js::Scalar::Type value = js::Scalar::MaxTypedArrayViewType;
};

template <js::Scalar::Type EType>
struct ScalarTypeOf<JS::TypedArray<EType>> {
  static constexpr js::Scalar::Type value = EType;
};
template <class ViewType, uint32_t ExpectedLength, uint32_t ExpectedByteLength>
bool TestViewType(JSContext* cx) {
  JS::Rooted<JSObject*> obj(cx, Create<ViewType>(cx, ExpectedLength));
  CHECK(obj);

  CHECK(JS_IsArrayBufferViewObject(obj));

  CHECK(JS_GetArrayBufferViewByteLength(obj) == ExpectedByteLength);

  {
    JS::AutoCheckCannotGC nogc;
    bool shared1;
    JSObject* unwrapped = js::UnwrapArrayBufferView(obj);
    uint8_t* data1 =
        (uint8_t*)JS_GetArrayBufferViewData(unwrapped, &shared1, nogc);

    auto view = ViewType::unwrap(obj);
    CHECK(JS_GetArrayBufferViewType(obj) == ScalarTypeOf<ViewType>::value);

    if (JS_IsTypedArrayObject(unwrapped)) {
      CHECK(unwrapped->as<TypedArrayObject>().type() ==
            TypeIDOfType<typename InternalType<ViewType>::Type>::id);
    }

    bool shared2;
    size_t len;
    uint8_t* data2 =
        reinterpret_cast<uint8_t*>(view.getLengthAndData(&len, &shared2, nogc));
    CHECK(obj == view.asObject());
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
