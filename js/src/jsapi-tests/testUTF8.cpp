/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Range.h"

#include "js/CharacterEncoding.h"
#include "jsapi-tests/tests.h"

BEGIN_TEST(testUTF8_badUTF8) {
  static const char badUTF8[] = "...\xC0...";
  JSString* str = JS_NewStringCopyZ(cx, badUTF8);
  CHECK(str);
  char16_t ch;
  if (!JS_GetStringCharAt(cx, str, 3, &ch)) {
    return false;
  }
  CHECK(ch == 0x00C0);
  return true;
}
END_TEST(testUTF8_badUTF8)

BEGIN_TEST(testUTF8_bigUTF8) {
  static const char bigUTF8[] = "...\xFB\xBF\xBF\xBF\xBF...";
  JSString* str = JS_NewStringCopyZ(cx, bigUTF8);
  CHECK(str);
  char16_t ch;
  if (!JS_GetStringCharAt(cx, str, 3, &ch)) {
    return false;
  }
  CHECK(ch == 0x00FB);
  return true;
}
END_TEST(testUTF8_bigUTF8)

BEGIN_TEST(testUTF8_badSurrogate) {
  static const char16_t badSurrogate[] = {'A', 'B', 'C', 0xDEEE, 'D', 'E', 0};
  mozilla::Range<const char16_t> tbchars(badSurrogate, js_strlen(badSurrogate));
  JS::Latin1CharsZ latin1 = JS::LossyTwoByteCharsToNewLatin1CharsZ(cx, tbchars);
  CHECK(latin1);
  CHECK(latin1[3] == 0x00EE);
  return true;
}
END_TEST(testUTF8_badSurrogate)
