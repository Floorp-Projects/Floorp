/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * Tests for operators and implicit type conversion.
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "tests.h"

static JSBool
my_convert(JSContext* context, JS::HandleObject obj, JSType type, jsval* rval)
{
    if (type == JSTYPE_VOID || type == JSTYPE_STRING || type == JSTYPE_NUMBER || type == JSTYPE_BOOLEAN)
        return JS_NewNumberValue(context, 123, rval);
    return JS_FALSE;
}

static JSClass myClass = {
    "MyClass",
    0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, my_convert
};

static JSBool
createMyObject(JSContext* context, unsigned argc, jsval *vp)
{
    JS_BeginRequest(context);

    //JS_GC(context); //<- if we make GC here, all is ok

    JSObject* myObject = JS_NewObject(context, &myClass, NULL, NULL);
    *vp = OBJECT_TO_JSVAL(myObject);

    JS_EndRequest(context);

    return JS_TRUE;
}

static JSFunctionSpec s_functions[] =
{
    { "createMyObject", createMyObject, 0 },
    { 0,0,0,0 }
};

BEGIN_TEST(testOps_bug559006)
{
    CHECK(JS_DefineFunctions(cx, global, s_functions));

    EXEC("function main() { while(1) return 0 + createMyObject(); }");

    for (int i = 0; i < 9; i++) {
        jsvalRoot rval(cx);
        CHECK(JS_CallFunctionName(cx, global, "main", 0, NULL, rval.addr()));
        CHECK_SAME(rval, INT_TO_JSVAL(123));
    }
    return true;
}
END_TEST(testOps_bug559006)

