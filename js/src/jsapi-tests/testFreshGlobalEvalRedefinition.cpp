/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"  // mozilla::ArrayLength
#include "mozilla/Utf8.h"        // mozilla::Utf8Unit

#include "js/CompilationAndEvaluation.h"  // JS::Evaluate
#include "js/SourceText.h"                // JS::Source{Ownership,Text}
#include "jsapi-tests/tests.h"

static bool GlobalResolve(JSContext* cx, JS::HandleObject obj, JS::HandleId id,
                          bool* resolvedp) {
  return JS_ResolveStandardClass(cx, obj, id, resolvedp);
}

BEGIN_TEST(testRedefineGlobalEval) {
  static const JSClassOps clsOps = {nullptr,
                                    nullptr,
                                    nullptr,
                                    JS_NewEnumerateStandardClasses,
                                    GlobalResolve,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    JS_GlobalObjectTraceHook};

  static const JSClass cls = {"global", JSCLASS_GLOBAL_FLAGS, &clsOps};

  /* Create the global object. */
  JS::RealmOptions options;
  JS::Rooted<JSObject*> g(
      cx,
      JS_NewGlobalObject(cx, &cls, nullptr, JS::FireOnNewGlobalHook, options));
  if (!g) {
    return false;
  }

  JSAutoRealm ar(cx, g);
  JS::Rooted<JS::Value> v(cx);
  CHECK(JS_GetProperty(cx, g, "Object", &v));

  static const char data[] =
      "Object.defineProperty(this, 'eval', { configurable: false });";

  JS::CompileOptions opts(cx);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  CHECK(srcBuf.init(cx, data, mozilla::ArrayLength(data) - 1,
                    JS::SourceOwnership::Borrowed));

  CHECK(JS::Evaluate(cx, opts.setFileAndLine(__FILE__, __LINE__), srcBuf, &v));

  return true;
}
END_TEST(testRedefineGlobalEval)
