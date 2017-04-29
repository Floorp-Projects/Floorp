/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Tests JS_TransplantObject
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsobj.h"
#include "jswrapper.h"

#include "jsapi-tests/tests.h"

#include "vm/ProxyObject.h"

static const js::ClassExtension OuterWrapperClassExtension = PROXY_MAKE_EXT(
    nullptr  /* objectMoved */
);

const js::Class OuterWrapperClass = PROXY_CLASS_WITH_EXT(
    "Proxy",
    JSCLASS_HAS_RESERVED_SLOTS(1), /* additional class flags */
    &OuterWrapperClassExtension);

static JSObject*
wrap(JSContext* cx, JS::HandleObject toWrap, JS::HandleObject target)
{
    JSAutoCompartment ac(cx, target);
    JS::RootedObject wrapper(cx, toWrap);
    if (!JS_WrapObject(cx, &wrapper))
        return nullptr;
    return wrapper;
}

static void
PreWrap(JSContext* cx, JS::HandleObject scope, JS::HandleObject obj,
        JS::HandleObject objectPassedToWrap,
        JS::MutableHandleObject retObj)
{
    JS_GC(cx);
    retObj.set(obj);
}

static JSObject*
Wrap(JSContext* cx, JS::HandleObject existing, JS::HandleObject obj)
{
    return js::Wrapper::New(cx, obj, &js::CrossCompartmentWrapper::singleton);
}

static const JSWrapObjectCallbacks WrapObjectCallbacks = {
    Wrap,
    PreWrap
};

BEGIN_TEST(testBug604087)
{
    js::SetWindowProxyClass(cx, &OuterWrapperClass);

    js::WrapperOptions options;
    options.setClass(&OuterWrapperClass);
    options.setSingleton(true);
    JS::RootedObject outerObj(cx, js::Wrapper::New(cx, global, &js::Wrapper::singleton, options));
    JS::CompartmentOptions globalOptions;
    JS::RootedObject compartment2(cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                                                         JS::FireOnNewGlobalHook, globalOptions));
    CHECK(compartment2 != nullptr);
    JS::RootedObject compartment3(cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                                                         JS::FireOnNewGlobalHook, globalOptions));
    CHECK(compartment3 != nullptr);
    JS::RootedObject compartment4(cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                                                         JS::FireOnNewGlobalHook, globalOptions));
    CHECK(compartment4 != nullptr);

    JS::RootedObject c2wrapper(cx, wrap(cx, outerObj, compartment2));
    CHECK(c2wrapper);
    c2wrapper->as<js::ProxyObject>().setReservedSlot(0, js::Int32Value(2));

    JS::RootedObject c3wrapper(cx, wrap(cx, outerObj, compartment3));
    CHECK(c3wrapper);
    c3wrapper->as<js::ProxyObject>().setReservedSlot(0, js::Int32Value(3));

    JS::RootedObject c4wrapper(cx, wrap(cx, outerObj, compartment4));
    CHECK(c4wrapper);
    c4wrapper->as<js::ProxyObject>().setReservedSlot(0, js::Int32Value(4));
    compartment4 = c4wrapper = nullptr;

    JS::RootedObject next(cx);
    {
        JSAutoCompartment ac(cx, compartment2);
        next = js::Wrapper::New(cx, compartment2, &js::Wrapper::singleton, options);
        CHECK(next);
    }

    JS_SetWrapObjectCallbacks(cx, &WrapObjectCallbacks);
    CHECK(JS_TransplantObject(cx, outerObj, next));
    return true;
}
END_TEST(testBug604087)
