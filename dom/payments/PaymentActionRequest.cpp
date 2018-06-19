/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArrayUtils.h"
#include "nsIMutableArray.h"
#include "PaymentActionRequest.h"
#include "PaymentRequestData.h"

using namespace mozilla::dom::payments;

namespace mozilla {
namespace dom {

/* PaymentActionRequest */

NS_IMPL_ISUPPORTS(PaymentActionRequest,
                  nsIPaymentActionRequest)

PaymentActionRequest::PaymentActionRequest()
  : mRequestId(EmptyString())
  , mType(nsIPaymentActionRequest::UNKNOWN_ACTION)
  , mCallback(nullptr)
{
}

NS_IMETHODIMP
PaymentActionRequest::Init(const nsAString& aRequestId,
                           const uint32_t aType,
                           nsIPaymentActionCallback* aCallback)
{
  mRequestId = aRequestId;
  mType = aType;
  mCallback = aCallback;
  return NS_OK;
}

NS_IMETHODIMP
PaymentActionRequest::GetRequestId(nsAString& aRequestId)
{
  aRequestId = mRequestId;
  return NS_OK;
}

NS_IMETHODIMP
PaymentActionRequest::GetType(uint32_t* aType)
{
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
PaymentActionRequest::GetCallback(nsIPaymentActionCallback** aCallback)
{
  NS_ENSURE_ARG_POINTER(aCallback);
  nsCOMPtr<nsIPaymentActionCallback> callback = mCallback;
  callback.forget(aCallback);
  return NS_OK;
}

/* PaymentCreateActionRequest */

NS_IMPL_ISUPPORTS_INHERITED(PaymentCreateActionRequest,
                            PaymentActionRequest,
                            nsIPaymentCreateActionRequest)

PaymentCreateActionRequest::PaymentCreateActionRequest()
  : mTabId(0)
{
}

NS_IMETHODIMP
PaymentCreateActionRequest::InitRequest(const nsAString& aRequestId,
                                        nsIPaymentActionCallback* aCallback,
                                        const uint64_t aTabId,
                                        nsIPrincipal* aTopLevelPrincipal,
                                        nsIArray* aMethodData,
                                        nsIPaymentDetails* aDetails,
                                        nsIPaymentOptions* aOptions,
                                        const nsAString& aShippingOption)
{
  NS_ENSURE_ARG_POINTER(aCallback);
  NS_ENSURE_ARG_POINTER(aTopLevelPrincipal);
  NS_ENSURE_ARG_POINTER(aMethodData);
  NS_ENSURE_ARG_POINTER(aDetails);
  NS_ENSURE_ARG_POINTER(aOptions);
  nsresult rv = Init(aRequestId, nsIPaymentActionRequest::CREATE_ACTION, aCallback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  mTabId = aTabId;
  mTopLevelPrincipal = aTopLevelPrincipal;
  mMethodData = aMethodData;
  mDetails = aDetails;
  mOptions = aOptions;
  mShippingOption = aShippingOption;
  return NS_OK;
}

NS_IMETHODIMP
PaymentCreateActionRequest::GetTabId(uint64_t* aTabId)
{
  NS_ENSURE_ARG_POINTER(aTabId);
  *aTabId = mTabId;
  return NS_OK;
}

NS_IMETHODIMP
PaymentCreateActionRequest::GetTopLevelPrincipal(nsIPrincipal** aTopLevelPrincipal)
{
  NS_ENSURE_ARG_POINTER(aTopLevelPrincipal);
  MOZ_ASSERT(mTopLevelPrincipal);
  nsCOMPtr<nsIPrincipal> principal = mTopLevelPrincipal;
  principal.forget(aTopLevelPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
PaymentCreateActionRequest::GetMethodData(nsIArray** aMethodData)
{
  NS_ENSURE_ARG_POINTER(aMethodData);
  MOZ_ASSERT(mMethodData);
  nsCOMPtr<nsIArray> methodData = mMethodData;
  methodData.forget(aMethodData);
  return NS_OK;
}

NS_IMETHODIMP
PaymentCreateActionRequest::GetDetails(nsIPaymentDetails** aDetails)
{
  NS_ENSURE_ARG_POINTER(aDetails);
  MOZ_ASSERT(mDetails);
  nsCOMPtr<nsIPaymentDetails> details = mDetails;
  details.forget(aDetails);
  return NS_OK;
}

NS_IMETHODIMP
PaymentCreateActionRequest::GetOptions(nsIPaymentOptions** aOptions)
{
  NS_ENSURE_ARG_POINTER(aOptions);
  MOZ_ASSERT(mOptions);
  nsCOMPtr<nsIPaymentOptions> options = mOptions;
  options.forget(aOptions);
  return NS_OK;
}

NS_IMETHODIMP
PaymentCreateActionRequest::GetShippingOption(nsAString& aShippingOption)
{
  aShippingOption = mShippingOption;
  return NS_OK;
}

/* PaymentCompleteActionRequest */

NS_IMPL_ISUPPORTS_INHERITED(PaymentCompleteActionRequest,
                            PaymentActionRequest,
                            nsIPaymentCompleteActionRequest)

PaymentCompleteActionRequest::PaymentCompleteActionRequest()
  : mCompleteStatus(EmptyString())
{
}

NS_IMETHODIMP
PaymentCompleteActionRequest::GetCompleteStatus(nsAString& aCompleteStatus)
{
  aCompleteStatus = mCompleteStatus;
  return NS_OK;
}

NS_IMETHODIMP
PaymentCompleteActionRequest::InitRequest(const nsAString& aRequestId,
                                          nsIPaymentActionCallback* aCallback,
                                          const nsAString& aCompleteStatus)
{
  NS_ENSURE_ARG_POINTER(aCallback);
  nsresult rv = Init(aRequestId, nsIPaymentActionRequest::COMPLETE_ACTION, aCallback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  mCompleteStatus = aCompleteStatus;
  return NS_OK;
}

/* PaymentUpdateActionRequest */

NS_IMPL_ISUPPORTS_INHERITED(PaymentUpdateActionRequest,
                            PaymentActionRequest,
                            nsIPaymentUpdateActionRequest)

NS_IMETHODIMP
PaymentUpdateActionRequest::GetDetails(nsIPaymentDetails** aDetails)
{
  NS_ENSURE_ARG_POINTER(aDetails);
  MOZ_ASSERT(mDetails);
  nsCOMPtr<nsIPaymentDetails> details = mDetails;
  details.forget(aDetails);
  return NS_OK;
}

NS_IMETHODIMP
PaymentUpdateActionRequest::GetShippingOption(nsAString& aShippingOption)
{
  aShippingOption = mShippingOption;
  return NS_OK;
}

NS_IMETHODIMP
PaymentUpdateActionRequest::InitRequest(const nsAString& aRequestId,
                                        nsIPaymentActionCallback* aCallback,
                                        nsIPaymentDetails* aDetails,
                                        const nsAString& aShippingOption)
{
  NS_ENSURE_ARG_POINTER(aCallback);
  NS_ENSURE_ARG_POINTER(aDetails);
  nsresult rv = Init(aRequestId, nsIPaymentActionRequest::UPDATE_ACTION, aCallback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  mDetails = aDetails;
  mShippingOption = aShippingOption;
  return NS_OK;
}

} // end of namespace dom
} // end of namespace mozilla
