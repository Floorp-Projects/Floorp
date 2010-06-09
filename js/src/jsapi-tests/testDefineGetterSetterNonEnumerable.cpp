/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"
#include "jsxdrapi.h"

static JSBool
native(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
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

    jsvalRoot vset(cx);
    JSFunction *funSet = JS_NewFunction(cx, native, 1, 0, NULL, "set");
    CHECK(funSet);

    CHECK(JS_DefineProperty(cx, JSVAL_TO_OBJECT(vobj), PROPERTY_NAME,
                            JSVAL_VOID,
                            JS_DATA_TO_FUNC_PTR(JSPropertyOp, jsval(vget)),
                            JS_DATA_TO_FUNC_PTR(JSPropertyOp, jsval(vset)),
                            JSPROP_GETTER | JSPROP_SETTER | JSPROP_ENUMERATE));

    CHECK(JS_DefineProperty(cx, JSVAL_TO_OBJECT(vobj), PROPERTY_NAME,
                            JSVAL_VOID,
                            JS_DATA_TO_FUNC_PTR(JSPropertyOp, jsval(vget)),
                            JS_DATA_TO_FUNC_PTR(JSPropertyOp, jsval(vset)),
                            JSPROP_GETTER | JSPROP_SETTER | JSPROP_PERMANENT));

    JSBool found = JS_FALSE;
    uintN attrs = 0;
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
