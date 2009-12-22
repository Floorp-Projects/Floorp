#include "tests.h"

BEGIN_TEST(testIntString_bug515273)
{
    jsvalRoot v(cx);
    EVAL("'42';", v.addr());

    JSString *str = JSVAL_TO_STRING(v.value());
    const char *bytes = JS_GetStringBytes(str);
    CHECK(strcmp(bytes, "42") == 0);
    return true;
}
END_TEST(testIntString_bug515273)
