/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcrtfuzzing/xpcrtfuzzing.h"

#include "mozilla/Assertions.h"  // MOZ_CRASH
#include "mozilla/Utf8.h"        // mozilla::Utf8Unit

#include <stdio.h>  // fflush, fprintf, fputs

#include "FuzzerDefs.h"
#include "FuzzingInterface.h"
#include "jsapi.h"  // JS_ClearPendingException, JS_IsExceptionPending, JS_SetProperty

#include "js/CompilationAndEvaluation.h"  // JS::Evaluate
#include "js/CompileOptions.h"            // JS::CompileOptions
#include "js/Conversions.h"               // JS::Conversions
#include "js/ErrorReport.h"               // JS::PrintError
#include "js/Exception.h"                 // JS::StealPendingExceptionStack
#include "js/experimental/TypedData.h"  // JS_GetUint8ClampedArrayData, JS_NewUint8ClampedArray
#include "js/RootingAPI.h"  // JS::Rooted
#include "js/SourceText.h"  // JS::Source{Ownership,Text}
#include "js/Value.h"       // JS::Value

using mozilla::dom::AutoJSAPI;

static AutoJSAPI* gJsapi = nullptr;
static std::string gFuzzModuleName;

static void CrashOnPendingException() {
  if (gJsapi->HasException()) {
    gJsapi->ReportException();

    MOZ_CRASH("Unhandled exception from JS runtime!");
  }
}

int FuzzXPCRuntimeStart(AutoJSAPI* jsapi, int* argc, char*** argv) {
  gFuzzModuleName = getenv("FUZZER");
  gJsapi = jsapi;

  int ret = FuzzXPCRuntimeInit();
  if (ret) {
    fprintf(stderr, "Fuzzing Interface: Error: Initialize callback failed\n");
    return ret;
  }

#ifdef LIBFUZZER
  return fuzzer::FuzzerDriver(argc, argv, FuzzXPCRuntimeFuzz);
#elif __AFL_COMPILER
  MOZ_CRASH("AFL is unsupported for XPC runtime fuzzing integration");
#endif
}

int FuzzXPCRuntimeInit() {
  JSContext* cx = gJsapi->cx();
  JS::Rooted<JS::Value> v(cx);
  JS::CompileOptions opts(cx);

  // Load the fuzzing module specified in the FUZZER environment variable
  JS::EvaluateUtf8Path(cx, opts, gFuzzModuleName.c_str(), &v);

  // Any errors while loading the fuzzing module should be fatal
  CrashOnPendingException();

  return 0;
}

int FuzzXPCRuntimeFuzz(const uint8_t* buf, size_t size) {
  if (!size) {
    return 0;
  }

  JSContext* cx = gJsapi->cx();

  JS::Rooted<JSObject*> arr(cx, JS_NewUint8ClampedArray(cx, size));
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

  JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
  JS::RootedValue arrVal(cx, JS::ObjectValue(*arr));
  if (!JS_SetProperty(cx, global, "fuzzBuf", arrVal)) {
    MOZ_CRASH("JS_SetProperty failed");
  }

  JS::Rooted<JS::Value> v(cx);
  JS::CompileOptions opts(cx);

  static const char data[] = "JSFuzzIterate();";

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, data, strlen(data), JS::SourceOwnership::Borrowed)) {
    return 1;
  }

  if (!JS::Evaluate(cx, opts.setFileAndLine(__FILE__, __LINE__), srcBuf, &v) &&
      !JS_IsExceptionPending(cx)) {
    // A return value of `false` without a pending exception indicates
    // a timeout as triggered by the `timeout` shell function.
    return 1;
  }

  // The fuzzing module is required to handle any exceptions
  CrashOnPendingException();

  int32_t ret = 0;
  if (!ToInt32(cx, v, &ret)) {
    MOZ_CRASH("Must return an int32 compatible return value!");
  }

  return ret;
}
