/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsnum.h"
#include "jsstr.h"

#include "vm/String-inl.h"

template<size_t N> JSFlatString *
NewString(JSContext *cx, const jschar (&chars)[N])
{
    return js_NewStringCopyN(cx, chars, N);
}

static const struct TestPair {
    uint32 num;
    const char *expected;
} tests[] = {
    { 0, "0" },
    { 1, "1" },
    { 2, "2" },
    { 9, "9" },
    { 10, "10" },
    { 15, "15" },
    { 16, "16" },
    { 17, "17" },
    { 99, "99" },
    { 100, "100" },
    { 255, "255" },
    { 256, "256" },
    { 257, "257" },
    { 999, "999" },
    { 1000, "1000" },
    { 4095, "4095" },
    { 4096, "4096" },
    { 9999, "9999" },
    { 1073741823, "1073741823" },
    { 1073741824, "1073741824" },
    { 1073741825, "1073741825" },
    { 2147483647, "2147483647" },
    { 2147483648u, "2147483648" },
    { 2147483649u, "2147483649" },
    { 4294967294u, "4294967294" },
    { 4294967295u, "4294967295" },
};

BEGIN_TEST(testIndexToString)
{
    for (size_t i = 0, sz = JS_ARRAY_LENGTH(tests); i < sz; i++) {
        uint32 u = tests[i].num;
        JSString *str = js::IndexToString(cx, u);
        CHECK(str);

        if (!JSAtom::hasUintStatic(u))
            CHECK(cx->compartment->dtoaCache.lookup(10, u) == str);

        JSBool match = JS_FALSE;
        CHECK(JS_StringEqualsAscii(cx, str, tests[i].expected, &match));
        CHECK(match);
    }

    return true;
}
END_TEST(testIndexToString)

BEGIN_TEST(testStringIsElement)
{
    for (size_t i = 0, sz = JS_ARRAY_LENGTH(tests); i < sz; i++) {
        uint32 u = tests[i].num;
        JSFlatString *str = js::IndexToString(cx, u);
        CHECK(str);

        uint32 n;
        CHECK(str->isElement(&n));
        CHECK(u == n);
    }

    return true;
}
END_TEST(testStringIsElement)

BEGIN_TEST(testStringToPropertyName)
{
    uint32 index;

    static const jschar hiChars[] = { 'h', 'i' };
    JSFlatString *hiStr = NewString(cx, hiChars);
    CHECK(hiStr);
    CHECK(!hiStr->isElement(&index));
    CHECK(hiStr->toPropertyName(cx) != NULL);

    static const jschar maxChars[] = { '4', '2', '9', '4', '9', '6', '7', '2', '9', '5' };
    JSFlatString *maxStr = NewString(cx, maxChars);
    CHECK(maxStr);
    CHECK(maxStr->isElement(&index));
    CHECK(index == UINT32_MAX);

    static const jschar maxPlusOneChars[] = { '4', '2', '9', '4', '9', '6', '7', '2', '9', '6' };
    JSFlatString *maxPlusOneStr = NewString(cx, maxPlusOneChars);
    CHECK(maxPlusOneStr);
    CHECK(!maxPlusOneStr->isElement(&index));
    CHECK(maxPlusOneStr->toPropertyName(cx) != NULL);

    return true;
}
END_TEST(testStringToPropertyName)
