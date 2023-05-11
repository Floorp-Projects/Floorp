/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>

#include "js/JSON.h"
#include "jsapi-tests/tests.h"

using namespace JS;

BEGIN_FRONTEND_TEST(testIsValidJSONLatin1) {
  const char* source;

  source = "true";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "false";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "null";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "0";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "1";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "-1";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "1.75";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "9000000000";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "\"foo\"";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "[]";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "[1, true]";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "{}";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "{\"key\": 10}";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "{\"key\": 10, \"prop\": 20}";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  source = "1 ";
  CHECK(IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                    strlen(source)));

  // Invalid cases.

  source = "";
  CHECK(!IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                     strlen(source)));

  source = "1 1";
  CHECK(!IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                     strlen(source)));

  source = ".1";
  CHECK(!IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                     strlen(source)));

  source = "undefined";
  CHECK(!IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                     strlen(source)));

  source = "TRUE";
  CHECK(!IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                     strlen(source)));

  source = "'foo'";
  CHECK(!IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                     strlen(source)));

  source = "[";
  CHECK(!IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                     strlen(source)));

  source = "{";
  CHECK(!IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                     strlen(source)));

  source = "/a/";
  CHECK(!IsValidJSON(reinterpret_cast<const JS::Latin1Char*>(source),
                     strlen(source)));

  return true;
}

END_TEST(testIsValidJSONLatin1)

BEGIN_FRONTEND_TEST(testIsValidJSONTwoBytes) {
  const char16_t* source;

  source = u"true";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"false";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"null";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"0";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"1";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"-1";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"1.75";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"9000000000";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"\"foo\"";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"[]";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"[1, true]";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"{}";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"{\"key\": 10}";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"{\"key\": 10, \"prop\": 20}";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"1 ";
  CHECK(IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  // Invalid cases.

  source = u"";
  CHECK(!IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"1 1";
  CHECK(!IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u".1";
  CHECK(!IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"undefined";
  CHECK(!IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"TRUE";
  CHECK(!IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"'foo'";
  CHECK(!IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"[";
  CHECK(!IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"{";
  CHECK(!IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  source = u"/a/";
  CHECK(!IsValidJSON(source, std::char_traits<char16_t>::length(source)));

  return true;
}

END_TEST(testIsValidJSONTwoBytes)
