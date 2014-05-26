/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

/*
 * Test that resolve hook recursion for the same object and property is
 * prevented.
 */

BEGIN_TEST(testResolveRecursion)
{
    static const JSClass my_resolve_class = {
        "MyResolve",
        JSCLASS_NEW_RESOLVE | JSCLASS_HAS_PRIVATE,

        JS_PropertyStub,       // add
        JS_DeletePropertyStub, // delete
        JS_PropertyStub,       // get
        JS_StrictPropertyStub, // set
        JS_EnumerateStub,
        (JSResolveOp) my_resolve,
        JS_ConvertStub
    };

    obj1 = obj2 = nullptr;
    JS::AddObjectRoot(cx, &obj1);
    JS::AddObjectRoot(cx, &obj2);

    obj1 = JS_NewObject(cx, &my_resolve_class, JS::NullPtr(), JS::NullPtr());
    CHECK(obj1);
    obj2 = JS_NewObject(cx, &my_resolve_class, JS::NullPtr(), JS::NullPtr());
    CHECK(obj2);
    JS_SetPrivate(obj1, this);
    JS_SetPrivate(obj2, this);

    JS::RootedValue obj1Val(cx, ObjectValue(*obj1));
    JS::RootedValue obj2Val(cx, ObjectValue(*obj2));
    CHECK(JS_DefineProperty(cx, global, "obj1", obj1Val, 0));
    CHECK(JS_DefineProperty(cx, global, "obj2", obj2Val, 0));

    resolveEntryCount = 0;
    resolveExitCount = 0;

    /* Start the essence of the test via invoking the first resolve hook. */
    JS::RootedValue v(cx);
    EVAL("obj1.x", &v);
    CHECK_SAME(v, JSVAL_FALSE);
    CHECK_EQUAL(resolveEntryCount, 4);
    CHECK_EQUAL(resolveExitCount, 4);

    obj1 = nullptr;
    obj2 = nullptr;
    JS::RemoveObjectRoot(cx, &obj1);
    JS::RemoveObjectRoot(cx, &obj2);
    return true;
}

JS::Heap<JSObject *> obj1;
JS::Heap<JSObject *> obj2;
unsigned resolveEntryCount;
unsigned resolveExitCount;

struct AutoIncrCounters {

    explicit AutoIncrCounters(cls_testResolveRecursion *t) : t(t) {
        t->resolveEntryCount++;
    }

    ~AutoIncrCounters() {
        t->resolveExitCount++;
    }

    cls_testResolveRecursion *t;
};

bool
doResolve(JS::HandleObject obj, JS::HandleId id, JS::MutableHandleObject objp)
{
    CHECK_EQUAL(resolveExitCount, 0);
    AutoIncrCounters incr(this);
    CHECK_EQUAL(obj, obj1 || obj == obj2);

    CHECK(JSID_IS_STRING(id));

    JSFlatString *str = JS_FlattenString(cx, JSID_TO_STRING(id));
    CHECK(str);
    JS::RootedValue v(cx);
    if (JS_FlatStringEqualsAscii(str, "x")) {
        if (obj == obj1) {
            /* First resolve hook invocation. */
            CHECK_EQUAL(resolveEntryCount, 1);
            EVAL("obj2.y = true", &v);
            CHECK_SAME(v, JSVAL_TRUE);
            CHECK(JS_DefinePropertyById(cx, obj, id, JS::FalseHandleValue, 0));
            objp.set(obj);
            return true;
        }
        if (obj == obj2) {
            CHECK_EQUAL(resolveEntryCount, 4);
            objp.set(nullptr);
            return true;
        }
    } else if (JS_FlatStringEqualsAscii(str, "y")) {
        if (obj == obj2) {
            CHECK_EQUAL(resolveEntryCount, 2);
            CHECK(JS_DefinePropertyById(cx, obj, id, JS::NullHandleValue, 0));
            EVAL("obj1.x", &v);
            CHECK(v.isUndefined());
            EVAL("obj1.y", &v);
            CHECK_SAME(v, JSVAL_ZERO);
            objp.set(obj);
            return true;
        }
        if (obj == obj1) {
            CHECK_EQUAL(resolveEntryCount, 3);
            EVAL("obj1.x", &v);
            CHECK(v.isUndefined());
            EVAL("obj1.y", &v);
            CHECK(v.isUndefined());
            EVAL("obj2.y", &v);
            CHECK(v.isNull());
            EVAL("obj2.x", &v);
            CHECK(v.isUndefined());
            EVAL("obj1.y = 0", &v);
            CHECK_SAME(v, JSVAL_ZERO);
            objp.set(obj);
            return true;
        }
    }
    CHECK(false);
    return false;
}

static bool
my_resolve(JSContext *cx, JS::HandleObject obj, JS::HandleId id, JS::MutableHandleObject objp)
{
    return static_cast<cls_testResolveRecursion *>(JS_GetPrivate(obj))->
           doResolve(obj, id, objp);
}

END_TEST(testResolveRecursion)
