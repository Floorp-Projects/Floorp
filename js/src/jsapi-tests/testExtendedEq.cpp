/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This tests user-specified (via JSExtendedClass) equality operations on
 * trace.
 */

#include "tests.h"

static JSBool
my_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    *bp = JS_TRUE;
    return JS_TRUE;
}

JSExtendedClass TestExtendedEq_JSClass = {
    { "TestExtendedEq",
        JSCLASS_IS_EXTENDED,
        JS_PropertyStub,    JS_PropertyStub,   JS_PropertyStub,   JS_PropertyStub,
        JS_EnumerateStub,   JS_ResolveStub,    NULL,              NULL,
        NULL,               NULL,              NULL,              NULL,
        NULL,               NULL,              NULL,              NULL
    },
    // JSExtendedClass initialization
    my_Equality,
    NULL, NULL, NULL, NULL, JSCLASS_NO_RESERVED_MEMBERS
};

BEGIN_TEST(testExtendedEq_bug530489)
{
    JSClass *clasp = (JSClass *) &TestExtendedEq_JSClass;

    JSObject *global = JS_GetGlobalObject(cx);
    JS_InitClass(cx, global, global, clasp, NULL, 0, NULL, NULL, NULL, NULL);

    JS_DefineObject(cx, global, "obj1", clasp, NULL, 0);
    JS_DefineObject(cx, global, "obj2", clasp, NULL, 0);

    jsval v;
    EVAL("(function() { var r; for (var i = 0; i < 10; ++i) r = obj1 == obj2; return r; })()", &v);
    CHECK_SAME(v, JSVAL_TRUE);
    return true;
}
END_TEST(testExtendedEq_bug530489)
