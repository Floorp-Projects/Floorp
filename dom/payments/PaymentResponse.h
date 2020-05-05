/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentResponse_h
#define mozilla_dom_PaymentResponse_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/PaymentResponseBinding.h"  // PaymentComplete
#include "nsPIDOMWindow.h"
#include "nsITimer.h"

namespace mozilla {
namespace dom {

class PaymentAddress;
class PaymentRequest;
class Promise;

class GeneralData final {
 public:
  GeneralData() = default;
  ~GeneralData() = default;
  nsString data;
};

class BasicCardData final {
 public:
  struct Address {
    nsString country;
    CopyableTArray<nsString> addressLine;
    nsString region;
    nsString regionCode;
    nsString city;
    nsString dependentLocality;
    nsString postalCode;
    nsString sortingCode;
    nsString organization;
    nsString recipient;
    nsString phone;
  };
  BasicCardData() = default;
  ~BasicCardData() = default;

  nsString cardholderName;
  nsString cardNumber;
  nsString expiryMonth;
  nsString expiryYear;
  nsString cardSecurityCode;
  Address billingAddress;
};

class ResponseData final {
 public:
  enum Type { Unknown = 0, GeneralResponse = 1, BasicCardResponse };
  ResponseData() : mType(ResponseData::Unknown) {}
  explicit ResponseData(const GeneralData& aGeneralData)
      : mType(GeneralResponse), mGeneralData(aGeneralData) {}
  explicit ResponseData(const BasicCardData& aBasicCardData)
      : mType(BasicCardResponse), mBasicCardData(aBasicCardData) {}
  ResponseData& operator=(const GeneralData& aGeneralData) {
    mType = GeneralResponse;
    mGeneralData = aGeneralData;
    mBasicCardData = BasicCardData();
    return *this;
  }
  ResponseData& operator=(const BasicCardData& aBasicCardData) {
    mType = BasicCardResponse;
    mGeneralData = GeneralData();
    mBasicCardData = aBasicCardData;
    return *this;
  }
  ~ResponseData() = default;

  const Type& type() const { return mType; }
  const GeneralData& generalData() const { return mGeneralData; }
  const BasicCardData& basicCardData() const { return mBasicCardData; }

 private:
  Type mType;
  GeneralData mGeneralData;
  BasicCardData mBasicCardData;
};

class PaymentResponse final : public DOMEventTargetHelper,
                              public nsITimerCallback {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(PaymentResponse,
                                                         DOMEventTargetHelper)

  NS_IMETHOD Notify(nsITimer* aTimer) override;

  PaymentResponse(nsPIDOMWindowInner* aWindow, PaymentRequest* aRequest,
                  const nsAString& aRequestId, const nsAString& aMethodName,
                  const nsAString& aShippingOption,
                  PaymentAddress* aShippingAddress,
                  const ResponseData& aDetails, const nsAString& aPayerName,
                  const nsAString& aPayerEmail, const nsAString& aPayerPhone);

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

  IMPL_EVENT_HANDLER(payerdetailchange);

  nsresult UpdatePayerDetail(const nsAString& aPayerName,
                             const nsAString& aPayerEmail,
                             const nsAString& aPayerPhone);

  already_AddRefed<Promise> Retry(JSContext* aCx,
                                  const PaymentValidationErrors& errorField,
                                  ErrorResult& aRv);

  void RespondRetry(const nsAString& aMethodName,
                    const nsAString& aShippingOption,
                    PaymentAddress* aShippingAddress,
                    const ResponseData& aDetails, const nsAString& aPayerName,
                    const nsAString& aPayerEmail, const nsAString& aPayerPhone);
  void RejectRetry(ErrorResult&& aRejectReason);

 protected:
  ~PaymentResponse();

  void ValidatePaymentValidationErrors(const PaymentValidationErrors& aErrors,
                                       ErrorResult& aRv);

  void ConvertPaymentMethodErrors(JSContext* aCx,
                                  const PaymentValidationErrors& aErrors,
                                  ErrorResult& aRv) const;

  nsresult DispatchUpdateEvent(const nsAString& aType);

 private:
  bool mCompleteCalled;
  PaymentRequest* mRequest;
  nsString mRequestId;
  nsString mMethodName;
  ResponseData mDetails;
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
  // Promise for "PaymentResponse::Retry"
  RefPtr<Promise> mRetryPromise;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PaymentResponse_h
