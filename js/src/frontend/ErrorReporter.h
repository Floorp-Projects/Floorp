/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ErrorReporter_h
#define frontend_ErrorReporter_h

#include "mozilla/Variant.h"

#include <stdarg.h>  // for va_list
#include <stddef.h>  // for size_t
#include <stdint.h>  // for uint32_t

#include "js/CompileOptions.h"
#include "js/UniquePtr.h"
#include "vm/ErrorReporting.h"  // ErrorMetadata, ReportCompile{Error,Warning}

class JSErrorNotes;

namespace js {
namespace frontend {

// An interface class to provide strictMode getter method, which is used by
// ErrorReportMixin::strictModeError* methods.
//
// This class is separated to be used as a back-channel from TokenStream to the
// strict mode flag which is available inside Parser, to avoid exposing the
// rest of SharedContext to TokenStream.
class StrictModeGetter {
 public:
  virtual bool strictMode() const = 0;
};

// This class provides error reporting methods, including warning, extra
// warning, and strict mode error.
//
// A class that inherits this class must provide the following methods:
//   * options
//   * getContext
//   * computeErrorMetadata
class ErrorReportMixin : public StrictModeGetter {
 public:
  // Returns a compile options (extra warning, warning as error) for current
  // compilation.
  virtual const JS::ReadOnlyCompileOptions& options() const = 0;

  // Returns the current context.
  virtual JSContext* getContext() const = 0;

  // A variant class for the offset of the error or warning.
  struct Current {};
  struct NoOffset {};
  using ErrorOffset = mozilla::Variant<uint32_t, Current, NoOffset>;

  // Fills ErrorMetadata fields for an error or warning at given offset.
  //   * offset is uint32_t if methods ending with "At" is called
  //   * offset is NoOffset if methods ending with "NoOffset" is called
  //   * offset is Current otherwise
  [[nodiscard]] virtual bool computeErrorMetadata(
      ErrorMetadata* err, const ErrorOffset& offset) = 0;

  // ==== error ====
  //
  // Reports an error.
  //
  // Methods ending with "At" are for an error at given offset.
  // The offset is passed to computeErrorMetadata method and is transparent
  // for this class.
  //
  // Methods ending with "NoOffset" are for an error that doesn't correspond
  // to any offset. NoOffset is passed to computeErrorMetadata for them.
  //
  // Other methods except errorWithNotesAtVA are for an error at the current
  // offset. Current is passed to computeErrorMetadata for them.
  //
  // Methods contains "WithNotes" can be used if there are error notes.
  //
  // errorWithNotesAtVA is the actual implementation for all of above.
  // This can be called if the caller already has a va_list.

  void error(unsigned errorNumber, ...) {
    va_list args;
    va_start(args, errorNumber);

    errorWithNotesAtVA(nullptr, mozilla::AsVariant(Current()), errorNumber,
                       &args);

    va_end(args);
  }
  void errorWithNotes(UniquePtr<JSErrorNotes> notes, unsigned errorNumber,
                      ...) {
    va_list args;
    va_start(args, errorNumber);

    errorWithNotesAtVA(std::move(notes), mozilla::AsVariant(Current()),
                       errorNumber, &args);

    va_end(args);
  }
  void errorAt(uint32_t offset, unsigned errorNumber, ...) {
    va_list args;
    va_start(args, errorNumber);

    errorWithNotesAtVA(nullptr, mozilla::AsVariant(offset), errorNumber, &args);

    va_end(args);
  }
  void errorWithNotesAt(UniquePtr<JSErrorNotes> notes, uint32_t offset,
                        unsigned errorNumber, ...) {
    va_list args;
    va_start(args, errorNumber);

    errorWithNotesAtVA(std::move(notes), mozilla::AsVariant(offset),
                       errorNumber, &args);

    va_end(args);
  }
  void errorNoOffset(unsigned errorNumber, ...) {
    va_list args;
    va_start(args, errorNumber);

    errorWithNotesAtVA(nullptr, mozilla::AsVariant(NoOffset()), errorNumber,
                       &args);

    va_end(args);
  }
  void errorWithNotesNoOffset(UniquePtr<JSErrorNotes> notes,
                              unsigned errorNumber, ...) {
    va_list args;
    va_start(args, errorNumber);

    errorWithNotesAtVA(std::move(notes), mozilla::AsVariant(NoOffset()),
                       errorNumber, &args);

    va_end(args);
  }
  void errorWithNotesAtVA(UniquePtr<JSErrorNotes> notes,
                          const ErrorOffset& offset, unsigned errorNumber,
                          va_list* args) {
    ErrorMetadata metadata;
    if (!computeErrorMetadata(&metadata, offset)) {
      return;
    }

    ReportCompileErrorLatin1(getContext(), std::move(metadata),
                             std::move(notes), errorNumber, args);
  }

  // ==== warning ====
  //
  // Reports a warning.
  //
  // Returns true if the warning is reported.
  // Returns false if the warning is treated as an error, or an error occurs
  // while reporting.
  //
  // See the comment on the error section for details on what the arguments
  // and function names indicate for all these functions.

