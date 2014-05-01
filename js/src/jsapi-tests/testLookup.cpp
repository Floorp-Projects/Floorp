/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsfun.h"  // for js::IsInternalFunctionObject

#include "jsapi-tests/tests.h"

#include "jsobjinlines.h"

BEGIN_TEST(testLookup_bug522590)
{
    // Define a function that makes method-bearing objects.
    JS::RootedValue x(cx);
    EXEC("function mkobj() { return {f: function () {return 2;}} }");

    // Calling mkobj() multiple times must create multiple functions in ES5.
    EVAL("mkobj().f !== mkobj().f", &x);
    CHECK_SAME(x, JSVAL_TRUE);

    // Now make x.f a method.
    EVAL("mkobj()", &x);
    JS::RootedObject xobj(cx, x.toObjectOrNull());

    // This lookup must not return an internal function object.
    JS::RootedValue r(cx);
    CHECK(JS_LookupProperty(cx, xobj, "f", &r));
    CHECK(r.isObject());
    JSObject *funobj = &r.toObject();
    CHECK(funobj->is<JSFunction>());
    CHECK(!js::IsInternalFunctionObject(funobj));

    return true;
}
END_TEST(testLookup_bug522590)

static const JSClass DocumentAllClass = {
    "DocumentAll",
    JSCLASS_EMULATES_UNDEFINED,
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

bool
document_resolve(JSContext *cx, JS::HandleObject obj, JS::HandleId id,
                 JS::MutableHandleObject objp)
{
    // If id is "all", resolve document.all=true.
    JS::RootedValue v(cx);
    if (!JS_IdToValue(cx, id, &v))
        return false;
    if (v.isString()) {
        JSString *str = v.toString();
        JSFlatString *flatStr = JS_FlattenString(cx, str);
        if (!flatStr)
            return false;
        if (JS_FlatStringEqualsAscii(flatStr, "all")) {
            JS::Rooted<JSObject*> docAll(cx,
                                         JS_NewObject(cx, &DocumentAllClass, JS::NullPtr(), JS::NullPtr()));
            if (!docAll)
                return false;
            JS::Rooted<JS::Value> allValue(cx, ObjectValue(*docAll));
            bool ok = JS_DefinePropertyById(cx, obj, id, allValue, 0);
            objp.set(ok ? obj.get() : nullptr);
            return ok;
        }
    }
    objp.set(nullptr);
    return true;
}

static const JSClass document_class = {
    "document", JSCLASS_NEW_RESOLVE,
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, (JSResolveOp) document_resolve, JS_ConvertStub
};

BEGIN_TEST(testLookup_bug570195)
{
    JS::RootedObject obj(cx, JS_NewObject(cx, &document_class, JS::NullPtr(), JS::NullPtr()));
    CHECK(obj);
    CHECK(JS_DefineProperty(cx, global, "document", obj, 0));
    JS::RootedValue v(cx);
    EVAL("document.all ? true : false", &v);
    CHECK_SAME(v, JSVAL_FALSE);
    EVAL("document.hasOwnProperty('all')", &v);
    CHECK_SAME(v, JSVAL_TRUE);
    return true;
}
END_TEST(testLookup_bug570195)
