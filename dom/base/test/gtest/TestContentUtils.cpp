/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "jsapi.h"
#include "nsContentUtils.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/SimpleGlobalObject.h"

TEST(DOM_Base_ContentUtils, StringifyJSON_EmptyValue)
{
  JSObject* globalObject = mozilla::dom::SimpleGlobalObject::Create(
      mozilla::dom::SimpleGlobalObject::GlobalType::BindingDetail);
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
  JSObject* globalObject = mozilla::dom::SimpleGlobalObject::Create(
      mozilla::dom::SimpleGlobalObject::GlobalType::BindingDetail);
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
