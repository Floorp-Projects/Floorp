/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Error-reporting types and structures.
 *
 * Despite these types and structures existing in js/public, significant parts
 * of their heritage date back to distant SpiderMonkey past, and they are not
 * all universally well-thought-out as ideal, intended-to-be-permanent API.
 * We may eventually replace this with something more consistent with
 * ECMAScript the language and less consistent with '90s-era JSAPI inventions,
 * but it's doubtful this will happen any time soon.
 */

#ifndef js_ErrorReport_h
#define js_ErrorReport_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include <iterator>  // std::input_iterator_tag, std::iterator
#include <stddef.h>  // size_t
#include <stdint.h>  // int16_t, uint16_t
#include <string.h>  // strlen

#include "jstypes.h"  // JS_PUBLIC_API

#include "js/AllocPolicy.h"        // js::SystemAllocPolicy
#include "js/CharacterEncoding.h"  // JS::ConstUTF8CharsZ
#include "js/UniquePtr.h"          // js::UniquePtr
#include "js/Vector.h"             // js::Vector

struct JSContext;
class JSString;

/**
 * Possible exception types. These types are part of a JSErrorFormatString
 * structure. They define which error to throw in case of a runtime error.
 *
 * JSEXN_WARN is used for warnings in js.msg files (for instance because we
 * don't want to prepend 'Error:' to warning messages). This value can go away
 * if we ever decide to use an entirely separate mechanism for warnings.
 */
enum JSExnType {
  JSEXN_ERR,
  JSEXN_FIRST = JSEXN_ERR,
  JSEXN_INTERNALERR,
  JSEXN_EVALERR,
  JSEXN_RANGEERR,
  JSEXN_REFERENCEERR,
  JSEXN_SYNTAXERR,
  JSEXN_TYPEERR,
  JSEXN_URIERR,
  JSEXN_DEBUGGEEWOULDRUN,
  JSEXN_WASMCOMPILEERROR,
  JSEXN_WASMLINKERROR,
  JSEXN_WASMRUNTIMEERROR,
  JSEXN_ERROR_LIMIT,
  JSEXN_WARN = JSEXN_ERROR_LIMIT,
  JSEXN_NOTE,
  JSEXN_LIMIT
};

struct JSErrorFormatString {
  /** The error message name in ASCII. */
  const char* name;

  /** The error format string in ASCII. */
  const char* format;

  /** The number of arguments to expand in the formatted error message. */
  uint16_t argCount;

  /** One of the JSExnType constants above. */
  int16_t exnType;
};

using JSErrorCallback =
    const JSErrorFormatString* (*)(void* userRef, const unsigned errorNumber);

/**
 * Base class that implements parts shared by JSErrorReport and
 * JSErrorNotes::Note.
 */
class JSErrorBase {
 private:
  // The (default) error message.
  // If ownsMessage_ is true, the it is freed in destructor.
  JS::ConstUTF8CharsZ message_;

 public:
  // Source file name, URL, etc., or null.
  const char* filename;

  // Unique identifier for the script source.
  unsigned sourceId;

  // Source line number.
  unsigned lineno;

  // Zero-based column index in line.
  unsigned column;

  // the error number, e.g. see js.msg.
  unsigned errorNumber;

 private:
  bool ownsMessage_ : 1;

 public:
  JSErrorBase()
      : filename(nullptr),
        sourceId(0),
        lineno(0),
        column(0),
        errorNumber(0),
        ownsMessage_(false) {}

  ~JSErrorBase() { freeMessage(); }

 public:
  const JS::ConstUTF8CharsZ message() const { return message_; }

  void initOwnedMessage(const char* messageArg) {
    initBorrowedMessage(messageArg);
    ownsMessage_ = true;
  }
  void initBorrowedMessage(const char* messageArg) {
    MOZ_ASSERT(!message_);
    message_ = JS::ConstUTF8CharsZ(messageArg, strlen(messageArg));
  }

  JSString* newMessageString(JSContext* cx);

 private:
  void freeMessage();
};

/**
 * Notes associated with JSErrorReport.
 */
class JSErrorNotes {
 public:
  class Note final : public JSErrorBase {};

 private:
  // Stores pointers to each note.
  js::Vector<js::UniquePtr<Note>, 1, js::SystemAllocPolicy> notes_;

 public:
  JSErrorNotes();
  ~JSErrorNotes();

