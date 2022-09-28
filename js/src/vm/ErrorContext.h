/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ErrorContext_h
#define vm_ErrorContext_h

#include "js/ErrorReport.h"
#include "vm/ErrorReporting.h"
#include "vm/MallocProvider.h"

namespace js {

class ErrorContext;

struct OffThreadFrontendErrors {
  OffThreadFrontendErrors() = default;
  // Any errors or warnings produced during compilation. These are reported
  // when finishing the script.
  Vector<UniquePtr<CompileError>, 0, SystemAllocPolicy> errors;
  Vector<UniquePtr<CompileError>, 0, SystemAllocPolicy> warnings;
  bool overRecursed = false;
  bool outOfMemory = false;
  bool allocationOverflow = false;

  bool hadErrors() const {
    return outOfMemory || overRecursed || allocationOverflow || !errors.empty();
  }
};

class ErrorAllocator : public MallocProvider<ErrorAllocator> {
 private:
  ErrorContext* const context_;

 public:
  explicit ErrorAllocator(ErrorContext* ec) : context_(ec) {}

  void* onOutOfMemory(js::AllocFunction allocFunc, arena_id_t arena,
                      size_t nbytes, void* reallocPtr = nullptr);
  void reportAllocationOverflow();
};

class ErrorContext {
  ErrorAllocator alloc_;

 public:
  explicit ErrorContext() : alloc_(this) {}
  virtual ~ErrorContext() = default;

  ErrorAllocator* getAllocator() { return &alloc_; }

  // Report CompileErrors
  virtual void reportError(js::CompileError* err) = 0;
  virtual void reportWarning(js::CompileError* err) = 0;

  // Report ErrorAllocator errors
  virtual void* onOutOfMemory(js::AllocFunction allocFunc, arena_id_t arena,
                              size_t nbytes, void* reallocPtr = nullptr) = 0;
  virtual void onAllocationOverflow() = 0;

  virtual void onOutOfMemory() = 0;
  virtual void onOverRecursed() = 0;

  virtual void recoverFromOutOfMemory() = 0;

  virtual const JSErrorFormatString* gcSafeCallback(
      JSErrorCallback callback, void* userRef, const unsigned errorNumber) = 0;

  // Status of errors reported to this ErrorContext
  virtual bool hadOutOfMemory() const = 0;
  virtual bool hadOverRecursed() const = 0;
  virtual bool hadAllocationOverflow() const = 0;
  virtual bool hadErrors() const = 0;

#ifdef __wasi__
  virtual void incWasiRecursionDepth() = 0;
  virtual void decWasiRecursionDepth() = 0;
  virtual bool checkWasiRecursionLimit() = 0;
#endif  // __wasi__
};

class OffThreadErrorContext : public ErrorContext {
 private:
  js::OffThreadFrontendErrors errors_;

 protected:
  // (optional) Current JSContext to support main-thread-specific
  // handling for error reporting, GC, and memory allocation.
  //
  // Set by setCurrentJSContext.
  JSContext* maybeCx_ = nullptr;

 public:
  OffThreadErrorContext() = default;

  void setCurrentJSContext(JSContext* cx);

  enum class Warning { Suppress, Report };

  void convertToRuntimeError(JSContext* cx, Warning warning = Warning::Report);

  void linkWithJSContext(JSContext* cx);

  const Vector<UniquePtr<CompileError>, 0, SystemAllocPolicy>& errors() const {
    return errors_.errors;
  }
  const Vector<UniquePtr<CompileError>, 0, SystemAllocPolicy>& warnings()
      const {
    return errors_.warnings;
  }

  // Report CompileErrors
  void reportError(js::CompileError* err) override;
  void reportWarning(js::CompileError* err) override;

  // Report ErrorAllocator errors
  void* onOutOfMemory(js::AllocFunction allocFunc, arena_id_t arena,
                      size_t nbytes, void* reallocPtr = nullptr) override;
  void onAllocationOverflow() override;

  void onOutOfMemory() override;
  void onOverRecursed() override;

  void recoverFromOutOfMemory() override;

  const JSErrorFormatString* gcSafeCallback(
      JSErrorCallback callback, void* userRef,
      const unsigned errorNumber) override;

  // Status of errors reported to this ErrorContext
  bool hadOutOfMemory() const override { return errors_.outOfMemory; }
  bool hadOverRecursed() const override { return errors_.overRecursed; }
  bool hadAllocationOverflow() const override {
    return errors_.allocationOverflow;
  }
  bool hadErrors() const override;

#ifdef __wasi__
  void incWasiRecursionDepth() override;
  void decWasiRecursionDepth() override;
  bool checkWasiRecursionLimit() override;
#endif  // __wasi__

 private:
  void ReportOutOfMemory();
  void addPendingOutOfMemory();
};

// Automatically report any pending exception when leaving the scope.
class MOZ_STACK_CLASS AutoReportFrontendContext : public OffThreadErrorContext {
  // The target JSContext to report the errors to.
  JSContext* cx_;

  Warning warning_;

 public:
  explicit AutoReportFrontendContext(JSContext* cx,
                                     Warning warning = Warning::Report)
      : OffThreadErrorContext(), cx_(cx), warning_(warning) {
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

}  // namespace js

#endif /* vm_ErrorContext_h */
