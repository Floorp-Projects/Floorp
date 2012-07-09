/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "tests.h"

/*
 * Test that resolve hook recursion for the same object and property is
 * prevented.
 */

BEGIN_TEST(testResolveRecursion)
{
    static JSClass my_resolve_class = {
        "MyResolve",
        JSCLASS_NEW_RESOLVE | JSCLASS_HAS_PRIVATE,

        JS_PropertyStub,       // add
        JS_PropertyStub,       // delete
        JS_PropertyStub,         // get
        JS_StrictPropertyStub, // set
        JS_EnumerateStub,
        (JSResolveOp) my_resolve,
        JS_ConvertStub
    };

    obj1 = JS_NewObject(cx, &my_resolve_class, NULL, NULL);
    CHECK(obj1);
    obj2 = JS_NewObject(cx, &my_resolve_class, NULL, NULL);
    CHECK(obj2);
    JS_SetPrivate(obj1, this);
    JS_SetPrivate(obj2, this);

    CHECK(JS_DefineProperty(cx, global, "obj1", OBJECT_TO_JSVAL(obj1), NULL, NULL, 0));
    CHECK(JS_DefineProperty(cx, global, "obj2", OBJECT_TO_JSVAL(obj2), NULL, NULL, 0));

    resolveEntryCount = 0;
    resolveExitCount = 0;

    /* Start the essence of the test via invoking the first resolve hook. */
    jsval v;
    EVAL("obj1.x", &v);
    CHECK_SAME(v, JSVAL_FALSE);
    CHECK_EQUAL(resolveEntryCount, 4);
    CHECK_EQUAL(resolveExitCount, 4);
    return true;
}

JSObject *obj1;
JSObject *obj2;
unsigned resolveEntryCount;
unsigned resolveExitCount;

struct AutoIncrCounters {

    AutoIncrCounters(cls_testResolveRecursion *t) : t(t) {
        t->resolveEntryCount++;
    }

    ~AutoIncrCounters() {
        t->resolveExitCount++;
    }

    cls_testResolveRecursion *t;
};

bool
doResolve(JSObject *obj, jsid id, unsigned flags, JSMutableHandleObject objp)
{
    CHECK_EQUAL(resolveExitCount, 0);
    AutoIncrCounters incr(this);
    CHECK_EQUAL(obj, obj1 || obj == obj2);

    CHECK(JSID_IS_STRING(id));

    JSFlatString *str = JS_FlattenString(cx, JSID_TO_STRING(id));
    CHECK(str);
    jsval v;
    if (JS_FlatStringEqualsAscii(str, "x")) {
        if (obj == obj1) {
            /* First resolve hook invocation. */
            CHECK_EQUAL(resolveEntryCount, 1);
            EVAL("obj2.y = true", &v);
            CHECK_SAME(v, JSVAL_TRUE);
            CHECK(JS_DefinePropertyById(cx, obj, id, JSVAL_FALSE, NULL, NULL, 0));
            objp.set(obj);
            return true;
        }
        if (obj == obj2) {
            CHECK_EQUAL(resolveEntryCount, 4);
            objp.set(NULL);
            return true;
        }
    } else if (JS_FlatStringEqualsAscii(str, "y")) {
        if (obj == obj2) {
            CHECK_EQUAL(resolveEntryCount, 2);
            CHECK(JS_DefinePropertyById(cx, obj, id, JSVAL_NULL, NULL, NULL, 0));
            EVAL("obj1.x", &v);
            CHECK(JSVAL_IS_VOID(v));
            EVAL("obj1.y", &v);
            CHECK_SAME(v, JSVAL_ZERO);
            objp.set(obj);
            return true;
        }
        if (obj == obj1) {
            CHECK_EQUAL(resolveEntryCount, 3);
            EVAL("obj1.x", &v);
            CHECK(JSVAL_IS_VOID(v));
            EVAL("obj1.y", &v);
            CHECK(JSVAL_IS_VOID(v));
            EVAL("obj2.y", &v);
            CHECK(JSVAL_IS_NULL(v));
            EVAL("obj2.x", &v);
            CHECK(JSVAL_IS_VOID(v));
            EVAL("obj1.y = 0", &v);
            CHECK_SAME(v, JSVAL_ZERO);
            objp.set(obj);
            return true;
        }
    }
    CHECK(false);
    return false;
}

static JSBool
my_resolve(JSContext *cx, JSHandleObject obj, JSHandleId id, unsigned flags,
           JSMutableHandleObject objp)
{
    return static_cast<cls_testResolveRecursion *>(JS_GetPrivate(obj))->
           doResolve(obj, id, flags, objp);
}

END_TEST(testResolveRecursion)
