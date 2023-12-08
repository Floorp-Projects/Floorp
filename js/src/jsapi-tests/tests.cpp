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

JSAPIRuntimeTest* JSAPIRuntimeTest::list;
JSAPIFrontendTest* JSAPIFrontendTest::list;

bool JSAPIRuntimeTest::init(JSContext* maybeReusableContext) {
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

JSContext* JSAPIRuntimeTest::maybeForgetContext() {
  if (!reuseGlobal) {
    return nullptr;
  }

  JSContext* reusableCx = cx;
  global.reset();
  cx = nullptr;
  return reusableCx;
}

/* static */
void JSAPIRuntimeTest::MaybeFreeContext(JSContext* maybeCx) {
  if (maybeCx) {
    JS::LeaveRealm(maybeCx, nullptr);
    JS_DestroyContext(maybeCx);
  }
}

void JSAPIRuntimeTest::uninit() {
  global.reset();
  MaybeFreeContext(cx);
  cx = nullptr;
  msgs.clear();
}

bool JSAPIRuntimeTest::exec(const char* utf8, const char* filename,
                            int lineno) {
  JS::CompileOptions opts(cx);
  opts.setFileAndLine(filename, lineno);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  JS::RootedValue v(cx);
  return (srcBuf.init(cx, utf8, strlen(utf8), JS::SourceOwnership::Borrowed) &&
          JS::Evaluate(cx, opts, srcBuf, &v)) ||
         fail(JSAPITestString(utf8), filename, lineno);
}

bool JSAPIRuntimeTest::execDontReport(const char* utf8, const char* filename,
                                      int lineno) {
  JS::CompileOptions opts(cx);
  opts.setFileAndLine(filename, lineno);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  JS::RootedValue v(cx);
  return srcBuf.init(cx, utf8, strlen(utf8), JS::SourceOwnership::Borrowed) &&
         JS::Evaluate(cx, opts, srcBuf, &v);
}

bool JSAPIRuntimeTest::evaluate(const char* utf8, const char* filename,
                                int lineno, JS::MutableHandleValue vp) {
  JS::CompileOptions opts(cx);
  opts.setFileAndLine(filename, lineno);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  return (srcBuf.init(cx, utf8, strlen(utf8), JS::SourceOwnership::Borrowed) &&
          JS::Evaluate(cx, opts, srcBuf, vp)) ||
         fail(JSAPITestString(utf8), filename, lineno);
}

bool JSAPIRuntimeTest::definePrint() {
  return JS_DefineFunction(cx, global, "print", (JSNative)print, 0, 0);
}

JSObject* JSAPIRuntimeTest::createGlobal(JSPrincipals* principals) {
  /* Create the global object. */
  JS::RootedObject newGlobal(cx);
  JS::RealmOptions options;
  options.creationOptions()
      .setWeakRefsEnabled(JS::WeakRefSpecifier::EnabledWithCleanupSome)
      .setSharedMemoryAndAtomicsEnabled(true)
#ifdef NIGHTLY_BUILD
      .setSymbolsAsWeakMapKeysEnabled(true)
#endif
      ;
  newGlobal = JS_NewGlobalObject(cx, getGlobalClass(), principals,
                                 JS::FireOnNewGlobalHook, options);
  if (!newGlobal) {
    return nullptr;
  }

  global = newGlobal;
  return newGlobal;
}

struct CommandOptions {
  bool list = false;
  bool frontendOnly = false;
  bool help = false;
  const char* filter = nullptr;
};

void parseArgs(int argc, char* argv[], CommandOptions& options) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      options.help = true;
      continue;
    }

    if (strcmp(argv[i], "--list") == 0) {
      options.list = true;
      continue;
    }

    if (strcmp(argv[i], "--frontend-only") == 0) {
      options.frontendOnly = true;
      continue;
    }

    if (!options.filter) {
      options.filter = argv[i];
      continue;
    }

    printf("error: Unrecognized option: %s\n", argv[i]);
    options.help = true;
  }
}

template <typename TestT>
void PrintTests(TestT* list) {
  for (TestT* test = list; test; test = test->next) {
    printf("%s\n", test->name());
  }
}

template <typename TestT, typename InitF, typename RunF, typename BeforeUninitF>
void RunTests(int& total, int& failures, CommandOptions& options, TestT* list,
              InitF init, RunF run, BeforeUninitF beforeUninit) {
  for (TestT* test = list; test; test = test->next) {
    const char* name = test->name();
    if (options.filter && strstr(name, options.filter) == nullptr) {
      continue;
    }

    total += 1;

    printf("%s\n", name);

    // Make sure the test name is printed before we enter the test that can
    // crash on failure.
    fflush(stdout);

    if (!init(test)) {
      printf("TEST-UNEXPECTED-FAIL | %s | Failed to initialize.\n", name);
      failures++;
      test->uninit();
      continue;
    }

    if (run(test)) {
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

    beforeUninit(test);

    test->uninit();
  }
}

int main(int argc, char* argv[]) {
  int total = 0;
  int failures = 0;
  CommandOptions options;
  parseArgs(argc, argv, options);

  if (options.help) {
    printf("Usage: jsapi-tests [OPTIONS] [FILTER]\n");
    printf("\n");
    printf("Options:\n");
    printf("    -h, --help          Display this message\n");
    printf("        --list          List all tests\n");
    printf(
        "        --frontend-only Run tests for frontend-only APIs, with "
        "light-weight entry point\n");
    return 0;
  }

  if (!options.frontendOnly) {
    if (!JS_Init()) {
      printf("TEST-UNEXPECTED-FAIL | jsapi-tests | JS_Init() failed.\n");
      return 1;
    }
  } else {
    if (!JS_FrontendOnlyInit()) {
      printf("TEST-UNEXPECTED-FAIL | jsapi-tests | JS_Init() failed.\n");
      return 1;
    }
  }

  if (options.list) {
    PrintTests(JSAPIRuntimeTest::list);
    PrintTests(JSAPIFrontendTest::list);
    return 0;
  }

  // Reinitializing the global for every test is quite slow, due to having to
  // recompile all self-hosted builtins. Allow tests to opt-in to reusing the
  // global.
  JSContext* maybeReusedContext = nullptr;

  if (!options.frontendOnly) {
    RunTests(
        total, failures, options, JSAPIRuntimeTest::list,
        [&maybeReusedContext](JSAPIRuntimeTest* test) {
          return test->init(maybeReusedContext);
        },
        [](JSAPIRuntimeTest* test) { return test->run(test->global); },
        [&maybeReusedContext](JSAPIRuntimeTest* test) {
          // Return a non-nullptr pointer if the context & global can safely be
          // reused for the next test.
          maybeReusedContext = test->maybeForgetContext();
        });
  }
  RunTests(
      total, failures, options, JSAPIFrontendTest::list,
      [](JSAPIFrontendTest* test) { return test->init(); },
      [](JSAPIFrontendTest* test) { return test->run(); },
      [](JSAPIFrontendTest* test) {});

  if (!options.frontendOnly) {
    JSAPIRuntimeTest::MaybeFreeContext(maybeReusedContext);

    MOZ_RELEASE_ASSERT(!JSRuntime::hasLiveRuntimes());
    JS_ShutDown();
  } else {
    JS_FrontendOnlyShutDown();
  }

  if (failures) {
    printf("\n%d unexpected failure%s.\n", failures,
           (failures == 1 ? "" : "s"));
    return 1;
  }
  printf("\nPassed: ran %d tests.\n", total);
  return 0;
}
