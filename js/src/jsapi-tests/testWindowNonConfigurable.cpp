/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static bool windowProxy_defineProperty(JSContext* cx, JS::HandleObject obj,
                                       JS::HandleId id,
                                       JS::Handle<JS::PropertyDescriptor> desc,
                                       JS::ObjectOpResult& result) {
  if (desc.hasConfigurable() && !desc.configurable()) {
    result.failCantDefineWindowNonConfigurable();
    return true;
  }

  return NativeDefineProperty(cx, obj.as<js::NativeObject>(), id, desc, result);
}

static const js::ObjectOps windowProxy_objectOps = {nullptr,
                                                    windowProxy_defineProperty};

static const JSClass windowProxy_class = {
    "WindowProxy", 0, nullptr, nullptr, nullptr, &windowProxy_objectOps};

BEGIN_TEST(testWindowNonConfigurable) {
  JS::RootedObject obj(cx, JS_NewObject(cx, &windowProxy_class));
  CHECK(obj);
  CHECK(JS_DefineProperty(cx, global, "windowProxy", obj, 0));
  JS::RootedValue v(cx);
  EVAL(
      "Object.defineProperty(windowProxy, 'bar', {value: 1, configurable: "
      "false})",
      &v);
  CHECK(v.isNull());  // This is the important bit!
  EVAL(
      "Object.defineProperty(windowProxy, 'bar', {value: 1, configurable: "
      "true})",
      &v);
  CHECK(&v.toObject() == obj);
  EVAL(
      "Reflect.defineProperty(windowProxy, 'foo', {value: 1, configurable: "
      "false})",
      &v);
  CHECK(v.isFalse());
  EVAL(
      "Reflect.defineProperty(windowProxy, 'foo', {value: 1, configurable: "
      "true})",
      &v);
  CHECK(v.isTrue());

  return true;
}
END_TEST(testWindowNonConfigurable)
