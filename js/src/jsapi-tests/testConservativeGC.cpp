#include "tests.h"
#include "jsobj.h"
#include "jsstr.h"

BEGIN_TEST(testConservativeGC)
{
    jsval v1;
    EVAL("Math.sqrt(42);", &v1);
    CHECK(JSVAL_IS_DOUBLE(v1));
    double numCopy = *JSVAL_TO_DOUBLE(v1);

    jsval v2;
    EVAL("({foo: 'bar'});", &v2);
    CHECK(JSVAL_IS_OBJECT(v2));
    JSObject objCopy = *JSVAL_TO_OBJECT(v2);

    jsval v3;
    EVAL("String(Math.PI);", &v3);
    CHECK(JSVAL_IS_STRING(v3));
    JSString strCopy = *JSVAL_TO_STRING(v3);

    jsval tmp;
    EVAL("Math.sqrt(41);", &tmp);
    CHECK(JSVAL_IS_DOUBLE(tmp));
    jsdouble *num2 = JSVAL_TO_DOUBLE(tmp);
    jsdouble num2Copy = *num2;

    EVAL("({foo2: 'bar2'});", &tmp);
    CHECK(JSVAL_IS_OBJECT(tmp));
    JSObject *obj2 = JSVAL_TO_OBJECT(tmp);
    JSObject obj2Copy = *obj2;

    EVAL("String(Math.sqrt(3));", &tmp);
    CHECK(JSVAL_IS_STRING(tmp));
    JSString *str2 = JSVAL_TO_STRING(tmp);
    JSString str2Copy = *str2;

    tmp = JSVAL_NULL;

    JS_GC(cx);

    EVAL("var a = [];\n"
         "for (var i = 0; i != 10000; ++i) {\n"
         "a.push(i + 0.1, [1, 2], String(Math.sqrt(i)));\n"
         "}", &tmp);

    JS_GC(cx);

    CHECK(numCopy == *JSVAL_TO_DOUBLE(v1));
    CHECK(!memcmp(&objCopy,  JSVAL_TO_OBJECT(v2), sizeof(objCopy)));
    CHECK(!memcmp(&strCopy,  JSVAL_TO_STRING(v3), sizeof(strCopy)));

    CHECK(num2Copy == *num2);
    CHECK(!memcmp(&obj2Copy,  obj2, sizeof(obj2Copy)));
    CHECK(!memcmp(&str2Copy,  str2, sizeof(str2Copy)));

    return true;
}
END_TEST(testConservativeGC)
