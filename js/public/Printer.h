/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_Printer_h
#define js_Printer_h

#include "mozilla/Attributes.h"
#include "mozilla/Range.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "js/TypeDecls.h"
#include "js/Utility.h"
#include "util/Text.h"

namespace js {

class LifoAlloc;

// Generic printf interface, similar to an ostream in the standard library.
//
// This class is useful to make generic printers which can work either with a
// file backend, with a buffer allocated with an JSContext or a link-list
// of chunks allocated with a LifoAlloc.
class JS_PUBLIC_API GenericPrinter {
 protected:
  bool hadOOM_;  // whether reportOutOfMemory() has been called.

  constexpr GenericPrinter() : hadOOM_(false) {}

 public:
  // Puts |len| characters from |s| at the current position. This function might
  // silently fail and the error can be tested using `hadOutOfMemory()`. Calling
  // this function or any other printing functions after a failures is accepted,
  // but the outcome would still remain incorrect and `hadOutOfMemory()` would
  // still report any of the previous errors.
  virtual void put(const char* s, size_t len) = 0;
  inline void put(const char* s) { put(s, strlen(s)); }
  virtual void put(mozilla::Span<const JS::Latin1Char> str);
  // Would crash unless putChar is overriden, like in EscapePrinter.
  virtual void put(mozilla::Span<const char16_t> str);

  virtual inline void putChar(const char c) { put(&c, 1); }
  virtual inline void putChar(const JS::Latin1Char c) { putChar(char(c)); }
  virtual inline void putChar(const char16_t c) {
    MOZ_CRASH("Use an EscapePrinter to handle all characters");
  }

  virtual void putAsciiPrintable(mozilla::Span<const JS::Latin1Char> str);
  virtual void putAsciiPrintable(mozilla::Span<const char16_t> str);

  inline void putAsciiPrintable(const char c) {
    MOZ_ASSERT(IsAsciiPrintable(c));
    putChar(c);
  }
  inline void putAsciiPrintable(const char16_t c) {
    MOZ_ASSERT(IsAsciiPrintable(c));
    putChar(char(c));
  }

  virtual void putString(JSContext* cx, JSString* str);

  // Prints a formatted string into the buffer.
  void printf(const char* fmt, ...) MOZ_FORMAT_PRINTF(2, 3);
  void vprintf(const char* fmt, va_list ap) MOZ_FORMAT_PRINTF(2, 0);

  virtual bool canPutFromIndex() const { return false; }
  virtual void putFromIndex(size_t index, size_t length) {
    MOZ_CRASH("Calls to putFromIndex should be guarded by canPutFromIndex.");
  }
  virtual size_t index() const { return 0; }

  // In some printers, this ensure that the content is fully written.
  virtual void flush() { /* Do nothing */
  }

  // Report that a string operation failed to get the memory it requested.
  virtual void reportOutOfMemory();

  // Return true if this Sprinter ran out of memory.
  virtual bool hadOutOfMemory() const { return hadOOM_; }
};

// Sprintf, but with unlimited and automatically allocated buffering.
class JS_PUBLIC_API Sprinter final : public GenericPrinter {
 public:
  struct InvariantChecker {
    const Sprinter* parent;

    explicit InvariantChecker(const Sprinter* p) : parent(p) {
      parent->checkInvariants();
    }

    ~InvariantChecker() { parent->checkInvariants(); }
  };

  JSContext* maybeCx;  // context executing the decompiler

 private:
  static const size_t DefaultSize;
#ifdef DEBUG
  bool initialized;  // true if this is initialized, use for debug builds
#endif
  bool shouldReportOOM;  // whether to report OOM to the maybeCx
  char* base;            // malloc'd buffer address
  size_t size;           // size of buffer allocated at base
  ptrdiff_t offset;      // offset of next free char in buffer

  [[nodiscard]] bool realloc_(size_t newSize);

