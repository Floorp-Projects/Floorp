/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef util_Text_h
#define util_Text_h

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Casting.h"
#include "mozilla/Latin1.h"
#include "mozilla/Likely.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Utf8.h"

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <type_traits>
#include <utility>

#include "NamespaceImports.h"

#include "js/Utility.h"
#include "util/Unicode.h"
#include "vm/Printer.h"

class JSLinearString;

template <typename CharT>
static constexpr MOZ_ALWAYS_INLINE size_t js_strlen(const CharT* s) {
  return std::char_traits<CharT>::length(s);
}

template <typename CharT>
extern const CharT* js_strchr_limit(const CharT* s, char16_t c,
                                    const CharT* limit);

template <typename CharT>
static MOZ_ALWAYS_INLINE size_t js_strnlen(const CharT* s, size_t maxlen) {
  for (size_t i = 0; i < maxlen; ++i) {
    if (s[i] == '\0') {
      return i;
    }
  }
  return maxlen;
}

extern int32_t js_fputs(const char16_t* s, FILE* f);

namespace js {

class StringBuffer;

template <typename CharT>
constexpr uint8_t AsciiDigitToNumber(CharT c) {
  using UnsignedCharT = std::make_unsigned_t<CharT>;
  auto uc = static_cast<UnsignedCharT>(c);
  return uc - '0';
}

template <typename CharT>
static constexpr bool IsAsciiPrintable(CharT c) {
  using UnsignedCharT = std::make_unsigned_t<CharT>;
  auto uc = static_cast<UnsignedCharT>(c);
  return ' ' <= uc && uc <= '~';
}

template <typename Char1, typename Char2>
inline bool EqualChars(const Char1* s1, const Char2* s2, size_t len) {
  return mozilla::ArrayEqual(s1, s2, len);
}

// Return less than, equal to, or greater than zero depending on whether
// s1 is less than, equal to, or greater than s2.
template <typename Char1, typename Char2>
inline int32_t CompareChars(const Char1* s1, size_t len1, const Char2* s2,
                            size_t len2) {
  size_t n = std::min(len1, len2);
  for (size_t i = 0; i < n; i++) {
    if (int32_t cmp = s1[i] - s2[i]) {
      return cmp;
    }
  }

  return int32_t(len1 - len2);
}

// Return s advanced past any Unicode white space characters.
template <typename CharT>
static inline const CharT* SkipSpace(const CharT* s, const CharT* end) {
  MOZ_ASSERT(s <= end);

  while (s < end && unicode::IsSpace(*s)) {
    s++;
  }

  return s;
}

extern UniqueChars DuplicateStringToArena(arena_id_t destArenaId, JSContext* cx,
                                          const char* s);

extern UniqueChars DuplicateStringToArena(arena_id_t destArenaId, JSContext* cx,
                                          const char* s, size_t n);

extern UniqueLatin1Chars DuplicateStringToArena(arena_id_t destArenaId,
                                                JSContext* cx,
                                                const Latin1Char* s, size_t n);

extern UniqueTwoByteChars DuplicateStringToArena(arena_id_t destArenaId,
                                                 JSContext* cx,
                                                 const char16_t* s);

extern UniqueTwoByteChars DuplicateStringToArena(arena_id_t destArenaId,
                                                 JSContext* cx,
                                                 const char16_t* s, size_t n);

/*
 * These variants do not report OOMs, you must arrange for OOMs to be reported
 * yourself.
 */
extern UniqueChars DuplicateStringToArena(arena_id_t destArenaId,
                                          const char* s);

extern UniqueChars DuplicateStringToArena(arena_id_t destArenaId, const char* s,
                                          size_t n);

extern UniqueLatin1Chars DuplicateStringToArena(arena_id_t destArenaId,
                                                const JS::Latin1Char* s,
                                                size_t n);

extern UniqueTwoByteChars DuplicateStringToArena(arena_id_t destArenaId,
                                                 const char16_t* s);

extern UniqueTwoByteChars DuplicateStringToArena(arena_id_t destArenaId,
                                                 const char16_t* s, size_t n);

extern UniqueChars DuplicateString(JSContext* cx, const char* s);

extern UniqueChars DuplicateString(JSContext* cx, const char* s, size_t n);

extern UniqueLatin1Chars DuplicateString(JSContext* cx, const JS::Latin1Char* s,
                                         size_t n);

extern UniqueTwoByteChars DuplicateString(JSContext* cx, const char16_t* s);

extern UniqueTwoByteChars DuplicateString(JSContext* cx, const char16_t* s,
                                          size_t n);

/*
 * These variants do not report OOMs, you must arrange for OOMs to be reported
 * yourself.
 */
extern UniqueChars DuplicateString(const char* s);

extern UniqueChars DuplicateString(const char* s, size_t n);

extern UniqueLatin1Chars DuplicateString(const JS::Latin1Char* s, size_t n);

extern UniqueTwoByteChars DuplicateString(const char16_t* s);

extern UniqueTwoByteChars DuplicateString(const char16_t* s, size_t n);

/*
 * Inflate bytes in ASCII encoding to char16_t code units. Return null on error,
 * otherwise return the char16_t buffer that was malloc'ed. A null char is
 * appended.
 */
extern char16_t* InflateString(JSContext* cx, const char* bytes, size_t length);

/**
 * For a valid UTF-8, Latin-1, or WTF-16 code unit sequence, expose its contents
 * as the sequence of WTF-16 |char16_t| code units that would identically
 * constitute it.
 */
template <typename CharT>
class InflatedChar16Sequence {
 private:
  const CharT* units_;
  const CharT* limit_;

