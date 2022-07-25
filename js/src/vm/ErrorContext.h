/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ErrorContext_h
#define vm_ErrorContext_h

#include "vm/ErrorReporting.h"
#include "vm/MallocProvider.h"

namespace js {

struct OffThreadFrontendErrors {
  OffThreadFrontendErrors() : overRecursed(false), outOfMemory(false) {}
  // Any errors or warnings produced during compilation. These are reported
  // when finishing the script.
  Vector<UniquePtr<CompileError>, 0, SystemAllocPolicy> errors;
  bool overRecursed;
  bool outOfMemory;
};

class ErrorContext {
 public:
  virtual ~ErrorContext() = default;
  virtual bool addPendingError(js::CompileError** error) = 0;
  virtual void* onOutOfMemory(js::AllocFunction allocFunc, arena_id_t arena,
                              size_t nbytes, void* reallocPtr = nullptr) = 0;
  virtual void reportAllocationOverflow() = 0;

  virtual const JSErrorFormatString* gcSafeCallback(
      JSErrorCallback callback, void* userRef, const unsigned errorNumber) = 0;

  virtual void reportError(js::CompileError* err) = 0;
  virtual void reportWarning(js::CompileError* err) = 0;

  virtual bool hadOutOfMemory() const = 0;
  virtual bool hadOverRecursed() const = 0;
  virtual const Vector<UniquePtr<CompileError>, 0, SystemAllocPolicy>& errors()
      const = 0;
};

class GeneralErrorContext : public ErrorContext {
 private:
  JSContext* cx_;

 public:
  explicit GeneralErrorContext(JSContext* cx);

  bool addPendingError(js::CompileError** error) override;
  virtual void* onOutOfMemory(js::AllocFunction allocFunc, arena_id_t arena,
                              size_t nbytes,
                              void* reallocPtr = nullptr) override;
  virtual void reportAllocationOverflow() override;

  const JSErrorFormatString* gcSafeCallback(
      JSErrorCallback callback, void* userRef,
      const unsigned errorNumber) override;

  virtual void reportError(js::CompileError* err) override;
  virtual void reportWarning(js::CompileError* err) override;

  virtual bool hadOutOfMemory() const override;
  virtual bool hadOverRecursed() const override;
  virtual const Vector<UniquePtr<CompileError>, 0, SystemAllocPolicy>& errors()
      const override;
};

class OffThreadErrorContext : public ErrorContext {
 private:
  JSAllocator* alloc_;

  js::OffThreadFrontendErrors errors_;

 public:
  OffThreadErrorContext() : alloc_(nullptr) {}
  bool addPendingError(js::CompileError** error) override;
  void* onOutOfMemory(js::AllocFunction allocFunc, arena_id_t arena,
                      size_t nbytes, void* reallocPtr = nullptr) override;
  void reportAllocationOverflow() override;

  const JSErrorFormatString* gcSafeCallback(
      JSErrorCallback callback, void* userRef,
      const unsigned errorNumber) override;

  virtual void reportError(js::CompileError* err) override;
  virtual void reportWarning(js::CompileError* err) override;

  void ReportOutOfMemory();
  void addPendingOutOfMemory();

  void setAllocator(JSAllocator* alloc);

  bool hadOutOfMemory() const override { return errors_.outOfMemory; }
  bool hadOverRecursed() const override { return errors_.overRecursed; }
  const Vector<UniquePtr<CompileError>, 0, SystemAllocPolicy>& errors()
      const override {
    return errors_.errors;
  }
};

template <typename Context>
class ErrorAllocator : public MallocProvider<ErrorAllocator<Context>> {
 private:
  Context* context_;

 public:
  explicit ErrorAllocator(Context* ec) : context_(ec) {}

  void* onOutOfMemory(js::AllocFunction allocFunc, arena_id_t arena,
                      size_t nbytes, void* reallocPtr = nullptr) {
    return context_->onOutOfMemory(allocFunc, arena, nbytes, reallocPtr);
  }
  void reportAllocationOverflow() { context_->reportAllocationOverflow(); }
};

}  // namespace js

#endif /* vm_ErrorContext_h */