 public:
  // JSContext* parameter is optional and can be omitted if the following
  // are not used.
  //   * putString method with JSString
  //   * QuoteString function with JSString
  //   * JSONQuoteString function with JSString
  //
  // If JSContext* parameter is not provided, or shouldReportOOM is false,
  // the consumer should manually report OOM on any failure.
  explicit Sprinter(JSContext* maybeCx = nullptr, bool shouldReportOOM = true);
  ~Sprinter();

  // Initialize this sprinter, returns false on error.
  [[nodiscard]] bool init();

  void checkInvariants() const;

  JS::UniqueChars release();
  JSString* releaseJS(JSContext* cx);

  // Attempt to reserve len + 1 space (for a trailing nullptr byte). If the
  // attempt succeeds, return a pointer to the start of that space and adjust
  // the internal content. The caller *must* completely fill this space on
  // success.
  char* reserve(size_t len);

  // Puts |len| characters from |s| at the current position and
  // return true on success, false on failure.
  virtual void put(const char* s, size_t len) override;
  using GenericPrinter::put;  // pick up |inline bool put(const char* s);|

  virtual bool canPutFromIndex() const override { return true; }
  virtual void putFromIndex(size_t index, size_t length) override {
    MOZ_ASSERT(index <= this->index());
    MOZ_ASSERT(index + length <= this->index());
    put(base + index, length);
  }
  virtual size_t index() const override { return length(); }

  virtual void putString(JSContext* cx, JSString* str) override;

  size_t length() const;

  // When an OOM has already been reported on the Sprinter, this function will
  // forward this error to the JSContext given in the Sprinter initialization.
  //
  // If no JSContext had been provided or the Sprinter is configured to not
  // report OOM, then nothing happens.
  void forwardOutOfMemory();
};

// Fprinter, print a string directly into a file.
class JS_PUBLIC_API Fprinter final : public GenericPrinter {
 private:
  FILE* file_;
  bool init_;

 public:
  explicit Fprinter(FILE* fp);

  constexpr Fprinter() : file_(nullptr), init_(false) {}

#ifdef DEBUG
  ~Fprinter();
#endif

  // Initialize this printer, returns false on error.
  [[nodiscard]] bool init(const char* path);
  void init(FILE* fp);
  bool isInitialized() const { return file_ != nullptr; }
  void flush() override;
  void finish();

  // Puts |len| characters from |s| at the current position and
  // return true on success, false on failure.
  virtual void put(const char* s, size_t len) override;
  using GenericPrinter::put;  // pick up |inline bool put(const char* s);|
};

// LSprinter, is similar to Sprinter except that instead of using an
// JSContext to allocate strings, it use a LifoAlloc as a backend for the
// allocation of the chunk of the string.
class JS_PUBLIC_API LSprinter final : public GenericPrinter {
 private:
  struct Chunk {
    Chunk* next;
    size_t length;

    char* chars() { return reinterpret_cast<char*>(this + 1); }
    char* end() { return chars() + length; }
  };

 private:
  LifoAlloc* alloc_;  // LifoAlloc used as a backend of chunk allocations.
  Chunk* head_;
  Chunk* tail_;
  size_t unused_;

 public:
  explicit LSprinter(LifoAlloc* lifoAlloc);
  ~LSprinter();

  // Copy the content of the chunks into another printer, such that we can
  // flush the content of this printer to a file.
  void exportInto(GenericPrinter& out) const;

  // Drop the current string, and let them be free with the LifoAlloc.
  void clear();

  // Puts |len| characters from |s| at the current position and
  // return true on success, false on failure.
  virtual void put(const char* s, size_t len) override;
  using GenericPrinter::put;  // pick up |inline bool put(const char* s);|
};

// Escaping printers work like any other printer except that any added character
// are checked for escaping sequences. This one would escape a string such that
// it can safely be embedded in a JS string.
template <typename Delegate, typename Escape>
class JS_PUBLIC_API EscapePrinter final : public GenericPrinter {
  size_t lengthOfSafeChars(const char* s, size_t len) {
    for (size_t i = 0; i < len; i++) {
      if (!esc.isSafeChar(s[i])) {
        return i;
      }
    }
    return len;
  }