  static_assert(std::is_same_v<CharT, char16_t> ||
                    std::is_same_v<CharT, JS::Latin1Char>,
                "InflatedChar16Sequence only supports UTF-8/Latin-1/WTF-16");

 public:
  InflatedChar16Sequence(const CharT* units, size_t len)
      : units_(units), limit_(units_ + len) {}

  bool hasMore() { return units_ < limit_; }

  char16_t next() {
    MOZ_ASSERT(hasMore());
    return static_cast<char16_t>(*units_++);
  }

  HashNumber computeHash() const {
    auto copy = *this;
    HashNumber hash = 0;
    while (copy.hasMore()) {
      hash = mozilla::AddToHash(hash, copy.next());
    }
    return hash;
  }
};

template <>
class InflatedChar16Sequence<mozilla::Utf8Unit> {
 private:
  const mozilla::Utf8Unit* units_;
  const mozilla::Utf8Unit* limit_;

  char16_t pendingTrailingSurrogate_ = 0;

 public:
  InflatedChar16Sequence(const mozilla::Utf8Unit* units, size_t len)
      : units_(units), limit_(units + len) {}

  bool hasMore() { return pendingTrailingSurrogate_ || units_ < limit_; }

  char16_t next() {
    MOZ_ASSERT(hasMore());

    if (MOZ_UNLIKELY(pendingTrailingSurrogate_)) {
      char16_t trail = 0;
      std::swap(pendingTrailingSurrogate_, trail);
      return trail;
    }

    mozilla::Utf8Unit unit = *units_++;
    if (mozilla::IsAscii(unit)) {
      return static_cast<char16_t>(unit.toUint8());
    }

    mozilla::Maybe<char32_t> cp =
        mozilla::DecodeOneUtf8CodePoint(unit, &units_, limit_);
    MOZ_ASSERT(cp.isSome(), "input code unit sequence required to be valid");

    char32_t v = cp.value();
    if (v < unicode::NonBMPMin) {
      return mozilla::AssertedCast<char16_t>(v);
    }

    char16_t lead;
    unicode::UTF16Encode(v, &lead, &pendingTrailingSurrogate_);

    MOZ_ASSERT(unicode::IsLeadSurrogate(lead));

    MOZ_ASSERT(pendingTrailingSurrogate_ != 0,
               "pendingTrailingSurrogate_ must be nonzero to be detected and "
               "returned next go-around");
    MOZ_ASSERT(unicode::IsTrailSurrogate(pendingTrailingSurrogate_));

    return lead;
  }

