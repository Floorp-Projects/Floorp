/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SimpleGlobalObject.h"
#include "mozilla/gtest/MozAssertions.h"
#include "nsITelemetry.h"
#include "QuotaManagerDependencyFixture.h"

namespace mozilla::dom::quota::test {

namespace {

namespace js_helpers {

void CallFunction(JSContext* aCx, JS::Handle<JS::Value> aValue,
                  const char* aName, JS::MutableHandle<JS::Value> aResult) {
  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());

  JS::Rooted<JS::Value> result(aCx);
  ASSERT_TRUE(JS_CallFunctionName(aCx, obj, aName,
                                  JS::HandleValueArray::empty(), &result));

  aResult.set(result);
}

void HasProperty(JSContext* aCx, JS::Handle<JS::Value> aValue,
                 const char* aName, bool* aResult) {
  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());

  bool result;
  ASSERT_TRUE(JS_HasProperty(aCx, obj, aName, &result));

  *aResult = result;
}

void GetProperty(JSContext* aCx, JS::Handle<JS::Value> aValue,
                 const char* aName, JS::MutableHandle<JS::Value> aResult) {
  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());

  JS::Rooted<JS::Value> result(aCx);
  ASSERT_TRUE(JS_GetProperty(aCx, obj, aName, &result));

  aResult.set(result);
}

void Enumerate(JSContext* aCx, JS::Handle<JS::Value> aValue,
               JS::MutableHandle<JS::IdVector> aResult) {
  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  ASSERT_TRUE(JS_Enumerate(aCx, obj, aResult));
}

void GetElement(JSContext* aCx, JS::Handle<JS::Value> aValue, uint32_t aIndex,
                JS::MutableHandle<JS::Value> aResult) {
  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());

  JS::Rooted<JS::Value> result(aCx);
  ASSERT_TRUE(JS_GetElement(aCx, obj, aIndex, &result));

  aResult.set(result);
}

void GetIndexValue(JSContext* aCx, JS::Handle<JS::Value> aValues,
                   JS::PropertyKey aIndexId, uint32_t* aResult) {
  nsAutoJSString indexId;
  ASSERT_TRUE(indexId.init(aCx, aIndexId));

  nsresult rv;
  const auto index = indexId.ToInteger64(&rv);
  ASSERT_NS_SUCCEEDED(rv);

  JS::Rooted<JS::Value> element(aCx);
  GetElement(aCx, aValues, index, &element);

  uint32_t value = 0;
  ASSERT_TRUE(JS::ToUint32(aCx, element, &value));

  *aResult = value;
}

}  // namespace js_helpers

}  // namespace

TEST(DOM_Quota_Telemetry, ShutdownTime)
{
  nsCOMPtr<nsITelemetry> telemetry =
      do_GetService("@mozilla.org/base/telemetry;1");

  JSObject* simpleGlobal = dom::SimpleGlobalObject::Create(
      dom::SimpleGlobalObject::GlobalType::BindingDetail);

  JS::Rooted<JSObject*> global(dom::RootingCx(), simpleGlobal);

  AutoJSAPI jsapi;
  ASSERT_TRUE(jsapi.Init(global));

  JSContext* cx = jsapi.cx();

  JS::Rooted<JS::Value> histogram(cx);
  ASSERT_NS_SUCCEEDED(telemetry->GetKeyedHistogramById("QM_SHUTDOWN_TIME_V0"_ns,
                                                       cx, &histogram));

  JS::Rooted<JS::Value> dummy(cx);
  ASSERT_NO_FATAL_FAILURE(
      js_helpers::CallFunction(cx, histogram, "clear", &dummy));

  ASSERT_NO_FATAL_FAILURE(QuotaManagerDependencyFixture::InitializeFixture());
  ASSERT_NO_FATAL_FAILURE(QuotaManagerDependencyFixture::ShutdownFixture());

  JS::Rooted<JS::Value> snapshot(cx);
  ASSERT_NO_FATAL_FAILURE(
      js_helpers::CallFunction(cx, histogram, "snapshot", &snapshot));

  {
    bool hasKey = false;
    ASSERT_NO_FATAL_FAILURE(
        js_helpers::HasProperty(cx, snapshot, "Normal", &hasKey));
    ASSERT_TRUE(hasKey);

    JS::Rooted<JS::Value> key(cx);
    ASSERT_NO_FATAL_FAILURE(
        js_helpers::GetProperty(cx, snapshot, "Normal", &key));

    JS::Rooted<JS::Value> values(cx);
    ASSERT_NO_FATAL_FAILURE(
        js_helpers::GetProperty(cx, key, "values", &values));

    JS::Rooted<JS::IdVector> indexIds(cx, JS::IdVector(cx));
    ASSERT_NO_FATAL_FAILURE(js_helpers::Enumerate(cx, values, &indexIds));
    ASSERT_TRUE(indexIds.length() == 2u || indexIds.length() == 3u);

    if (indexIds.length() == 2) {
      uint32_t value = 0u;

      ASSERT_NO_FATAL_FAILURE(
          js_helpers::GetIndexValue(cx, values, indexIds[0], &value));
      ASSERT_EQ(value, 1u);

      ASSERT_NO_FATAL_FAILURE(
          js_helpers::GetIndexValue(cx, values, indexIds[1], &value));
      ASSERT_EQ(value, 0u);
    } else {
      uint32_t value = 1u;

      ASSERT_NO_FATAL_FAILURE(
          js_helpers::GetIndexValue(cx, values, indexIds[0], &value));
      ASSERT_EQ(value, 0u);

      ASSERT_NO_FATAL_FAILURE(
          js_helpers::GetIndexValue(cx, values, indexIds[1], &value));
      ASSERT_EQ(value, 1u);

      ASSERT_NO_FATAL_FAILURE(
          js_helpers::GetIndexValue(cx, values, indexIds[2], &value));
      ASSERT_EQ(value, 0u);
    }
  }

  {
    bool hasKey = true;
    ASSERT_NO_FATAL_FAILURE(
        js_helpers::HasProperty(cx, snapshot, "WasSuspended", &hasKey));
    ASSERT_FALSE(hasKey);
  }

  {
    bool hasKey = true;
    ASSERT_NO_FATAL_FAILURE(
        js_helpers::HasProperty(cx, snapshot, "TimeStampErr1", &hasKey));
    ASSERT_FALSE(hasKey);
  }

  {
    bool hasKey = true;
    ASSERT_NO_FATAL_FAILURE(
        js_helpers::HasProperty(cx, snapshot, "TimeStampErr2", &hasKey));
    ASSERT_FALSE(hasKey);
  }
}

}  // namespace mozilla::dom::quota::test
