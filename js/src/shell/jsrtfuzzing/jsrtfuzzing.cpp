/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "shell/jsrtfuzzing/jsrtfuzzing.h"

#include "mozilla/Assertions.h"  // MOZ_CRASH
#include "mozilla/Utf8.h"        // mozilla::Utf8Unit

#include <stdio.h>  // fflush, fprintf, fputs

#include "FuzzerDefs.h"
#include "FuzzingInterface.h"
#include "jsapi.h"  // JS_ClearPendingException, JS_IsExceptionPending

#include "js/CompilationAndEvaluation.h"  // JS::Evaluate
#include "js/CompileOptions.h"            // JS::CompileOptions
#include "js/Conversions.h"               // JS::ToInt32
#include "js/ErrorReport.h"               // JS::PrintError
#include "js/Exception.h"                 // JS::StealPendingExceptionStack
#include "js/experimental/TypedData.h"  // JS_GetUint8ClampedArrayData, JS_NewUint8ClampedArray
#include "js/PropertyAndElement.h"  // JS_SetProperty
#include "js/RootingAPI.h"          // JS::Rooted
#include "js/SourceText.h"          // JS::Source{Ownership,Text}
#include "js/Value.h"               // JS::Value
#include "shell/jsshell.h"  // js::shell::{reportWarnings,PrintStackTrace,sArg{c,v}}
#include "util/Text.h"
#include "vm/Interpreter.h"
#include "vm/TypedArrayObject.h"

#include "vm/ArrayBufferObject-inl.h"
#include "vm/JSContext-inl.h"

static JSContext* gCx = nullptr;
static std::string gFuzzModuleName;

static void CrashOnPendingException() {
  if (JS_IsExceptionPending(gCx)) {
    JS::ExceptionStack exnStack(gCx);
    (void)JS::StealPendingExceptionStack(gCx, &exnStack);

    JS::ErrorReportBuilder report(gCx);
    if (!report.init(gCx, exnStack, JS::ErrorReportBuilder::WithSideEffects)) {
      fprintf(stderr, "out of memory initializing JS::ErrorReportBuilder\n");
      fflush(stderr);
    } else {
      JS::PrintError(stderr, report, js::shell::reportWarnings);
      if (!js::shell::PrintStackTrace(gCx, exnStack.stack())) {
        fputs("(Unable to print stack trace)\n", stderr);
      }
    }

    MOZ_CRASH("Unhandled exception from JS runtime!");
  }
}

int js::shell::FuzzJSRuntimeStart(JSContext* cx, int* argc, char*** argv) {
  gCx = cx;
  gFuzzModuleName = getenv("FUZZER");

  int ret = FuzzJSRuntimeInit(argc, argv);
  if (ret) {
    fprintf(stderr, "Fuzzing Interface: Error: Initialize callback failed\n");
    return ret;
  }

#ifdef LIBFUZZER
  fuzzer::FuzzerDriver(&shell::sArgc, &shell::sArgv, FuzzJSRuntimeFuzz);
#elif AFLFUZZ
  MOZ_CRASH("AFL is unsupported for JS runtime fuzzing integration");
#endif
  return 0;
}

int js::shell::FuzzJSRuntimeInit(int* argc, char*** argv) {
  JS::Rooted<JS::Value> v(gCx);
  JS::CompileOptions opts(gCx);

  // Load the fuzzing module specified in the FUZZER environment variable
  JS::EvaluateUtf8Path(gCx, opts, gFuzzModuleName.c_str(), &v);

  // Any errors while loading the fuzzing module should be fatal
  CrashOnPendingException();

  return 0;
}

int js::shell::FuzzJSRuntimeFuzz(const uint8_t* buf, size_t size) {
  if (!size) {
    return 0;
  }

  JS::Rooted<JSObject*> arr(gCx, JS_NewUint8ClampedArray(gCx, size));
  if (!arr) {
    MOZ_CRASH("OOM");
  }

  do {
    JS::AutoCheckCannotGC nogc;
    bool isShared;
    uint8_t* data = JS_GetUint8ClampedArrayData(arr, &isShared, nogc);
    MOZ_RELEASE_ASSERT(!isShared);
    memcpy(data, buf, size);
  } while (false);

  JS::RootedValue arrVal(gCx, JS::ObjectValue(*arr));
  if (!JS_SetProperty(gCx, gCx->global(), "fuzzBuf", arrVal)) {
    MOZ_CRASH("JS_SetProperty failed");
  }

  JS::Rooted<JS::Value> v(gCx);
  JS::CompileOptions opts(gCx);

  static const char data[] = "JSFuzzIterate();";

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  if (!srcBuf.init(gCx, data, js_strlen(data), JS::SourceOwnership::Borrowed)) {
    return 1;
  }

  if (!JS::Evaluate(gCx, opts.setFileAndLine(__FILE__, __LINE__), srcBuf, &v) &&
      !JS_IsExceptionPending(gCx)) {
    // A return value of `false` without a pending exception indicates
    // a timeout as triggered by the `timeout` shell function.
    return 1;
  }

  // The fuzzing module is required to handle any exceptions
  CrashOnPendingException();

  int32_t ret = 0;
  if (!JS::ToInt32(gCx, v, &ret)) {
    MOZ_CRASH("Must return an int32 compatible return value!");
  }

  return ret;
}
