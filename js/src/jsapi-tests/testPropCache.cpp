/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"

static int g_counter;

static JSBool
CounterAdd(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    g_counter++;
    return JS_TRUE;
}

static JSClass CounterClass = {
    "Counter",  /* name */
    0,  /* flags */
    CounterAdd, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

BEGIN_TEST(testPropCache_bug505798)
{
    g_counter = 0;
    EXEC("var x = {};");
    CHECK(JS_DefineObject(cx, global, "y", &CounterClass, NULL, JSPROP_ENUMERATE));
    EXEC("var arr = [x, y];\n"
         "for (var i = 0; i < arr.length; i++)\n"
         "    arr[i].p = 1;\n");
    CHECK(g_counter == 1);
    return true;
}
END_TEST(testPropCache_bug505798)
