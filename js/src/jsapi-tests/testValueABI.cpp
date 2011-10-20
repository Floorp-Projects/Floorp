#include "tests.h"

/*
 * Bug 689101 - jsval is technically a non-POD type because it has a private
 * data member. On gcc, this doesn't seem to matter. On MSVC, this prevents
 * returning a jsval from a function between C and C++ because it will use a
 * retparam in C++ and a direct return value in C.
 */

extern "C" {

extern JSBool
C_ValueToObject(JSContext *cx, jsval v, JSObject **obj);

extern jsval
C_GetEmptyStringValue(JSContext *cx);

}

BEGIN_TEST(testValueABI)
{
    JSObject* obj = JS_GetGlobalObject(cx);
    jsval v = OBJECT_TO_JSVAL(obj);
    obj = NULL;
    CHECK(C_ValueToObject(cx, v, &obj));
    JSBool equal;
    CHECK(JS_StrictlyEqual(cx, v, OBJECT_TO_JSVAL(obj), &equal));
    CHECK(equal);

    v = C_GetEmptyStringValue(cx);
    CHECK(JSVAL_IS_STRING(v));

    return true;
}
END_TEST(testValueABI)
