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
    jsvalRoot vobj(cx);
    JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);
    CHECK(obj);
    vobj = OBJECT_TO_JSVAL(obj);

    jsvalRoot vget(cx);
    JSFunction *funGet = JS_NewFunction(cx, native, 0, 0, NULL, "get");
    CHECK(funGet);
    JSObject *funGetObj = JS_GetFunctionObject(funGet);
    vget = OBJECT_TO_JSVAL(funGetObj);

    jsvalRoot vset(cx);
    JSFunction *funSet = JS_NewFunction(cx, native, 1, 0, NULL, "set");
    CHECK(funSet);
    JSObject *funSetObj = JS_GetFunctionObject(funSet);
    vset = OBJECT_TO_JSVAL(funSetObj);

    CHECK(JS_DefineProperty(cx, JSVAL_TO_OBJECT(vobj), PROPERTY_NAME,
                            JSVAL_VOID,
                            JS_DATA_TO_FUNC_PTR(JSPropertyOp, funGetObj),
                            JS_DATA_TO_FUNC_PTR(JSStrictPropertyOp, funSetObj),
                            JSPROP_GETTER | JSPROP_SETTER | JSPROP_ENUMERATE));

    CHECK(JS_DefineProperty(cx, JSVAL_TO_OBJECT(vobj), PROPERTY_NAME,
                            JSVAL_VOID,
                            JS_DATA_TO_FUNC_PTR(JSPropertyOp, funGetObj),
                            JS_DATA_TO_FUNC_PTR(JSStrictPropertyOp, funSetObj),
                            JSPROP_GETTER | JSPROP_SETTER | JSPROP_PERMANENT));

    JSBool found = JS_FALSE;
    unsigned attrs = 0;
    CHECK(JS_GetPropertyAttributes(cx, JSVAL_TO_OBJECT(vobj), PROPERTY_NAME,
                                   &attrs, &found));
    CHECK(found);
    CHECK(attrs & JSPROP_GETTER);
    CHECK(attrs & JSPROP_SETTER);
    CHECK(attrs & JSPROP_PERMANENT);
    CHECK(!(attrs & JSPROP_ENUMERATE));

    return true;
}
END_TEST(testDefineGetterSetterNonEnumerable)
