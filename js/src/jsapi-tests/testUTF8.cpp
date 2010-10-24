/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"

BEGIN_TEST(testUTF8_bug589917)
{
    const jschar surrogate_pair[] = { 0xd800, 0xdc00 };
    char output_buffer[10];
    size_t utf8_len = sizeof(output_buffer);

    CHECK(JS_EncodeCharacters(cx, surrogate_pair, 2, output_buffer, &utf8_len));
    CHECK(utf8_len == 4);

    CHECK(JS_EncodeCharacters(cx, surrogate_pair, 2, NULL, &utf8_len));
    CHECK(utf8_len == 4);

    return true;
}
END_TEST(testUTF8_bug589917)
