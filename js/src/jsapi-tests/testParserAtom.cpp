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
#include "vm/ErrorContext.h"

// Test empty strings behave consistently.
BEGIN_TEST(testParserAtom_empty) {
  using js::frontend::ParserAtom;
  using js::frontend::ParserAtomsTable;
  using js::frontend::ParserAtomVector;
  using js::frontend::TaggedParserAtomIndex;

  js::MainThreadErrorContext ec(cx);
  js::LifoAlloc alloc(512);
  ParserAtomsTable atomTable(cx->runtime(), alloc);

  const char ascii[] = {};
  const JS::Latin1Char latin1[] = {};
  const mozilla::Utf8Unit utf8[] = {};
  const char16_t char16[] = {};

  // Check that the well-known empty atom matches for different entry points.
  auto refIndex = TaggedParserAtomIndex::WellKnown::empty();
  CHECK(atomTable.internAscii(&ec, ascii, 0) == refIndex);
  CHECK(atomTable.internLatin1(&ec, latin1, 0) == refIndex);
  CHECK(atomTable.internUtf8(&ec, utf8, 0) == refIndex);
  CHECK(atomTable.internChar16(&ec, char16, 0) == refIndex);

  return true;
}
END_TEST(testParserAtom_empty)

// Test length-1 fast-path is consistent across entry points for ASCII.
BEGIN_TEST(testParserAtom_tiny1_ASCII) {
  using js::frontend::ParserAtom;
  using js::frontend::ParserAtomsTable;
  using js::frontend::ParserAtomVector;

  js::MainThreadErrorContext ec(cx);
  js::LifoAlloc alloc(512);
  ParserAtomsTable atomTable(cx->runtime(), alloc);

  char16_t a = 'a';
  const char ascii[] = {'a'};
  JS::Latin1Char latin1[] = {'a'};
  const mozilla::Utf8Unit utf8[] = {mozilla::Utf8Unit('a')};
  char16_t char16[] = {'a'};

  auto refIndex = cx->runtime()->commonParserNames->lookupTinyIndex(&a, 1);
  CHECK(refIndex);
  CHECK(atomTable.internAscii(&ec, ascii, 1) == refIndex);
  CHECK(atomTable.internLatin1(&ec, latin1, 1) == refIndex);
  CHECK(atomTable.internUtf8(&ec, utf8, 1) == refIndex);
  CHECK(atomTable.internChar16(&ec, char16, 1) == refIndex);

  return true;
}
END_TEST(testParserAtom_tiny1_ASCII)

// Test length-1 fast-path is consistent across entry points for non-ASCII.
BEGIN_TEST(testParserAtom_tiny1_nonASCII) {
  using js::frontend::ParserAtom;
  using js::frontend::ParserAtomsTable;
  using js::frontend::ParserAtomVector;

  js::MainThreadErrorContext ec(cx);
  js::LifoAlloc alloc(512);
  ParserAtomsTable atomTable(cx->runtime(), alloc);

  {
    char16_t euro = 0x0080;
    JS::Latin1Char latin1[] = {0x80};
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC2)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0x80))};
    char16_t char16[] = {0x0080};

    auto refIndex = cx->runtime()->commonParserNames->lookupTinyIndex(&euro, 1);
    CHECK(refIndex);
    CHECK(atomTable.internLatin1(&ec, latin1, 1) == refIndex);
    CHECK(atomTable.internUtf8(&ec, utf8, 2) == refIndex);
    CHECK(atomTable.internChar16(&ec, char16, 1) == refIndex);
  }

  {
    char16_t frac12 = 0x00BD;
    JS::Latin1Char latin1[] = {0xBD};
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC2)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0xBD))};
    char16_t char16[] = {0x00BD};

    auto refIndex =
        cx->runtime()->commonParserNames->lookupTinyIndex(&frac12, 1);
    CHECK(refIndex);
    CHECK(atomTable.internLatin1(&ec, latin1, 1) == refIndex);
    CHECK(atomTable.internUtf8(&ec, utf8, 2) == refIndex);
    CHECK(atomTable.internChar16(&ec, char16, 1) == refIndex);
  }

  {
    char16_t iquest = 0x00BF;
    JS::Latin1Char latin1[] = {0xBF};
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC2)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0xBF))};
    char16_t char16[] = {0x00BF};

    auto refIndex =
        cx->runtime()->commonParserNames->lookupTinyIndex(&iquest, 1);
    CHECK(refIndex);
    CHECK(atomTable.internLatin1(&ec, latin1, 1) == refIndex);
    CHECK(atomTable.internUtf8(&ec, utf8, 2) == refIndex);
    CHECK(atomTable.internChar16(&ec, char16, 1) == refIndex);
  }

  {
    char16_t agrave = 0x00C0;
    JS::Latin1Char latin1[] = {0xC0};
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC3)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0x80))};
    char16_t char16[] = {0x00C0};

    auto refIndex =
        cx->runtime()->commonParserNames->lookupTinyIndex(&agrave, 1);
    CHECK(refIndex);
    CHECK(atomTable.internLatin1(&ec, latin1, 1) == refIndex);
    CHECK(atomTable.internUtf8(&ec, utf8, 2) == refIndex);
    CHECK(atomTable.internChar16(&ec, char16, 1) == refIndex);
  }

  {
    char16_t ae = 0x00E6;
    JS::Latin1Char latin1[] = {0xE6};
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC3)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0xA6))};
    char16_t char16[] = {0x00E6};

    auto refIndex = cx->runtime()->commonParserNames->lookupTinyIndex(&ae, 1);
    CHECK(refIndex);
    CHECK(atomTable.internLatin1(&ec, latin1, 1) == refIndex);
    CHECK(atomTable.internUtf8(&ec, utf8, 2) == refIndex);
    CHECK(atomTable.internChar16(&ec, char16, 1) == refIndex);
  }

  {
    char16_t yuml = 0x00FF;
    JS::Latin1Char latin1[] = {0xFF};
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC3)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0xBF))};
    char16_t char16[] = {0x00FF};

    auto refIndex = cx->runtime()->commonParserNames->lookupTinyIndex(&yuml, 1);
    CHECK(refIndex);
    CHECK(atomTable.internLatin1(&ec, latin1, 1) == refIndex);
    CHECK(atomTable.internUtf8(&ec, utf8, 2) == refIndex);
    CHECK(atomTable.internChar16(&ec, char16, 1) == refIndex);
  }

  return true;
}
END_TEST(testParserAtom_tiny1_nonASCII)

