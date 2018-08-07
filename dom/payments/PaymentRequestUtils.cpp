/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArrayUtils.h"
#include "PaymentRequestUtils.h"
#include "nsIMutableArray.h"
#include "nsISupportsPrimitives.h"

namespace mozilla {
namespace dom {

nsresult
SerializeFromJSObject(JSContext* aCx, JS::HandleObject aObject, nsAString& aSerializedObject)
{
  MOZ_ASSERT(aCx);
  JS::RootedValue value(aCx, JS::ObjectValue(*aObject));
  return SerializeFromJSVal(aCx, value, aSerializedObject);
}

nsresult
SerializeFromJSVal(JSContext* aCx, JS::HandleValue aValue, nsAString& aSerializedValue)
{
  aSerializedValue.Truncate();
  nsAutoString serializedValue;
  JS::RootedValue value(aCx, aValue.get());
  NS_ENSURE_TRUE(nsContentUtils::StringifyJSON(aCx, &value, serializedValue), NS_ERROR_XPC_BAD_CONVERT_JS);
  NS_ENSURE_TRUE(!serializedValue.IsEmpty(), NS_ERROR_FAILURE);
  aSerializedValue = serializedValue;
  return NS_OK;
}

nsresult
DeserializeToJSObject(const nsAString& aSerializedObject, JSContext* aCx, JS::MutableHandleObject aObject)
{
  MOZ_ASSERT(aCx);
  JS::RootedValue value(aCx);
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

nsresult
DeserializeToJSValue(const nsAString& aSerializedObject, JSContext* aCx, JS::MutableHandleValue aValue)
{
  MOZ_ASSERT(aCx);
  if (!JS_ParseJSON(aCx, static_cast<const char16_t*>(PromiseFlatString(aSerializedObject).get()),
                    aSerializedObject.Length(), aValue)) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

} // end of namespace dom
} // end of namespace mozilla
