/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include <stdio.h>

#include "js/CompilationAndEvaluation.h"  // JS::EvaluateDontInflate
#include "js/Initialization.h"
#include "js/RootingAPI.h"
#include "js/SourceText.h"  // JS::Source{Ownership,Text}

JSAPITest* JSAPITest::list;

bool JSAPITest::init() {
  cx = createContext();
  if (!cx) {
    return false;
  }
  js::UseInternalJobQueues(cx);
  if (!JS::InitSelfHostedCode(cx)) {
    return false;
  }
  global.init(cx);
  createGlobal();
  if (!global) {
    return false;
  }
  JS::EnterRealm(cx, global);
  return true;
}

void JSAPITest::uninit() {
  if (global) {
    JS::LeaveRealm(cx, nullptr);
    global = nullptr;
  }
  if (cx) {
    destroyContext();
    cx = nullptr;
  }
  msgs.clear();
}

bool JSAPITest::exec(const char* utf8, const char* filename, int lineno) {
  JS::CompileOptions opts(cx);
  opts.setFileAndLine(filename, lineno);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  JS::RootedValue v(cx);
  return (srcBuf.init(cx, utf8, strlen(utf8), JS::SourceOwnership::Borrowed) &&
          JS::EvaluateDontInflate(cx, opts, srcBuf, &v)) ||
         fail(JSAPITestString(utf8), filename, lineno);
}

bool JSAPITest::execDontReport(const char* utf8, const char* filename,
                               int lineno) {
  JS::CompileOptions opts(cx);
  opts.setFileAndLine(filename, lineno);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  JS::RootedValue v(cx);
  return srcBuf.init(cx, utf8, strlen(utf8), JS::SourceOwnership::Borrowed) &&
         JS::EvaluateDontInflate(cx, opts, srcBuf, &v);
}

bool JSAPITest::evaluate(const char* utf8, const char* filename, int lineno,
                         JS::MutableHandleValue vp) {
  JS::CompileOptions opts(cx);
  opts.setFileAndLine(filename, lineno);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  return (srcBuf.init(cx, utf8, strlen(utf8), JS::SourceOwnership::Borrowed) &&
          JS::EvaluateDontInflate(cx, opts, srcBuf, vp)) ||
         fail(JSAPITestString(utf8), filename, lineno);
}

bool JSAPITest::definePrint() {
  return JS_DefineFunction(cx, global, "print", (JSNative)print, 0, 0);
}

JSObject* JSAPITest::createGlobal(JSPrincipals* principals) {
  /* Create the global object. */
  JS::RootedObject newGlobal(cx);
  JS::RealmOptions options;
  options.creationOptions()
      .setStreamsEnabled(true)
      .setFieldsEnabled(true)
      .setAwaitFixEnabled(true);
  newGlobal = JS_NewGlobalObject(cx, getGlobalClass(), principals,
                                 JS::FireOnNewGlobalHook, options);
  if (!newGlobal) {
    return nullptr;
  }

  global = newGlobal;
  return newGlobal;
}

int main(int argc, char* argv[]) {
  int total = 0;
  int failures = 0;
  const char* filter = (argc == 2) ? argv[1] : nullptr;

  if (!JS_Init()) {
    printf("TEST-UNEXPECTED-FAIL | jsapi-tests | JS_Init() failed.\n");
    return 1;
  }

  for (JSAPITest* test = JSAPITest::list; test; test = test->next) {
    const char* name = test->name();
    if (filter && strstr(name, filter) == nullptr) {
      continue;
    }

    total += 1;

    printf("%s\n", name);
    if (!test->init()) {
      printf("TEST-UNEXPECTED-FAIL | %s | Failed to initialize.\n", name);
      failures++;
      test->uninit();
      continue;
    }

    if (test->run(test->global)) {
      printf("TEST-PASS | %s | ok\n", name);
    } else {
      JSAPITestString messages = test->messages();
      printf("%s | %s | %.*s\n",
             (test->knownFail ? "TEST-KNOWN-FAIL" : "TEST-UNEXPECTED-FAIL"),
             name, (int)messages.length(), messages.begin());
      if (!test->knownFail) {
        failures++;
      }
    }
    test->uninit();
  }

  MOZ_RELEASE_ASSERT(!JSRuntime::hasLiveRuntimes());
  JS_ShutDown();

  if (failures) {
    printf("\n%d unexpected failure%s.\n", failures,
           (failures == 1 ? "" : "s"));
    return 1;
  }
  printf("\nPassed: ran %d tests.\n", total);
  return 0;
}
