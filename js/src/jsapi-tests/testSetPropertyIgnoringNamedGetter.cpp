/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */

#include "jsfriendapi.h"

#include "js/Proxy.h"

#include "jsapi-tests/tests.h"

using namespace js;
using namespace JS;

class CustomProxyHandler : public Wrapper {
 public:
  CustomProxyHandler() : Wrapper(0) {}

  bool getOwnPropertyDescriptor(
      JSContext* cx, HandleObject proxy, HandleId id,
      MutableHandle<mozilla::Maybe<PropertyDescriptor>> desc) const override {
    if (JSID_IS_STRING(id) &&
        JS_LinearStringEqualsLiteral(JSID_TO_LINEAR_STRING(id), "phantom")) {
      desc.set(mozilla::Some(PropertyDescriptor::Data(
          Int32Value(42),
          {PropertyAttribute::Configurable, PropertyAttribute::Enumerable,
           PropertyAttribute::Writable})));
      return true;
    }

    return Wrapper::getOwnPropertyDescriptor(cx, proxy, id, desc);
  }

  bool set(JSContext* cx, HandleObject proxy, HandleId id, HandleValue v,
           HandleValue receiver, ObjectOpResult& result) const override {
    Rooted<mozilla::Maybe<PropertyDescriptor>> desc(cx);
    if (!Wrapper::getOwnPropertyDescriptor(cx, proxy, id, &desc)) {
      return false;
    }
    return SetPropertyIgnoringNamedGetter(cx, proxy, id, v, receiver, desc,
                                          result);
  }
};

const CustomProxyHandler customProxyHandler;

BEGIN_TEST(testSetPropertyIgnoringNamedGetter_direct) {
  RootedValue protov(cx);
  EVAL("Object.prototype", &protov);

  RootedValue targetv(cx);
  EVAL("({})", &targetv);

  RootedObject proxyObj(cx, NewProxyObject(cx, &customProxyHandler, targetv,
                                           &protov.toObject(), ProxyOptions()));
  CHECK(proxyObj);

  CHECK(JS_DefineProperty(cx, global, "target", targetv, 0));
  CHECK(JS_DefineProperty(cx, global, "proxy", proxyObj, 0));

  RootedValue v(cx);
  EVAL("Object.getOwnPropertyDescriptor(proxy, 'phantom').value", &v);
  CHECK_SAME(v, Int32Value(42));

  EXEC("proxy.phantom = 123");
  EVAL("Object.getOwnPropertyDescriptor(proxy, 'phantom').value", &v);
  CHECK_SAME(v, Int32Value(42));
  EVAL("target.phantom", &v);
  CHECK_SAME(v, Int32Value(123));

  return true;
}
END_TEST(testSetPropertyIgnoringNamedGetter_direct)
