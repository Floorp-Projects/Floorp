/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * Tests JS_TransplantObject
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "tests.h"
#include "jsobj.h"
#include "jswrapper.h"

#include "jsobjinlines.h"

struct OuterWrapper : js::DirectWrapper
{
    OuterWrapper() : DirectWrapper(0) {}

    virtual bool isOuterWindow() {
        return true;
    }

    static OuterWrapper singleton;
};

OuterWrapper
OuterWrapper::singleton;

static JSObject *
wrap(JSContext *cx, JS::HandleObject toWrap, JS::HandleObject target)
{
    JSAutoCompartment ac(cx, target);
    js::RootedObject wrapper(cx, toWrap);
    if (!JS_WrapObject(cx, wrapper.address()))
        return NULL;
    return wrapper;
}

static JSObject *
SameCompartmentWrap(JSContext *cx, JSObject *objArg)
{
    js::RootedObject obj(cx, objArg);
    JS_GC(JS_GetRuntime(cx));
    return obj;
}

static JSObject *
PreWrap(JSContext *cx, JSObject *scopeArg, JSObject *objArg, unsigned flags)
{
    js::RootedObject scope(cx, scopeArg);
    js::RootedObject obj(cx, objArg);
    JS_GC(JS_GetRuntime(cx));
    return obj;
}

static JSObject *
Wrap(JSContext *cx, JSObject *objArg, JSObject *protoArg, JSObject *parentArg, unsigned flags)
{
    js::RootedObject obj(cx, objArg);
    js::RootedObject proto(cx, protoArg);
    js::RootedObject parent(cx, parentArg);
    return js::Wrapper::New(cx, obj, proto, parent, &js::CrossCompartmentWrapper::singleton);
}

BEGIN_TEST(testBug604087)
{
    js::RootedObject outerObj(cx, js::Wrapper::New(cx, global, global->getProto(), global,
                                               &OuterWrapper::singleton));
    js::RootedObject compartment2(cx, JS_NewGlobalObject(cx, getGlobalClass(), NULL));
    js::RootedObject compartment3(cx, JS_NewGlobalObject(cx, getGlobalClass(), NULL));
    js::RootedObject compartment4(cx, JS_NewGlobalObject(cx, getGlobalClass(), NULL));

    js::RootedObject c2wrapper(cx, wrap(cx, outerObj, compartment2));
    CHECK(c2wrapper);
    js::SetProxyExtra(c2wrapper, 0, js::Int32Value(2));

    js::RootedObject c3wrapper(cx, wrap(cx, outerObj, compartment3));
    CHECK(c3wrapper);
    js::SetProxyExtra(c3wrapper, 0, js::Int32Value(3));

    js::RootedObject c4wrapper(cx, wrap(cx, outerObj, compartment4));
    CHECK(c4wrapper);
    js::SetProxyExtra(c4wrapper, 0, js::Int32Value(4));
    compartment4 = c4wrapper = NULL;

    js::RootedObject next(cx);
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
