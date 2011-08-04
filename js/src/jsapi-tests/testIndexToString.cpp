/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsnum.h"

#include "vm/String-inl.h"

BEGIN_TEST(testIndexToString)
{
    const struct TestPair {
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
        { 2147483648, "2147483648" },
        { 2147483649, "2147483649" },
        { 4294967294, "4294967294" },
        { 4294967295, "4294967295" },
    };

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
