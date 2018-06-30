/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentRequest_h
#define mozilla_dom_PaymentRequest_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/PaymentRequestBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/ErrorResult.h"
#include "nsWrapperCache.h"
#include "PaymentRequestUpdateEvent.h"

namespace mozilla {
namespace dom {

class EventHandlerNonNull;
class PaymentAddress;
class PaymentRequestChild;
class PaymentResponse;

class PaymentRequest final : public DOMEventTargetHelper
                           , public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(PaymentRequest, DOMEventTargetHelper)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<PaymentRequest>
  CreatePaymentRequest(nsPIDOMWindowInner* aWindow, nsresult& aRv);

  static bool PrefEnabled(JSContext* aCx, JSObject* aObj);

  static nsresult IsValidStandardizedPMI(const nsAString& aIdentifier,
                                         nsAString& aErrorMsg);

  static nsresult IsValidPaymentMethodIdentifier(const nsAString& aIdentifier,
                                                 nsAString& aErrorMsg);

  static nsresult IsValidMethodData(JSContext* aCx,
                                    const Sequence<PaymentMethodData>& aMethodData,
                                    nsAString& aErrorMsg);

  static nsresult
  IsValidNumber(const nsAString& aItem,
                const nsAString& aStr,
                nsAString& aErrorMsg);
  static nsresult
  IsNonNegativeNumber(const nsAString& aItem,
                      const nsAString& aStr,
                      nsAString& aErrorMsg);

  static nsresult
  IsValidCurrencyAmount(const nsAString& aItem,
                        const PaymentCurrencyAmount& aAmount,
                        const bool aIsTotalItem,
                        nsAString& aErrorMsg);

  static nsresult
  IsValidCurrency(const nsAString& aItem,
                  const nsAString& aCurrency,
                  nsAString& aErrorMsg);

  static nsresult
  IsValidDetailsInit(const PaymentDetailsInit& aDetails,
                     const bool aRequestShipping,
                     nsAString& aErrorMsg);

  static nsresult
  IsValidDetailsUpdate(const PaymentDetailsUpdate& aDetails,
                       const bool aRequestShipping);

  static nsresult
  IsValidDetailsBase(const PaymentDetailsBase& aDetails,
                     const bool aRequestShipping,
                     nsAString& aErrorMsg);

  static already_AddRefed<PaymentRequest>
  Constructor(const GlobalObject& aGlobal,
              const Sequence<PaymentMethodData>& aMethodData,
              const PaymentDetailsInit& aDetails,
              const PaymentOptions& aOptions,
              ErrorResult& aRv);

  already_AddRefed<Promise> CanMakePayment(ErrorResult& aRv);
  void RespondCanMakePayment(bool aResult);

  already_AddRefed<Promise> Show(const Optional<OwningNonNull<Promise>>& detailsPromise,
                                 ErrorResult& aRv);
  void RespondShowPayment(const nsAString& aMethodName,
                          const nsAString& aDetails,
                          const nsAString& aPayerName,
                          const nsAString& aPayerEmail,
                          const nsAString& aPayerPhone,
                          nsresult aRv);
  void RejectShowPayment(nsresult aRejectReason);
  void RespondComplete();

  already_AddRefed<Promise> Abort(ErrorResult& aRv);
  void RespondAbortPayment(bool aResult);

  void GetId(nsAString& aRetVal) const;
  void GetInternalId(nsAString& aRetVal);
  void SetId(const nsAString& aId);

  bool Equals(const nsAString& aInternalId) const;

  bool ReadyForUpdate();
  bool IsUpdating() const { return mUpdating; }
  void SetUpdating(bool aUpdating);

  already_AddRefed<PaymentAddress> GetShippingAddress() const;
  // Update mShippingAddress and fire shippingaddresschange event
  nsresult UpdateShippingAddress(const nsAString& aCountry,
                                 const nsTArray<nsString>& aAddressLine,
                                 const nsAString& aRegion,
                                 const nsAString& aCity,
                                 const nsAString& aDependentLocality,
                                 const nsAString& aPostalCode,
                                 const nsAString& aSortingCode,
                                 const nsAString& aLanguageCode,
                                 const nsAString& aOrganization,
                                 const nsAString& aRecipient,
                                 const nsAString& aPhone);


  void SetShippingOption(const nsAString& aShippingOption);
  void GetShippingOption(nsAString& aRetVal) const;
  nsresult UpdateShippingOption(const nsAString& aShippingOption);

  nsresult UpdatePayment(JSContext* aCx, const PaymentDetailsUpdate& aDetails,
                         bool aDeferredShow);
  void AbortUpdate(nsresult aRv, bool aDeferredShow);

  void SetShippingType(const Nullable<PaymentShippingType>& aShippingType);
  Nullable<PaymentShippingType> GetShippingType() const;

  inline void ShippingWasRequested()
  {
    mRequestShipping = true;
  }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;
  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  IMPL_EVENT_HANDLER(shippingaddresschange);
  IMPL_EVENT_HANDLER(shippingoptionchange);
  IMPL_EVENT_HANDLER(paymentmethodchange);

  void SetIPC(PaymentRequestChild* aChild)
  {
    mIPC = aChild;
  }

  PaymentRequestChild* GetIPC()
  {
    return mIPC;
  }

protected:
  ~PaymentRequest();

  nsresult DispatchUpdateEvent(const nsAString& aType);

  PaymentRequest(nsPIDOMWindowInner* aWindow, const nsAString& aInternalId);

  // Id for internal identification
  nsString mInternalId;
  // Id for communicating with merchant side
  // mId is initialized to details.id if it exists
  // otherwise, mId has the same value as mInternalId.
  nsString mId;
  // Promise for "PaymentRequest::CanMakePayment"
  RefPtr<Promise> mResultPromise;
  // Promise for "PaymentRequest::Show"
  RefPtr<Promise> mAcceptPromise;
  // Promise for "PaymentRequest::Abort"
  RefPtr<Promise> mAbortPromise;
  // Resolve mAcceptPromise with mResponse if user accepts the request.
  RefPtr<PaymentResponse> mResponse;
  // The redacted shipping address.
  RefPtr<PaymentAddress> mShippingAddress;
  // The full shipping address to be used in the response upon payment.
  RefPtr<PaymentAddress> mFullShippingAddress;
  // It is populated when the user chooses a shipping option.
  nsString mShippingOption;

  Nullable<PaymentShippingType> mShippingType;

  // "true" when there is a pending updateWith() call to update the payment request
  // and "false" otherwise.
  bool mUpdating;

  // Whether shipping was requested. This models [[options]].requestShipping,
  // but we don't actually store the full [[options]] internal slot.
  bool mRequestShipping;

  // True if the user passed a promise to show, causing us to defer telling the
  // front end about it.
  bool mDeferredShow;

  // The error is set in AbortUpdate(). The value is NS_OK by default.
  nsresult mUpdateError;

  enum {
    eUnknown,
    eCreated,
    eInteractive,
    eClosed
  } mState;

  PaymentRequestChild* mIPC;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PaymentRequest_h
