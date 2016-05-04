/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */

#include "jsfriendapi.h"

#include "js/Proxy.h"

#include "jsapi-tests/tests.h"

using namespace js;
using namespace JS;

class CustomProxyHandler : public Wrapper
{
  public:
    CustomProxyHandler() : Wrapper(0) {}

    bool getPropertyDescriptor(JSContext* cx, HandleObject proxy, HandleId id,
                               MutableHandle<PropertyDescriptor> desc) const override
    {
        return impl(cx, proxy, id, desc, false);
    }

    bool getOwnPropertyDescriptor(JSContext* cx, HandleObject proxy, HandleId id,
                                  MutableHandle<PropertyDescriptor> desc) const override
    {
        return impl(cx, proxy, id, desc, true);
    }

    bool set(JSContext* cx, HandleObject proxy, HandleId id, HandleValue v, HandleValue receiver,
             ObjectOpResult& result) const override
    {
        Rooted<PropertyDescriptor> desc(cx);
        if (!Wrapper::getPropertyDescriptor(cx, proxy, id, &desc))
            return false;
        return SetPropertyIgnoringNamedGetter(cx, proxy, id, v, receiver, desc, result);
    }

  private:
    bool impl(JSContext* cx, HandleObject proxy, HandleId id,
              MutableHandle<PropertyDescriptor> desc, bool ownOnly) const
    {
        if (JSID_IS_STRING(id)) {
            bool match;
            if (!JS_StringEqualsAscii(cx, JSID_TO_STRING(id), "phantom", &match))
                return false;
            if (match) {
                desc.object().set(proxy);
                desc.attributesRef() = JSPROP_ENUMERATE;
                desc.value().setInt32(42);
                return true;
            }
        }

        if (ownOnly)
            return Wrapper::getOwnPropertyDescriptor(cx, proxy, id, desc);
        return Wrapper::getPropertyDescriptor(cx, proxy, id, desc);
    }

};

const CustomProxyHandler customProxyHandler;


BEGIN_TEST(testSetPropertyIgnoringNamedGetter_direct)
{
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
