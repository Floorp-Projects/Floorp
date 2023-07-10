/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "jsapi.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SimpleGlobalObject.h"

using namespace mozilla::dom;

struct IsURIInListMatch {
  nsLiteralCString pattern;
  bool firstMatch, secondMatch;
};

std::ostream& operator<<(std::ostream& aStream,
                         const nsContentUtils::ParsedRange& aParsedRange) {
  if (aParsedRange.Start()) {
    aStream << *aParsedRange.Start();
  }

  aStream << "-";

  if (aParsedRange.End()) {
    aStream << *aParsedRange.End();
  }

  return aStream;
}

TEST(DOM_Base_ContentUtils, IsURIInList)
{
  nsCOMPtr<nsIURI> uri, subURI;
  nsresult rv = NS_NewURI(getter_AddRefs(uri),
                          "https://example.com/path/favicon.ico#"_ns);
  ASSERT_TRUE(rv == NS_OK);

  rv = NS_NewURI(getter_AddRefs(subURI),
                 "http://sub.example.com/favicon.ico?"_ns);
  ASSERT_TRUE(rv == NS_OK);

  static constexpr IsURIInListMatch patterns[] = {
      {"bar.com,*.example.com,example.com,foo.com"_ns, true, true},
      {"bar.com,example.com,*.example.com,foo.com"_ns, true, true},
      {"*.example.com,example.com,foo.com"_ns, true, true},
      {"example.com,*.example.com,foo.com"_ns, true, true},
      {"*.example.com,example.com"_ns, true, true},
      {"example.com,*.example.com"_ns, true, true},
      {"*.example.com/,example.com/"_ns, true, true},
      {"example.com/,*.example.com/"_ns, true, true},
      {"*.example.com/pa,example.com/pa"_ns, false, false},
      {"example.com/pa,*.example.com/pa"_ns, false, false},
      {"*.example.com/pa/,example.com/pa/"_ns, false, false},
      {"example.com/pa/,*.example.com/pa/"_ns, false, false},
      {"*.example.com/path,example.com/path"_ns, false, false},
      {"example.com/path,*.example.com/path"_ns, false, false},
      {"*.example.com/path/,example.com/path/"_ns, true, false},
      {"example.com/path/,*.example.com/path/"_ns, true, false},
      {"*.example.com/favicon.ico"_ns, false, true},
      {"example.com/path/favicon.ico"_ns, true, false},
      {"*.example.com"_ns, false, true},
      {"example.com"_ns, true, false},
      {"foo.com"_ns, false, false},
      {"*.foo.com"_ns, false, false},
  };

  for (auto& entry : patterns) {
    bool result = nsContentUtils::IsURIInList(uri, entry.pattern);
    ASSERT_EQ(result, entry.firstMatch) << "Matching " << entry.pattern;

    result = nsContentUtils::IsURIInList(subURI, entry.pattern);
    ASSERT_EQ(result, entry.secondMatch) << "Matching " << entry.pattern;
  }
}

TEST(DOM_Base_ContentUtils,
     StringifyJSON_EmptyValue_UndefinedIsNullStringLiteral)
{
  JS::Rooted<JSObject*> globalObject(
      mozilla::dom::RootingCx(),
      mozilla::dom::SimpleGlobalObject::Create(
          mozilla::dom::SimpleGlobalObject::GlobalType::BindingDetail));
  mozilla::dom::AutoJSAPI jsAPI;
  ASSERT_TRUE(jsAPI.Init(globalObject));
  JSContext* cx = jsAPI.cx();
  nsAutoString serializedValue;

  ASSERT_TRUE(nsContentUtils::StringifyJSON(cx, JS::UndefinedHandleValue,
                                            serializedValue,
                                            UndefinedIsNullStringLiteral));
  ASSERT_TRUE(serializedValue.EqualsLiteral("null"));
}

TEST(DOM_Base_ContentUtils, StringifyJSON_Object_UndefinedIsNullStringLiteral)
{
  JS::Rooted<JSObject*> globalObject(
      mozilla::dom::RootingCx(),
      mozilla::dom::SimpleGlobalObject::Create(
          mozilla::dom::SimpleGlobalObject::GlobalType::BindingDetail));
  mozilla::dom::AutoJSAPI jsAPI;
  ASSERT_TRUE(jsAPI.Init(globalObject));
  JSContext* cx = jsAPI.cx();
  nsAutoString serializedValue;

  JS::Rooted<JSObject*> jsObj(cx, JS_NewPlainObject(cx));
  JS::Rooted<JSString*> valueStr(cx, JS_NewStringCopyZ(cx, "Hello World!"));
  ASSERT_TRUE(JS_DefineProperty(cx, jsObj, "key1", valueStr, JSPROP_ENUMERATE));
  JS::Rooted<JS::Value> jsValue(cx, JS::ObjectValue(*jsObj));

  ASSERT_TRUE(nsContentUtils::StringifyJSON(cx, jsValue, serializedValue,
                                            UndefinedIsNullStringLiteral));

  ASSERT_TRUE(serializedValue.EqualsLiteral("{\"key1\":\"Hello World!\"}"));
}

