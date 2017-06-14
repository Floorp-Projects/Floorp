/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaymentActionResponse.h"
#include "nsIRunnable.h"

namespace mozilla {
namespace dom {

/* PaymentActionResponse */

NS_IMPL_ISUPPORTS(PaymentActionResponse,
                  nsIPaymentActionResponse)

PaymentActionResponse::PaymentActionResponse()
  : mRequestId(EmptyString())
  , mType(nsIPaymentActionResponse::NO_TYPE)
{
}

NS_IMETHODIMP
PaymentActionResponse::GetRequestId(nsAString& aRequestId)
{
  aRequestId = mRequestId;
  return NS_OK;
}

NS_IMETHODIMP
PaymentActionResponse::GetType(uint32_t* aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  *aType = mType;
  return NS_OK;
}

/* PaymentCanMakeActionResponse */

NS_IMPL_ISUPPORTS_INHERITED(PaymentCanMakeActionResponse,
                            PaymentActionResponse,
                            nsIPaymentCanMakeActionResponse)

PaymentCanMakeActionResponse::PaymentCanMakeActionResponse()
{
  mType = nsIPaymentActionResponse::CANMAKE_ACTION;
}

NS_IMETHODIMP
PaymentCanMakeActionResponse::GetResult(bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mResult;
  return NS_OK;
}

NS_IMETHODIMP
PaymentCanMakeActionResponse::Init(const nsAString& aRequestId, const bool aResult)
{
  mRequestId = aRequestId;
  mResult = aResult;
  return NS_OK;
}

/* PaymentShowActionResponse */

NS_IMPL_ISUPPORTS_INHERITED(PaymentShowActionResponse,
                            PaymentActionResponse,
                            nsIPaymentShowActionResponse)

PaymentShowActionResponse::PaymentShowActionResponse()
{
  mType = nsIPaymentActionResponse::SHOW_ACTION;
}

NS_IMETHODIMP
PaymentShowActionResponse::GetAcceptStatus(uint32_t* aAcceptStatus)
{
  NS_ENSURE_ARG_POINTER(aAcceptStatus);
  *aAcceptStatus = mAcceptStatus;
  return NS_OK;
}

NS_IMETHODIMP
PaymentShowActionResponse::GetMethodName(nsAString& aMethodName)
{
  aMethodName = mMethodName;
  return NS_OK;
}

NS_IMETHODIMP
PaymentShowActionResponse::GetData(nsAString& aData)
{
  aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
PaymentShowActionResponse::GetPayerName(nsAString& aPayerName)
{
  aPayerName = mPayerName;
  return NS_OK;
}

NS_IMETHODIMP
PaymentShowActionResponse::GetPayerEmail(nsAString& aPayerEmail)
{
  aPayerEmail = mPayerEmail;
  return NS_OK;
}

NS_IMETHODIMP
PaymentShowActionResponse::GetPayerPhone(nsAString& aPayerPhone)
{
  aPayerPhone = mPayerPhone;
  return NS_OK;
}

NS_IMETHODIMP
PaymentShowActionResponse::Init(const nsAString& aRequestId,
                                const uint32_t aAcceptStatus,
                                const nsAString& aMethodName,
                                const nsAString& aData,
                                const nsAString& aPayerName,
                                const nsAString& aPayerEmail,
                                const nsAString& aPayerPhone)
{
  mRequestId = aRequestId;
  mAcceptStatus = aAcceptStatus;
  mMethodName = aMethodName;
  mData = aData;
  mPayerName = aPayerName;
  mPayerEmail = aPayerEmail;
  mPayerPhone = aPayerPhone;
  return NS_OK;
}

NS_IMETHODIMP
PaymentShowActionResponse::IsAccepted(bool* aIsAccepted)
{
  NS_ENSURE_ARG_POINTER(aIsAccepted);
  *aIsAccepted = (mAcceptStatus == nsIPaymentActionResponse::PAYMENT_ACCEPTED);
  return NS_OK;
}

/* PaymentAbortActionResponse */

NS_IMPL_ISUPPORTS_INHERITED(PaymentAbortActionResponse,
                            PaymentActionResponse,
                            nsIPaymentAbortActionResponse)

PaymentAbortActionResponse::PaymentAbortActionResponse()
{
  mType = nsIPaymentActionResponse::ABORT_ACTION;
}

NS_IMETHODIMP
PaymentAbortActionResponse::GetAbortStatus(uint32_t* aAbortStatus)
{
  NS_ENSURE_ARG_POINTER(aAbortStatus);
  *aAbortStatus = mAbortStatus;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAbortActionResponse::Init(const nsAString& aRequestId,
                                 const uint32_t aAbortStatus)
{
  mRequestId = aRequestId;
  mAbortStatus = aAbortStatus;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAbortActionResponse::IsSucceeded(bool* aIsSucceeded)
{
  NS_ENSURE_ARG_POINTER(aIsSucceeded);
  *aIsSucceeded = (mAbortStatus == nsIPaymentActionResponse::ABORT_SUCCEEDED);
  return NS_OK;
}

/* PaymentCompleteActionResponse */

NS_IMPL_ISUPPORTS_INHERITED(PaymentCompleteActionResponse,
                            PaymentActionResponse,
                            nsIPaymentCompleteActionResponse)

PaymentCompleteActionResponse::PaymentCompleteActionResponse()
{
  mType = nsIPaymentActionResponse::COMPLETE_ACTION;
}

nsresult
PaymentCompleteActionResponse::Init(const nsAString& aRequestId,
                                    const uint32_t aCompleteStatus)
{
  mRequestId = aRequestId;
  mCompleteStatus = aCompleteStatus;
  return NS_OK;
}

nsresult
PaymentCompleteActionResponse::GetCompleteStatus(uint32_t* aCompleteStatus)
{
  NS_ENSURE_ARG_POINTER(aCompleteStatus);
  *aCompleteStatus = mCompleteStatus;
  return NS_OK;
}

nsresult
PaymentCompleteActionResponse::IsCompleted(bool* aIsCompleted)
{
  NS_ENSURE_ARG_POINTER(aIsCompleted);
  *aIsCompleted = (mCompleteStatus == nsIPaymentActionResponse::COMPLETE_SUCCEEDED);
  return NS_OK;
}

} // end of namespace dom
} // end of namespace mozilla
