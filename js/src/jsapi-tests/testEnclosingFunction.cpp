/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Test script cloning.
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsfriendapi.h"

#include "js/OldDebugAPI.h"
#include "jsapi-tests/tests.h"

using namespace js;

static JSScript *foundScript = nullptr;

static bool
CheckEnclosing(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    foundScript = js::GetOutermostEnclosingFunctionOfScriptedCaller(cx);

    args.rval().set(UndefinedValue());
    return true;
}

BEGIN_TEST(test_enclosingFunction)
{
    CHECK(JS_DefineFunction(cx, global, "checkEnclosing", CheckEnclosing, 0, 0));

    EXEC("checkEnclosing()");
    CHECK(foundScript == nullptr);

    RootedFunction fun(cx);

    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, __LINE__);

    const char s1chars[] = "checkEnclosing()";
    fun = JS_CompileFunction(cx, global, "s1", 0, nullptr, s1chars,
                             strlen(s1chars), options);
    CHECK(fun);
    EXEC("s1()");
    CHECK(foundScript == JS_GetFunctionScript(cx, fun));

    const char s2chars[] = "return function() { checkEnclosing() }";
    fun = JS_CompileFunction(cx, global, "s2", 0, nullptr, s2chars,
                             strlen(s2chars), options);
    CHECK(fun);
    EXEC("s2()()");
    CHECK(foundScript == JS_GetFunctionScript(cx, fun));

    const char s3chars[] = "return function() { let (x) { function g() { checkEnclosing() } return g() } }";
    fun = JS_CompileFunction(cx, global, "s3", 0, nullptr, s3chars,
                             strlen(s3chars), options);
    CHECK(fun);
    EXEC("s3()()");
    CHECK(foundScript == JS_GetFunctionScript(cx, fun));

    return true;
}
END_TEST(test_enclosingFunction)
