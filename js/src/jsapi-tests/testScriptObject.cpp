/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

struct ScriptObjectFixture : public JSAPITest {
    static const int code_size;
    static const char code[];
    static char16_t uc_code[];

    ScriptObjectFixture()
    {
        for (int i = 0; i < code_size; i++)
            uc_code[i] = code[i];
    }

    bool tryScript(JS::HandleScript script)
    {
        CHECK(script);

        JS_GC(cx);

        /* After a garbage collection, the script should still work. */
        JS::RootedValue result(cx);
        CHECK(JS_ExecuteScript(cx, script, &result));

        return true;
    }
};

const char ScriptObjectFixture::code[] =
    "(function(a, b){return a+' '+b;}('hello', 'world'))";
const int ScriptObjectFixture::code_size = sizeof(ScriptObjectFixture::code) - 1;
char16_t ScriptObjectFixture::uc_code[ScriptObjectFixture::code_size];

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_CompileScript)
{
    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, __LINE__);
    JS::RootedScript script(cx);
    CHECK(JS_CompileScript(cx, code, code_size, options, &script));
    return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_CompileScript)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_CompileScript_empty)
{
    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, __LINE__);
    JS::RootedScript script(cx);
    CHECK(JS_CompileScript(cx, "", 0, options, &script));
    return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_CompileScript_empty)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_CompileScriptForPrincipals)
{
    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, __LINE__);
    JS::RootedScript script(cx);
    CHECK(JS_CompileScript(cx, code, code_size, options, &script));
    return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_CompileScriptForPrincipals)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileUCScript)
{
    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, __LINE__);
    JS::RootedScript script(cx);
    CHECK(JS_CompileUCScript(cx, uc_code, code_size, options, &script));
    return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileUCScript)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileUCScript_empty)
{
    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, __LINE__);
    JS::RootedScript script(cx);
    CHECK(JS_CompileUCScript(cx, uc_code, 0, options, &script));
    return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileUCScript_empty)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileUCScriptForPrincipals)
{
    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, __LINE__);
    JS::RootedScript script(cx);
    CHECK(JS_CompileUCScript(cx, uc_code, code_size, options, &script));
    return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileUCScriptForPrincipals)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFile)
{
    TempFile tempScript;
    static const char script_filename[] = "temp-bug438633_JS_CompileFile";
    FILE* script_stream = tempScript.open(script_filename);
    CHECK(fputs(code, script_stream) != EOF);
    tempScript.close();
    JS::CompileOptions options(cx);
    options.setFileAndLine(script_filename, 1);
    JS::RootedScript script(cx);
    CHECK(JS::Compile(cx, options, script_filename, &script));
    tempScript.remove();
    return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFile)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFile_empty)
{
    TempFile tempScript;
    static const char script_filename[] = "temp-bug438633_JS_CompileFile_empty";
    tempScript.open(script_filename);
    tempScript.close();
    JS::CompileOptions options(cx);
    options.setFileAndLine(script_filename, 1);
    JS::RootedScript script(cx);
    CHECK(JS::Compile(cx, options, script_filename, &script));
    tempScript.remove();
    return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFile_empty)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFileHandle)
{
    const char* script_filename = "temporary file";
    TempFile tempScript;
    FILE* script_stream = tempScript.open("temp-bug438633_JS_CompileFileHandle");
    CHECK(fputs(code, script_stream) != EOF);
    CHECK(fseek(script_stream, 0, SEEK_SET) != EOF);
    JS::CompileOptions options(cx);
    options.setFileAndLine(script_filename, 1);
    JS::RootedScript script(cx);
    CHECK(JS::Compile(cx, options, script_stream, &script));
    return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFileHandle)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFileHandle_empty)
{
    const char* script_filename = "empty temporary file";
    TempFile tempScript;
    FILE* script_stream = tempScript.open("temp-bug438633_JS_CompileFileHandle_empty");
    JS::CompileOptions options(cx);
    options.setFileAndLine(script_filename, 1);
    JS::RootedScript script(cx);
    CHECK(JS::Compile(cx, options, script_stream, &script));
    return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFileHandle_empty)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFileHandleForPrincipals)
{
    TempFile tempScript;
    FILE* script_stream = tempScript.open("temp-bug438633_JS_CompileFileHandleForPrincipals");
    CHECK(fputs(code, script_stream) != EOF);
    CHECK(fseek(script_stream, 0, SEEK_SET) != EOF);
    JS::CompileOptions options(cx);
    options.setFileAndLine("temporary file", 1);
    JS::RootedScript script(cx);
    CHECK(JS::Compile(cx, options, script_stream, &script));
    return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFileHandleForPrincipals)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, CloneAndExecuteScript)
{
    JS::RootedValue fortyTwo(cx);
    fortyTwo.setInt32(42);
    CHECK(JS_SetProperty(cx, global, "val", fortyTwo));
    JS::RootedScript script(cx);
    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, __LINE__);
    CHECK(JS_CompileScript(cx, "val", 3, options, &script));
    JS::RootedValue value(cx);
    CHECK(JS_ExecuteScript(cx, script, &value));
    CHECK(value.toInt32() == 42);
    {
        JS::RootedObject global2(cx, createGlobal());
        CHECK(global2);
        JSAutoRealm ar(cx, global2);
        CHECK(JS_WrapValue(cx, &fortyTwo));
        CHECK(JS_SetProperty(cx, global, "val", fortyTwo));
        JS::RootedValue value2(cx);
        CHECK(JS::CloneAndExecuteScript(cx, script, &value2));
        CHECK(value2.toInt32() == 42);
    }
    JS::RootedValue value3(cx);
    CHECK(JS_ExecuteScript(cx, script, &value3));
    CHECK(value3.toInt32() == 42);
    return true;
}
END_FIXTURE_TEST(ScriptObjectFixture, CloneAndExecuteScript)
