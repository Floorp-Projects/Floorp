/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Maybe.h"  // mozilla::Maybe

#include <string>

#include "js/AllocPolicy.h"  // js::SystemAllocPolicy
#include "js/JSON.h"
#include "js/Vector.h"  // js::Vector
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

BEGIN_FRONTEND_TEST(testParseJSONWithHandler) {
  {
    MyHandler handler;

    const char* source =
        "{ \"prop1\": 10.5, \"prop\\uff12\": [true, false, null, \"Ascii\", "
        "\"\\u3042\\u3044\\u3046\", \"\\u0020\", \"\\u0080\"] }";
    CHECK(JS::ParseJSONWithHandler((const JS::Latin1Char*)source,
                                   std::char_traits<char>::length(source),
                                   &handler));

    size_t i = 0;

    CHECK(handler.events[i++] == MyHandler::Event::StartObject);

    // Non-escaped ASCII property name in Latin1 input should be passed with
    // Latin1.
    CHECK(handler.events[i++] == MyHandler::Event::Latin1Prop1);

    CHECK(handler.events[i++] == MyHandler::Event::Number);

    // Escaped non-Latin1 property name in Latin1 input should be passed with
    // TwoBytes.
    CHECK(handler.events[i++] == MyHandler::Event::TwoBytesProp2);
    CHECK(handler.events[i++] == MyHandler::Event::StartArray);
    CHECK(handler.events[i++] == MyHandler::Event::True);
    CHECK(handler.events[i++] == MyHandler::Event::False);
    CHECK(handler.events[i++] == MyHandler::Event::Null);

    // Non-escaped ASCII string in Latin1 input should be passed with Latin1.
    CHECK(handler.events[i++] == MyHandler::Event::Latin1Str1);

    // Escaped non-Latin1 string in Latin1 input should be passed with TwoBytes.
    CHECK(handler.events[i++] == MyHandler::Event::TwoBytesStr2);

    // Escaped ASCII-range string in Latin1 input should be passed with Latin1.
    CHECK(handler.events[i++] == MyHandler::Event::Latin1Str3);

    // Escaped Latin1-range string in Latin1 input should be passed with Latin1.
    CHECK(handler.events[i++] == MyHandler::Event::Latin1Str4);

    CHECK(handler.events[i++] == MyHandler::Event::EndArray);
    CHECK(handler.events[i++] == MyHandler::Event::EndObject);
    CHECK(handler.events.length() == i);
  }
  {
    MyHandler handler;

    const char* source = "{";
    CHECK(!JS::ParseJSONWithHandler((const JS::Latin1Char*)source,
                                    std::char_traits<char>::length(source),
                                    &handler));

    size_t i = 0;

    CHECK(handler.events[i++] == MyHandler::Event::StartObject);
    CHECK(handler.events[i++] == MyHandler::Event::Error);
    CHECK(handler.events.length() == i);
  }

  {
    MyHandler handler;

    const char16_t* source =
        u"{ \"prop1\": 10.5, \"prop\uff12\": [true, false, null, \"Ascii\", "
        u"\"\\u3042\\u3044\\u3046\", \"\\u0020\", \"\\u0080\"] }";
    CHECK(JS::ParseJSONWithHandler(
        source, std::char_traits<char16_t>::length(source), &handler));

    size_t i = 0;

    CHECK(handler.events[i++] == MyHandler::Event::StartObject);

    // Non-escaped ASCII property name in TwoBytes input should be passed with
    // TwoBytes.
    CHECK(handler.events[i++] == MyHandler::Event::TwoBytesProp1);

    CHECK(handler.events[i++] == MyHandler::Event::Number);

    // Escaped non-Latin1 property name in TwoBytes input should be passed with
    // TwoBytes.
    CHECK(handler.events[i++] == MyHandler::Event::TwoBytesProp2);

    CHECK(handler.events[i++] == MyHandler::Event::StartArray);
    CHECK(handler.events[i++] == MyHandler::Event::True);
    CHECK(handler.events[i++] == MyHandler::Event::False);
    CHECK(handler.events[i++] == MyHandler::Event::Null);

    // Non-escaped ASCII string in TwoBytes input should be passed with
    // TwoBytes.
    CHECK(handler.events[i++] == MyHandler::Event::TwoBytesStr1);

    // Escaped non-Latin1 string in TwoBytes input should be passed with
    // TwoBytes.
    CHECK(handler.events[i++] == MyHandler::Event::TwoBytesStr2);

    // Escaped ASCII-range string in TwoBytes input should be passed with
    // Latin1.
    CHECK(handler.events[i++] == MyHandler::Event::Latin1Str3);

    // Escaped Latin1-range string in TwoBytes input should be passed with
    // Latin1.
    CHECK(handler.events[i++] == MyHandler::Event::Latin1Str4);

    CHECK(handler.events[i++] == MyHandler::Event::EndArray);
    CHECK(handler.events[i++] == MyHandler::Event::EndObject);
    CHECK(handler.events.length() == i);
  }

  {
    MyHandler handler;

    const char16_t* source = u"{";
    CHECK(!JS::ParseJSONWithHandler(
        source, std::char_traits<char16_t>::length(source), &handler));

    size_t i = 0;

    CHECK(handler.events[i++] == MyHandler::Event::StartObject);
    CHECK(handler.events[i++] == MyHandler::Event::Error);
    CHECK(handler.events.length() == i);
  }

  // Verify the failure case is handled properly and no methods are called
  // after the failure.

  bool checkedLast = false;
  for (size_t failAt = 1; !checkedLast; failAt++) {
    MyHandler handler;
    handler.failAt.emplace(failAt);

    const char* source =
        "{ \"prop1\": 10.5, \"prop\\uff12\": [true, false, null, \"Ascii\", "
        "\"\\u3042\\u3044\\u3046\", \"\\u0020\", \"\\u0080\"] }";
    CHECK(!JS::ParseJSONWithHandler((const JS::Latin1Char*)source,
                                    std::char_traits<char>::length(source),
                                    &handler));

    CHECK(handler.events.length() == failAt);

    size_t i = 0;

    CHECK(handler.events[i++] == MyHandler::Event::StartObject);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::Latin1Prop1);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::Number);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::TwoBytesProp2);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::StartArray);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::True);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::False);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::Null);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::Latin1Str1);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::TwoBytesStr2);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::Latin1Str3);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::Latin1Str4);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::EndArray);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::EndObject);
    checkedLast = true;
  }

  checkedLast = false;
  for (size_t failAt = 1; !checkedLast; failAt++) {
    MyHandler handler;
    handler.failAt.emplace(failAt);

    const char16_t* source =
        u"{ \"prop1\": 10.5, \"prop\uff12\": [true, false, null, \"Ascii\", "
        u"\"\\u3042\\u3044\\u3046\", \"\\u0020\", \"\\u0080\"] }";
    CHECK(!JS::ParseJSONWithHandler(
        source, std::char_traits<char16_t>::length(source), &handler));

    CHECK(handler.events.length() == failAt);

    size_t i = 0;

    CHECK(handler.events[i++] == MyHandler::Event::StartObject);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::TwoBytesProp1);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::Number);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::TwoBytesProp2);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::StartArray);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::True);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::False);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::Null);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::TwoBytesStr1);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::TwoBytesStr2);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::Latin1Str3);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::Latin1Str4);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::EndArray);
    if (i >= failAt) {
      continue;
    }
    CHECK(handler.events[i++] == MyHandler::Event::EndObject);
    checkedLast = true;
  }

  {
    const size_t failAt = 1;
    MyHandler handler;
    const char16_t* source;

#define IMMEDIATE_FAIL(json)                                          \
  handler.failAt.emplace(failAt);                                     \
  source = json;                                                      \
  CHECK(!JS::ParseJSONWithHandler(                                    \
      source, std::char_traits<char16_t>::length(source), &handler)); \
  CHECK(handler.events.length() == failAt);                           \
  handler.events.clear();

    IMMEDIATE_FAIL(u"{");
    IMMEDIATE_FAIL(u"[");
    IMMEDIATE_FAIL(u"\"string\"");
    IMMEDIATE_FAIL(u"1");
    IMMEDIATE_FAIL(u"true");
    IMMEDIATE_FAIL(u"null");

#undef IMMEDIATE_FAIL
  }

  return true;
}

