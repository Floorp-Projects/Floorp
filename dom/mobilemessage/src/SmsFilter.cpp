/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsFilter.h"
#include "jsapi.h"
#include "jsfriendapi.h" // For js_DateGetMsecSinceEpoch.
#include "js/Utility.h"
#include "mozilla/dom/mobilemessage/Constants.h" // For MessageType
#include "mozilla/dom/ToJSValue.h"
#include "nsDOMString.h"
#include "nsError.h"
#include "nsIDOMClassInfo.h"
#include "nsJSUtils.h"

using namespace mozilla::dom::mobilemessage;

DOMCI_DATA(MozSmsFilter, mozilla::dom::SmsFilter)

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN(SmsFilter)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozSmsFilter)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozSmsFilter)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(SmsFilter)
NS_IMPL_RELEASE(SmsFilter)

SmsFilter::SmsFilter()
{
  mData.startDate() = 0;
  mData.endDate() = 0;
  mData.delivery() = eDeliveryState_Unknown;
  mData.read() = eReadState_Unknown;
  mData.threadId() = 0;
}

SmsFilter::SmsFilter(const SmsFilterData& aData)
  : mData(aData)
{
}

/* static */ nsresult
SmsFilter::NewSmsFilter(nsISupports** aSmsFilter)
{
  NS_ADDREF(*aSmsFilter = new SmsFilter());
  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::GetStartDate(JSContext* aCx, JS::MutableHandle<JS::Value> aStartDate)
{
  if (mData.startDate() == 0) {
    aStartDate.setNull();
    return NS_OK;
  }

  aStartDate.setObjectOrNull(JS_NewDateObjectMsec(aCx, mData.startDate()));
  NS_ENSURE_TRUE(aStartDate.isObject(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::SetStartDate(JSContext* aCx, JS::Handle<JS::Value> aStartDate)
{
  if (aStartDate.isNull()) {
    mData.startDate() = 0;
    return NS_OK;
  }

  if (!aStartDate.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  JS::Rooted<JSObject*> obj(aCx, &aStartDate.toObject());
  if (!JS_ObjectIsDate(aCx, obj)) {
    return NS_ERROR_INVALID_ARG;
  }

  mData.startDate() = js_DateGetMsecSinceEpoch(obj);
  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::GetEndDate(JSContext* aCx, JS::MutableHandle<JS::Value> aEndDate)
{
  if (mData.endDate() == 0) {
    aEndDate.setNull();
    return NS_OK;
  }

  aEndDate.setObjectOrNull(JS_NewDateObjectMsec(aCx, mData.endDate()));
  NS_ENSURE_TRUE(aEndDate.isObject(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::SetEndDate(JSContext* aCx, JS::Handle<JS::Value> aEndDate)
{
  if (aEndDate.isNull()) {
    mData.endDate() = 0;
    return NS_OK;
  }

  if (!aEndDate.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  JS::Rooted<JSObject*> obj(aCx, &aEndDate.toObject());
  if (!JS_ObjectIsDate(aCx, obj)) {
    return NS_ERROR_INVALID_ARG;
  }

  mData.endDate() = js_DateGetMsecSinceEpoch(obj);
  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::GetNumbers(JSContext* aCx, JS::MutableHandle<JS::Value> aNumbers)
{
  uint32_t length = mData.numbers().Length();

  if (length == 0) {
    aNumbers.setNull();
    return NS_OK;
  }

  if (!ToJSValue(aCx, mData.numbers(), aNumbers)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::SetNumbers(JSContext* aCx, JS::Handle<JS::Value> aNumbers)
{
  if (aNumbers.isNull()) {
    mData.numbers().Clear();
    return NS_OK;
  }

  if (!aNumbers.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  JS::Rooted<JSObject*> obj(aCx, &aNumbers.toObject());
  if (!JS_IsArrayObject(aCx, obj)) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t size;
  MOZ_ALWAYS_TRUE(JS_GetArrayLength(aCx, obj, &size));

  nsTArray<nsString> numbers;

  for (uint32_t i=0; i<size; ++i) {
    JS::Rooted<JS::Value> jsNumber(aCx);
    if (!JS_GetElement(aCx, obj, i, &jsNumber)) {
      return NS_ERROR_INVALID_ARG;
    }

    if (!jsNumber.isString()) {
      return NS_ERROR_INVALID_ARG;
    }

    nsDependentJSString number;
    number.init(aCx, jsNumber.toString());

    numbers.AppendElement(number);
  }

  mData.numbers().Clear();
  mData.numbers().AppendElements(numbers);

  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::GetDelivery(nsAString& aDelivery)
{
  switch (mData.delivery()) {
    case eDeliveryState_Received:
      aDelivery = DELIVERY_RECEIVED;
      break;
    case eDeliveryState_Sent:
      aDelivery = DELIVERY_SENT;
      break;
    case eDeliveryState_Unknown:
      SetDOMStringToNull(aDelivery);
      break;
    default:
      NS_ASSERTION(false, "We shouldn't get another delivery state!");
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::SetDelivery(const nsAString& aDelivery)
{
  if (aDelivery.IsEmpty()) {
    mData.delivery() = eDeliveryState_Unknown;
    return NS_OK;
  }

  if (aDelivery.Equals(DELIVERY_RECEIVED)) {
    mData.delivery() = eDeliveryState_Received;
    return NS_OK;
  }

  if (aDelivery.Equals(DELIVERY_SENT)) {
    mData.delivery() = eDeliveryState_Sent;
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
SmsFilter::GetRead(JSContext* aCx, JS::MutableHandle<JS::Value> aRead)
{
  if (mData.read() == eReadState_Unknown) {
    aRead.setNull();
    return NS_OK;
  }

  aRead.setBoolean(mData.read());
  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::SetRead(JSContext* aCx, JS::Handle<JS::Value> aRead)
{
  if (aRead.isNull()) {
    mData.read() = eReadState_Unknown;
    return NS_OK;
  }

  if (!aRead.isBoolean()) {
    return NS_ERROR_INVALID_ARG;
  }

  mData.read() = aRead.toBoolean() ? eReadState_Read : eReadState_Unread;
  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::GetThreadId(JSContext* aCx, JS::MutableHandle<JS::Value> aThreadId)
{
  if (!mData.threadId()) {
    aThreadId.setNull();
    return NS_OK;
  }

  aThreadId.setNumber(static_cast<double>(mData.threadId()));
  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::SetThreadId(JSContext* aCx, JS::Handle<JS::Value> aThreadId)
{
  if (aThreadId.isNull()) {
    mData.threadId() = 0;
    return NS_OK;
  }

  if (!aThreadId.isNumber()) {
    return NS_ERROR_INVALID_ARG;
  }

  double number = aThreadId.toNumber();
  uint64_t integer = static_cast<uint64_t>(number);
  if (integer == 0 || integer != number) {
    return NS_ERROR_INVALID_ARG;
  }
  mData.threadId() = integer;

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
