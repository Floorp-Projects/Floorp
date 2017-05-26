/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClearOnShutdown.h"
#include "PaymentRequestData.h"
#include "PaymentRequestService.h"

namespace mozilla {
namespace dom {

StaticRefPtr<PaymentRequestService> gPaymentService;

namespace {

class PaymentRequestEnumerator final : public nsISimpleEnumerator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

  PaymentRequestEnumerator()
    : mIndex(0)
  {}
private:
  ~PaymentRequestEnumerator() = default;
  uint32_t mIndex;
};

NS_IMPL_ISUPPORTS(PaymentRequestEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
PaymentRequestEnumerator::HasMoreElements(bool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = false;
  if (NS_WARN_IF(!gPaymentService)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<PaymentRequestService> service = gPaymentService;
  *aReturn = mIndex < service->NumPayments();
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestEnumerator::GetNext(nsISupports** aItem)
{
  NS_ENSURE_ARG_POINTER(aItem);
  if (NS_WARN_IF(!gPaymentService)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIPaymentRequest> request =
    gPaymentService->GetPaymentRequestByIndex(mIndex);
  if (NS_WARN_IF(!request)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsISupports> item = do_QueryInterface(request);
  if (NS_WARN_IF(!item)) {
    return NS_ERROR_FAILURE;
  }
  mIndex++;
  item.forget(aItem);
  return NS_OK;
}

} // end of anonymous namespace

/* PaymentRequestService */

NS_IMPL_ISUPPORTS(PaymentRequestService,
                  nsIPaymentRequestService)

already_AddRefed<PaymentRequestService>
PaymentRequestService::GetSingleton()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!gPaymentService) {
    gPaymentService = new PaymentRequestService();
    ClearOnShutdown(&gPaymentService);
  }
  RefPtr<PaymentRequestService> service = gPaymentService;
  return service.forget();
}

uint32_t
PaymentRequestService::NumPayments() const
{
  return mRequestQueue.Length();
}

already_AddRefed<nsIPaymentRequest>
PaymentRequestService::GetPaymentRequestByIndex(const uint32_t aIndex)
{
  if (aIndex >= mRequestQueue.Length()) {
    return nullptr;
  }
  nsCOMPtr<nsIPaymentRequest> request = mRequestQueue[aIndex];
  MOZ_ASSERT(request);
  return request.forget();
}

NS_IMETHODIMP
PaymentRequestService::GetPaymentRequestById(const nsAString& aRequestId,
                                             nsIPaymentRequest** aRequest)
{
  NS_ENSURE_ARG_POINTER(aRequest);
  *aRequest = nullptr;
  uint32_t numRequests = mRequestQueue.Length();
  for (uint32_t index = 0; index < numRequests; ++index) {
    nsCOMPtr<nsIPaymentRequest> request = mRequestQueue[index];
    MOZ_ASSERT(request);
    nsAutoString requestId;
    nsresult rv = request->GetRequestId(requestId);
    NS_ENSURE_SUCCESS(rv, rv);
    if (requestId == aRequestId) {
      request.forget(aRequest);
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::Enumerate(nsISimpleEnumerator** aEnumerator)
{
  NS_ENSURE_ARG_POINTER(aEnumerator);
  nsCOMPtr<nsISimpleEnumerator> enumerator = new PaymentRequestEnumerator();
  enumerator.forget(aEnumerator);
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::Cleanup()
{
  mRequestQueue.Clear();
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::RequestPayment(nsIPaymentActionRequest* aRequest)
{
  NS_ENSURE_ARG_POINTER(aRequest);
  uint32_t type;
  nsresult rv = aRequest->GetType(&type);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  switch (type) {
    case nsIPaymentActionRequest::CREATE_ACTION: {
      nsCOMPtr<nsIPaymentCreateActionRequest> request =
        do_QueryInterface(aRequest);
      MOZ_ASSERT(request);
      uint64_t tabId;
      rv = request->GetTabId(&tabId);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsString requestId;
      rv = request->GetRequestId(requestId);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsCOMPtr<nsIArray> methodData;
      rv = request->GetMethodData(getter_AddRefs(methodData));
      if (NS_WARN_IF(NS_FAILED(rv) || !methodData)) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIPaymentDetails> details;
      rv = request->GetDetails(getter_AddRefs(details));
      if (NS_WARN_IF(NS_FAILED(rv) || !details)) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIPaymentOptions> options;
      rv = request->GetOptions(getter_AddRefs(options));
      if (NS_WARN_IF(NS_FAILED(rv) || !options)) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIPaymentRequest> payment =
         new payments::PaymentRequest(tabId, requestId, methodData, details, options);

      if (!mRequestQueue.AppendElement(payment, mozilla::fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      break;
    }
    default: {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

} // end of namespace dom
} // end of namespace mozilla
