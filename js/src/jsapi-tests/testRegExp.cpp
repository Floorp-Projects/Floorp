#include "tests.h"

BEGIN_TEST(testObjectIsRegExp)
{
    jsvalRoot val(cx);
    JSObject *obj;

    EVAL("new Object", val.addr());
    obj = JSVAL_TO_OBJECT(val.value());
    CHECK(!JS_ObjectIsRegExp(cx, obj));

    EVAL("/foopy/", val.addr());
    obj = JSVAL_TO_OBJECT(val.value());
    CHECK(JS_ObjectIsRegExp(cx, obj));

    return true;
}
END_TEST(testObjectIsRegExp)
