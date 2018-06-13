/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(test_GetPropertyDescriptor)
{
  JS::RootedValue v(cx);
  EVAL("({ somename : 123 })", &v);
  CHECK(v.isObject());

  JS::RootedObject obj(cx, &v.toObject());
  JS::Rooted<JS::PropertyDescriptor> desc(cx);

  CHECK(JS_GetPropertyDescriptor(cx, obj, "somename", &desc));
  CHECK_EQUAL(desc.object(), obj);
  CHECK_SAME(desc.value(), JS::Int32Value(123));

  JS::RootedValue descValue(cx);
  CHECK(JS::FromPropertyDescriptor(cx, desc, &descValue));
  CHECK(descValue.isObject());
  JS::RootedObject descObj(cx, &descValue.toObject());
  JS::RootedValue value(cx);
  CHECK(JS_GetProperty(cx, descObj, "value", &value));
  CHECK_EQUAL(value.toInt32(), 123);
  CHECK(JS_GetProperty(cx, descObj, "get", &value));
  CHECK(value.isUndefined());
  CHECK(JS_GetProperty(cx, descObj, "set", &value));
  CHECK(value.isUndefined());
  CHECK(JS_GetProperty(cx, descObj, "writable", &value));
  CHECK(value.isTrue());
  CHECK(JS_GetProperty(cx, descObj, "configurable", &value));
  CHECK(value.isTrue());
  CHECK(JS_GetProperty(cx, descObj, "enumerable", &value));
  CHECK(value.isTrue());

  CHECK(JS_GetPropertyDescriptor(cx, obj, "not-here", &desc));
  CHECK_EQUAL(desc.object(), nullptr);

  CHECK(JS_GetPropertyDescriptor(cx, obj, "toString", &desc));
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  CHECK(objectProto);
  CHECK_EQUAL(desc.object(), objectProto);
  CHECK(desc.value().isObject());
  CHECK(JS::IsCallable(&desc.value().toObject()));

  return true;
}
END_TEST(test_GetPropertyDescriptor)
