/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <type_traits>

#include "jsfriendapi.h"

#include "js/ArrayBuffer.h"  // JS::NewArrayBuffer
#include "js/experimental/TypedData.h"
#include "js/ScalarType.h"  // JS::Scalar::Type
#include "jsapi-tests/tests.h"

#include "vm/Realm-inl.h"

using namespace js;

template <class ViewType>
static JSObject* CreateObj(JSContext* cx, size_t len) {
  return ViewType::create(cx, len).asObject();
}

template <>
JSObject* CreateObj<JS::DataView>(JSContext* cx, size_t len) {
  JS::Rooted<JSObject*> buffer(cx, JS::NewArrayBuffer(cx, len));
  if (!buffer) {
    return nullptr;
  }
  return JS_NewDataView(cx, buffer, 0, len);
}

BEGIN_TEST(testArrayBufferOrView_type) {
  JS::RealmOptions options;
  JS::RootedObject otherGlobal(
      cx, JS_NewGlobalObject(cx, basicGlobalClass(), nullptr,
                             JS::DontFireOnNewGlobalHook, options));
  CHECK(otherGlobal);

  CHECK((TestType<JS::TypedArray<Scalar::Uint8>>(cx, otherGlobal)));
  CHECK((TestType<JS::TypedArray<Scalar::Int8>>(cx, otherGlobal)));
  CHECK((TestType<JS::TypedArray<Scalar::Uint8Clamped>>(cx, otherGlobal)));
  CHECK((TestType<JS::TypedArray<Scalar::Uint16>>(cx, otherGlobal)));
  CHECK((TestType<JS::TypedArray<Scalar::Int16>>(cx, otherGlobal)));
  CHECK((TestType<JS::TypedArray<Scalar::Uint32>>(cx, otherGlobal)));
  CHECK((TestType<JS::TypedArray<Scalar::Int32>>(cx, otherGlobal)));
  CHECK((TestType<JS::TypedArray<Scalar::Float32>>(cx, otherGlobal)));
  CHECK((TestType<JS::TypedArray<Scalar::Float64>>(cx, otherGlobal)));
  CHECK((TestType<JS::DataView>(cx, otherGlobal)));
  CHECK((TestType<JS::ArrayBuffer>(cx, otherGlobal)));

  return true;
}

template <class APIType>
bool TestType(JSContext* cx, Handle<JSObject*> otherGlobal) {
  JS::Rooted<JSObject*> obj(cx, CreateObj<APIType>(cx, 8));
  CHECK(obj);

  // Any of these should be creatable as an ArrayBufferOrView.
  JS::Rooted<JS::ArrayBufferOrView> abov(
      cx, JS::ArrayBufferOrView::fromObject(obj));
  CHECK(abov);

  // And that should allow unwrapping as well.
  abov = JS::ArrayBufferOrView::unwrap(obj);
  CHECK(abov);

  if constexpr (!std::is_same_v<APIType, JS::Uint16Array>) {
    // Check that we can't make an API object of a different type.
    JS::Rooted<JS::Uint16Array> nope(cx, JS::Uint16Array::unwrap(obj));
    CHECK(!nope);

    // And that we can't make an API object from an object of a different type.
    JS::Rooted<JSObject*> u16array(cx, CreateObj<JS::Uint16Array>(cx, 10));
    CHECK(u16array);
    auto deny = APIType::fromObject(u16array);
    CHECK(!deny);
    deny = APIType::unwrap(u16array);
    CHECK(!deny);
  }

  CHECK_EQUAL(abov.asObject(), obj);

  JS::Rooted<JSObject*> wrapped(cx);
  {
    AutoRealm ar(cx, otherGlobal);
    wrapped = CreateObj<APIType>(cx, 8);  // Not wrapped yet!
    CHECK(wrapped);
  }
  CHECK(wrapped->compartment() == otherGlobal->compartment());
  CHECK(JS_WrapObject(cx, &wrapped));  // Now it's wrapped.
  CHECK(wrapped->compartment() == global->compartment());

  abov = JS::ArrayBufferOrView::fromObject(wrapped);
  CHECK(!abov);
  abov = JS::ArrayBufferOrView::unwrap(wrapped);
  CHECK(abov);

  JS::Rooted<APIType> dummy(cx, APIType::fromObject(obj));
  CHECK(obj);
  CHECK(dummy);
  CHECK(dummy.asObject());

  return true;
}

END_TEST(testArrayBufferOrView_type)
