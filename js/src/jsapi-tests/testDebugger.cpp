/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/CallAndConstruct.h"
#include "js/GlobalObject.h"        // JS_NewGlobalObject
#include "js/PropertyAndElement.h"  // JS_SetProperty
#include "jsapi-tests/tests.h"
#include "vm/JSContext.h"

using namespace js;

BEGIN_TEST(testDebugger_newScriptHook) {
  // Test that top-level indirect eval fires the newScript hook.
  CHECK(JS_DefineDebuggerObject(cx, global));
  JS::RealmOptions options;
  JS::RootedObject g(cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                                            JS::FireOnNewGlobalHook, options));
  CHECK(g);

  JS::RootedObject gWrapper(cx, g);
  CHECK(JS_WrapObject(cx, &gWrapper));
  JS::RootedValue v(cx, JS::ObjectValue(*gWrapper));
  CHECK(JS_SetProperty(cx, global, "g", v));

  EXEC(
      "var dbg = Debugger(g);\n"
      "var hits = 0;\n"
      "dbg.onNewScript = function (s) {\n"
      "    hits += Number(s instanceof Debugger.Script);\n"
      "};\n");

  // Since g is a debuggee, g.eval should trigger newScript, regardless of
  // what scope object we use to enter the compartment.
  //
  // Scripts are associated with the global where they're compiled, so we
  // deliver them only to debuggers that are watching that particular global.
  //
  return testIndirectEval(g, "Math.abs(0)");
}

bool testIndirectEval(JS::HandleObject global, const char* code) {
  EXEC("hits = 0;");

  {
    JSAutoRealm ar(cx, global);
    JSString* codestr = JS_NewStringCopyZ(cx, code);
    CHECK(codestr);
    JS::RootedValue arg(cx, JS::StringValue(codestr));
    JS::RootedValue v(cx);
    CHECK(JS_CallFunctionName(cx, global, "eval", HandleValueArray(arg), &v));
  }

  JS::RootedValue hitsv(cx);
  EVAL("hits", &hitsv);
  CHECK(hitsv.isInt32(1));
  return true;
}
END_TEST(testDebugger_newScriptHook)
