/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"  // mozilla::ArrayLength

#include "js/RootingAPI.h"      // JS::Rooted
#include "jsapi-tests/tests.h"  // BEGIN_TEST, END_TEST, CHECK
#include "vm/JSAtom.h"          // js::AtomizeChars, js::AtomizeUTF8Chars
#include "vm/StringType.h"      // JSAtom

using mozilla::ArrayLength;

using JS::Latin1Char;
using JS::Rooted;

BEGIN_TEST(testAtomizeUTF8AndUTF16Agree) {
  Rooted<JSAtom*> atom16(cx);
  Rooted<JSAtom*> atom8(cx);
  for (Latin1Char i = 0xFF; i >= 0x80; i--) {
    const char16_t utf16[] = {i};
    atom16 = js::AtomizeChars(cx, utf16, 1);
    CHECK(atom16);
    CHECK(atom16->latin1OrTwoByteChar(0) == i);
    CHECK(atom16->length() == 1);

    const char utf8[] = {static_cast<char>(0b1100'0000 | (i >> 6)),
                         static_cast<char>(0b1000'0000 | (i & 0b0011'1111))};
    atom8 = js::AtomizeUTF8Chars(cx, utf8, 2);
    CHECK(atom8);
    CHECK(atom8->latin1OrTwoByteChar(0) == i);
    CHECK(atom8->length() == 1);

    CHECK(atom16 == atom8);
  }

  return true;
}
END_TEST(testAtomizeUTF8AndUTF16Agree)
