/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static bool
native(JSContext *cx, unsigned argc, jsval *vp)
{
    return true;
}

static const char PROPERTY_NAME[] = "foo";

BEGIN_TEST(testDefineGetterSetterNonEnumerable)
{
    JS::RootedValue vobj(cx);
    JS::RootedObject obj(cx, JS_NewObject(cx, nullptr, nullptr, nullptr));
    CHECK(obj);
    vobj = OBJECT_TO_JSVAL(obj);

    JSFunction *funGet = JS_NewFunction(cx, native, 0, 0, nullptr, "get");
    CHECK(funGet);
    JS::RootedObject funGetObj(cx, JS_GetFunctionObject(funGet));
    JS::RootedValue vget(cx, OBJECT_TO_JSVAL(funGetObj));

    JSFunction *funSet = JS_NewFunction(cx, native, 1, 0, nullptr, "set");
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

    JS::Rooted<JSPropertyDescriptor> desc(cx);
    CHECK(JS_GetOwnPropertyDescriptor(cx, vObject, PROPERTY_NAME, 0, &desc));
    CHECK(desc.object());
    CHECK(desc.hasGetterObject());
    CHECK(desc.hasSetterObject());
    CHECK(desc.isPermanent());
    CHECK(!desc.isEnumerable());

    return true;
}
END_TEST(testDefineGetterSetterNonEnumerable)
