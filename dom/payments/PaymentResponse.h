/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentResponse_h
#define mozilla_dom_PaymentResponse_h

#include "mozilla/dom/PaymentResponseBinding.h" // PaymentComplete
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"
#include "nsITimer.h"

namespace mozilla {
namespace dom {

class PaymentAddress;
class PaymentRequest;
class Promise;

class PaymentResponse final : public nsITimerCallback,
                              public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PaymentResponse)

  NS_IMETHOD Notify(nsITimer* aTimer) override;

  PaymentResponse(nsPIDOMWindowInner* aWindow,
                  PaymentRequest* aRequest,
                  const nsAString& aRequestId,
                  const nsAString& aMethodName,
                  const nsAString& aShippingOption,
                  RefPtr<PaymentAddress> aShippingAddress,
                  const nsAString& aDetails,
                  const nsAString& aPayerName,
                  const nsAString& aPayerEmail,
                  const nsAString& aPayerPhone);

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void GetRequestId(nsString& aRetVal) const;

  void GetMethodName(nsString& aRetVal) const;

  void GetDetails(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal) const;

  already_AddRefed<PaymentAddress> GetShippingAddress() const;

  void GetShippingOption(nsString& aRetVal) const;

  void GetPayerName(nsString& aRetVal) const;

  void GetPayerEmail(nsString& aRetVal) const;

  void GetPayerPhone(nsString& aRetVal) const;

  // Return a raw pointer here to avoid refcounting, but make sure it's safe
  // (the object should be kept alive by the callee).
  already_AddRefed<Promise> Complete(PaymentComplete result, ErrorResult& aRv);

  void RespondComplete();

protected:
  ~PaymentResponse();

private:
  nsCOMPtr<nsPIDOMWindowInner> mOwner;
  bool mCompleteCalled;
  PaymentRequest* mRequest;
  nsString mRequestId;
  nsString mMethodName;
  nsString mDetails;
  nsString mShippingOption;
  nsString mPayerName;
  nsString mPayerEmail;
  nsString mPayerPhone;
  RefPtr<PaymentAddress> mShippingAddress;
  // Promise for "PaymentResponse::Complete"
  RefPtr<Promise> mPromise;
  // Timer for timing out if the page doesn't call
  // complete()
  nsCOMPtr<nsITimer> mTimer;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PaymentResponse_h
