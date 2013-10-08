/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/StructuredClone.h"

#include "jsapi-tests/tests.h"

BEGIN_TEST(testStructuredClone_object)
{
    JS::RootedObject g1(cx, createGlobal());
    JS::RootedObject g2(cx, createGlobal());
    CHECK(g1);
    CHECK(g2);

    JS::RootedValue v1(cx);

    {
        JSAutoCompartment ac(cx, g1);
        JS::RootedValue prop(cx, JS::Int32Value(1337));

        v1 = JS::ObjectOrNullValue(JS_NewObject(cx, nullptr, nullptr, nullptr));
        CHECK(v1.isObject());
        CHECK(JS_SetProperty(cx, &v1.toObject(), "prop", prop));
    }

    {
        JSAutoCompartment ac(cx, g2);
        JS::RootedValue v2(cx);

        CHECK(JS_StructuredClone(cx, v1, v2.address(), nullptr, nullptr));
        CHECK(v2.isObject());

        JS::RootedValue prop(cx);
        CHECK(JS_GetProperty(cx, &v2.toObject(), "prop", &prop));
        CHECK(prop.isInt32());
        CHECK(&v1.toObject() != &v2.toObject());
        CHECK_EQUAL(prop.toInt32(), 1337);
    }

    return true;
}
END_TEST(testStructuredClone_object)

BEGIN_TEST(testStructuredClone_string)
{
    JS::RootedObject g1(cx, createGlobal());
    JS::RootedObject g2(cx, createGlobal());
    CHECK(g1);
    CHECK(g2);

    JS::RootedValue v1(cx);

    {
        JSAutoCompartment ac(cx, g1);
        JS::RootedValue prop(cx, JS::Int32Value(1337));

        v1 = JS::StringValue(JS_NewStringCopyZ(cx, "Hello World!"));
        CHECK(v1.isString());
        CHECK(v1.toString());
    }

    {
        JSAutoCompartment ac(cx, g2);
        JS::RootedValue v2(cx);

        CHECK(JS_StructuredClone(cx, v1, v2.address(), nullptr, nullptr));
        CHECK(v2.isString());
        CHECK(v2.toString());
        
        JS::RootedValue expected(cx, JS::StringValue(
            JS_NewStringCopyZ(cx, "Hello World!")));
        CHECK_SAME(v2, expected);
    }

    return true;
}
END_TEST(testStructuredClone_string)
