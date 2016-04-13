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
    CHECK(x.isTrue());

    // Now make x.f a method.
    EVAL("mkobj()", &x);
    JS::RootedObject xobj(cx, x.toObjectOrNull());

    // This lookup must not return an internal function object.
    JS::RootedValue r(cx);
    CHECK(JS_GetProperty(cx, xobj, "f", &r));
    CHECK(r.isObject());
    JSObject* funobj = &r.toObject();
    CHECK(funobj->is<JSFunction>());
    CHECK(!js::IsInternalFunctionObject(*funobj));

    return true;
}
END_TEST(testLookup_bug522590)

static const JSClass DocumentAllClass = {
    "DocumentAll",
    JSCLASS_EMULATES_UNDEFINED
};

bool
document_resolve(JSContext* cx, JS::HandleObject obj, JS::HandleId id, bool* resolvedp)
{
    // If id is "all", resolve document.all=true.
    JS::RootedValue v(cx);
    if (!JS_IdToValue(cx, id, &v))
        return false;

    if (v.isString()) {
        JSString* str = v.toString();
        JSFlatString* flatStr = JS_FlattenString(cx, str);
        if (!flatStr)
            return false;
        if (JS_FlatStringEqualsAscii(flatStr, "all")) {
            JS::Rooted<JSObject*> docAll(cx, JS_NewObject(cx, &DocumentAllClass));
            if (!docAll)
                return false;

            JS::Rooted<JS::Value> allValue(cx, JS::ObjectValue(*docAll));
            if (!JS_DefinePropertyById(cx, obj, id, allValue, JSPROP_RESOLVING))
                return false;

            *resolvedp = true;
            return true;
        }
    }

    *resolvedp = false;
    return true;
}

static const JSClassOps document_classOps = {
    nullptr, nullptr, nullptr, nullptr,
    nullptr, document_resolve, nullptr
};

static const JSClass document_class = {
    "document", 0,
    &document_classOps
};

BEGIN_TEST(testLookup_bug570195)
{
    JS::RootedObject obj(cx, JS_NewObject(cx, &document_class));
    CHECK(obj);
    CHECK(JS_DefineProperty(cx, global, "document", obj, 0));
    JS::RootedValue v(cx);
    EVAL("document.all ? true : false", &v);
    CHECK(v.isFalse());
    EVAL("document.hasOwnProperty('all')", &v);
    CHECK(v.isTrue());
    return true;
}
END_TEST(testLookup_bug570195)
