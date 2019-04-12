/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "js/CompilationAndEvaluation.h"  // JS::Compile{,DontInflate,Utf8{FileDontInflate,Path}}
#include "js/SourceText.h"  // JS::Source{Ownership,Text}
#include "jsapi-tests/tests.h"

struct ScriptObjectFixture : public JSAPITest {
  static const int code_size;
  static const char code[];
  static char16_t uc_code[];

  ScriptObjectFixture() {
    for (int i = 0; i < code_size; i++) {
      uc_code[i] = code[i];
    }
  }

  bool tryScript(JS::HandleScript script) {
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
const int ScriptObjectFixture::code_size =
    sizeof(ScriptObjectFixture::code) - 1;
char16_t ScriptObjectFixture::uc_code[ScriptObjectFixture::code_size];

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_CompileScript) {
  JS::CompileOptions options(cx);
  options.setFileAndLine(__FILE__, __LINE__);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  CHECK(srcBuf.init(cx, code, code_size, JS::SourceOwnership::Borrowed));

  JS::RootedScript script(cx, JS::CompileDontInflate(cx, options, srcBuf));
  CHECK(script);

  return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_CompileScript)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_CompileScript_empty) {
  JS::CompileOptions options(cx);
  options.setFileAndLine(__FILE__, __LINE__);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  CHECK(srcBuf.init(cx, "", 0, JS::SourceOwnership::Borrowed));

  JS::RootedScript script(cx, JS::CompileDontInflate(cx, options, srcBuf));
  CHECK(script);

  return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_CompileScript_empty)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_CompileScriptForPrincipals) {
  JS::CompileOptions options(cx);
  options.setFileAndLine(__FILE__, __LINE__);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  CHECK(srcBuf.init(cx, code, code_size, JS::SourceOwnership::Borrowed));

  JS::RootedScript script(cx, JS::CompileDontInflate(cx, options, srcBuf));

  return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_CompileScriptForPrincipals)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileUCScript) {
  JS::CompileOptions options(cx);
  options.setFileAndLine(__FILE__, __LINE__);

  JS::SourceText<char16_t> srcBuf;
  CHECK(srcBuf.init(cx, uc_code, code_size, JS::SourceOwnership::Borrowed));

  JS::RootedScript script(cx, JS::Compile(cx, options, srcBuf));
  CHECK(script);

  return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileUCScript)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileUCScript_empty) {
  JS::CompileOptions options(cx);
  options.setFileAndLine(__FILE__, __LINE__);

  JS::SourceText<char16_t> srcBuf;
  CHECK(srcBuf.init(cx, uc_code, 0, JS::SourceOwnership::Borrowed));

  JS::RootedScript script(cx, JS::Compile(cx, options, srcBuf));
  CHECK(script);

  return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileUCScript_empty)

BEGIN_FIXTURE_TEST(ScriptObjectFixture,
                   bug438633_JS_CompileUCScriptForPrincipals) {
  JS::CompileOptions options(cx);
  options.setFileAndLine(__FILE__, __LINE__);

  JS::SourceText<char16_t> srcBuf;
  CHECK(srcBuf.init(cx, uc_code, code_size, JS::SourceOwnership::Borrowed));

  JS::RootedScript script(cx, JS::Compile(cx, options, srcBuf));
  CHECK(script);

  return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileUCScriptForPrincipals)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFile) {
  TempFile tempScript;
  static const char script_filename[] = "temp-bug438633_JS_CompileFile";
  FILE* script_stream = tempScript.open(script_filename);
  CHECK(fputs(code, script_stream) != EOF);
  tempScript.close();

  JS::CompileOptions options(cx);
  options.setFileAndLine(script_filename, 1);

  JS::RootedScript script(cx,
                          JS::CompileUtf8Path(cx, options, script_filename));
  CHECK(script);

  tempScript.remove();
  return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFile)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFile_empty) {
  TempFile tempScript;
  static const char script_filename[] = "temp-bug438633_JS_CompileFile_empty";
  tempScript.open(script_filename);
  tempScript.close();

  JS::CompileOptions options(cx);
  options.setFileAndLine(script_filename, 1);

  JS::RootedScript script(cx,
                          JS::CompileUtf8Path(cx, options, script_filename));
  CHECK(script);

  tempScript.remove();
  return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFile_empty)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFileHandle) {
  TempFile tempScript;
  FILE* script_stream = tempScript.open("temp-bug438633_JS_CompileFileHandle");
  CHECK(fputs(code, script_stream) != EOF);
  CHECK(fseek(script_stream, 0, SEEK_SET) != EOF);

  JS::CompileOptions options(cx);
  options.setFileAndLine("temporary file", 1);

  JS::RootedScript script(
      cx, JS::CompileUtf8FileDontInflate(cx, options, script_stream));
  CHECK(script);

  return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFileHandle)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFileHandle_empty) {
  TempFile tempScript;
  FILE* script_stream =
      tempScript.open("temp-bug438633_JS_CompileFileHandle_empty");

  JS::CompileOptions options(cx);
  options.setFileAndLine("empty temporary file", 1);

  JS::RootedScript script(
      cx, JS::CompileUtf8FileDontInflate(cx, options, script_stream));
  CHECK(script);

  return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture, bug438633_JS_CompileFileHandle_empty)

BEGIN_FIXTURE_TEST(ScriptObjectFixture,
                   bug438633_JS_CompileFileHandleForPrincipals) {
  TempFile tempScript;
  FILE* script_stream =
      tempScript.open("temp-bug438633_JS_CompileFileHandleForPrincipals");
  CHECK(fputs(code, script_stream) != EOF);
  CHECK(fseek(script_stream, 0, SEEK_SET) != EOF);

  JS::CompileOptions options(cx);
  options.setFileAndLine("temporary file", 1);

  JS::RootedScript script(
      cx, JS::CompileUtf8FileDontInflate(cx, options, script_stream));
  CHECK(script);

  return tryScript(script);
}
END_FIXTURE_TEST(ScriptObjectFixture,
                 bug438633_JS_CompileFileHandleForPrincipals)

BEGIN_FIXTURE_TEST(ScriptObjectFixture, CloneAndExecuteScript) {
  JS::RootedValue fortyTwo(cx);
  fortyTwo.setInt32(42);
  CHECK(JS_SetProperty(cx, global, "val", fortyTwo));

  JS::CompileOptions options(cx);
  options.setFileAndLine(__FILE__, __LINE__);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  CHECK(srcBuf.init(cx, "val", 3, JS::SourceOwnership::Borrowed));

  JS::RootedScript script(cx, JS::CompileDontInflate(cx, options, srcBuf));
  CHECK(script);

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
