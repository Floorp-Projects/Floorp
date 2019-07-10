/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JavaScript API. */

#ifndef js_ContextOptions_h
#define js_ContextOptions_h

#include "jstypes.h"  // JS_PUBLIC_API

struct JSContext;

namespace JS {

class JS_PUBLIC_API ContextOptions {
 public:
  ContextOptions()
      : asmJS_(true),
        wasm_(true),
        wasmVerbose_(false),
        wasmBaseline_(true),
        wasmIon_(true),
#ifdef ENABLE_WASM_CRANELIFT
        wasmCranelift_(false),
#endif
#ifdef ENABLE_WASM_GC
        wasmGc_(false),
#endif
        testWasmAwaitTier2_(false),
        throwOnAsmJSValidationFailure_(false),
        asyncStack_(true),
        throwOnDebuggeeWouldRun_(true),
        dumpStackOnDebuggeeWouldRun_(false),
        werror_(false),
        strictMode_(false),
        extraWarnings_(false)
#ifdef FUZZING
        ,
        fuzzing_(false)
#endif
  {
  }

  bool asmJS() const { return asmJS_; }
  ContextOptions& setAsmJS(bool flag) {
    asmJS_ = flag;
    return *this;
  }
  ContextOptions& toggleAsmJS() {
    asmJS_ = !asmJS_;
    return *this;
  }

  bool wasm() const { return wasm_; }
  ContextOptions& setWasm(bool flag) {
    wasm_ = flag;
    return *this;
  }
  ContextOptions& toggleWasm() {
    wasm_ = !wasm_;
    return *this;
  }

  bool wasmVerbose() const { return wasmVerbose_; }
  ContextOptions& setWasmVerbose(bool flag) {
    wasmVerbose_ = flag;
    return *this;
  }

  bool wasmBaseline() const { return wasmBaseline_; }
  ContextOptions& setWasmBaseline(bool flag) {
    wasmBaseline_ = flag;
    return *this;
  }

  bool wasmIon() const { return wasmIon_; }
  ContextOptions& setWasmIon(bool flag) {
    wasmIon_ = flag;
    return *this;
  }

#ifdef ENABLE_WASM_CRANELIFT
  bool wasmCranelift() const { return wasmCranelift_; }
  ContextOptions& setWasmCranelift(bool flag) {
    wasmCranelift_ = flag;
    return *this;
  }
#endif

  bool testWasmAwaitTier2() const { return testWasmAwaitTier2_; }
  ContextOptions& setTestWasmAwaitTier2(bool flag) {
    testWasmAwaitTier2_ = flag;
    return *this;
  }

#ifdef ENABLE_WASM_GC
  bool wasmGc() const { return wasmGc_; }
  ContextOptions& setWasmGc(bool flag) {
    wasmGc_ = flag;
    return *this;
  }
#endif

  bool throwOnAsmJSValidationFailure() const {
    return throwOnAsmJSValidationFailure_;
  }
  ContextOptions& setThrowOnAsmJSValidationFailure(bool flag) {
    throwOnAsmJSValidationFailure_ = flag;
    return *this;
  }
  ContextOptions& toggleThrowOnAsmJSValidationFailure() {
    throwOnAsmJSValidationFailure_ = !throwOnAsmJSValidationFailure_;
    return *this;
  }

  bool asyncStack() const { return asyncStack_; }
  ContextOptions& setAsyncStack(bool flag) {
    asyncStack_ = flag;
    return *this;
  }

  bool throwOnDebuggeeWouldRun() const { return throwOnDebuggeeWouldRun_; }
  ContextOptions& setThrowOnDebuggeeWouldRun(bool flag) {
    throwOnDebuggeeWouldRun_ = flag;
    return *this;
  }

  bool dumpStackOnDebuggeeWouldRun() const {
    return dumpStackOnDebuggeeWouldRun_;
  }
  ContextOptions& setDumpStackOnDebuggeeWouldRun(bool flag) {
    dumpStackOnDebuggeeWouldRun_ = flag;
    return *this;
  }

  bool werror() const { return werror_; }
  ContextOptions& setWerror(bool flag) {
    werror_ = flag;
    return *this;
  }
  ContextOptions& toggleWerror() {
    werror_ = !werror_;
    return *this;
  }

  bool strictMode() const { return strictMode_; }
  ContextOptions& setStrictMode(bool flag) {
    strictMode_ = flag;
    return *this;
  }
  ContextOptions& toggleStrictMode() {
    strictMode_ = !strictMode_;
    return *this;
  }

  bool extraWarnings() const { return extraWarnings_; }
  ContextOptions& setExtraWarnings(bool flag) {
    extraWarnings_ = flag;
    return *this;
  }
  ContextOptions& toggleExtraWarnings() {
    extraWarnings_ = !extraWarnings_;
    return *this;
  }

#ifdef FUZZING
  bool fuzzing() const { return fuzzing_; }
  ContextOptions& setFuzzing(bool flag) {
    fuzzing_ = flag;
    return *this;
  }
#endif

  void disableOptionsForSafeMode() {
    setAsmJS(false);
    setWasm(false);
    setWasmBaseline(false);
    setWasmIon(false);
#ifdef ENABLE_WASM_GC
    setWasmGc(false);
#endif
  }

 private:
  bool asmJS_ : 1;
  bool wasm_ : 1;
  bool wasmVerbose_ : 1;
  bool wasmBaseline_ : 1;
  bool wasmIon_ : 1;
#ifdef ENABLE_WASM_CRANELIFT
  bool wasmCranelift_ : 1;
#endif
#ifdef ENABLE_WASM_GC
  bool wasmGc_ : 1;
#endif
  bool testWasmAwaitTier2_ : 1;
  bool throwOnAsmJSValidationFailure_ : 1;
  bool asyncStack_ : 1;
  bool throwOnDebuggeeWouldRun_ : 1;
  bool dumpStackOnDebuggeeWouldRun_ : 1;
  bool werror_ : 1;
  bool strictMode_ : 1;
  bool extraWarnings_ : 1;
#ifdef FUZZING
  bool fuzzing_ : 1;
#endif
};

JS_PUBLIC_API ContextOptions& ContextOptionsRef(JSContext* cx);

}  // namespace JS

#endif  // js_ContextOptions_h
