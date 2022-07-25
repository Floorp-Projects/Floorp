/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ErrorContext_h
#define vm_ErrorContext_h

#include "vm/ErrorReporting.h"

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

  virtual void reportError(js::CompileError* err) = 0;
  virtual void reportWarning(js::CompileError* err) = 0;
};

class GeneralErrorContext : public ErrorContext {
 private:
  JSContext* cx_;

 public:
  explicit GeneralErrorContext(JSContext* cx);

  bool addPendingError(js::CompileError** error) override;

  virtual void reportError(js::CompileError* err) override;
  virtual void reportWarning(js::CompileError* err) override;
};

class OffThreadErrorContext : public ErrorContext {
 private:
  JSAllocator* alloc_;

  js::OffThreadFrontendErrors errors_;

 public:
  explicit OffThreadErrorContext(JSAllocator* alloc);
  bool addPendingError(js::CompileError** error) override;

  virtual void reportError(js::CompileError* err) override;
  virtual void reportWarning(js::CompileError* err) override;

  void ReportOutOfMemory();
  void addPendingOutOfMemory();

  void setAllocator(JSAllocator* alloc) { alloc_ = alloc; }
};

}  // namespace js

#endif /* vm_ErrorContext_h */
