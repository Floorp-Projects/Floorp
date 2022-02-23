/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCPrefableContextOptions_h__
#define XPCPrefableContextOptions_h__

#include "js/ContextOptions.h"

namespace xpc {

// Worker supports overriding JS prefs with different prefix.
// Generate pref names for both of them by macro.
#define JS_OPTIONS_PREFIX "javascript.options."
#define WORKERS_OPTIONS_PREFIX "dom.workers.options."
#define PREF_NAMES(s) JS_OPTIONS_PREFIX s, WORKERS_OPTIONS_PREFIX s

// Load and set JS::ContextOptions flags shared between the main thread and
// workers.
//
// `load`'s signature is `bool (*)(const char* jsPref, const char* workerPref)`.
template <typename T>
void SetPrefableContextOptions(JS::ContextOptions& options, T load) {
  options
      .setAsmJS(load(PREF_NAMES("asmjs")))
#ifdef FUZZING
      .setFuzzing(load(PREF_NAMES("fuzzing.enabled")))
#endif
      .setWasm(load(PREF_NAMES("wasm")))
      .setWasmForTrustedPrinciples(load(PREF_NAMES("wasm_trustedprincipals")))
#ifdef ENABLE_WASM_CRANELIFT
      .setWasmCranelift(load(PREF_NAMES("wasm_optimizingjit")))
      .setWasmIon(false)
#else
      .setWasmCranelift(false)
      .setWasmIon(load(PREF_NAMES("wasm_optimizingjit")))
#endif
      .setWasmBaseline(load(PREF_NAMES("wasm_baselinejit")))
#define WASM_FEATURE(NAME, LOWER_NAME, COMPILE_PRED, COMPILER_PRED, FLAG_PRED, \
                     SHELL, PREF)                                              \
  .setWasm##NAME(load(PREF_NAMES("wasm_" PREF)))
          JS_FOR_WASM_FEATURES(WASM_FEATURE, WASM_FEATURE, WASM_FEATURE)
#undef WASM_FEATURE
#ifdef ENABLE_WASM_SIMD_WORMHOLE
#  ifdef EARLY_BETA_OR_EARLIER
      .setWasmSimdWormhole(load(PREF_NAMES("wasm_simd_wormhole")))
#  else
      .setWasmSimdWormhole(false)
#  endif
#endif
      .setWasmVerbose(load(PREF_NAMES("wasm_verbose")))
      .setThrowOnAsmJSValidationFailure(
          load(PREF_NAMES("throw_on_asmjs_validation_failure")))
      .setSourcePragmas(load(PREF_NAMES("source_pragmas")))
      .setAsyncStack(load(PREF_NAMES("asyncstack")))
      .setAsyncStackCaptureDebuggeeOnly(
          load(PREF_NAMES("asyncstack_capture_debuggee_only")))
      .setPrivateClassFields(load(PREF_NAMES("experimental.private_fields")))
      .setPrivateClassMethods(load(PREF_NAMES("experimental.private_methods")))
      .setClassStaticBlocks(
          load(PREF_NAMES("experimental.class_static_blocks")))
#ifdef ENABLE_CHANGE_ARRAY_BY_COPY
      .setChangeArrayByCopy(
          load(PREF_NAMES("experimental.enable_change_array_by_copy")))
#endif
#ifdef NIGHTLY_BUILD
      .setImportAssertions(load(PREF_NAMES("experimental.import_assertions")))
#endif
      .setErgnomicBrandChecks(
          load(PREF_NAMES("experimental.ergonomic_brand_checks")));
}

#undef PREF_NAMES
#undef WORKERS_OPTIONS_PREFIX
#undef JS_OPTIONS_PREFIX

}  // namespace xpc

#endif  // XPCPrefableContextOptions_h__