// Test for tiny1 UTF-8 with valid/invalid code units.
//
// NOTE: Passing invalid UTF-8 to internUtf8 hits assertion failure, so
//       test in the opposite way.
//       lookupTinyIndexUTF8 is used inside internUtf8.
BEGIN_TEST(testParserAtom_tiny1_invalidUTF8) {
  using js::frontend::ParserAtom;
  using js::frontend::ParserAtomsTable;

  js::MainThreadErrorContext ec(cx);
  js::LifoAlloc alloc(512);
  ParserAtomsTable atomTable(cx->runtime(), alloc);

  {
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC1)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0x80))};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndexUTF8(utf8, 2));
  }

  {
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC2)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0x7F))};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndexUTF8(utf8, 2));
  }

  {
    JS::Latin1Char latin1[] = {0x80};
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC2)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0x80))};

    auto refIndex =
        cx->runtime()->commonParserNames->lookupTinyIndexUTF8(utf8, 2);
    CHECK(refIndex);
    CHECK(atomTable.internLatin1(&ec, latin1, 1) == refIndex);
  }

  {
    JS::Latin1Char latin1[] = {0xBF};
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC2)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0xBF))};

    auto refIndex =
        cx->runtime()->commonParserNames->lookupTinyIndexUTF8(utf8, 2);
    CHECK(refIndex);
    CHECK(atomTable.internLatin1(&ec, latin1, 1) == refIndex);
  }

  {
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC2)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC0))};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndexUTF8(utf8, 2));
  }

  {
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC3)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0x7F))};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndexUTF8(utf8, 2));
  }

  {
    JS::Latin1Char latin1[] = {0xC0};
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC3)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0x80))};

    auto refIndex =
        cx->runtime()->commonParserNames->lookupTinyIndexUTF8(utf8, 2);
    CHECK(refIndex);
    CHECK(atomTable.internLatin1(&ec, latin1, 1) == refIndex);
  }

  {
    JS::Latin1Char latin1[] = {0xFF};
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC3)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0xBF))};

    auto refIndex =
        cx->runtime()->commonParserNames->lookupTinyIndexUTF8(utf8, 2);
    CHECK(refIndex);
    CHECK(atomTable.internLatin1(&ec, latin1, 1) == refIndex);
  }

  {
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC3)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC0))};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndexUTF8(utf8, 2));
  }

  {
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC4)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0x7F))};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndexUTF8(utf8, 2));
  }

  {
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC4)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0x80))};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndexUTF8(utf8, 2));
  }

  {
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC4)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0xBF))};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndexUTF8(utf8, 2));
  }

  {
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC4)),
        mozilla::Utf8Unit(static_cast<unsigned char>(0xC0))};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndexUTF8(utf8, 2));
  }

  return true;
}
END_TEST(testParserAtom_tiny1_invalidUTF8)

