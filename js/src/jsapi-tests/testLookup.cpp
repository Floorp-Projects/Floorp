/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "tests.h"
#include "jsfun.h"  // for js::IsInternalFunctionObject

#include "jsobjinlines.h"

BEGIN_TEST(testLookup_bug522590)
{
    // Define a function that makes method-bearing objects.
    JS::RootedValue x(cx);
    EXEC("function mkobj() { return {f: function () {return 2;}} }");

    // Calling mkobj() multiple times must create multiple functions in ES5.
    EVAL("mkobj().f !== mkobj().f", x.address());
    CHECK_SAME(x, JSVAL_TRUE);

    // Now make x.f a method.
    EVAL("mkobj()", x.address());
    JS::RootedObject xobj(cx, JSVAL_TO_OBJECT(x));

    // This lookup must not return an internal function object.
    JS::RootedValue r(cx);
    CHECK(JS_LookupProperty(cx, xobj, "f", r.address()));
    CHECK(r.isObject());
    JSObject *funobj = &r.toObject();
    CHECK(funobj->isFunction());
    CHECK(!js::IsInternalFunctionObject(funobj));

    return true;
}
END_TEST(testLookup_bug522590)

JSBool
document_resolve(JSContext *cx, JSHandleObject obj, JSHandleId id, unsigned flags,
                 JSMutableHandleObject objp)
{
    // If id is "all", and we're not detecting, resolve document.all=true.
    JS::RootedValue v(cx);
    if (!JS_IdToValue(cx, id, v.address()))
        return false;
    if (JSVAL_IS_STRING(v)) {
        JSString *str = JSVAL_TO_STRING(v);
        JSFlatString *flatStr = JS_FlattenString(cx, str);
        if (!flatStr)
            return false;
        if (JS_FlatStringEqualsAscii(flatStr, "all") && !(flags & JSRESOLVE_DETECTING)) {
            JSBool ok = JS_DefinePropertyById(cx, obj, id, JSVAL_TRUE, NULL, NULL, 0);
            objp.set(ok ? obj.get() : NULL);
            return ok;
        }
    }
    objp.set(NULL);
    return true;
}

static JSClass document_class = {
    "document", JSCLASS_NEW_RESOLVE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, (JSResolveOp) document_resolve, JS_ConvertStub
};

BEGIN_TEST(testLookup_bug570195)
{
    JS::RootedObject obj(cx, JS_NewObject(cx, &document_class, NULL, NULL));
    CHECK(obj);
    CHECK(JS_DefineProperty(cx, global, "document", OBJECT_TO_JSVAL(obj), NULL, NULL, 0));
    JS::RootedValue v(cx);
    EVAL("document.all ? true : false", v.address());
    CHECK_SAME(v, JSVAL_FALSE);
    EVAL("document.hasOwnProperty('all')", v.address());
    CHECK_SAME(v, JSVAL_FALSE);
    return true;
}
END_TEST(testLookup_bug570195)
