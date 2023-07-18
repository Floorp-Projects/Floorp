/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Maybe.h"  // mozilla::Maybe
#include "mozilla/Utf8.h"  // mozilla::IsTrailingUnit, mozilla::Utf8Unit, mozilla::DecodeOneUtf8CodePoint

#include <inttypes.h>  // UINT8_MAX
#include <stdint.h>    // uint16_t

#include "js/Exception.h"   // JS_IsExceptionPending, JS_ClearPendingException
#include "js/RootingAPI.h"  // JS::Rooted, JS::MutableHandle
#include "jsapi-tests/tests.h"  // BEGIN_TEST, END_TEST, CHECK
#include "vm/JSAtomUtils.h"     // js::AtomizeChars, js::AtomizeUTF8Chars
#include "vm/StringType.h"      // JSAtom

using mozilla::DecodeOneUtf8CodePoint;
using mozilla::IsAscii;
using mozilla::IsTrailingUnit;
using mozilla::Maybe;
using mozilla::Utf8Unit;

using JS::Latin1Char;
using JS::MutableHandle;
using JS::Rooted;

BEGIN_TEST(testAtomizeTwoByteUTF8) {
  Rooted<JSAtom*> atom16(cx);
  Rooted<JSAtom*> atom8(cx);

  for (uint16_t i = 0; i <= UINT8_MAX; i++) {
    // Test cases where the first unit is ASCII.
    if (IsAscii(char16_t(i))) {
      for (uint16_t j = 0; j <= UINT8_MAX; j++) {
        if (IsAscii(char16_t(j))) {
          // If both units are ASCII, the sequence encodes a two-code point
          // string.
          if (!shouldBeTwoCodePoints(i, j, &atom16, &atom8)) {
            return false;
          }
        } else {
          // ASCII followed by non-ASCII should be invalid.
          if (!shouldBeInvalid(i, j)) {
            return false;
          }
        }
      }

      continue;
    }

    // Test remaining cases where the first unit isn't a two-byte lead.
    if ((i & 0b1110'0000) != 0b1100'0000) {
      for (uint16_t j = 0; j <= UINT8_MAX; j++) {
        // If the first unit isn't a two-byte lead, the sequence is invalid no
        // matter what the second unit is.
        if (!shouldBeInvalid(i, j)) {
          return false;
        }
      }

      continue;
    }

    // Test remaining cases where the first unit is the two-byte lead of a
    // non-Latin-1 code point.
    if (i >= 0b1100'0100) {
      for (uint16_t j = 0; j <= UINT8_MAX; j++) {
        if (IsTrailingUnit(Utf8Unit(static_cast<uint8_t>(j)))) {
          if (!shouldBeSingleNonLatin1(i, j, &atom16, &atom8)) {
            return false;
          }
        } else {
          if (!shouldBeInvalid(i, j)) {
            return false;
          }
        }
      }

      continue;
    }

    // Test remaining cases where the first unit is the two-byte lead of an
    // overlong ASCII code point.
    if (i < 0b1100'0010) {
      for (uint16_t j = 0; j <= UINT8_MAX; j++) {
        if (!shouldBeInvalid(i, j)) {
          return false;
        }
      }

      continue;
    }

    // Finally, test remaining cases where the first unit is the two-byte lead
    // of a Latin-1 code point.
    for (uint16_t j = 0; j <= UINT8_MAX; j++) {
      if (IsTrailingUnit(Utf8Unit(static_cast<uint8_t>(j)))) {
        if (!shouldBeSingleLatin1(i, j, &atom16, &atom8)) {
          return false;
        }
      } else {
        if (!shouldBeInvalid(i, j)) {
          return false;
        }
      }
    }
  }

  return true;
}

bool shouldBeTwoCodePoints(uint16_t first, uint16_t second,
                           MutableHandle<JSAtom*> atom16,
                           MutableHandle<JSAtom*> atom8) {
  CHECK(first <= UINT8_MAX);
  CHECK(second <= UINT8_MAX);
  CHECK(IsAscii(char16_t(first)));
  CHECK(IsAscii(char16_t(second)));

  const char16_t utf16[] = {static_cast<char16_t>(first),
                            static_cast<char16_t>(second)};
  atom16.set(js::AtomizeChars(cx, utf16, 2));
  CHECK(atom16);
  CHECK(atom16->length() == 2);
  CHECK(atom16->latin1OrTwoByteChar(0) == first);
  CHECK(atom16->latin1OrTwoByteChar(1) == second);

  const char utf8[] = {static_cast<char>(first), static_cast<char>(second)};
  atom8.set(js::AtomizeUTF8Chars(cx, utf8, 2));
  CHECK(atom8);
  CHECK(atom8->length() == 2);
  CHECK(atom8->latin1OrTwoByteChar(0) == first);
  CHECK(atom8->latin1OrTwoByteChar(1) == second);

  CHECK(atom16 == atom8);

  return true;
}

bool shouldBeOneCodePoint(uint16_t first, uint16_t second, char32_t v,
                          MutableHandle<JSAtom*> atom16,
                          MutableHandle<JSAtom*> atom8) {
  CHECK(first <= UINT8_MAX);
  CHECK(second <= UINT8_MAX);
  CHECK(v <= UINT16_MAX);

  const char16_t utf16[] = {static_cast<char16_t>(v)};
  atom16.set(js::AtomizeChars(cx, utf16, 1));
  CHECK(atom16);
  CHECK(atom16->length() == 1);
  CHECK(atom16->latin1OrTwoByteChar(0) == v);

  const char utf8[] = {static_cast<char>(first), static_cast<char>(second)};
  atom8.set(js::AtomizeUTF8Chars(cx, utf8, 2));
  CHECK(atom8);
  CHECK(atom8->length() == 1);
  CHECK(atom8->latin1OrTwoByteChar(0) == v);

  CHECK(atom16 == atom8);

  return true;
}

bool shouldBeSingleNonLatin1(uint16_t first, uint16_t second,
                             MutableHandle<JSAtom*> atom16,
                             MutableHandle<JSAtom*> atom8) {
  CHECK(first <= UINT8_MAX);
  CHECK(second <= UINT8_MAX);

  const char bytes[] = {static_cast<char>(first), static_cast<char>(second)};
  const char* iter = &bytes[1];
  Maybe<char32_t> cp =
      DecodeOneUtf8CodePoint(Utf8Unit(bytes[0]), &iter, bytes + 2);
  CHECK(cp.isSome());

  char32_t v = cp.value();
  CHECK(v > UINT8_MAX);

  return shouldBeOneCodePoint(first, second, v, atom16, atom8);
}

bool shouldBeSingleLatin1(uint16_t first, uint16_t second,
                          MutableHandle<JSAtom*> atom16,
                          MutableHandle<JSAtom*> atom8) {
  CHECK(first <= UINT8_MAX);
  CHECK(second <= UINT8_MAX);

  const char bytes[] = {static_cast<char>(first), static_cast<char>(second)};
  const char* iter = &bytes[1];
  Maybe<char32_t> cp =
      DecodeOneUtf8CodePoint(Utf8Unit(bytes[0]), &iter, bytes + 2);
  CHECK(cp.isSome());

  char32_t v = cp.value();
  CHECK(v <= UINT8_MAX);

  return shouldBeOneCodePoint(first, second, v, atom16, atom8);
}

bool shouldBeInvalid(uint16_t first, uint16_t second) {
  CHECK(first <= UINT8_MAX);
  CHECK(second <= UINT8_MAX);

  const char invalid[] = {static_cast<char>(first), static_cast<char>(second)};
  CHECK(!js::AtomizeUTF8Chars(cx, invalid, 2));
  CHECK(JS_IsExceptionPending(cx));
  JS_ClearPendingException(cx);

  return true;
}
END_TEST(testAtomizeTwoByteUTF8)
