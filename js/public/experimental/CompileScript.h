/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JavaScript API for compiling scripts to stencil without depending on
 * JSContext. */

#ifndef js_experimental_CompileScript_h
#define js_experimental_CompileScript_h

#include "jspubtd.h"
#include "js/ErrorReport.h"  // JSErrorReport
#include "js/experimental/JSStencil.h"
#include "js/GCAnnotations.h"
#include "js/Modules.h"
#include "js/Stack.h"
#include "js/UniquePtr.h"

namespace js {
class FrontendContext;
namespace frontend {
struct CompilationInput;
}  // namespace frontend
}  // namespace js

namespace JS {
using FrontendContext = js::FrontendContext;

// Create a new front-end context.
JS_PUBLIC_API JS::FrontendContext* NewFrontendContext();

// Destroy a front-end context allocated with NewFrontendContext.
JS_PUBLIC_API void DestroyFrontendContext(JS::FrontendContext* fc);

// Set the size of the native stack that should not be exceed. To disable
// stack size checking pass 0.
//
// WARNING: When the stack size checking is enabled, the JS::FrontendContext
// can be used only in the thread where JS::SetNativeStackQuota is called.
JS_PUBLIC_API void SetNativeStackQuota(JS::FrontendContext* fc,
                                       JS::NativeStackSize stackSize);

// Return the stack quota that can be passed to SetNativeStackQuota, for given
// stack size.
// This subtracts a margin from given stack size, to make sure the stack quota
// check performed internally is sufficient.
JS_PUBLIC_API JS::NativeStackSize ThreadStackQuotaForSize(size_t stackSize);

// Returns true if there was any error reported to given FrontendContext.
JS_PUBLIC_API bool HadFrontendErrors(JS::FrontendContext* fc);

// Convert the error reported to FrontendContext into runtime error in
// JSContext.  Returns false if the error cannot be converted (such as due to
// OOM). An error might still be reported to the given JSContext. Also, returns
// false when OOM is converted. Returns true otherwise.
//
// The options parameter isn't actually used, but the CompileOptions
// provided to the compile/decode operation owns the filename pointer
// that the error and warnings reported to FrontendContext point to,
// so the CompileOptions must be alive until this call.
JS_PUBLIC_API bool ConvertFrontendErrorsToRuntimeErrors(
    JSContext* cx, JS::FrontendContext* fc,
    const JS::ReadOnlyCompileOptions& options);

// Returns an error report if given JS::FrontendContext had error and it has
// an error report associated.
//
// This can be nullptr even if JS::HadFrontendErrors returned true, if
// the error is one of:
//   * over recursed
//   * out of memory
//   * allocation overflow
//
// The returned pointer is valid only while the given JS::FrontendContext is
// alive.
//
// See ConvertFrontendErrorsToRuntimeErrors for options parameter.
JS_PUBLIC_API const JSErrorReport* GetFrontendErrorReport(
    JS::FrontendContext* fc, const JS::ReadOnlyCompileOptions& options);

// Returns true if the JS::FrontendContext had over recuresed error.
JS_PUBLIC_API bool HadFrontendOverRecursed(JS::FrontendContext* fc);

// Returns true if the JS::FrontendContext had out of memory error.
JS_PUBLIC_API bool HadFrontendOutOfMemory(JS::FrontendContext* fc);

// Returns true if the JS::FrontendContext had allocation overflow error.
JS_PUBLIC_API bool HadFrontendAllocationOverflow(JS::FrontendContext* fc);

// Clear errors reported to the JS::FrontendContext.
// No-op when there's no errors.
JS_PUBLIC_API void ClearFrontendErrors(JS::FrontendContext* fc);

// Returns the number of warnings reported to the JS::FrontendContext.
JS_PUBLIC_API size_t GetFrontendWarningCount(JS::FrontendContext* fc);

// Returns an error report represents the index-th warning.
//
// The returned pointer is valid only while the JS::FrontendContext is alive.
//
// See ConvertFrontendErrorsToRuntimeErrors for options parameter.
JS_PUBLIC_API const JSErrorReport* GetFrontendWarningAt(
    JS::FrontendContext* fc, size_t index,
    const JS::ReadOnlyCompileOptions& options);

/*
 * Set supported import assertions on a FrontendContext to be used with
 * CompileModuleScriptToStencil. May only be set once for each FrontendContext.
 * The default list of supported import assertions is empty.
 */
JS_PUBLIC_API bool SetSupportedImportAssertions(
    JS::FrontendContext* fc,
    const JS::ImportAssertionVector& supportedImportAssertions);

// Temporary storage used during compiling and preparing to instantiate a
// Stencil.
//
// Off-thread consumers can allocate this instance off main thread, and pass it
// back to the main thread, in order to reduce the main thread allocation.
struct CompilationStorage {
 private:
  // Owned CompilationInput.
  //
  // This uses raw pointer instead of UniquePtr because CompilationInput
  // is opaque.
  JS_HAZ_NON_GC_POINTER js::frontend::CompilationInput* input_ = nullptr;
  bool isBorrowed_ = false;

 public:
  CompilationStorage() = default;
  explicit CompilationStorage(js::frontend::CompilationInput* input)
      : input_(input), isBorrowed_(true) {}
  CompilationStorage(CompilationStorage&& other)
      : input_(other.input_), isBorrowed_(other.isBorrowed_) {
    other.input_ = nullptr;
  }

  ~CompilationStorage();

 private:
  CompilationStorage(const CompilationStorage& other) = delete;
  void operator=(const CompilationStorage& aOther) = delete;

 public:
  bool hasInput() { return !!input_; }

  // Internal function that initializes the CompilationInput. It should only be
  // called once.
  bool allocateInput(FrontendContext* fc,
                     const JS::ReadOnlyCompileOptions& options);

  js::frontend::CompilationInput& getInput() {
    MOZ_ASSERT(hasInput());
    return *input_;
  }

  // Size of dynamic data. Note that GC data is counted by GC and not here.
  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  void trace(JSTracer* trc);
};

extern JS_PUBLIC_API already_AddRefed<JS::Stencil> CompileGlobalScriptToStencil(
    JS::FrontendContext* fc, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<mozilla::Utf8Unit>& srcBuf,
    JS::CompilationStorage& compileStorage);

extern JS_PUBLIC_API already_AddRefed<JS::Stencil> CompileGlobalScriptToStencil(
    JS::FrontendContext* fc, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, JS::CompilationStorage& compileStorage);

extern JS_PUBLIC_API already_AddRefed<JS::Stencil> CompileModuleScriptToStencil(
    JS::FrontendContext* fc, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<mozilla::Utf8Unit>& srcBuf,
    JS::CompilationStorage& compileStorage);

extern JS_PUBLIC_API already_AddRefed<JS::Stencil> CompileModuleScriptToStencil(
    JS::FrontendContext* fc, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, JS::CompilationStorage& compileStorage);

extern JS_PUBLIC_API bool PrepareForInstantiate(
    JS::FrontendContext* fc, JS::Stencil& stencil,
    JS::InstantiationStorage& storage);

}  // namespace JS

#endif  // js_experimental_CompileScript_h
