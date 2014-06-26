/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/OldDebugAPI.h"
#include "jsapi-tests/tests.h"

const char code[] =
    "xx = 1;       \n\
                   \n\
try {              \n\
	 debugger; \n\
                   \n\
	 xx += 1;  \n\
}                  \n\
catch (e)          \n\
{                  \n\
	 xx += 1;  \n\
}\n\
//@ sourceMappingURL=http://example.com/path/to/source-map.json";

// Bug 670958 - fix JS_GetScriptLineExtent, among others
BEGIN_TEST(testScriptInfo)
{
    unsigned startLine = 1000;

    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, startLine);
    JS::RootedScript script(cx, JS_CompileScript(cx, global, code, strlen(code),
                                                 options));

    CHECK(script);

    jsbytecode *start = JS_LineNumberToPC(cx, script, startLine);
    CHECK_EQUAL(JS_GetScriptBaseLineNumber(cx, script), startLine);
    CHECK_EQUAL(JS_PCToLineNumber(cx, script, start), startLine);
    CHECK_EQUAL(JS_GetScriptLineExtent(cx, script), 11u);
    CHECK(strcmp(JS_GetScriptFilename(script), __FILE__) == 0);
    const jschar *sourceMap = JS_GetScriptSourceMap(cx, script);
    CHECK(sourceMap);
    CHECK(CharsMatch(sourceMap, "http://example.com/path/to/source-map.json"));

    return true;
}
static bool
CharsMatch(const jschar *p, const char *q)
{
    while (*q) {
        if (*p++ != *q++)
            return false;
    }
    return true;
}
END_TEST(testScriptInfo)
