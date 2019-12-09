/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "jsapi.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/SimpleGlobalObject.h"

struct IsURIInListMatch {
  nsLiteralCString pattern;
  bool firstMatch, secondMatch;
};

TEST(DOM_Base_ContentUtils, IsURIInList)
{
  nsCOMPtr<nsIURI> uri, subURI;
  nsresult rv =
      NS_NewURI(getter_AddRefs(uri),
                NS_LITERAL_CSTRING("https://example.com/path/favicon.ico#"));
  ASSERT_TRUE(rv == NS_OK);

  rv = NS_NewURI(getter_AddRefs(subURI),
                 NS_LITERAL_CSTRING("http://sub.example.com/favicon.ico?"));
  ASSERT_TRUE(rv == NS_OK);

  static constexpr IsURIInListMatch patterns[] = {
      {NS_LITERAL_CSTRING("bar.com,*.example.com,example.com,foo.com"), true,
       true},
      {NS_LITERAL_CSTRING("bar.com,example.com,*.example.com,foo.com"), true,
       true},
      {NS_LITERAL_CSTRING("*.example.com,example.com,foo.com"), true, true},
      {NS_LITERAL_CSTRING("example.com,*.example.com,foo.com"), true, true},
      {NS_LITERAL_CSTRING("*.example.com,example.com"), true, true},
      {NS_LITERAL_CSTRING("example.com,*.example.com"), true, true},
      {NS_LITERAL_CSTRING("*.example.com/,example.com/"), true, true},
      {NS_LITERAL_CSTRING("example.com/,*.example.com/"), true, true},
      {NS_LITERAL_CSTRING("*.example.com/pa,example.com/pa"), false, false},
      {NS_LITERAL_CSTRING("example.com/pa,*.example.com/pa"), false, false},
      {NS_LITERAL_CSTRING("*.example.com/pa/,example.com/pa/"), false, false},
      {NS_LITERAL_CSTRING("example.com/pa/,*.example.com/pa/"), false, false},
      {NS_LITERAL_CSTRING("*.example.com/path,example.com/path"), false, false},
      {NS_LITERAL_CSTRING("example.com/path,*.example.com/path"), false, false},
      {NS_LITERAL_CSTRING("*.example.com/path/,example.com/path/"), true,
       false},
      {NS_LITERAL_CSTRING("example.com/path/,*.example.com/path/"), true,
       false},
      {NS_LITERAL_CSTRING("*.example.com/favicon.ico"), false, true},
      {NS_LITERAL_CSTRING("example.com/path/favicon.ico"), true, false},
      {NS_LITERAL_CSTRING("*.example.com"), false, true},
      {NS_LITERAL_CSTRING("example.com"), true, false},
      {NS_LITERAL_CSTRING("foo.com"), false, false},
      {NS_LITERAL_CSTRING("*.foo.com"), false, false},
  };

  for (auto& entry : patterns) {
    bool result = nsContentUtils::IsURIInList(uri, entry.pattern);
    ASSERT_EQ(result, entry.firstMatch) << "Matching " << entry.pattern;

    result = nsContentUtils::IsURIInList(subURI, entry.pattern);
    ASSERT_EQ(result, entry.secondMatch) << "Matching " << entry.pattern;
  }
}

TEST(DOM_Base_ContentUtils, StringifyJSON_EmptyValue)
{
  JS::RootedObject globalObject(
      mozilla::dom::RootingCx(),
      mozilla::dom::SimpleGlobalObject::Create(
          mozilla::dom::SimpleGlobalObject::GlobalType::BindingDetail));
  mozilla::dom::AutoJSAPI jsAPI;
  ASSERT_TRUE(jsAPI.Init(globalObject));
  JSContext* cx = jsAPI.cx();
  nsAutoString serializedValue;

  JS::RootedValue jsValue(cx);
  ASSERT_TRUE(nsContentUtils::StringifyJSON(cx, &jsValue, serializedValue));

  ASSERT_TRUE(serializedValue.EqualsLiteral("null"));
}

TEST(DOM_Base_ContentUtils, StringifyJSON_Object)
{
  JS::RootedObject globalObject(
      mozilla::dom::RootingCx(),
      mozilla::dom::SimpleGlobalObject::Create(
          mozilla::dom::SimpleGlobalObject::GlobalType::BindingDetail));
  mozilla::dom::AutoJSAPI jsAPI;
  ASSERT_TRUE(jsAPI.Init(globalObject));
  JSContext* cx = jsAPI.cx();
  nsAutoString serializedValue;

  JS::RootedObject jsObj(cx, JS_NewPlainObject(cx));
  JS::RootedString valueStr(cx, JS_NewStringCopyZ(cx, "Hello World!"));
  ASSERT_TRUE(JS_DefineProperty(cx, jsObj, "key1", valueStr, JSPROP_ENUMERATE));
  JS::RootedValue jsValue(cx, JS::ObjectValue(*jsObj));

  ASSERT_TRUE(nsContentUtils::StringifyJSON(cx, &jsValue, serializedValue));

  ASSERT_TRUE(serializedValue.EqualsLiteral("{\"key1\":\"Hello World!\"}"));
}
