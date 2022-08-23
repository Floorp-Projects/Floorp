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
  OffThreadFrontendErrors() : overRecursed(false), outOfMemory(false) {}
  // Any errors or warnings produced during compilation. These are reported
  // when finishing the script.
  Vector<UniquePtr<CompileError>, 0, SystemAllocPolicy> errors;
  bool overRecursed;
  bool outOfMemory;
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
  virtual bool hadErrors() const = 0;

#ifdef __wasi__
  virtual void incWasiRecursionDepth() = 0;
  virtual void decWasiRecursionDepth() = 0;
  virtual bool checkWasiRecursionLimit() = 0;
#endif  // __wasi__
};

class MainThreadErrorContext : public ErrorContext {
 private:
  JSContext* cx_;

 public:
  explicit MainThreadErrorContext(JSContext* cx);

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
  bool hadOutOfMemory() const override;
  bool hadOverRecursed() const override;
  bool hadErrors() const override;

#ifdef __wasi__
  void incWasiRecursionDepth() override;
  void decWasiRecursionDepth() override;
  bool checkWasiRecursionLimit() override;
#endif  // __wasi__
};

class OffThreadErrorContext : public ErrorContext {
 private:
  js::OffThreadFrontendErrors errors_;

 public:
  OffThreadErrorContext() = default;

  void linkWithJSContext(JSContext* cx);
  const Vector<UniquePtr<CompileError>, 0, SystemAllocPolicy>& errors() const {
    return errors_.errors;
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
  bool hadErrors() const override {
    return hadOutOfMemory() || hadOverRecursed() || !errors_.errors.empty();
  }

#ifdef __wasi__
  void incWasiRecursionDepth() override;
  void decWasiRecursionDepth() override;
  bool checkWasiRecursionLimit() override;
#endif  // __wasi__

 private:
  void ReportOutOfMemory();
  void addPendingOutOfMemory();
};

}  // namespace js

#endif /* vm_ErrorContext_h */
