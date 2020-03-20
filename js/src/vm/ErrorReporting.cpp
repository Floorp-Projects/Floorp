/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ErrorReporting.h"

#include <stdarg.h>
#include <utility>

#include "jsexn.h"
#include "jsfriendapi.h"

#include "js/Warnings.h"  // JS::WarningReporter
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"

#include "vm/JSContext-inl.h"

using JS::HandleObject;
using JS::HandleValue;
using JS::UniqueTwoByteChars;

void js::CallWarningReporter(JSContext* cx, JSErrorReport* reportp) {
  MOZ_ASSERT(reportp->isWarning());

  if (JS::WarningReporter warningReporter = cx->runtime()->warningReporter) {
    warningReporter(cx, reportp);
  }
}

void js::CompileError::throwError(JSContext* cx) {
  if (isWarning()) {
    CallWarningReporter(cx, this);
    return;
  }

  // If there's a runtime exception type associated with this error
  // number, set that as the pending exception.  For errors occuring at
  // compile time, this is very likely to be a JSEXN_SYNTAXERR.
  ErrorToException(cx, this, nullptr, nullptr);
}

bool js::ReportExceptionClosure::operator()(JSContext* cx) {
  cx->setPendingExceptionAndCaptureStack(exn_);
  return false;
}

bool js::ReportCompileWarning(JSContext* cx, ErrorMetadata&& metadata,
                              UniquePtr<JSErrorNotes> notes,
                              unsigned errorNumber, va_list* args) {
  // On the main thread, report the error immediately. When compiling off
  // thread, save the error so that the thread finishing the parse can report
  // it later.
  CompileError tempErr;
  CompileError* err = &tempErr;
  if (cx->isHelperThreadContext() && !cx->addPendingCompileError(&err)) {
    return false;
  }

  err->notes = std::move(notes);
  err->flags = JSREPORT_WARNING;
  err->errorNumber = errorNumber;

  err->filename = metadata.filename;
  err->lineno = metadata.lineNumber;
  err->column = metadata.columnNumber;
  err->isMuted = metadata.isMuted;

  if (UniqueTwoByteChars lineOfContext = std::move(metadata.lineOfContext)) {
    err->initOwnedLinebuf(lineOfContext.release(), metadata.lineLength,
                          metadata.tokenOffset);
  }

  if (!ExpandErrorArgumentsVA(cx, GetErrorMessage, nullptr, errorNumber,
                              ArgumentsAreLatin1, err, *args)) {
    return false;
  }

  if (!cx->isHelperThreadContext()) {
    err->throwError(cx);
  }

  return true;
}

static void ReportCompileErrorImpl(JSContext* cx, js::ErrorMetadata&& metadata,
                                   js::UniquePtr<JSErrorNotes> notes,
                                   unsigned errorNumber, va_list* args,
                                   js::ErrorArgumentsType argumentsType) {
  // On the main thread, report the error immediately. When compiling off
  // thread, save the error so that the thread finishing the parse can report
  // it later.
  js::CompileError tempErr;
  js::CompileError* err = &tempErr;
  if (cx->isHelperThreadContext() && !cx->addPendingCompileError(&err)) {
    return;
  }

  err->notes = std::move(notes);
  err->flags = JSREPORT_ERROR;
  err->errorNumber = errorNumber;

  err->filename = metadata.filename;
  err->lineno = metadata.lineNumber;
  err->column = metadata.columnNumber;
  err->isMuted = metadata.isMuted;

  if (UniqueTwoByteChars lineOfContext = std::move(metadata.lineOfContext)) {
    err->initOwnedLinebuf(lineOfContext.release(), metadata.lineLength,
                          metadata.tokenOffset);
  }

  if (!js::ExpandErrorArgumentsVA(cx, js::GetErrorMessage, nullptr, errorNumber,
                                  argumentsType, err, *args)) {
    return;
  }

  if (!cx->isHelperThreadContext()) {
    err->throwError(cx);
  }
}

void js::ReportCompileErrorLatin1(JSContext* cx, ErrorMetadata&& metadata,
                                  UniquePtr<JSErrorNotes> notes,
                                  unsigned errorNumber, va_list* args) {
  ReportCompileErrorImpl(cx, std::move(metadata), std::move(notes), errorNumber,
                         args, ArgumentsAreLatin1);
}

void js::ReportCompileErrorUTF8(JSContext* cx, ErrorMetadata&& metadata,
                                UniquePtr<JSErrorNotes> notes,
                                unsigned errorNumber, va_list* args) {
  ReportCompileErrorImpl(cx, std::move(metadata), std::move(notes), errorNumber,
                         args, ArgumentsAreUTF8);
}

void js::ReportErrorToGlobal(JSContext* cx, Handle<GlobalObject*> global,
                             HandleValue error) {
  MOZ_ASSERT(!cx->isExceptionPending());
#ifdef DEBUG
  // No assertSameCompartment version that doesn't take JSContext...
  if (error.isObject()) {
    AssertSameCompartment(global, &error.toObject());
  }
#endif  // DEBUG
  js::ReportExceptionClosure report(error);
  PrepareScriptEnvironmentAndInvoke(cx, global, report);
}
