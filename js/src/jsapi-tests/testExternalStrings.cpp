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

    const unsigned N = 1000;

    for (unsigned i = 0; i < N; ++i) {
        CHECK(JS_NewExternalString(cx, arr, arrlen, op));
        CHECK(JS_NewExternalStringWithClosure(cx, arr, arrlen, op, magic));
    }

    // clear that newborn root
    JS_NewUCStringCopyN(cx, arr, arrlen);

    JS_GC(cx);

    // a generous fudge factor to account for strings rooted by conservative gc
    const unsigned epsilon = 10;

    CHECK((N - finalized_noclosure) < epsilon);
    CHECK((N - finalized_closure) < epsilon);

    return true;
}
END_TEST(testExternalStrings)
