#include "tests.h"
#include "jsutil.h"

const jschar arr[] = { 'h', 'i', ',', 'd', 'o', 'n', '\'', 't', ' ', 'd', 'e', 'l', 'e', 't', 'e', ' ', 'm', 'e', '\0' };
size_t arrlen = sizeof(arr) / sizeof(arr[0]) - 1;
void *magic = (void *)0x42;

int finalized_noclosure = 0;
int finalized_closure = 0;

void finalize_str(JSContext *cx, JSString *str)
{
    size_t len;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &len);
    if (chars && len == arrlen && js::PodEqual(chars, arr, len)) {
        void *closure = JS_GetExternalStringClosure(cx, str);
        if (closure) {
            if (closure == magic)
                ++finalized_closure;
        } else {
            ++finalized_noclosure;
        }
    }
}

BEGIN_TEST(testExternalStrings)
{
    intN op = JS_AddExternalStringFinalizer(finalize_str);

    CHECK(JS_NewExternalString(cx, arr, arrlen, op));
    CHECK(JS_NewExternalStringWithClosure(cx, arr, arrlen, op, magic));

    // clear that newborn root
    JS_NewUCStringCopyN(cx, arr, arrlen);

    JS_GC(cx);
    CHECK(finalized_noclosure == 1);
    CHECK(finalized_closure == 1);

    return true;
}
END_TEST(testExternalStrings)
