/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Range.h"  // mozilla::Range
#include "mozilla/Utf8.h"   // mozilla::Utf8Unit

#include "frontend/ParserAtom.h"  // js::frontend::ParserAtomsTable
#include "js/TypeDecls.h"         // JS::Latin1Char
#include "jsapi-tests/tests.h"

// Test empty strings behave consistently.
BEGIN_TEST(testParserAtom_empty) {
  using js::frontend::ParserAtom;
  using js::frontend::ParserAtomsTable;

  ParserAtomsTable atomTable(cx->runtime());

  constexpr size_t len = 0;

  const char ascii[] = {};
  const JS::Latin1Char latin1[] = {};
  const mozilla::Utf8Unit utf8[] = {};
  const char16_t char16[] = {};

  // Check that the well-known empty atom matches for different entry points.
  const ParserAtom* ref = cx->parserNames().empty;
  CHECK(ref);
  CHECK(atomTable.internAscii(cx, ascii, len).unwrap() == ref);
  CHECK(atomTable.internLatin1(cx, latin1, len).unwrap() == ref);
  CHECK(atomTable.internUtf8(cx, utf8, len).unwrap() == ref);
  CHECK(atomTable.internChar16(cx, char16, len).unwrap() == ref);

  // Check concatenation works on empty atoms.
  const ParserAtom* concat[] = {
      cx->parserNames().empty,
      cx->parserNames().empty,
  };
  mozilla::Range<const ParserAtom*> concatRange(concat, 2);
  CHECK(atomTable.concatAtoms(cx, concatRange).unwrap() == ref);

  return true;
}
END_TEST(testParserAtom_empty)

// Test length-1 fast-path is consistent across entry points.
BEGIN_TEST(testParserAtom_tiny1) {
  using js::frontend::ParserAtom;
  using js::frontend::ParserAtomsTable;

  ParserAtomsTable atomTable(cx->runtime());

  char16_t a = 'a';
  const char ascii[] = {'a'};
  JS::Latin1Char latin1[] = {'a'};
  const mozilla::Utf8Unit utf8[] = {mozilla::Utf8Unit('a')};
  char16_t char16[] = {'a'};

  const ParserAtom* ref = cx->parserNames().lookupTiny(&a, 1);
  CHECK(ref);
  CHECK(atomTable.internAscii(cx, ascii, 1).unwrap() == ref);
  CHECK(atomTable.internLatin1(cx, latin1, 1).unwrap() == ref);
  CHECK(atomTable.internUtf8(cx, utf8, 1).unwrap() == ref);
  CHECK(atomTable.internChar16(cx, char16, 1).unwrap() == ref);

  const ParserAtom* concat[] = {
      ref,
      cx->parserNames().empty,
  };
  mozilla::Range<const ParserAtom*> concatRange(concat, 2);
  CHECK(atomTable.concatAtoms(cx, concatRange).unwrap() == ref);

  // Note: If Latin1-Extended characters become supported, then UTF-8 behaviour
  // should be tested.
  char16_t ae = 0x00E6;
  CHECK(cx->parserNames().lookupTiny(&ae, 1) == nullptr);

  return true;
}
END_TEST(testParserAtom_tiny1)

// Test length-2 fast-path is consistent across entry points.
BEGIN_TEST(testParserAtom_tiny2) {
  using js::frontend::ParserAtom;
  using js::frontend::ParserAtomsTable;

  ParserAtomsTable atomTable(cx->runtime());

  const char ascii[] = {'a', '0'};
  JS::Latin1Char latin1[] = {'a', '0'};
  const mozilla::Utf8Unit utf8[] = {mozilla::Utf8Unit('a'),
                                    mozilla::Utf8Unit('0')};
  char16_t char16[] = {'a', '0'};

  const ParserAtom* ref = cx->parserNames().lookupTiny(ascii, 2);
  CHECK(ref);
  CHECK(atomTable.internAscii(cx, ascii, 2).unwrap() == ref);
  CHECK(atomTable.internLatin1(cx, latin1, 2).unwrap() == ref);
  CHECK(atomTable.internUtf8(cx, utf8, 2).unwrap() == ref);
  CHECK(atomTable.internChar16(cx, char16, 2).unwrap() == ref);

  const ParserAtom* concat[] = {
      cx->parserNames().lookupTiny(ascii + 0, 1),
      cx->parserNames().lookupTiny(ascii + 1, 1),
  };
  mozilla::Range<const ParserAtom*> concatRange(concat, 2);
  CHECK(atomTable.concatAtoms(cx, concatRange).unwrap() == ref);

  // Note: If Latin1-Extended characters become supported, then UTF-8 behaviour
  // should be tested.
  char16_t ae0[] = {0x00E6, '0'};
  CHECK(cx->parserNames().lookupTiny(ae0, 2) == nullptr);

  return true;
}
END_TEST(testParserAtom_tiny2)

// "æ"    U+00E6
// "π"    U+03C0
// "🍕"   U+1F355
