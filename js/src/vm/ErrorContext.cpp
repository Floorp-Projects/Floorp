/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ErrorContext.h"

#include "gc/GC.h"
#include "util/DifferentialTesting.h"
#include "vm/JSContext.h"
#include "vm/SelfHosting.h"  // selfHosting_ErrorReporter

using namespace js;

void ErrorAllocator::reportAllocationOverflow() {
  context_->onAllocationOverflow();
}

void* ErrorAllocator::onOutOfMemory(AllocFunction allocFunc, arena_id_t arena,
                                    size_t nbytes, void* reallocPtr) {
  return context_->onOutOfMemory(allocFunc, arena, nbytes, reallocPtr);
}

MainThreadErrorContext::MainThreadErrorContext(JSContext* cx) : cx_(cx) {}

void* MainThreadErrorContext::onOutOfMemory(AllocFunction allocFunc,
                                            arena_id_t arena, size_t nbytes,
                                            void* reallocPtr) {
  return cx_->onOutOfMemory(allocFunc, arena, nbytes, reallocPtr);
}

void MainThreadErrorContext::onOutOfMemory() { cx_->onOutOfMemory(); }

void MainThreadErrorContext::onAllocationOverflow() {
  return cx_->reportAllocationOverflow();
}

void MainThreadErrorContext::onOverRecursed() { cx_->onOverRecursed(); }

void MainThreadErrorContext::recoverFromOutOfMemory() {
  cx_->recoverFromOutOfMemory();
}

const JSErrorFormatString* MainThreadErrorContext::gcSafeCallback(
    JSErrorCallback callback, void* userRef, const unsigned errorNumber) {
  gc::AutoSuppressGC suppressGC(cx_);
  return callback(userRef, errorNumber);
}

void MainThreadErrorContext::reportError(CompileError* err) {
  // On the main thread, report the error immediately.

  if (MOZ_UNLIKELY(!cx_->runtime()->hasInitializedSelfHosting())) {
    selfHosting_ErrorReporter(err);
    return;
  }

  err->throwError(cx_);
}

void MainThreadErrorContext::reportWarning(CompileError* err) {
  err->throwError(cx_);
}

bool MainThreadErrorContext::hadOutOfMemory() const {
  return cx_->offThreadFrontendErrors()->outOfMemory;
}

bool MainThreadErrorContext::hadOverRecursed() const {
  return cx_->offThreadFrontendErrors()->overRecursed;
}

bool MainThreadErrorContext::hadErrors() const {
  return hadOutOfMemory() || hadOverRecursed() ||
         !cx_->offThreadFrontendErrors()->errors.empty();
}

void* OffThreadErrorContext::onOutOfMemory(AllocFunction allocFunc,
                                           arena_id_t arena, size_t nbytes,
                                           void* reallocPtr) {
  addPendingOutOfMemory();
  return nullptr;
}

void OffThreadErrorContext::onAllocationOverflow() {
  // TODO Bug 1780599 - Currently allocation overflows are not reported for
  // helper threads; see js::reportAllocationOverflow()
}

void OffThreadErrorContext::onOutOfMemory() { addPendingOutOfMemory(); }

void OffThreadErrorContext::onOverRecursed() { errors_.overRecursed = true; }

void OffThreadErrorContext::recoverFromOutOfMemory() {
  errors_.outOfMemory = false;
}

const JSErrorFormatString* OffThreadErrorContext::gcSafeCallback(
    JSErrorCallback callback, void* userRef, const unsigned errorNumber) {
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
  reportError(err);
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

void OffThreadErrorContext::linkWithJSContext(JSContext* cx) {
  if (cx) {
    cx->setOffThreadFrontendErrors(&errors_);
  }
}

#ifdef __wasi__
void MainThreadErrorContext::incWasiRecursionDepth() {
  IncWasiRecursionDepth(cx_);
}

void MainThreadErrorContext::decWasiRecursionDepth() {
  DecWasiRecursionDepth(cx_);
}

bool MainThreadErrorContext::checkWasiRecursionLimit() {
  return CheckWasiRecursionLimit(cx_);
}

void OffThreadErrorContext::incWasiRecursionDepth() {
  // WASI doesn't support thread.
}

void OffThreadErrorContext::decWasiRecursionDepth() {
  // WASI doesn't support thread.
}

bool OffThreadErrorContext::checkWasiRecursionLimit() {
  // WASI doesn't support thread.
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
