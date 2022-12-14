/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ErrorContext_h
#define vm_ErrorContext_h

#include "mozilla/Maybe.h"  // mozilla::Maybe

#include "js/ErrorReport.h"
#include "vm/ErrorReporting.h"
#include "vm/MallocProvider.h"

namespace js {

class FrontendContext;

struct FrontendErrors {
  FrontendErrors() = default;
  // Any errors or warnings produced during compilation. These are reported
  // when finishing the script.
  mozilla::Maybe<CompileError> error;
  Vector<CompileError, 0, SystemAllocPolicy> warnings;
  bool overRecursed = false;
  bool outOfMemory = false;
  bool allocationOverflow = false;

  bool hadErrors() const {
    return outOfMemory || overRecursed || allocationOverflow || error;
  }
};

class FrontendAllocator : public MallocProvider<FrontendAllocator> {
 private:
  FrontendContext* const context_;

 public:
  explicit FrontendAllocator(FrontendContext* fc) : context_(fc) {}

  void* onOutOfMemory(js::AllocFunction allocFunc, arena_id_t arena,
                      size_t nbytes, void* reallocPtr = nullptr);
  void reportAllocationOverflow();
};

class FrontendContext {
 private:
  FrontendAllocator alloc_;
  js::FrontendErrors errors_;

 protected:
  // (optional) Current JSContext to support main-thread-specific
  // handling for error reporting, GC, and memory allocation.
  //
  // Set by setCurrentJSContext.
  JSContext* maybeCx_ = nullptr;

 public:
  FrontendContext() : alloc_(this) {}
  ~FrontendContext() = default;

  FrontendAllocator* getAllocator() { return &alloc_; }

  void setCurrentJSContext(JSContext* cx);

  enum class Warning { Suppress, Report };

  void convertToRuntimeError(JSContext* cx, Warning warning = Warning::Report);

  void linkWithJSContext(JSContext* cx);

  mozilla::Maybe<CompileError>& maybeError() { return errors_.error; }
  Vector<CompileError, 0, SystemAllocPolicy>& warnings() {
    return errors_.warnings;
  }

  // Report CompileErrors
  void reportError(js::CompileError&& err);
  bool reportWarning(js::CompileError&& err);

  // Report FrontendAllocator errors
  void* onOutOfMemory(js::AllocFunction allocFunc, arena_id_t arena,
                      size_t nbytes, void* reallocPtr = nullptr);
  void onAllocationOverflow();

  void onOutOfMemory();
  void onOverRecursed();

  void recoverFromOutOfMemory();

  const JSErrorFormatString* gcSafeCallback(JSErrorCallback callback,
                                            void* userRef,
                                            const unsigned errorNumber);

  // Status of errors reported to this FrontendContext
  bool hadOutOfMemory() const { return errors_.outOfMemory; }
  bool hadOverRecursed() const { return errors_.overRecursed; }
  bool hadAllocationOverflow() const { return errors_.allocationOverflow; }
  bool hadErrors() const;

#ifdef __wasi__
  void incWasiRecursionDepth();
  void decWasiRecursionDepth();
  bool checkWasiRecursionLimit();
#endif  // __wasi__

 private:
  void ReportOutOfMemory();
  void addPendingOutOfMemory();
};

// Automatically report any pending exception when leaving the scope.
class MOZ_STACK_CLASS AutoReportFrontendContext : public FrontendContext {
  // The target JSContext to report the errors to.
  JSContext* cx_;

  Warning warning_;

 public:
  explicit AutoReportFrontendContext(JSContext* cx,
                                     Warning warning = Warning::Report)
      : FrontendContext(), cx_(cx), warning_(warning) {
    setCurrentJSContext(cx_);
    MOZ_ASSERT(cx_ == maybeCx_);
  }

  ~AutoReportFrontendContext() {
    if (cx_) {
      convertToRuntimeErrorAndClear();
    }
  }

  void clearAutoReport() { cx_ = nullptr; }

  void convertToRuntimeErrorAndClear() {
    convertToRuntimeError(cx_, warning_);
    cx_ = nullptr;
  }
};

/*
 * Explicitly report any pending exception before leaving the scope.
 *
 * Before an instance of this class leaves the scope, you must call either
 * failure() (if there are exceptions to report) or ok() (if there are no
 * exceptions to report).
 */
class ManualReportFrontendContext : public FrontendContext {
  JSContext* cx_;
#ifdef DEBUG
  bool handled_ = false;
#endif

 public:
  explicit ManualReportFrontendContext(JSContext* cx)
      : FrontendContext(), cx_(cx) {
    setCurrentJSContext(cx_);
  }

  ~ManualReportFrontendContext() { MOZ_ASSERT(handled_); }

  void ok() {
#ifdef DEBUG
    handled_ = true;
#endif
  }

  void failure() {
#ifdef DEBUG
    handled_ = true;
#endif
    convertToRuntimeError(cx_);
  }
};

}  // namespace js

#endif /* vm_ErrorContext_h */