TEST(DOM_Base_ContentUtils, StringifyJSON_EmptyValue_UndefinedIsVoidString)
{
  JS::Rooted<JSObject*> globalObject(
      mozilla::dom::RootingCx(),
      mozilla::dom::SimpleGlobalObject::Create(
          mozilla::dom::SimpleGlobalObject::GlobalType::BindingDetail));
  mozilla::dom::AutoJSAPI jsAPI;
  ASSERT_TRUE(jsAPI.Init(globalObject));
  JSContext* cx = jsAPI.cx();
  nsAutoString serializedValue;

  ASSERT_TRUE(nsContentUtils::StringifyJSON(
      cx, JS::UndefinedHandleValue, serializedValue, UndefinedIsVoidString));

  ASSERT_TRUE(serializedValue.IsVoid());
}

TEST(DOM_Base_ContentUtils, StringifyJSON_Object_UndefinedIsVoidString)
{
  JS::Rooted<JSObject*> globalObject(
      mozilla::dom::RootingCx(),
      mozilla::dom::SimpleGlobalObject::Create(
          mozilla::dom::SimpleGlobalObject::GlobalType::BindingDetail));
  mozilla::dom::AutoJSAPI jsAPI;
  ASSERT_TRUE(jsAPI.Init(globalObject));
  JSContext* cx = jsAPI.cx();
  nsAutoString serializedValue;

  JS::Rooted<JSObject*> jsObj(cx, JS_NewPlainObject(cx));
  JS::Rooted<JSString*> valueStr(cx, JS_NewStringCopyZ(cx, "Hello World!"));
  ASSERT_TRUE(JS_DefineProperty(cx, jsObj, "key1", valueStr, JSPROP_ENUMERATE));
  JS::Rooted<JS::Value> jsValue(cx, JS::ObjectValue(*jsObj));

  ASSERT_TRUE(nsContentUtils::StringifyJSON(cx, jsValue, serializedValue,
                                            UndefinedIsVoidString));

  ASSERT_TRUE(serializedValue.EqualsLiteral("{\"key1\":\"Hello World!\"}"));
}

TEST(DOM_Base_ContentUtils, ParseSingleRangeHeader)
{
  // Parsing a simple range should succeed
  EXPECT_EQ(nsContentUtils::ParseSingleRangeRequest("bytes=0-42"_ns, false),
            mozilla::Some(nsContentUtils::ParsedRange(mozilla::Some(0),
                                                      mozilla::Some(42))));

  // Range containing a invalid rangeStart should fail
  EXPECT_EQ(nsContentUtils::ParseSingleRangeRequest("bytes= t-200"_ns, true),
            mozilla::Nothing());

  // Range containing whitespace, with allowWhitespace=false should fail.
  EXPECT_EQ(nsContentUtils::ParseSingleRangeRequest("bytes= 2-200"_ns, false),
            mozilla::Nothing());

  // Range containing whitespace, with allowWhitespace=true should succeed
  EXPECT_EQ(
      nsContentUtils::ParseSingleRangeRequest("bytes \t= 2 - 200"_ns, true),
      mozilla::Some(
          nsContentUtils::ParsedRange(mozilla::Some(2), mozilla::Some(200))));

  // Range containing invalid whitespace should fail
  EXPECT_EQ(
      nsContentUtils::ParseSingleRangeRequest("bytes \r= 2 - 200"_ns, true),
      mozilla::Nothing());

  // Range without a rangeStart should succeed
  EXPECT_EQ(nsContentUtils::ParseSingleRangeRequest("bytes\t=\t-200"_ns, true),
            mozilla::Some(nsContentUtils::ParsedRange(mozilla::Nothing(),
                                                      mozilla::Some(200))));

  // Range without a rangeEnd should succeed
  EXPECT_EQ(nsContentUtils::ParseSingleRangeRequest("bytes=55-"_ns, true),
            mozilla::Some(nsContentUtils::ParsedRange(mozilla::Some(55),
                                                      mozilla::Nothing())));

  // Range without a rangeStart or rangeEnd should fail
  EXPECT_EQ(nsContentUtils::ParseSingleRangeRequest("bytes\t=\t-"_ns, true),
            mozilla::Nothing());

  // Range with extra characters should fail
  EXPECT_EQ(nsContentUtils::ParseSingleRangeRequest("bytes=0-42 "_ns, true),
            mozilla::Nothing());

  // Range with rangeStart > rangeEnd should fail
  EXPECT_EQ(nsContentUtils::ParseSingleRangeRequest("bytes=42-0 "_ns, true),
            mozilla::Nothing());
}

TEST(DOM_Base_ContentUtils, IsAllowedNonCorsRange)
{
  EXPECT_EQ(nsContentUtils::IsAllowedNonCorsRange("bytes=-200"_ns), false);
  EXPECT_EQ(nsContentUtils::IsAllowedNonCorsRange("bytes= 200-"_ns), false);
  EXPECT_EQ(nsContentUtils::IsAllowedNonCorsRange("bytes=201-200"_ns), false);
  EXPECT_EQ(nsContentUtils::IsAllowedNonCorsRange("bytes=200-201 "_ns), false);
  EXPECT_EQ(nsContentUtils::IsAllowedNonCorsRange("bytes=200-"_ns), true);
  EXPECT_EQ(nsContentUtils::IsAllowedNonCorsRange("bytes=200-201"_ns), true);
  EXPECT_EQ(nsContentUtils::IsAllowedNonCorsRange("bytes=-200 "_ns), false);
}
