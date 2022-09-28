/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ErrorContext.h"

#include "gc/GC.h"
#include "js/AllocPolicy.h"         // js::ReportOutOfMemory
#include "js/friend/StackLimits.h"  // js::ReportOverRecursed
#include "util/DifferentialTesting.h"
#include "vm/JSContext.h"

using namespace js;

void ErrorAllocator::reportAllocationOverflow() {
  context_->onAllocationOverflow();
}

void* ErrorAllocator::onOutOfMemory(AllocFunction allocFunc, arena_id_t arena,
                                    size_t nbytes, void* reallocPtr) {
  return context_->onOutOfMemory(allocFunc, arena, nbytes, reallocPtr);
}

bool OffThreadErrorContext::hadErrors() const {
  if (maybeCx_) {
    if (maybeCx_->isExceptionPending()) {
      return true;
    }
  }

  return errors_.hadErrors();
}

void* OffThreadErrorContext::onOutOfMemory(AllocFunction allocFunc,
                                           arena_id_t arena, size_t nbytes,
                                           void* reallocPtr) {
  addPendingOutOfMemory();
  return nullptr;
}

void OffThreadErrorContext::onAllocationOverflow() {
  errors_.allocationOverflow = true;
}

void OffThreadErrorContext::onOutOfMemory() { addPendingOutOfMemory(); }

void OffThreadErrorContext::onOverRecursed() { errors_.overRecursed = true; }

void OffThreadErrorContext::recoverFromOutOfMemory() {
  // TODO: Remove this branch once error report directly against JSContext is
  //       removed from the frontend code.
  if (maybeCx_) {
    maybeCx_->recoverFromOutOfMemory();
  }

  errors_.outOfMemory = false;
}

const JSErrorFormatString* OffThreadErrorContext::gcSafeCallback(
    JSErrorCallback callback, void* userRef, const unsigned errorNumber) {
  if (maybeCx_) {
    gc::AutoSuppressGC suppressGC(maybeCx_);
    return callback(userRef, errorNumber);
  }

  return callback(userRef, errorNumber);
}

void OffThreadErrorContext::reportError(CompileError* err) {
  // When compiling off thread, save the error so that the thread finishing the
  // parse can report it later.
  auto errorPtr = getAllocator()->make_unique<CompileError>(std::move(*err));
  if (!errorPtr) {
    return;
  }
  if (!errors_.errors.append(std::move(errorPtr))) {
    ReportOutOfMemory();
    return;
  }
}

void OffThreadErrorContext::reportWarning(CompileError* err) {
  auto errorPtr = getAllocator()->make_unique<CompileError>(std::move(*err));
  if (!errorPtr) {
    return;
  }
  if (!errors_.warnings.append(std::move(errorPtr))) {
    ReportOutOfMemory();
    return;
  }
}

void OffThreadErrorContext::ReportOutOfMemory() {
  /*
   * OOMs are non-deterministic, especially across different execution modes
   * (e.g. interpreter vs JIT). When doing differential testing, print to
   * stderr so that the fuzzers can detect this.
   */
  if (SupportDifferentialTesting()) {
    fprintf(stderr, "ReportOutOfMemory called\n");
  }

  addPendingOutOfMemory();
}

void OffThreadErrorContext::addPendingOutOfMemory() {
  errors_.outOfMemory = true;
}

void OffThreadErrorContext::setCurrentJSContext(JSContext* cx) {
  maybeCx_ = cx;
}

void OffThreadErrorContext::convertToRuntimeError(
    JSContext* cx, Warning warning /* = Warning::Report */) {
  // Report out of memory errors eagerly, or errors could be malformed.
  if (hadOutOfMemory()) {
    js::ReportOutOfMemory(cx);
    return;
  }

  for (const UniquePtr<CompileError>& error : errors()) {
    error->throwError(cx);
  }
  if (warning == Warning::Report) {
    for (const UniquePtr<CompileError>& error : warnings()) {
      error->throwError(cx);
    }
  }
  if (hadOverRecursed()) {
    js::ReportOverRecursed(cx);
  }
  if (hadAllocationOverflow()) {
    js::ReportAllocationOverflow(cx);
  }
}

void OffThreadErrorContext::linkWithJSContext(JSContext* cx) {
  if (cx) {
    cx->setOffThreadFrontendErrors(&errors_);
  }
}

#ifdef __wasi__
void OffThreadErrorContext::incWasiRecursionDepth() {
  if (maybeCx_) {
    IncWasiRecursionDepth(maybeCx_);
  }
}

void OffThreadErrorContext::decWasiRecursionDepth() {
  if (maybeCx_) {
    DecWasiRecursionDepth(maybeCx_);
  }
}

bool OffThreadErrorContext::checkWasiRecursionLimit() {
  if (maybeCx_) {
    return CheckWasiRecursionLimit(maybeCx_);
  }
  return true;
}

JS_PUBLIC_API void js::IncWasiRecursionDepth(ErrorContext* ec) {
  ec->incWasiRecursionDepth();
}

JS_PUBLIC_API void js::DecWasiRecursionDepth(ErrorContext* ec) {
  ec->decWasiRecursionDepth();
}

JS_PUBLIC_API bool js::CheckWasiRecursionLimit(ErrorContext* ec) {
  return ec->checkWasiRecursionLimit();
}
#endif  // __wasi__