  // Add an note to the given position.
  bool addNoteASCII(JSContext* cx, const char* filename, unsigned sourceId,
                    unsigned lineno, unsigned column,
                    JSErrorCallback errorCallback, void* userRef,
                    const unsigned errorNumber, ...);
  bool addNoteLatin1(JSContext* cx, const char* filename, unsigned sourceId,
                     unsigned lineno, unsigned column,
                     JSErrorCallback errorCallback, void* userRef,
                     const unsigned errorNumber, ...);
  bool addNoteUTF8(JSContext* cx, const char* filename, unsigned sourceId,
                   unsigned lineno, unsigned column,
                   JSErrorCallback errorCallback, void* userRef,
                   const unsigned errorNumber, ...);

  JS_PUBLIC_API size_t length();

  // Create a deep copy of notes.
  js::UniquePtr<JSErrorNotes> copy(JSContext* cx);

  class iterator final {
   private:
    js::UniquePtr<Note>* note_;

   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = js::UniquePtr<Note>;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    explicit iterator(js::UniquePtr<Note>* note = nullptr) : note_(note) {}

    bool operator==(iterator other) const { return note_ == other.note_; }
    bool operator!=(iterator other) const { return !(*this == other); }
    iterator& operator++() {
      note_++;
      return *this;
    }
    reference operator*() { return *note_; }
  };

  JS_PUBLIC_API iterator begin();
  JS_PUBLIC_API iterator end();
};

/**
 * Describes a single error or warning that occurs in the execution of script.
 */
class JSErrorReport : public JSErrorBase {
 private:
  // Offending source line without final '\n'.
  // If ownsLinebuf_ is true, the buffer is freed in destructor.
  const char16_t* linebuf_;

  // Number of chars in linebuf_. Does not include trailing '\0'.
  size_t linebufLength_;

  // The 0-based offset of error token in linebuf_.
  size_t tokenOffset_;

 public:
  // Associated notes, or nullptr if there's no note.
  js::UniquePtr<JSErrorNotes> notes;

  // error/warning, etc.
  unsigned flags;

  // One of the JSExnType constants.
  int16_t exnType;

  // See the comment in TransitiveCompileOptions.
  bool isMuted : 1;

 private:
  bool ownsLinebuf_ : 1;

 public:
  JSErrorReport()
      : linebuf_(nullptr),
        linebufLength_(0),
        tokenOffset_(0),
        notes(nullptr),
        flags(0),
        exnType(0),
        isMuted(false),
        ownsLinebuf_(false) {}

  ~JSErrorReport() { freeLinebuf(); }

 public:
  const char16_t* linebuf() const { return linebuf_; }
  size_t linebufLength() const { return linebufLength_; }
  size_t tokenOffset() const { return tokenOffset_; }
  void initOwnedLinebuf(const char16_t* linebufArg, size_t linebufLengthArg,
                        size_t tokenOffsetArg) {
    initBorrowedLinebuf(linebufArg, linebufLengthArg, tokenOffsetArg);
    ownsLinebuf_ = true;
  }
  void initBorrowedLinebuf(const char16_t* linebufArg, size_t linebufLengthArg,
                           size_t tokenOffsetArg);

 private:
  void freeLinebuf();
};

/*
 * JSErrorReport flag values.  These may be freely composed.
 */
#define JSREPORT_ERROR 0x0     /* pseudo-flag for default case */
#define JSREPORT_WARNING 0x1   /* reported via JS::Warn* */
#define JSREPORT_EXCEPTION 0x2 /* exception was thrown */
#define JSREPORT_STRICT 0x4    /* error or warning due to strict option */

#define JSREPORT_USER_1 0x8 /* user-defined flag */

/*
 * If JSREPORT_EXCEPTION is set, then a JavaScript-catchable exception
 * has been thrown for this runtime error, and the host should ignore it.
 * Exception-aware hosts should also check for JS_IsExceptionPending if
 * JS_ExecuteScript returns failure, and signal or propagate the exception, as
 * appropriate.
 */
#define JSREPORT_IS_WARNING(flags) (((flags)&JSREPORT_WARNING) != 0)
#define JSREPORT_IS_EXCEPTION(flags) (((flags)&JSREPORT_EXCEPTION) != 0)
#define JSREPORT_IS_STRICT(flags) (((flags)&JSREPORT_STRICT) != 0)

#endif /* js_ErrorReport_h */
