/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Range.h"  // mozilla::Range
#include "mozilla/Utf8.h"   // mozilla::Utf8Unit

#include <string>   // std::char_traits
#include <utility>  // std::initializer_list
#include <vector>   // std::vector

#include "frontend/ParserAtom.h"  // js::frontend::ParserAtomsTable
#include "js/TypeDecls.h"         // JS::Latin1Char
#include "jsapi-tests/tests.h"

// Test empty strings behave consistently.
BEGIN_TEST(testParserAtom_empty) {
  using js::frontend::ParserAtom;
  using js::frontend::ParserAtomsTable;
  using js::frontend::ParserAtomVector;
  using js::frontend::TaggedParserAtomIndex;

  js::LifoAlloc alloc(512);
  ParserAtomsTable atomTable(cx->runtime(), alloc);

  const char ascii[] = {};
  const JS::Latin1Char latin1[] = {};
  const mozilla::Utf8Unit utf8[] = {};
  const char16_t char16[] = {};

  const uint8_t bytes[] = {};
  const js::LittleEndianChars leTwoByte(bytes);

  // Check that the well-known empty atom matches for different entry points.
  auto refIndex = TaggedParserAtomIndex::WellKnown::empty();
  CHECK(atomTable.internAscii(cx, ascii, 0) == refIndex);
  CHECK(atomTable.internLatin1(cx, latin1, 0) == refIndex);
  CHECK(atomTable.internUtf8(cx, utf8, 0) == refIndex);
  CHECK(atomTable.internChar16(cx, char16, 0) == refIndex);

  return true;
}
END_TEST(testParserAtom_empty)

// Test length-1 fast-path is consistent across entry points.
BEGIN_TEST(testParserAtom_tiny1) {
  using js::frontend::ParserAtom;
  using js::frontend::ParserAtomsTable;
  using js::frontend::ParserAtomVector;

  js::LifoAlloc alloc(512);
  ParserAtomsTable atomTable(cx->runtime(), alloc);

  char16_t a = 'a';
  const char ascii[] = {'a'};
  JS::Latin1Char latin1[] = {'a'};
  const mozilla::Utf8Unit utf8[] = {mozilla::Utf8Unit('a')};
  char16_t char16[] = {'a'};

  const uint8_t bytes[] = {'a', 0};
  const js::LittleEndianChars leTwoByte(bytes);

  auto refIndex = cx->runtime()->commonParserNames->lookupTinyIndex(&a, 1);
  CHECK(refIndex);
  CHECK(atomTable.internAscii(cx, ascii, 1) == refIndex);
  CHECK(atomTable.internLatin1(cx, latin1, 1) == refIndex);
  CHECK(atomTable.internUtf8(cx, utf8, 1) == refIndex);
  CHECK(atomTable.internChar16(cx, char16, 1) == refIndex);

  // Note: If Latin1-Extended characters become supported, then UTF-8 behaviour
  // should be tested.
  char16_t ae = 0x00E6;
  CHECK(!cx->runtime()->commonParserNames->lookupTinyIndex(&ae, 1));

  return true;
}
END_TEST(testParserAtom_tiny1)

// Test length-2 fast-path is consistent across entry points.
BEGIN_TEST(testParserAtom_tiny2) {
  using js::frontend::ParserAtom;
  using js::frontend::ParserAtomsTable;
  using js::frontend::ParserAtomVector;

  js::LifoAlloc alloc(512);
  ParserAtomsTable atomTable(cx->runtime(), alloc);

  const char ascii[] = {'a', '0'};
  JS::Latin1Char latin1[] = {'a', '0'};
  const mozilla::Utf8Unit utf8[] = {mozilla::Utf8Unit('a'),
                                    mozilla::Utf8Unit('0')};
  char16_t char16[] = {'a', '0'};

  const uint8_t bytes[] = {'a', 0, '0', 0};
  const js::LittleEndianChars leTwoByte(bytes);

  auto refIndex = cx->runtime()->commonParserNames->lookupTinyIndex(ascii, 2);
  CHECK(refIndex);
  CHECK(atomTable.internAscii(cx, ascii, 2) == refIndex);
  CHECK(atomTable.internLatin1(cx, latin1, 2) == refIndex);
  CHECK(atomTable.internUtf8(cx, utf8, 2) == refIndex);
  CHECK(atomTable.internChar16(cx, char16, 2) == refIndex);

  // Note: If Latin1-Extended characters become supported, then UTF-8 behaviour
  // should be tested.
  char16_t ae0[] = {0x00E6, '0'};
  CHECK(!cx->runtime()->commonParserNames->lookupTinyIndex(ae0, 2));

  return true;
}
END_TEST(testParserAtom_tiny2)

// "æ"    U+00E6
// "π"    U+03C0
// "🍕"   U+1F355
