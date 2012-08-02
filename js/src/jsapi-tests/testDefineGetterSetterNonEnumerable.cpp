/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "tests.h"

static JSBool
native(JSContext *cx, unsigned argc, jsval *vp)
{
    return JS_TRUE;
}

static const char PROPERTY_NAME[] = "foo";

BEGIN_TEST(testDefineGetterSetterNonEnumerable)
{
    JS::RootedValue vobj(cx);
    JS::RootedObject obj(cx, JS_NewObject(cx, NULL, NULL, NULL));
    CHECK(obj);
    vobj = OBJECT_TO_JSVAL(obj);

    JSFunction *funGet = JS_NewFunction(cx, native, 0, 0, NULL, "get");
    CHECK(funGet);
    JS::RootedObject funGetObj(cx, JS_GetFunctionObject(funGet));
    JS::RootedValue vget(cx, OBJECT_TO_JSVAL(funGetObj));

    JSFunction *funSet = JS_NewFunction(cx, native, 1, 0, NULL, "set");
    CHECK(funSet);
    JS::RootedObject funSetObj(cx, JS_GetFunctionObject(funSet));
    JS::RootedValue vset(cx, OBJECT_TO_JSVAL(funSetObj));

    JS::RootedObject vObject(cx, JSVAL_TO_OBJECT(vobj));
    CHECK(JS_DefineProperty(cx, vObject, PROPERTY_NAME,
                            JSVAL_VOID,
                            JS_DATA_TO_FUNC_PTR(JSPropertyOp, (JSObject*) funGetObj),
                            JS_DATA_TO_FUNC_PTR(JSStrictPropertyOp, (JSObject*) funSetObj),
                            JSPROP_GETTER | JSPROP_SETTER | JSPROP_ENUMERATE));

    CHECK(JS_DefineProperty(cx, vObject, PROPERTY_NAME,
                            JSVAL_VOID,
                            JS_DATA_TO_FUNC_PTR(JSPropertyOp, (JSObject*) funGetObj),
                            JS_DATA_TO_FUNC_PTR(JSStrictPropertyOp, (JSObject*) funSetObj),
                            JSPROP_GETTER | JSPROP_SETTER | JSPROP_PERMANENT));

    JSBool found = JS_FALSE;
    unsigned attrs = 0;
    CHECK(JS_GetPropertyAttributes(cx, vObject, PROPERTY_NAME, &attrs, &found));
    CHECK(found);
    CHECK(attrs & JSPROP_GETTER);
    CHECK(attrs & JSPROP_SETTER);
    CHECK(attrs & JSPROP_PERMANENT);
    CHECK(!(attrs & JSPROP_ENUMERATE));

    return true;
}
END_TEST(testDefineGetterSetterNonEnumerable)
