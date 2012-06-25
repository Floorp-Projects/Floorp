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
wrap(JSContext *cx, JSObject *toWrap, JSObject *target)
{
    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, target))
        return NULL;

    JSObject *wrapper = toWrap;
    if (!JS_WrapObject(cx, &wrapper))
        return NULL;
    return wrapper;
}

static JSObject *
SameCompartmentWrap(JSContext *cx, JSObject *obj)
{
    JS_GC(JS_GetRuntime(cx));
    return obj;
}

static JSObject *
PreWrap(JSContext *cx, JSObject *scope, JSObject *obj, unsigned flags)
{
    JS_GC(JS_GetRuntime(cx));
    return obj;
}

static JSObject *
Wrap(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent, unsigned flags)
{
    return js::Wrapper::New(cx, obj, proto, parent, &js::CrossCompartmentWrapper::singleton);
}

BEGIN_TEST(testBug604087)
{
    JSObject *outerObj = js::Wrapper::New(cx, global, global->getProto(), global,
                                          &OuterWrapper::singleton);
    JSObject *compartment2 = JS_NewGlobalObject(cx, getGlobalClass(), NULL);
    JSObject *compartment3 = JS_NewGlobalObject(cx, getGlobalClass(), NULL);
    JSObject *compartment4 = JS_NewGlobalObject(cx, getGlobalClass(), NULL);

    JSObject *c2wrapper = wrap(cx, outerObj, compartment2);
    CHECK(c2wrapper);
    js::SetProxyExtra(c2wrapper, 0, js::Int32Value(2));

    JSObject *c3wrapper = wrap(cx, outerObj, compartment3);
    CHECK(c3wrapper);
    js::SetProxyExtra(c3wrapper, 0, js::Int32Value(3));

    JSObject *c4wrapper = wrap(cx, outerObj, compartment4);
    CHECK(c4wrapper);
    js::SetProxyExtra(c4wrapper, 0, js::Int32Value(4));
    compartment4 = c4wrapper = NULL;

    JSObject *next;
    {
        JSAutoEnterCompartment ac;
        CHECK(ac.enter(cx, compartment2));
        next = js::Wrapper::New(cx, compartment2, compartment2->getProto(), compartment2,
                                &OuterWrapper::singleton);
        CHECK(next);
    }

    JS_SetWrapObjectCallbacks(JS_GetRuntime(cx), Wrap, SameCompartmentWrap, PreWrap);
    CHECK(JS_TransplantObject(cx, outerObj, next));
    return true;
}
END_TEST(testBug604087)