 private:
  Delegate& out;
  Escape& esc;

 public:
  EscapePrinter(Delegate& out, Escape& esc) : out(out), esc(esc) {}
  ~EscapePrinter() {}

  using GenericPrinter::put;
  void put(const char* s, size_t len) override {
    const char* b = s;
    while (len) {
      size_t index = lengthOfSafeChars(b, len);
      if (index) {
        out.put(b, index);
        len -= index;
        b += index;
      }
      if (len) {
        esc.convertInto(out, char16_t(*b));
        len -= 1;
        b += 1;
      }
    }
  }

  inline void putChar(const char c) override {
    if (esc.isSafeChar(char16_t(c))) {
      out.putChar(char(c));
      return;
    }
    esc.convertInto(out, char16_t(c));
  }

  inline void putChar(const JS::Latin1Char c) override {
    if (esc.isSafeChar(char16_t(c))) {
      out.putChar(char(c));
      return;
    }
    esc.convertInto(out, char16_t(c));
  }

  inline void putChar(const char16_t c) override {
    if (esc.isSafeChar(c)) {
      out.putChar(char(c));
      return;
    }
    esc.convertInto(out, c);
  }

  // Forward calls to delegated printer.
  bool canPutFromIndex() const override { return out.canPutFromIndex(); }
  void putFromIndex(size_t index, size_t length) final {
    out.putFromIndex(index, length);
  }
  size_t index() const final {return out.index(); }
  void flush()final { out.flush(); }
  void reportOutOfMemory() final { out.reportOutOfMemory(); }
  bool hadOutOfMemory() const final { return out.hadOutOfMemory(); }
};

class JS_PUBLIC_API JSONEscape {
 public:
  inline bool isSafeChar(char16_t c) {
    return js::IsAsciiPrintable(c) && c != '"' && c != '\\';
  }
  void convertInto(GenericPrinter& out, char16_t c);
};

class JS_PUBLIC_API StringEscape {
 private:
  const char quote = '\0';
 public:
  explicit StringEscape(const char quote = '\0') : quote(quote) {}

  inline bool isSafeChar(char16_t c) {
    return js::IsAsciiPrintable(c) && c != quote && c != '\\';
  }
  void convertInto(GenericPrinter& out, char16_t c);
};

// Map escaped code to the letter/symbol escaped with a backslash.
extern const char js_EscapeMap[];

// Return a C-string containing the chars in str, with any non-printing chars
// escaped. If the optional quote parameter is present and is not '\0', quotes
// (as specified by the quote argument) are also escaped, and the quote
// character is appended at the beginning and end of the result string.
// The returned string is guaranteed to contain only ASCII characters.
extern JS_PUBLIC_API JS::UniqueChars QuoteString(JSContext* cx, JSString* str,
                                                 char quote = '\0');

// Appends the quoted string to the given Sprinter. Follows the same semantics
// as QuoteString from above.
extern JS_PUBLIC_API bool QuoteString(Sprinter* sp, JSString* str,
                                      char quote = '\0');

// Appends the quoted string to the given Sprinter. Follows the same
// Appends the JSON quoted string to the given Sprinter.
extern JS_PUBLIC_API bool JSONQuoteString(Sprinter* sp, JSString* str);

// Internal implementation code for QuoteString methods above.
enum class QuoteTarget { String, JSON };

template <QuoteTarget target, typename CharT>
bool JS_PUBLIC_API QuoteString(Sprinter* sp,
                               const mozilla::Range<const CharT> chars,
                               char quote = '\0');

}  // namespace js

#endif  // js_Printer_h
