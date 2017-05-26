/* vim:set ts=2 sw=2 sts=2 et cindent: */
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

NS_IMETHODIMP
PaymentActionRequest::Init(const nsAString& aRequestId,
                           const uint32_t aType)
{
  mRequestId = aRequestId;
  mType = aType;
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

/* PaymentCreateActionRequest */

NS_IMPL_ISUPPORTS_INHERITED(PaymentCreateActionRequest,
                            PaymentActionRequest,
                            nsIPaymentCreateActionRequest)

NS_IMETHODIMP
PaymentCreateActionRequest::InitRequest(const nsAString& aRequestId,
                                        const uint64_t aTabId,
                                        nsIArray* aMethodData,
                                        nsIPaymentDetails* aDetails,
                                        nsIPaymentOptions* aOptions)
{
  NS_ENSURE_ARG_POINTER(aMethodData);
  NS_ENSURE_ARG_POINTER(aDetails);
  NS_ENSURE_ARG_POINTER(aOptions);
  Init(aRequestId, nsIPaymentActionRequest::CREATE_ACTION);
  mTabId = aTabId;
  mMethodData = aMethodData;
  mDetails = aDetails;
  mOptions = aOptions;
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
PaymentCreateActionRequest::GetMethodData(nsIArray** aMethodData)
{
  NS_ENSURE_ARG_POINTER(aMethodData);
  *aMethodData = nullptr;
  MOZ_ASSERT(mMethodData);
  nsCOMPtr<nsIArray> methodData = mMethodData;
  methodData.forget(aMethodData);
  return NS_OK;
}

NS_IMETHODIMP
PaymentCreateActionRequest::GetDetails(nsIPaymentDetails** aDetails)
{
  NS_ENSURE_ARG_POINTER(aDetails);
  *aDetails = nullptr;
  MOZ_ASSERT(mDetails);
  nsCOMPtr<nsIPaymentDetails> details = mDetails;
  details.forget(aDetails);
  return NS_OK;
}

NS_IMETHODIMP
PaymentCreateActionRequest::GetOptions(nsIPaymentOptions** aOptions)
{
  NS_ENSURE_ARG_POINTER(aOptions);
  *aOptions = nullptr;
  MOZ_ASSERT(mOptions);
  nsCOMPtr<nsIPaymentOptions> options = mOptions;
  options.forget(aOptions);
  return NS_OK;
}

} // end of namespace dom
} // end of namespace mozilla
