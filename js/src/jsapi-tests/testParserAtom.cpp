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

  ParserAtomsTable atomTable(cx->runtime());

  const char ascii[] = {};
  const JS::Latin1Char latin1[] = {};
  const mozilla::Utf8Unit utf8[] = {};
  const char16_t char16[] = {};

  const uint8_t bytes[] = {};
  const js::LittleEndianChars leTwoByte(bytes);

  // Check that the well-known empty atom matches for different entry points.
  const ParserAtom* ref = cx->parserNames().empty;
  CHECK(ref);
  CHECK(atomTable.internAscii(cx, ascii, 0).unwrap() == ref);
  CHECK(atomTable.internLatin1(cx, latin1, 0).unwrap() == ref);
  CHECK(atomTable.internUtf8(cx, utf8, 0).unwrap() == ref);
  CHECK(atomTable.internChar16(cx, char16, 0).unwrap() == ref);
  CHECK(atomTable.internChar16LE(cx, leTwoByte, 0).unwrap() == ref);

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

  const uint8_t bytes[] = {'a', 0};
  const js::LittleEndianChars leTwoByte(bytes);

  const ParserAtom* ref = cx->parserNames().lookupTiny(&a, 1);
  CHECK(ref);
  CHECK(atomTable.internAscii(cx, ascii, 1).unwrap() == ref);
  CHECK(atomTable.internLatin1(cx, latin1, 1).unwrap() == ref);
  CHECK(atomTable.internUtf8(cx, utf8, 1).unwrap() == ref);
  CHECK(atomTable.internChar16(cx, char16, 1).unwrap() == ref);
  CHECK(atomTable.internChar16LE(cx, leTwoByte, 1).unwrap() == ref);

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

  const uint8_t bytes[] = {'a', 0, '0', 0};
  const js::LittleEndianChars leTwoByte(bytes);

  const ParserAtom* ref = cx->parserNames().lookupTiny(ascii, 2);
  CHECK(ref);
  CHECK(atomTable.internAscii(cx, ascii, 2).unwrap() == ref);
  CHECK(atomTable.internLatin1(cx, latin1, 2).unwrap() == ref);
  CHECK(atomTable.internUtf8(cx, utf8, 2).unwrap() == ref);
  CHECK(atomTable.internChar16(cx, char16, 2).unwrap() == ref);
  CHECK(atomTable.internChar16LE(cx, leTwoByte, 2).unwrap() == ref);

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

BEGIN_TEST(testParserAtom_concat) {
  using js::frontend::ParserAtom;
  using js::frontend::ParserAtomsTable;

  ParserAtomsTable atomTable(cx->runtime());

  auto CheckConcat = [&](const char16_t* exp,
                         std::initializer_list<const char16_t*> args) -> bool {
    // Intern each argument literal
    std::vector<const ParserAtom*> inputs;
    for (const char16_t* arg : args) {
      size_t len = std::char_traits<char16_t>::length(arg);
      const ParserAtom* atom = atomTable.internChar16(cx, arg, len).unwrap();
      inputs.push_back(atom);
    }

    // Concatenate twice to test new vs existing pathways.
    mozilla::Range<const ParserAtom*> range(inputs.data(), inputs.size());
    const ParserAtom* once = atomTable.concatAtoms(cx, range).unwrap();
    const ParserAtom* twice = atomTable.concatAtoms(cx, range).unwrap();

    // Intern expected value literal _after_ the concat code to allow
    // allocation pathways a chance to be tested.
    size_t exp_len = std::char_traits<char16_t>::length(exp);
    const ParserAtom* ref = atomTable.internChar16(cx, exp, exp_len).unwrap();

    return (once == ref) && (twice == ref);
  };

  // Checks empty strings
  CHECK(CheckConcat(u"", {u"", u""}));
  CHECK(CheckConcat(u"", {u"", u"", u"", u""}));
  CHECK(CheckConcat(u"A", {u"", u"", u"A", u""}));
  CHECK(CheckConcat(u"AAAA", {u"", u"", u"AAAA", u""}));

  // Check WellKnown strings
  CHECK(CheckConcat(u"function", {u"fun", u"ction"}));
  CHECK(CheckConcat(u"object", {u"", u"object"}));
  CHECK(CheckConcat(u"objectNOTAWELLKNOWN", {u"object", u"NOTAWELLKNOWN"}));

  // Concat ASCII strings
  CHECK(CheckConcat(u"AAAA", {u"AAAA", u""}));
  CHECK(CheckConcat(u"AAAABBBB", {u"AAAA", u"BBBB"}));
  CHECK(CheckConcat(
      u"000000000011111111112222222222333333333344444444445555555555",
      {u"0000000000", u"1111111111", u"2222222222", u"3333333333",
       u"4444444444", u"5555555555"}));

  // Concat Latin1 strings
  CHECK(CheckConcat(u"\xE6_\xE6", {u"\xE6", u"_\xE6"}));
  CHECK(CheckConcat(u"\xE6_ae", {u"\xE6", u"_ae"}));

  // Concat char16 strings
  CHECK(CheckConcat(u"\u03C0_\xE6_A", {u"\u03C0", u"_\xE6", u"_A"}));
  CHECK(CheckConcat(u"\u03C0_\u03C0", {u"\u03C0", u"_\u03C0"}));
  CHECK(CheckConcat(u"\u03C0_\xE6", {u"\u03C0", u"_\xE6"}));
  CHECK(CheckConcat(u"\u03C0_A", {u"\u03C0", u"_A"}));

  return true;
}
END_TEST(testParserAtom_concat)

// "æ"    U+00E6
// "π"    U+03C0
// "🍕"   U+1F355