  [[nodiscard]] bool warning(unsigned errorNumber, ...) {
    va_list args;
    va_start(args, errorNumber);

    bool result = warningWithNotesAtVA(nullptr, mozilla::AsVariant(Current()),
                                       errorNumber, &args);

    va_end(args);

    return result;
  }
  [[nodiscard]] bool warningAt(uint32_t offset, unsigned errorNumber, ...) {
    va_list args;
    va_start(args, errorNumber);

    bool result = warningWithNotesAtVA(nullptr, mozilla::AsVariant(offset),
                                       errorNumber, &args);

    va_end(args);

    return result;
  }
  [[nodiscard]] bool warningNoOffset(unsigned errorNumber, ...) {
    va_list args;
    va_start(args, errorNumber);

    bool result = warningWithNotesAtVA(nullptr, mozilla::AsVariant(NoOffset()),
                                       errorNumber, &args);

    va_end(args);

    return result;
  }
  [[nodiscard]] bool warningWithNotesAtVA(UniquePtr<JSErrorNotes> notes,
                                          const ErrorOffset& offset,
                                          unsigned errorNumber, va_list* args) {
    ErrorMetadata metadata;
    if (!computeErrorMetadata(&metadata, offset)) {
      return false;
    }

    return compileWarning(std::move(metadata), std::move(notes), errorNumber,
                          args);
  }

  // ==== strictModeError ====
  //
  // Reports an error if in strict mode code, or warn if not.
  //
  // Returns true if not in strict mode and a warning is reported.
  // Returns false if the error reported, or an error occurs while reporting.
  //
  // See the comment on the error section for details on what the arguments
  // and function names indicate for all these functions.

  [[nodiscard]] bool strictModeError(unsigned errorNumber, ...) {
    va_list args;
    va_start(args, errorNumber);

    bool result = strictModeErrorWithNotesAtVA(
        nullptr, mozilla::AsVariant(Current()), errorNumber, &args);

    va_end(args);

    return result;
  }
  [[nodiscard]] bool strictModeErrorWithNotes(UniquePtr<JSErrorNotes> notes,
                                              unsigned errorNumber, ...) {
    va_list args;
    va_start(args, errorNumber);

    bool result = strictModeErrorWithNotesAtVA(
        std::move(notes), mozilla::AsVariant(Current()), errorNumber, &args);

    va_end(args);

    return result;
  }
  [[nodiscard]] bool strictModeErrorAt(uint32_t offset, unsigned errorNumber,
                                       ...) {
    va_list args;
    va_start(args, errorNumber);

    bool result = strictModeErrorWithNotesAtVA(
        nullptr, mozilla::AsVariant(offset), errorNumber, &args);

    va_end(args);

    return result;
  }
  [[nodiscard]] bool strictModeErrorWithNotesAt(UniquePtr<JSErrorNotes> notes,
                                                uint32_t offset,
                                                unsigned errorNumber, ...) {
    va_list args;
    va_start(args, errorNumber);

    bool result = strictModeErrorWithNotesAtVA(
        std::move(notes), mozilla::AsVariant(offset), errorNumber, &args);

    va_end(args);

    return result;
  }
  [[nodiscard]] bool strictModeErrorNoOffset(unsigned errorNumber, ...) {
    va_list args;
    va_start(args, errorNumber);

    bool result = strictModeErrorWithNotesAtVA(
        nullptr, mozilla::AsVariant(NoOffset()), errorNumber, &args);

    va_end(args);

    return result;
  }
  [[nodiscard]] bool strictModeErrorWithNotesNoOffset(
      UniquePtr<JSErrorNotes> notes, unsigned errorNumber, ...) {
    va_list args;
    va_start(args, errorNumber);

    bool result = strictModeErrorWithNotesAtVA(
        std::move(notes), mozilla::AsVariant(NoOffset()), errorNumber, &args);

    va_end(args);

    return result;
  }
  [[nodiscard]] bool strictModeErrorWithNotesAtVA(UniquePtr<JSErrorNotes> notes,
                                                  const ErrorOffset& offset,
                                                  unsigned errorNumber,
                                                  va_list* args) {
    if (!strictMode()) {
      return true;
    }

    ErrorMetadata metadata;
    if (!computeErrorMetadata(&metadata, offset)) {
      return false;
    }

    ReportCompileErrorLatin1(getContext(), std::move(metadata),
                             std::move(notes), errorNumber, args);
    return false;
  }

  // Reports a warning, or an error if the warning is treated as an error.
  [[nodiscard]] bool compileWarning(ErrorMetadata&& metadata,
                                    UniquePtr<JSErrorNotes> notes,
                                    unsigned errorNumber, va_list* args) {
    return ReportCompileWarning(getContext(), std::move(metadata),
                                std::move(notes), errorNumber, args);
  }
};

// An interface class to provide miscellaneous methods used by error reporting
// etc.  They're mostly used by BytecodeCompiler, BytecodeEmitter, and helper
// classes for emitter.
class ErrorReporter : public ErrorReportMixin {
 public:
  // Returns the line and column numbers for given offset.
  virtual void lineAndColumnAt(size_t offset, uint32_t* line,
                               uint32_t* column) const = 0;

  // Returns the line and column numbers for current offset.
  virtual void currentLineAndColumn(uint32_t* line, uint32_t* column) const = 0;

  // Sets *onThisLine to true if the given offset is inside the given line
  // number `lineNum`, or false otherwise, and returns true.
  //
  // Return false if an error happens.  This method itself doesn't report an
  // error, and any failure is supposed to be reported as OOM in the caller.
  virtual bool isOnThisLine(size_t offset, uint32_t lineNum,
                            bool* onThisLine) const = 0;

  // Returns the line number for given offset.
  virtual uint32_t lineAt(size_t offset) const = 0;

  // Returns the column number for given offset.
  virtual uint32_t columnAt(size_t offset) const = 0;

  // Returns true if tokenization is already started and hasn't yet finished.
  // currentLineAndColumn returns meaningful value only if this is true.
  virtual bool hasTokenizationStarted() const = 0;

  // Returns the filename which is currently being compiled.
  virtual const char* getFilename() const = 0;
};

}  // namespace frontend
}  // namespace js

#endif  // frontend_ErrorReporter_h
