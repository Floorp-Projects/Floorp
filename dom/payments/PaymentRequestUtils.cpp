/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/JSON.h"
#include "nsContentUtils.h"
#include "nsArrayUtils.h"
#include "nsTString.h"
#include "PaymentRequestUtils.h"

namespace mozilla::dom {

nsresult SerializeFromJSObject(JSContext* aCx, JS::Handle<JSObject*> aObject,
                               nsAString& aSerializedObject) {
  MOZ_ASSERT(aCx);
  JS::Rooted<JS::Value> value(aCx, JS::ObjectValue(*aObject));
  return SerializeFromJSVal(aCx, value, aSerializedObject);
}

nsresult SerializeFromJSVal(JSContext* aCx, JS::Handle<JS::Value> aValue,
                            nsAString& aSerializedValue) {
  aSerializedValue.Truncate();
  NS_ENSURE_TRUE(nsContentUtils::StringifyJSON(aCx, aValue, aSerializedValue,
                                               UndefinedIsNullStringLiteral),
                 NS_ERROR_XPC_BAD_CONVERT_JS);
  NS_ENSURE_TRUE(!aSerializedValue.IsEmpty(), NS_ERROR_FAILURE);
  return NS_OK;
}

nsresult DeserializeToJSObject(const nsAString& aSerializedObject,
                               JSContext* aCx,
                               JS::MutableHandle<JSObject*> aObject) {
  MOZ_ASSERT(aCx);
  JS::Rooted<JS::Value> value(aCx);
  nsresult rv = DeserializeToJSValue(aSerializedObject, aCx, &value);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (value.isObject()) {
    aObject.set(&value.toObject());
  } else {
    aObject.set(nullptr);
  }
  return NS_OK;
}

nsresult DeserializeToJSValue(const nsAString& aSerializedObject,
                              JSContext* aCx,
                              JS::MutableHandle<JS::Value> aValue) {
  MOZ_ASSERT(aCx);
  if (!JS_ParseJSON(aCx, aSerializedObject.BeginReading(),
                    aSerializedObject.Length(), aValue)) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

}  // namespace mozilla::dom
