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
    static const JSClassOps my_resolve_classOps = {
        nullptr, // add
        nullptr, // delete
        nullptr, // get
        nullptr, // set
        nullptr, // enumerate
        my_resolve
    };

    static const JSClass my_resolve_class = {
        "MyResolve",
        JSCLASS_HAS_PRIVATE,
        &my_resolve_classOps
    };

    obj1.init(cx, JS_NewObject(cx, &my_resolve_class));
    CHECK(obj1);
    obj2.init(cx, JS_NewObject(cx, &my_resolve_class));
    CHECK(obj2);
    JS_SetPrivate(obj1, this);
    JS_SetPrivate(obj2, this);

    JS::RootedValue obj1Val(cx, JS::ObjectValue(*obj1));
    JS::RootedValue obj2Val(cx, JS::ObjectValue(*obj2));
    CHECK(JS_DefineProperty(cx, global, "obj1", obj1Val, 0));
    CHECK(JS_DefineProperty(cx, global, "obj2", obj2Val, 0));

    resolveEntryCount = 0;
    resolveExitCount = 0;

    /* Start the essence of the test via invoking the first resolve hook. */
    JS::RootedValue v(cx);
    EVAL("obj1.x", &v);
    CHECK(v.isFalse());
    CHECK_EQUAL(resolveEntryCount, 4);
    CHECK_EQUAL(resolveExitCount, 4);

    obj1 = nullptr;
    obj2 = nullptr;
    return true;
}

JS::PersistentRootedObject obj1;
JS::PersistentRootedObject obj2;
int resolveEntryCount;
int resolveExitCount;

struct AutoIncrCounters {

    explicit AutoIncrCounters(cls_testResolveRecursion* t) : t(t) {
        t->resolveEntryCount++;
    }

    ~AutoIncrCounters() {
        t->resolveExitCount++;
    }

    cls_testResolveRecursion* t;
};

bool
doResolve(JS::HandleObject obj, JS::HandleId id, bool* resolvedp)
{
    CHECK_EQUAL(resolveExitCount, 0);
    AutoIncrCounters incr(this);
    CHECK(obj == obj1 || obj == obj2);

    CHECK(JSID_IS_STRING(id));

    JSFlatString* str = JS_FlattenString(cx, JSID_TO_STRING(id));
    CHECK(str);
    JS::RootedValue v(cx);
    if (JS_FlatStringEqualsAscii(str, "x")) {
        if (obj == obj1) {
            /* First resolve hook invocation. */
            CHECK_EQUAL(resolveEntryCount, 1);
            EVAL("obj2.y = true", &v);
            CHECK(v.isTrue());
            CHECK(JS_DefinePropertyById(cx, obj, id, JS::FalseHandleValue, JSPROP_RESOLVING));
            *resolvedp = true;
            return true;
        }
        if (obj == obj2) {
            CHECK_EQUAL(resolveEntryCount, 4);
            *resolvedp = false;
            return true;
        }
    } else if (JS_FlatStringEqualsAscii(str, "y")) {
        if (obj == obj2) {
            CHECK_EQUAL(resolveEntryCount, 2);
            CHECK(JS_DefinePropertyById(cx, obj, id, JS::NullHandleValue, JSPROP_RESOLVING));
            EVAL("obj1.x", &v);
            CHECK(v.isUndefined());
            EVAL("obj1.y", &v);
            CHECK(v.isInt32(0));
            *resolvedp = true;
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
            CHECK(v.isInt32(0));
            *resolvedp = true;
            return true;
        }
    }
    CHECK(false);
    return false;
}

static bool
my_resolve(JSContext* cx, JS::HandleObject obj, JS::HandleId id, bool* resolvedp)
{
    return static_cast<cls_testResolveRecursion*>(JS_GetPrivate(obj))->
           doResolve(obj, id, resolvedp);
}
END_TEST(testResolveRecursion)

/*
 * Test that JS_InitStandardClasses does not cause resolve hooks to be called.
 *
 * (XPConnect apparently does have global classes, such as the one created by
 * nsMessageManagerScriptExecutor::InitChildGlobalInternal(), that have resolve
 * hooks which can call back into JS, and on which JS_InitStandardClasses is
 * called. Calling back into JS in the middle of resolving `undefined` is bad.)
 */
BEGIN_TEST(testResolveRecursion_InitStandardClasses)
{
    CHECK(JS_InitStandardClasses(cx, global));
    return true;
}

const JSClass* getGlobalClass() override {
    static const JSClassOps myGlobalClassOps = {
        nullptr, // add
        nullptr, // delete
        nullptr, // get
        nullptr, // set
        nullptr, // enumerate
        my_resolve,
        nullptr, // mayResolve
        nullptr, // finalize
        nullptr, // call
        nullptr, // hasInstance
        nullptr, // construct
        JS_GlobalObjectTraceHook
    };

    static const JSClass myGlobalClass = {
        "testResolveRecursion_InitStandardClasses_myGlobalClass",
        JSCLASS_GLOBAL_FLAGS,
        &myGlobalClassOps
    };

    return &myGlobalClass;
}

static bool
my_resolve(JSContext* cx, JS::HandleObject obj, JS::HandleId id, bool* resolvedp)
{
    MOZ_ASSERT_UNREACHABLE("resolve hook should not be called from InitStandardClasses");
    JS_ReportErrorASCII(cx, "FAIL");
    return false;
}
END_TEST(testResolveRecursion_InitStandardClasses)
