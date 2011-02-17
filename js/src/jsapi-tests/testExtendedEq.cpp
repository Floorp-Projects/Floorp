/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This tests user-specified (via JSExtendedClass) equality operations on
 * trace.
 */

#include "tests.h"
#include "jsobj.h"

static JSBool
my_Equality(JSContext *cx, JSObject *obj, const jsval *, JSBool *bp)
{
    *bp = JS_TRUE;
    return JS_TRUE;
}

js::Class TestExtendedEq_JSClass = {
    "TestExtendedEq",
    0,
    js::PropertyStub,       /* addProperty */
    js::PropertyStub,       /* delProperty */
    js::PropertyStub,       /* getProperty */
    js::StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    NULL,           /* convert */
    NULL,           /* finalize */
    NULL,           /* reserved0   */
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* xdrObject   */
    NULL,           /* hasInstance */
    NULL,           /* mark        */
    {
        js::Valueify(my_Equality),
        NULL, /* outerObject    */
        NULL, /* innerObject    */
        NULL, /* iteratorObject */
        NULL, /* wrappedObject  */
    }
};

BEGIN_TEST(testExtendedEq_bug530489)
{
    JSClass *clasp = (JSClass *) &TestExtendedEq_JSClass;

    CHECK(JS_InitClass(cx, global, global, clasp, NULL, 0, NULL, NULL, NULL, NULL));

    CHECK(JS_DefineObject(cx, global, "obj1", clasp, NULL, 0));
    CHECK(JS_DefineObject(cx, global, "obj2", clasp, NULL, 0));

    jsval v;
    EVAL("(function() { var r; for (var i = 0; i < 10; ++i) r = obj1 == obj2; return r; })()", &v);
    CHECK_SAME(v, JSVAL_TRUE);
    return true;
}
END_TEST(testExtendedEq_bug530489)
