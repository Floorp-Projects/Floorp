/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include <stdio.h>

#include "js/ArrayBuffer.h"
#include "js/CompilationAndEvaluation.h"  // JS::Evaluate
#include "js/GlobalObject.h"              // JS_NewGlobalObject
#include "js/Initialization.h"
#include "js/PropertyAndElement.h"  // JS_DefineFunction
#include "js/RootingAPI.h"
#include "js/SourceText.h"  // JS::Source{Ownership,Text}

JSAPITest* JSAPITest::list;

bool JSAPITest::init(JSContext* maybeReusableContext) {
  if (maybeReusableContext && reuseGlobal) {
    cx = maybeReusableContext;
    global.init(cx, JS::CurrentGlobalOrNull(cx));
    return init();
  }

  MaybeFreeContext(maybeReusableContext);

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
  return init();
}

JSContext* JSAPITest::maybeForgetContext() {
  if (!reuseGlobal) {
    return nullptr;
  }

  JSContext* reusableCx = cx;
  global.reset();
  cx = nullptr;
  return reusableCx;
}

/* static */
void JSAPITest::MaybeFreeContext(JSContext* maybeCx) {
  if (maybeCx) {
    JS::LeaveRealm(maybeCx, nullptr);
    JS_DestroyContext(maybeCx);
  }
}

void JSAPITest::uninit() {
  global.reset();
  MaybeFreeContext(cx);
  cx = nullptr;
  msgs.clear();
}

bool JSAPITest::exec(const char* utf8, const char* filename, int lineno) {
  JS::CompileOptions opts(cx);
  opts.setFileAndLine(filename, lineno);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  JS::RootedValue v(cx);
  return (srcBuf.init(cx, utf8, strlen(utf8), JS::SourceOwnership::Borrowed) &&
          JS::Evaluate(cx, opts, srcBuf, &v)) ||
         fail(JSAPITestString(utf8), filename, lineno);
}

bool JSAPITest::execDontReport(const char* utf8, const char* filename,
                               int lineno) {
  JS::CompileOptions opts(cx);
  opts.setFileAndLine(filename, lineno);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  JS::RootedValue v(cx);
  return srcBuf.init(cx, utf8, strlen(utf8), JS::SourceOwnership::Borrowed) &&
         JS::Evaluate(cx, opts, srcBuf, &v);
}

bool JSAPITest::evaluate(const char* utf8, const char* filename, int lineno,
                         JS::MutableHandleValue vp) {
  JS::CompileOptions opts(cx);
  opts.setFileAndLine(filename, lineno);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  return (srcBuf.init(cx, utf8, strlen(utf8), JS::SourceOwnership::Borrowed) &&
          JS::Evaluate(cx, opts, srcBuf, vp)) ||
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
      .setWeakRefsEnabled(JS::WeakRefSpecifier::EnabledWithCleanupSome)
      .setSharedMemoryAndAtomicsEnabled(true);
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

  if (filter && strcmp(filter, "--list") == 0) {
    for (JSAPITest* test = JSAPITest::list; test; test = test->next) {
      printf("%s\n", test->name());
    }
    return 0;
  }

  // Reinitializing the global for every test is quite slow, due to having to
  // recompile all self-hosted builtins. Allow tests to opt-in to reusing the
  // global.
  JSContext* maybeReusedContext = nullptr;
  for (JSAPITest* test = JSAPITest::list; test; test = test->next) {
    const char* name = test->name();
    if (filter && strstr(name, filter) == nullptr) {
      continue;
    }

    total += 1;

    printf("%s\n", name);
    if (!test->init(maybeReusedContext)) {
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

    // Return a non-nullptr pointer if the context & global can safely be
    // reused for the next test.
    maybeReusedContext = test->maybeForgetContext();
    test->uninit();
  }
  JSAPITest::MaybeFreeContext(maybeReusedContext);

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