class MyHandler : public JS::JSONParseHandler {
 public:
  enum class Event {
    Uninitialized = 0,

    StartObject,
    Latin1Prop1,
    TwoBytesProp1,
    Number,
    TwoBytesProp2,
    StartArray,
    True,
    False,
    Null,
    Latin1Str1,
    TwoBytesStr1,
    TwoBytesStr2,
    Latin1Str3,
    Latin1Str4,
    EndArray,
    EndObject,
    Error,
    UnexpectedNumber,
    UnexpectedLatin1Prop,
    UnexpectedTwoBytesProp,
    UnexpectedLatin1String,
    UnexpectedTwoBytesString,
  };
  js::Vector<Event, 0, js::SystemAllocPolicy> events;
  mozilla::Maybe<size_t> failAt;

  MyHandler() {}
  virtual ~MyHandler() {}

  bool startObject() override {
    MOZ_ALWAYS_TRUE(events.append(Event::StartObject));
    if (failAt.isSome() && events.length() == *failAt) {
      failAt.reset();
      return false;
    }
    return true;
  }
  bool propertyName(const JS::Latin1Char* name, size_t length) override {
    if (length == 5 && name[0] == 'p' && name[1] == 'r' && name[2] == 'o' &&
        name[3] == 'p' && name[4] == '1') {
      MOZ_ALWAYS_TRUE(events.append(Event::Latin1Prop1));
    } else {
      MOZ_ALWAYS_TRUE(events.append(Event::UnexpectedLatin1Prop));
    }
    if (failAt.isSome() && events.length() == *failAt) {
      failAt.reset();
      return false;
    }
    return true;
  }
  bool propertyName(const char16_t* name, size_t length) override {
    if (length == 5 && name[0] == 'p' && name[1] == 'r' && name[2] == 'o' &&
        name[3] == 'p' && name[4] == '1') {
      MOZ_ALWAYS_TRUE(events.append(Event::TwoBytesProp1));
    } else if (length == 5 && name[0] == 'p' && name[1] == 'r' &&
               name[2] == 'o' && name[3] == 'p' && name[4] == 0xff12) {
      MOZ_ALWAYS_TRUE(events.append(Event::TwoBytesProp2));
    } else {
      MOZ_ALWAYS_TRUE(events.append(Event::UnexpectedTwoBytesProp));
    }
    if (failAt.isSome() && events.length() == *failAt) {
      failAt.reset();
      return false;
    }
    return true;
  }
  bool endObject() override {
    MOZ_ALWAYS_TRUE(events.append(Event::EndObject));
    if (failAt.isSome() && events.length() == *failAt) {
      failAt.reset();
      return false;
    }
    return true;
  }

