/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>

#include "jsfriendapi.h"

#include "jsapi-tests/tests.h"

static const struct TestTuple {
  /* The string being tested. */
  const char16_t* string;
  /* The number of characters from the string to use. */
  size_t length;
  /* Whether this string is an index. */
  bool isindex;
  /* If it's an index, what index it is.  Ignored if not an index. */
  uint32_t index;

  constexpr TestTuple(const char16_t* string, bool isindex, uint32_t index)
      : TestTuple(string, std::char_traits<char16_t>::length(string), isindex,
                  index) {}

  constexpr TestTuple(const char16_t* string, size_t length, bool isindex,
                      uint32_t index)
      : string(string), length(length), isindex(isindex), index(index) {}
} tests[] = {
    {u"0", true, 0},
    {u"1", true, 1},
    {u"2", true, 2},
    {u"9", true, 9},
    {u"10", true, 10},
    {u"15", true, 15},
    {u"16", true, 16},
    {u"17", true, 17},
    {u"99", true, 99},
    {u"100", true, 100},
    {u"255", true, 255},
    {u"256", true, 256},
    {u"257", true, 257},
    {u"999", true, 999},
    {u"1000", true, 1000},
    {u"4095", true, 4095},
    {u"4096", true, 4096},
    {u"9999", true, 9999},
    {u"1073741823", true, 1073741823},
    {u"1073741824", true, 1073741824},
    {u"1073741825", true, 1073741825},
    {u"2147483647", true, 2147483647},
    {u"2147483648", true, 2147483648u},
    {u"2147483649", true, 2147483649u},
    {u"4294967294", true, 4294967294u},
    {u"4294967295", false,
     0},  // Not an array index because need to be able to represent length
    {u"-1", false, 0},
    {u"abc", false, 0},
    {u" 0", false, 0},
    {u"0 ", false, 0},
    // Tests to make sure the passed-in length is taken into account
    {u"0 ", 1, true, 0},
    {u"123abc", 3, true, 123},
    {u"123abc", 2, true, 12},
};

BEGIN_TEST(testStringIsArrayIndex) {
  for (const auto& test : tests) {
    uint32_t index;
    bool isindex = js::StringIsArrayIndex(test.string, test.length, &index);
    CHECK_EQUAL(isindex, test.isindex);
    if (isindex) {
      CHECK_EQUAL(index, test.index);
    }
  }

  return true;
}
END_TEST(testStringIsArrayIndex)