  HashNumber computeHash() const {
    auto copy = *this;
    HashNumber hash = 0;
    while (copy.hasMore()) {
      hash = mozilla::AddToHash(hash, copy.next());
    }
    return hash;
  }
};

/*
 * Inflate bytes to JS chars in an existing buffer. 'dst' must be large
 * enough for 'srclen' char16_t code units. The buffer is NOT null-terminated.
 */
inline void CopyAndInflateChars(char16_t* dst, const char* src, size_t srclen) {
  mozilla::ConvertLatin1toUtf16(mozilla::Span(src, srclen),
                                mozilla::Span(dst, srclen));
}

inline void CopyAndInflateChars(char16_t* dst, const JS::Latin1Char* src,
                                size_t srclen) {
  mozilla::ConvertLatin1toUtf16(mozilla::AsChars(mozilla::Span(src, srclen)),
                                mozilla::Span(dst, srclen));
}

/*
 * Convert one UCS-4 char and write it into a UTF-8 buffer, which must be at
 * least 4 bytes long.  Return the number of UTF-8 bytes of data written.
 */
extern uint32_t OneUcs4ToUtf8Char(uint8_t* utf8Buffer, uint32_t ucs4Char);

extern size_t PutEscapedStringImpl(char* buffer, size_t size,
                                   GenericPrinter* out, JSLinearString* str,
                                   uint32_t quote);

template <typename CharT>
extern size_t PutEscapedStringImpl(char* buffer, size_t bufferSize,
                                   GenericPrinter* out, const CharT* chars,
                                   size_t length, uint32_t quote);

/*
 * Write str into buffer escaping any non-printable or non-ASCII character
 * using \escapes for JS string literals.
 * Guarantees that a NUL is at the end of the buffer unless size is 0. Returns
 * the length of the written output, NOT including the NUL. Thus, a return
 * value of size or more means that the output was truncated. If buffer
 * is null, just returns the length of the output. If quote is not 0, it must
 * be a single or double quote character that will quote the output.
 */
inline size_t PutEscapedString(char* buffer, size_t size, JSLinearString* str,
                               uint32_t quote) {
  size_t n = PutEscapedStringImpl(buffer, size, nullptr, str, quote);

  /* PutEscapedStringImpl can only fail with a file. */
  MOZ_ASSERT(n != size_t(-1));
  return n;
}

template <typename CharT>
inline size_t PutEscapedString(char* buffer, size_t bufferSize,
                               const CharT* chars, size_t length,
                               uint32_t quote) {
  size_t n =
      PutEscapedStringImpl(buffer, bufferSize, nullptr, chars, length, quote);

  /* PutEscapedStringImpl can only fail with a file. */
  MOZ_ASSERT(n != size_t(-1));
  return n;
}

inline bool EscapedStringPrinter(GenericPrinter& out, JSLinearString* str,
                                 uint32_t quote) {
  return PutEscapedStringImpl(nullptr, 0, &out, str, quote) != size_t(-1);
}

inline bool EscapedStringPrinter(GenericPrinter& out, const char* chars,
                                 size_t length, uint32_t quote) {
  return PutEscapedStringImpl(nullptr, 0, &out, chars, length, quote) !=
         size_t(-1);
}

/*
 * Write str into file escaping any non-printable or non-ASCII character.
 * If quote is not 0, it must be a single or double quote character that
 * will quote the output.
 */
inline bool FileEscapedString(FILE* fp, JSLinearString* str, uint32_t quote) {
  Fprinter out(fp);
  bool res = EscapedStringPrinter(out, str, quote);
  out.finish();
  return res;
}

inline bool FileEscapedString(FILE* fp, const char* chars, size_t length,
                              uint32_t quote) {
  Fprinter out(fp);
  bool res = EscapedStringPrinter(out, chars, length, quote);
  out.finish();
  return res;
}

JSString* EncodeURI(JSContext* cx, const char* chars, size_t length);

// Return true if input string contains a given flag in a comma separated list.
bool ContainsFlag(const char* str, const char* flag);

namespace unicode {

/** Compute the number of code points in the valid UTF-8 range [begin, end). */
extern size_t CountCodePoints(const mozilla::Utf8Unit* begin,
                              const mozilla::Utf8Unit* end);

/**
 * Count the number of code points in [begin, end).
 *
 * Unlike the UTF-8 case above, consistent with legacy ECMAScript practice,
 * every sequence of 16-bit units is considered valid.  Lone surrogates are
 * treated as if they represented a code point of the same value.
 */
extern size_t CountCodePoints(const char16_t* begin, const char16_t* end);

}  // namespace unicode

}  // namespace js

#endif  // util_Text_h
