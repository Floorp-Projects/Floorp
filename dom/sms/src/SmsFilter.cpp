/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsFilter.h"
#include "nsIDOMClassInfo.h"
#include "Constants.h"
#include "nsError.h"
#include "Constants.h"
#include "jsapi.h"
#include "jsfriendapi.h" // For js_DateGetMsecSinceEpoch.
#include "js/Utility.h"
#include "nsJSUtils.h"
#include "nsDOMString.h"

DOMCI_DATA(MozSmsFilter, mozilla::dom::sms::SmsFilter)

namespace mozilla {
namespace dom {
namespace sms {

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
SmsFilter::GetStartDate(JSContext* aCx, jsval* aStartDate)
{
  if (mData.startDate() == 0) {
    *aStartDate = JSVAL_NULL;
    return NS_OK;
  }

  aStartDate->setObjectOrNull(JS_NewDateObjectMsec(aCx, mData.startDate()));
  NS_ENSURE_TRUE(aStartDate->isObject(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::SetStartDate(JSContext* aCx, const jsval& aStartDate)
{
  if (aStartDate == JSVAL_NULL) {
    mData.startDate() = 0;
    return NS_OK;
  }

  if (!aStartDate.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  JSObject& obj = aStartDate.toObject();
  if (!JS_ObjectIsDate(aCx, &obj)) {
    return NS_ERROR_INVALID_ARG;
  }

  mData.startDate() = js_DateGetMsecSinceEpoch(aCx, &obj);
  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::GetEndDate(JSContext* aCx, jsval* aEndDate)
{
  if (mData.endDate() == 0) {
    *aEndDate = JSVAL_NULL;
    return NS_OK;
  }

  aEndDate->setObjectOrNull(JS_NewDateObjectMsec(aCx, mData.endDate()));
  NS_ENSURE_TRUE(aEndDate->isObject(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::SetEndDate(JSContext* aCx, const jsval& aEndDate)
{
  if (aEndDate == JSVAL_NULL) {
    mData.endDate() = 0;
    return NS_OK;
  }

  if (!aEndDate.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  JSObject& obj = aEndDate.toObject();
  if (!JS_ObjectIsDate(aCx, &obj)) {
    return NS_ERROR_INVALID_ARG;
  }

  mData.endDate() = js_DateGetMsecSinceEpoch(aCx, &obj);
  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::GetNumbers(JSContext* aCx, jsval* aNumbers)
{
  PRUint32 length = mData.numbers().Length();

  if (length == 0) {
    *aNumbers = JSVAL_NULL;
    return NS_OK;
  }

  jsval* numbers = new jsval[length];

  for (PRUint32 i=0; i<length; ++i) {
    numbers[i].setString(JS_NewUCStringCopyN(aCx, mData.numbers()[i].get(),
                                             mData.numbers()[i].Length()));
  }

  aNumbers->setObjectOrNull(JS_NewArrayObject(aCx, length, numbers));
  NS_ENSURE_TRUE(aNumbers->isObject(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::SetNumbers(JSContext* aCx, const jsval& aNumbers)
{
  if (aNumbers == JSVAL_NULL) {
    mData.numbers().Clear();
    return NS_OK;
  }

  if (!aNumbers.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  JSObject& obj = aNumbers.toObject();
  if (!JS_IsArrayObject(aCx, &obj)) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t size;
  JS_ALWAYS_TRUE(JS_GetArrayLength(aCx, &obj, &size));

  nsTArray<nsString> numbers;

  for (uint32_t i=0; i<size; ++i) {
    jsval jsNumber;
    if (!JS_GetElement(aCx, &obj, i, &jsNumber)) {
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
SmsFilter::GetRead(JSContext* aCx, jsval* aRead)
{
  if (mData.read() == eReadState_Unknown) {
    *aRead = JSVAL_VOID;
    return NS_OK;
  }

  aRead->setBoolean(mData.read());

  return NS_OK;
}

NS_IMETHODIMP
SmsFilter::SetRead(JSContext* aCx, const jsval& aRead)
{
  if (aRead == JSVAL_VOID) {
    mData.read() = eReadState_Unknown;
    return NS_OK;
  }

  if (!aRead.isBoolean()) {
    return NS_ERROR_INVALID_ARG;
  }

  mData.read() = aRead.toBoolean() ? eReadState_Read : eReadState_Unread;
  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
