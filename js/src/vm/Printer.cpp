/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Printer.h"

#include "mozilla/PodOperations.h"
#include "mozilla/Printf.h"
#include "mozilla/RangedPtr.h"

#include <stdarg.h>
#include <stdio.h>

#include "ds/LifoAlloc.h"
#include "js/CharacterEncoding.h"
#include "util/Memory.h"
#include "util/Text.h"
#include "util/WindowsWrapper.h"
#include "vm/JSContext.h"

using mozilla::PodCopy;

namespace {

class GenericPrinterPrintfTarget : public mozilla::PrintfTarget {
 public:
  explicit GenericPrinterPrintfTarget(js::GenericPrinter& p) : printer(p) {}

  bool append(const char* sp, size_t len) override {
    return printer.put(sp, len);
  }

 private:
  js::GenericPrinter& printer;
};

}  // namespace

namespace js {

void GenericPrinter::reportOutOfMemory() {
  if (hadOOM_) {
    return;
  }
  hadOOM_ = true;
}

bool GenericPrinter::hadOutOfMemory() const { return hadOOM_; }

bool GenericPrinter::printf(const char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  bool r = vprintf(fmt, va);
  va_end(va);
  return r;
}

bool GenericPrinter::vprintf(const char* fmt, va_list ap) {
  // Simple shortcut to avoid allocating strings.
  if (strchr(fmt, '%') == nullptr) {
    return put(fmt);
  }

  GenericPrinterPrintfTarget printer(*this);
  if (!printer.vprint(fmt, ap)) {
    reportOutOfMemory();
    return false;
  }
  return true;
}

const size_t Sprinter::DefaultSize = 64;

bool Sprinter::realloc_(size_t newSize) {
  MOZ_ASSERT(newSize > (size_t)offset);
  char* newBuf = (char*)js_realloc(base, newSize);
  if (!newBuf) {
    reportOutOfMemory();
    return false;
  }
  base = newBuf;
  size = newSize;
  base[size - 1] = '\0';
  return true;
}

Sprinter::Sprinter(JSContext* cx, bool shouldReportOOM)
    : context(cx),
#ifdef DEBUG
      initialized(false),
#endif
      shouldReportOOM(shouldReportOOM),
      base(nullptr),
      size(0),
      offset(0) {
}

Sprinter::~Sprinter() {
#ifdef DEBUG
  if (initialized) {
    checkInvariants();
  }
#endif
  js_free(base);
}

bool Sprinter::init() {
  MOZ_ASSERT(!initialized);
  base = js_pod_malloc<char>(DefaultSize);
  if (!base) {
    reportOutOfMemory();
    return false;
  }
#ifdef DEBUG
  initialized = true;
#endif
  *base = '\0';
  size = DefaultSize;
  base[size - 1] = '\0';
  return true;
}

void Sprinter::checkInvariants() const {
  MOZ_ASSERT(initialized);
  MOZ_ASSERT((size_t)offset < size);
  MOZ_ASSERT(base[size - 1] == '\0');
}

UniqueChars Sprinter::release() {
  checkInvariants();
  if (hadOOM_) {
    return nullptr;
  }

  char* str = base;
  base = nullptr;
  offset = size = 0;
#ifdef DEBUG
  initialized = false;
#endif
  return UniqueChars(str);
}

char* Sprinter::stringAt(ptrdiff_t off) const {
  MOZ_ASSERT(off >= 0 && (size_t)off < size);
  return base + off;
}

char& Sprinter::operator[](size_t off) {
  MOZ_ASSERT(off < size);
  return *(base + off);
}

char* Sprinter::reserve(size_t len) {
  InvariantChecker ic(this);

  while (len + 1 > size - offset) { /* Include trailing \0 */
    if (!realloc_(size * 2)) {
      return nullptr;
    }
  }

  char* sb = base + offset;
  offset += len;
  return sb;
}

bool Sprinter::put(const char* s, size_t len) {
  InvariantChecker ic(this);

  const char* oldBase = base;
  const char* oldEnd = base + size;

  char* bp = reserve(len);
  if (!bp) {
    return false;
  }

  /* s is within the buffer already */
  if (s >= oldBase && s < oldEnd) {
    /* buffer was realloc'ed */
    if (base != oldBase) {
      s = stringAt(s - oldBase); /* this is where it lives now */
    }
    memmove(bp, s, len);
  } else {
    js_memcpy(bp, s, len);
  }

  bp[len] = '\0';
  return true;
}

bool Sprinter::putString(JSString* s) {
  InvariantChecker ic(this);

  JSLinearString* linear = s->ensureLinear(context);
  if (!linear) {
    return false;
  }

  size_t length = JS::GetDeflatedUTF8StringLength(linear);

  char* buffer = reserve(length);
  if (!buffer) {
    return false;
  }

  mozilla::DebugOnly<size_t> written =
      JS::DeflateStringToUTF8Buffer(linear, mozilla::Span(buffer, length));
  MOZ_ASSERT(written == length);

  buffer[length] = '\0';
  return true;
}

ptrdiff_t Sprinter::getOffset() const { return offset; }

void Sprinter::reportOutOfMemory() {
  if (hadOOM_) {
    return;
  }
  if (context && shouldReportOOM) {
    ReportOutOfMemory(context);
  }
  hadOOM_ = true;
}

bool Sprinter::jsprintf(const char* format, ...) {
  va_list ap;
  va_start(ap, format);

  bool r = vprintf(format, ap);
  va_end(ap);

  return r;
}

const char js_EscapeMap[] = {
    // clang-format off
    '\b', 'b',
    '\f', 'f',
    '\n', 'n',
    '\r', 'r',
    '\t', 't',
    '\v', 'v',
    '"',  '"',
    '\'', '\'',
    '\\', '\\',
    '\0'
    // clang-format on
};

static const char JSONEscapeMap[] = {
    // clang-format off
    '\b', 'b',
    '\f', 'f',
    '\n', 'n',
    '\r', 'r',
    '\t', 't',
    '"',  '"',
    '\\', '\\',
    '\0'
    // clang-format on
};

template <QuoteTarget target, typename CharT>
bool QuoteString(Sprinter* sp, const mozilla::Range<const CharT> chars,
                 char quote) {
  MOZ_ASSERT_IF(target == QuoteTarget::JSON, quote == '\0');

  using CharPtr = mozilla::RangedPtr<const CharT>;

  const char* escapeMap =
      (target == QuoteTarget::String) ? js_EscapeMap : JSONEscapeMap;

  if (quote) {
    if (!sp->putChar(quote)) {
      return false;
    }
  }

  const CharPtr end = chars.end();

  /* Loop control variables: end points at end of string sentinel. */
  for (CharPtr t = chars.begin(); t < end; ++t) {
    /* Move t forward from s past un-quote-worthy characters. */
    const CharPtr s = t;
    char16_t c = *t;
    while (c < 127 && c != '\\') {
      if (target == QuoteTarget::String) {
        if (!IsAsciiPrintable(c) || c == quote || c == '\t') {
          break;
        }
      } else {
        if (c < ' ' || c == '"') {
          break;
        }
      }

      ++t;
      if (t == end) {
        break;
      }
      c = *t;
    }

    {
      ptrdiff_t len = t - s;
      ptrdiff_t base = sp->getOffset();
      if (!sp->reserve(len)) {
        return false;
      }

      for (ptrdiff_t i = 0; i < len; ++i) {
        (*sp)[base + i] = char(s[i]);
      }
      (*sp)[base + len] = '\0';
    }

    if (t == end) {
      break;
    }

    /* Use escapeMap, \u, or \x only if necessary. */
    const char* escape;
    if (!(c >> 8) && c != 0 &&
        (escape = strchr(escapeMap, int(c))) != nullptr) {
      if (!sp->jsprintf("\\%c", escape[1])) {
        return false;
      }
    } else {
      /*
       * Use \x only if the high byte is 0 and we're in a quoted string,
       * because ECMA-262 allows only \u, not \x, in Unicode identifiers
       * (see bug 621814).
       */
      if (!sp->jsprintf((quote && !(c >> 8)) ? "\\x%02X" : "\\u%04X", c)) {
        return false;
      }
    }
  }

  /* Sprint the closing quote and return the quoted string. */
  if (quote) {
    if (!sp->putChar(quote)) {
      return false;
    }
  }

  return true;
}

template bool QuoteString<QuoteTarget::String, Latin1Char>(
    Sprinter* sp, const mozilla::Range<const Latin1Char> chars, char quote);

template bool QuoteString<QuoteTarget::String, char16_t>(
    Sprinter* sp, const mozilla::Range<const char16_t> chars, char quote);

template bool QuoteString<QuoteTarget::JSON, Latin1Char>(
    Sprinter* sp, const mozilla::Range<const Latin1Char> chars, char quote);

template bool QuoteString<QuoteTarget::JSON, char16_t>(
    Sprinter* sp, const mozilla::Range<const char16_t> chars, char quote);

bool QuoteString(Sprinter* sp, JSString* str, char quote /*= '\0' */) {
  JSLinearString* linear = str->ensureLinear(sp->context);
  if (!linear) {
    return false;
  }

  JS::AutoCheckCannotGC nogc;
  return linear->hasLatin1Chars() ? QuoteString<QuoteTarget::String>(
                                        sp, linear->latin1Range(nogc), quote)
                                  : QuoteString<QuoteTarget::String>(
                                        sp, linear->twoByteRange(nogc), quote);
}

UniqueChars QuoteString(JSContext* cx, JSString* str, char quote /* = '\0' */) {
  Sprinter sprinter(cx);
  if (!sprinter.init()) {
    return nullptr;
  }
  if (!QuoteString(&sprinter, str, quote)) {
    return nullptr;
  }
  return sprinter.release();
}

bool JSONQuoteString(Sprinter* sp, JSString* str) {
  JSLinearString* linear = str->ensureLinear(sp->context);
  if (!linear) {
    return false;
  }

  JS::AutoCheckCannotGC nogc;
  return linear->hasLatin1Chars() ? QuoteString<QuoteTarget::JSON>(
                                        sp, linear->latin1Range(nogc), '\0')
                                  : QuoteString<QuoteTarget::JSON>(
                                        sp, linear->twoByteRange(nogc), '\0');
}

Fprinter::Fprinter(FILE* fp) : file_(nullptr), init_(false) { init(fp); }

#ifdef DEBUG
Fprinter::~Fprinter() { MOZ_ASSERT_IF(init_, !file_); }
#endif

bool Fprinter::init(const char* path) {
  MOZ_ASSERT(!file_);
  file_ = fopen(path, "w");
  if (!file_) {
    return false;
  }
  init_ = true;
  return true;
}

void Fprinter::init(FILE* fp) {
  MOZ_ASSERT(!file_);
  file_ = fp;
  init_ = false;
}

void Fprinter::flush() {
  MOZ_ASSERT(file_);
  fflush(file_);
}

void Fprinter::finish() {
  MOZ_ASSERT(file_);
  if (init_) {
    fclose(file_);
  }
  file_ = nullptr;
}

bool Fprinter::put(const char* s, size_t len) {
  MOZ_ASSERT(file_);
  int i = fwrite(s, /*size=*/1, /*nitems=*/len, file_);
  if (size_t(i) != len) {
    reportOutOfMemory();
    return false;
  }
#ifdef XP_WIN
  if ((file_ == stderr) && (IsDebuggerPresent())) {
    UniqueChars buf = DuplicateString(s, len);
    if (!buf) {
      reportOutOfMemory();
      return false;
    }
    OutputDebugStringA(buf.get());
  }
#endif
  return true;
}

LSprinter::LSprinter(LifoAlloc* lifoAlloc)
    : alloc_(lifoAlloc), head_(nullptr), tail_(nullptr), unused_(0) {}

LSprinter::~LSprinter() {
  // This LSprinter might be allocated as part of the same LifoAlloc, so we
  // should not expect the destructor to be called.
}

void LSprinter::exportInto(GenericPrinter& out) const {
  if (!head_) {
    return;
  }

  for (Chunk* it = head_; it != tail_; it = it->next) {
    out.put(it->chars(), it->length);
  }
  out.put(tail_->chars(), tail_->length - unused_);
}

void LSprinter::clear() {
  head_ = nullptr;
  tail_ = nullptr;
  unused_ = 0;
  hadOOM_ = false;
}

bool LSprinter::put(const char* s, size_t len) {
  // Compute how much data will fit in the current chunk.
  size_t existingSpaceWrite = 0;
  size_t overflow = len;
  if (unused_ > 0 && tail_) {
    existingSpaceWrite = std::min(unused_, len);
    overflow = len - existingSpaceWrite;
  }

  // If necessary, allocate a new chunk for overflow data.
  size_t allocLength = 0;
  Chunk* last = nullptr;
  if (overflow > 0) {
    allocLength =
        AlignBytes(sizeof(Chunk) + overflow, js::detail::LIFO_ALLOC_ALIGN);

    LifoAlloc::AutoFallibleScope fallibleAllocator(alloc_);
    last = reinterpret_cast<Chunk*>(alloc_->alloc(allocLength));
    if (!last) {
      reportOutOfMemory();
      return false;
    }
  }

  // All fallible operations complete: now fill up existing space, then
  // overflow space in any new chunk.
  MOZ_ASSERT(existingSpaceWrite + overflow == len);

  if (existingSpaceWrite > 0) {
    PodCopy(tail_->end() - unused_, s, existingSpaceWrite);
    unused_ -= existingSpaceWrite;
    s += existingSpaceWrite;
  }

  if (overflow > 0) {
    if (tail_ && reinterpret_cast<char*>(last) == tail_->end()) {
      // tail_ and last are consecutive in memory.  LifoAlloc has no
      // metadata and is just a bump allocator, so we can cheat by
      // appending the newly-allocated space to tail_.
      unused_ = allocLength;
      tail_->length += allocLength;
    } else {
      // Remove the size of the header from the allocated length.
      size_t availableSpace = allocLength - sizeof(Chunk);
      last->next = nullptr;
      last->length = availableSpace;

      unused_ = availableSpace;
      if (!head_) {
        head_ = last;
      } else {
        tail_->next = last;
      }

      tail_ = last;
    }

    PodCopy(tail_->end() - unused_, s, overflow);

    MOZ_ASSERT(unused_ >= overflow);
    unused_ -= overflow;
  }

  MOZ_ASSERT(len <= INT_MAX);
  return true;
}

}  // namespace js
