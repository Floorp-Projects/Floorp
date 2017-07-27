/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArrayUtils.h"
#include "PaymentRequestUtils.h"
#include "nsIMutableArray.h"
#include "nsISupportsPrimitives.h"
#include "nsIJSON.h"

namespace mozilla {
namespace dom {

nsresult
SerializeFromJSObject(JSContext* aCx, JS::HandleObject aObject, nsAString& aSerializedObject)
{
  MOZ_ASSERT(aCx);
  nsCOMPtr<nsIJSON> serializer = do_CreateInstance("@mozilla.org/dom/json;1");
  if (NS_WARN_IF(!serializer)) {
    return NS_ERROR_FAILURE;
  }
  JS::RootedValue value(aCx, JS::ObjectValue(*aObject));
  return serializer->EncodeFromJSVal(value.address(), aCx, aSerializedObject);
}

nsresult
SerializeFromJSVal(JSContext* aCx, JS::HandleValue aValue, nsAString& aSerializedValue)
{
  MOZ_ASSERT(aCx);
  nsCOMPtr<nsIJSON> serializer = do_CreateInstance("@mozilla.org/dom/json;1");
  if (NS_WARN_IF(!serializer)) {
    return NS_ERROR_FAILURE;
  }
  JS::RootedValue value(aCx, aValue.get());
  return serializer->EncodeFromJSVal(value.address(), aCx, aSerializedValue);
}

nsresult
DeserializeToJSObject(const nsAString& aSerializedObject, JSContext* aCx, JS::MutableHandleObject aObject)
{
  MOZ_ASSERT(aCx);
  nsCOMPtr<nsIJSON> deserializer = do_CreateInstance("@mozilla.org/dom/json;1");
  if (NS_WARN_IF(!deserializer)) {
    return NS_ERROR_FAILURE;
  }
  JS::RootedValue value(aCx);
  JS::MutableHandleValue handleVal(&value);
  nsresult rv = deserializer->DecodeToJSVal(aSerializedObject, aCx, handleVal);
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
  nsCOMPtr<nsIJSON> deserializer = do_CreateInstance("@mozilla.org/dom/json;1");
  if (NS_WARN_IF(!deserializer)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = deserializer->DecodeToJSVal(aSerializedObject, aCx, aValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

} // end of namespace dom
} // end of namespace mozilla
