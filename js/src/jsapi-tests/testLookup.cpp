/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"
#include "jsfun.h"  // for js::IsInternalFunctionObject

#include "jsobjinlines.h"

BEGIN_TEST(testLookup_bug522590)
{
    // Define a function that makes method-bearing objects.
    jsvalRoot x(cx);
    EXEC("function mkobj() { return {f: function () {return 2;}} }");

    // Calling mkobj() multiple times must create multiple functions in ES5.
    EVAL("mkobj().f !== mkobj().f", x.addr());
    CHECK_SAME(x, JSVAL_TRUE);

    // Now make x.f a method.
    EVAL("mkobj()", x.addr());
    JSObject *xobj = JSVAL_TO_OBJECT(x);

    // This lookup must not return an internal function object.
    jsvalRoot r(cx);
    CHECK(JS_LookupProperty(cx, xobj, "f", r.addr()));
    CHECK(JSVAL_IS_OBJECT(r));
    JSObject *funobj = JSVAL_TO_OBJECT(r);
    CHECK(funobj->isFunction());
    CHECK(!js::IsInternalFunctionObject(funobj));
    CHECK(funobj->toFunction()->isClonedMethod());

    return true;
}
END_TEST(testLookup_bug522590)

JSBool
document_resolve(JSContext *cx, JSObject *obj, jsid id, unsigned flags, JSObject **objp)
{
    // If id is "all", and we're not detecting, resolve document.all=true.
    jsvalRoot v(cx);
    if (!JS_IdToValue(cx, id, v.addr()))
        return false;
    if (JSVAL_IS_STRING(v.value())) {
        JSString *str = JSVAL_TO_STRING(v.value());
        JSFlatString *flatStr = JS_FlattenString(cx, str);
        if (!flatStr)
            return false;
        if (JS_FlatStringEqualsAscii(flatStr, "all") && !(flags & JSRESOLVE_DETECTING)) {
            JSBool ok = JS_DefinePropertyById(cx, obj, id, JSVAL_TRUE, NULL, NULL, 0);
            *objp = ok ? obj : NULL;
            return ok;
        }
    }
    *objp = NULL;
    return true;
}

static JSClass document_class = {
    "document", JSCLASS_NEW_RESOLVE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, (JSResolveOp) document_resolve, JS_ConvertStub
};

BEGIN_TEST(testLookup_bug570195)
{
    JSObject *obj = JS_NewObject(cx, &document_class, NULL, NULL);
    CHECK(obj);
    CHECK(JS_DefineProperty(cx, global, "document", OBJECT_TO_JSVAL(obj), NULL, NULL, 0));
    jsvalRoot v(cx);
    EVAL("document.all ? true : false", v.addr());
    CHECK_SAME(v.value(), JSVAL_FALSE);
    EVAL("document.hasOwnProperty('all')", v.addr());
    CHECK_SAME(v.value(), JSVAL_FALSE);
    return true;
}
END_TEST(testLookup_bug570195)