// Test length-2 fast-path is consistent across entry points.
BEGIN_TEST(testParserAtom_tiny2) {
  using js::frontend::ParserAtom;
  using js::frontend::ParserAtomsTable;
  using js::frontend::ParserAtomVector;

  js::MainThreadErrorContext ec(cx);
  js::LifoAlloc alloc(512);
  ParserAtomsTable atomTable(cx->runtime(), alloc);

  const char ascii[] = {'a', '0'};
  JS::Latin1Char latin1[] = {'a', '0'};
  const mozilla::Utf8Unit utf8[] = {mozilla::Utf8Unit('a'),
                                    mozilla::Utf8Unit('0')};
  char16_t char16[] = {'a', '0'};

  auto refIndex = cx->runtime()->commonParserNames->lookupTinyIndex(ascii, 2);
  CHECK(refIndex);
  CHECK(atomTable.internAscii(&ec, ascii, 2) == refIndex);
  CHECK(atomTable.internLatin1(&ec, latin1, 2) == refIndex);
  CHECK(atomTable.internUtf8(&ec, utf8, 2) == refIndex);
  CHECK(atomTable.internChar16(&ec, char16, 2) == refIndex);

  // Note: If Latin1-Extended characters become supported, then UTF-8 behaviour
  // should be tested.
  char16_t ae0[] = {0x00E6, '0'};
  CHECK(!cx->runtime()->commonParserNames->lookupTinyIndex(ae0, 2));

  return true;
}
END_TEST(testParserAtom_tiny2)

// Test length-3 fast-path is consistent across entry points.
BEGIN_TEST(testParserAtom_int) {
  using js::frontend::ParserAtom;
  using js::frontend::ParserAtomsTable;
  using js::frontend::ParserAtomVector;

  js::MainThreadErrorContext ec(cx);
  js::LifoAlloc alloc(512);
  ParserAtomsTable atomTable(cx->runtime(), alloc);

  {
    const char ascii[] = {'1', '0', '0'};
    JS::Latin1Char latin1[] = {'1', '0', '0'};
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit('1'), mozilla::Utf8Unit('0'), mozilla::Utf8Unit('0')};
    char16_t char16[] = {'1', '0', '0'};

    auto refIndex = cx->runtime()->commonParserNames->lookupTinyIndex(ascii, 3);
    CHECK(refIndex);
    CHECK(atomTable.internAscii(&ec, ascii, 3) == refIndex);
    CHECK(atomTable.internLatin1(&ec, latin1, 3) == refIndex);
    CHECK(atomTable.internUtf8(&ec, utf8, 3) == refIndex);
    CHECK(atomTable.internChar16(&ec, char16, 3) == refIndex);
  }

  {
    const char ascii[] = {'2', '5', '5'};
    JS::Latin1Char latin1[] = {'2', '5', '5'};
    const mozilla::Utf8Unit utf8[] = {
        mozilla::Utf8Unit('2'), mozilla::Utf8Unit('5'), mozilla::Utf8Unit('5')};
    char16_t char16[] = {'2', '5', '5'};

    auto refIndex = cx->runtime()->commonParserNames->lookupTinyIndex(ascii, 3);
    CHECK(refIndex);
    CHECK(atomTable.internAscii(&ec, ascii, 3) == refIndex);
    CHECK(atomTable.internLatin1(&ec, latin1, 3) == refIndex);
    CHECK(atomTable.internUtf8(&ec, utf8, 3) == refIndex);
    CHECK(atomTable.internChar16(&ec, char16, 3) == refIndex);
  }

  {
    const char ascii[] = {'0', '9', '9'};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndex(ascii, 3));
  }

  {
    const char ascii[] = {'0', 'F', 'F'};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndex(ascii, 3));
  }

  {
    const char ascii[] = {'1', '0', 'A'};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndex(ascii, 3));
  }

  {
    const char ascii[] = {'1', '0', 'a'};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndex(ascii, 3));
  }

  {
    const char ascii[] = {'2', '5', '6'};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndex(ascii, 3));
  }

  {
    const char ascii[] = {'3', '0', '0'};

    CHECK(!cx->runtime()->commonParserNames->lookupTinyIndex(ascii, 3));
  }

  return true;
}
END_TEST(testParserAtom_int)

// "€"    U+0080
// "½"    U+00BD
// "¿"    U+00BF
// "À"    U+00C0
// "æ"    U+00E6
// "ÿ"    U+00FF
// "π"    U+03C0
// "🍕"   U+1F355
