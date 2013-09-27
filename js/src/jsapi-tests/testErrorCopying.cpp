/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Tests that the column number of error reports is properly copied over from
 * other reports when invoked from the C++ api.
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static uint32_t column = 0;

static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    column = report->column;
}

BEGIN_TEST(testErrorCopying_columnCopied)
{
        //0         1         2
        //0123456789012345678901234567
    EXEC("function check() { Object; foo; }");

    JS::RootedValue rval(cx);
    JS_SetErrorReporter(cx, my_ErrorReporter);
    CHECK(!JS_CallFunctionName(cx, global, "check", 0, NULL, rval.address()));
    CHECK(column == 27);
    return true;
}
END_TEST(testErrorCopying_columnCopied)
