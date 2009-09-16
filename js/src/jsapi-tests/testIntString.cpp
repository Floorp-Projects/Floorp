#include "tests.h"

BEGIN_TEST(testIntString_bug515273)
{
    jsval v;
    EVAL("'42';", &v);

    JSString *str = JSVAL_TO_STRING(v);
    const char *bytes = JS_GetStringBytes(str);
    CHECK(strcmp(bytes, "42") == 0);
    return true;
}
END_TEST(testIntString_bug515273)
