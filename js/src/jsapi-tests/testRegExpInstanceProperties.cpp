/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscompartment.h"
#include "jsgc.h"

#include "jsapi-tests/tests.h"
#include "vm/Shape.h"

BEGIN_TEST(testRegExpInstanceProperties)
{
    jsval regexpProtoVal;
    EVAL("RegExp.prototype", &regexpProtoVal);

    JSObject *regexpProto = JSVAL_TO_OBJECT(regexpProtoVal);

    if (!helper(regexpProto))
        return false;

    JS_GC(cx);

    CHECK_EQUAL(regexpProto->compartment()->initialRegExpShape, nullptr);

    jsval regexp;
    EVAL("/foopy/", &regexp);
    JSObject *robj = JSVAL_TO_OBJECT(regexp);

    CHECK(robj->lastProperty());
    CHECK_EQUAL(robj->compartment()->initialRegExpShape, robj->lastProperty());

    return true;
}

/*
 * Do this all in a nested function evaluation so as (hopefully) not to get
 * screwed up by the conservative stack scanner when GCing.
 */
MOZ_NEVER_INLINE bool helper(JSObject *regexpProto)
{
    CHECK(!regexpProto->inDictionaryMode());

    // Verify the compartment's cached shape is being used by RegExp.prototype.
    const js::Shape *shape = regexpProto->lastProperty();
    js::AutoShapeRooter root(cx, shape);
    for (js::Shape::Range r = shape;
         &r.front() != regexpProto->compartment()->initialRegExpShape;
         r.popFront())
    {
         CHECK(!r.empty());
    }

    JS::RootedValue v(cx, INT_TO_JSVAL(17));
    CHECK(JS_SetProperty(cx, regexpProto, "foopy", v));
    v = INT_TO_JSVAL(42);
    CHECK(JS_SetProperty(cx, regexpProto, "bunky", v));
    CHECK(JS_DeleteProperty(cx, regexpProto, "foopy"));
    CHECK(regexpProto->inDictionaryMode());

    const js::Shape *shape2 = regexpProto->lastProperty();
    js::AutoShapeRooter root2(cx, shape2);
    js::Shape::Range r2 = shape2;
    while (!r2.empty()) {
        CHECK(&r2.front() != regexpProto->compartment()->initialRegExpShape);
        r2.popFront();
    }

    return true;
}
END_TEST(testRegExpInstanceProperties)