  bool startArray() override {
    MOZ_ALWAYS_TRUE(events.append(Event::StartArray));
    if (failAt.isSome() && events.length() == *failAt) {
      failAt.reset();
      return false;
    }
    return true;
  }
  bool endArray() override {
    MOZ_ALWAYS_TRUE(events.append(Event::EndArray));
    if (failAt.isSome() && events.length() == *failAt) {
      failAt.reset();
      return false;
    }
    return true;
  }

  bool stringValue(const JS::Latin1Char* name, size_t length) override {
    if (length == 5 && name[0] == 'A' && name[1] == 's' && name[2] == 'c' &&
        name[3] == 'i' && name[4] == 'i') {
      MOZ_ALWAYS_TRUE(events.append(Event::Latin1Str1));
    } else if (length == 1 && name[0] == ' ') {
      MOZ_ALWAYS_TRUE(events.append(Event::Latin1Str3));
    } else if (length == 1 && name[0] == 0x80) {
      MOZ_ALWAYS_TRUE(events.append(Event::Latin1Str4));
    } else {
      MOZ_ALWAYS_TRUE(events.append(Event::UnexpectedLatin1String));
    }
    if (failAt.isSome() && events.length() == *failAt) {
      failAt.reset();
      return false;
    }
    return true;
  }
  bool stringValue(const char16_t* name, size_t length) override {
    if (length == 5 && name[0] == 'A' && name[1] == 's' && name[2] == 'c' &&
        name[3] == 'i' && name[4] == 'i') {
      MOZ_ALWAYS_TRUE(events.append(Event::TwoBytesStr1));
    } else if (length == 3 && name[0] == 0x3042 && name[1] == 0x3044 &&
               name[2] == 0x3046) {
      MOZ_ALWAYS_TRUE(events.append(Event::TwoBytesStr2));
    } else {
      MOZ_ALWAYS_TRUE(events.append(Event::UnexpectedTwoBytesString));
    }
    if (failAt.isSome() && events.length() == *failAt) {
      failAt.reset();
      return false;
    }
    return true;
  }
  bool numberValue(double d) override {
    if (d == 10.5) {
      MOZ_ALWAYS_TRUE(events.append(Event::Number));
    } else {
      MOZ_ALWAYS_TRUE(events.append(Event::UnexpectedNumber));
    }
    if (failAt.isSome() && events.length() == *failAt) {
      failAt.reset();
      return false;
    }
    return true;
  }
  bool booleanValue(bool v) override {
    if (v) {
      MOZ_ALWAYS_TRUE(events.append(Event::True));
    } else {
      MOZ_ALWAYS_TRUE(events.append(Event::False));
    }
    if (failAt.isSome() && events.length() == *failAt) {
      failAt.reset();
      return false;
    }
    return true;
  }
  bool nullValue() override {
    MOZ_ALWAYS_TRUE(events.append(Event::Null));
    if (failAt.isSome() && events.length() == *failAt) {
      failAt.reset();
      return false;
    }
    return true;
  }

  void error(const char* msg, uint32_t line, uint32_t column) override {
    MOZ_ALWAYS_TRUE(events.append(Event::Error));
  }
};

END_TEST(testParseJSONWithHandler)
