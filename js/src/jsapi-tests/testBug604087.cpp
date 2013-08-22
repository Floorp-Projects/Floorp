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

#include "jsobjinlines.h"

struct OuterWrapper : js::Wrapper
{
    OuterWrapper() : Wrapper(0) {}

    virtual bool isOuterWindow() {
        return true;
    }

    virtual bool finalizeInBackground(JS::Value priv) {
        return false;
    }

    static OuterWrapper singleton;
};

OuterWrapper
OuterWrapper::singleton;

static JSObject *
wrap(JSContext *cx, JS::HandleObject toWrap, JS::HandleObject target)
{
    JSAutoCompartment ac(cx, target);
    JS::RootedObject wrapper(cx, toWrap);
    if (!JS_WrapObject(cx, wrapper.address()))
        return NULL;
    return wrapper;
}

static JSObject *
SameCompartmentWrap(JSContext *cx, JS::HandleObject obj)
{
    JS_GC(JS_GetRuntime(cx));
    return obj;
}

static JSObject *
PreWrap(JSContext *cx, JS::HandleObject scope, JS::HandleObject obj, unsigned flags)
{
    JS_GC(JS_GetRuntime(cx));
    return obj;
}

static JSObject *
Wrap(JSContext *cx, JS::HandleObject existing, JS::HandleObject obj,
     JS::HandleObject proto, JS::HandleObject parent, unsigned flags)
{
    return js::Wrapper::New(cx, obj, proto, parent, &js::CrossCompartmentWrapper::singleton);
}

BEGIN_TEST(testBug604087)
{
    JS::RootedObject outerObj(cx, js::Wrapper::New(cx, global, global->getProto(), global,
                                               &OuterWrapper::singleton));
    JS::RootedObject compartment2(cx, JS_NewGlobalObject(cx, getGlobalClass(), NULL, JS::FireOnNewGlobalHook));
    JS::RootedObject compartment3(cx, JS_NewGlobalObject(cx, getGlobalClass(), NULL, JS::FireOnNewGlobalHook));
    JS::RootedObject compartment4(cx, JS_NewGlobalObject(cx, getGlobalClass(), NULL, JS::FireOnNewGlobalHook));

    JS::RootedObject c2wrapper(cx, wrap(cx, outerObj, compartment2));
    CHECK(c2wrapper);
    c2wrapper->as<js::ProxyObject>().setExtra(0, js::Int32Value(2));

    JS::RootedObject c3wrapper(cx, wrap(cx, outerObj, compartment3));
    CHECK(c3wrapper);
    c3wrapper->as<js::ProxyObject>().setExtra(0, js::Int32Value(3));

    JS::RootedObject c4wrapper(cx, wrap(cx, outerObj, compartment4));
    CHECK(c4wrapper);
    c4wrapper->as<js::ProxyObject>().setExtra(0, js::Int32Value(4));
    compartment4 = c4wrapper = NULL;

    JS::RootedObject next(cx);
    {
        JSAutoCompartment ac(cx, compartment2);
        next = js::Wrapper::New(cx, compartment2, compartment2->getProto(), compartment2,
                                &OuterWrapper::singleton);
        CHECK(next);
    }

    JS_SetWrapObjectCallbacks(JS_GetRuntime(cx), Wrap, SameCompartmentWrap, PreWrap);
    CHECK(JS_TransplantObject(cx, outerObj, next));
    return true;
}
END_TEST(testBug604087)
