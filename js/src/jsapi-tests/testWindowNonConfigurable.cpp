/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/PropertyAndElement.h"  // JS_DefineProperty
#include "js/Proxy.h"
#include "jsapi-tests/tests.h"

class WindowProxyHandler : public js::ForwardingProxyHandler {
 public:
  constexpr WindowProxyHandler() : js::ForwardingProxyHandler(&family) {}

  static const char family;

  virtual bool defineProperty(JSContext* cx, JS::HandleObject proxy,
                              JS::HandleId id,
                              JS::Handle<JS::PropertyDescriptor> desc,
                              JS::ObjectOpResult& result) const override {
    if (desc.hasConfigurable() && !desc.configurable()) {
      result.failCantDefineWindowNonConfigurable();
      return true;
    }
    return ForwardingProxyHandler::defineProperty(cx, proxy, id, desc, result);
  }
};
const char WindowProxyHandler::family = 0;

static const JSClass windowProxy_class =
    PROXY_CLASS_DEF("TestWindowProxy", JSCLASS_HAS_RESERVED_SLOTS(1));
static const WindowProxyHandler windowProxy_handler;

BEGIN_TEST(testWindowNonConfigurable) {
  JS::RootedObject wrapped(cx, JS_NewObject(cx, nullptr));
  CHECK(wrapped);
  JS::RootedValue wrappedVal(cx, JS::ObjectValue(*wrapped));
  js::ProxyOptions options;
  options.setClass(&windowProxy_class);
  JS::RootedObject obj(cx, NewProxyObject(cx, &windowProxy_handler, wrappedVal,
                                          nullptr, options));
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
