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
#include "nsIDocumentActivity.h"
#include "nsWrapperCache.h"
#include "PaymentRequestUpdateEvent.h"

namespace mozilla {
namespace dom {

class PaymentAddress;
class PaymentRequestChild;
class PaymentResponse;
class ResponseData;

class GeneralDetails final {
 public:
  GeneralDetails() = default;
  ~GeneralDetails() = default;
  nsString details;
};

class BasicCardDetails final {
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
  BasicCardDetails() = default;
  ~BasicCardDetails() = default;

  Address billingAddress;
};

class ChangeDetails final {
 public:
  enum Type { Unknown = 0, GeneralMethodDetails = 1, BasicCardMethodDetails };
  ChangeDetails() : mType(ChangeDetails::Unknown) {}
  explicit ChangeDetails(const GeneralDetails& aGeneralDetails)
      : mType(GeneralMethodDetails), mGeneralDetails(aGeneralDetails) {}
  explicit ChangeDetails(const BasicCardDetails& aBasicCardDetails)
      : mType(BasicCardMethodDetails), mBasicCardDetails(aBasicCardDetails) {}
  ChangeDetails& operator=(const GeneralDetails& aGeneralDetails) {
    mType = GeneralMethodDetails;
    mGeneralDetails = aGeneralDetails;
    mBasicCardDetails = BasicCardDetails();
    return *this;
  }
  ChangeDetails& operator=(const BasicCardDetails& aBasicCardDetails) {
    mType = BasicCardMethodDetails;
    mGeneralDetails = GeneralDetails();
    mBasicCardDetails = aBasicCardDetails;
    return *this;
  }
  ~ChangeDetails() = default;

  const Type& type() const { return mType; }
  const GeneralDetails& generalDetails() const { return mGeneralDetails; }
  const BasicCardDetails& basicCardDetails() const { return mBasicCardDetails; }

 private:
  Type mType;
  GeneralDetails mGeneralDetails;
  BasicCardDetails mBasicCardDetails;
};

class PaymentRequest final : public DOMEventTargetHelper,
                             public PromiseNativeHandler,
                             public nsIDocumentActivity {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(PaymentRequest,
                                                         DOMEventTargetHelper)
  NS_DECL_NSIDOCUMENTACTIVITY

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<PaymentRequest> CreatePaymentRequest(
      nsPIDOMWindowInner* aWindow, ErrorResult& aRv);

  static bool PrefEnabled(JSContext* aCx, JSObject* aObj);

  // Parameter validation methods
  static void IsValidStandardizedPMI(const nsAString& aIdentifier,
                                     ErrorResult& aRv);

  static void IsValidPaymentMethodIdentifier(const nsAString& aIdentifier,
                                             ErrorResult& aRv);

  static void IsValidMethodData(JSContext* aCx,
                                const Sequence<PaymentMethodData>& aMethodData,
                                ErrorResult& aRv);

  static void IsValidNumber(const nsAString& aItem, const nsAString& aStr,
                            ErrorResult& aRv);

  static void IsNonNegativeNumber(const nsAString& aItem, const nsAString& aStr,
                                  ErrorResult& aRv);

  static void IsValidCurrencyAmount(const nsAString& aItem,
                                    const PaymentCurrencyAmount& aAmount,
                                    const bool aIsTotalItem, ErrorResult& aRv);

  static void IsValidCurrency(const nsAString& aItem,
                              const nsAString& aCurrency, ErrorResult& aRv);

  static void IsValidDetailsInit(const PaymentDetailsInit& aDetails,
                                 const bool aRequestShipping, ErrorResult& aRv);

  static void IsValidDetailsUpdate(const PaymentDetailsUpdate& aDetails,
                                   const bool aRequestShipping,
                                   ErrorResult& aRv);

  static void IsValidDetailsBase(const PaymentDetailsBase& aDetails,
                                 const bool aRequestShipping, ErrorResult& aRv);

  // Webidl implementation
  static already_AddRefed<PaymentRequest> Constructor(
      const GlobalObject& aGlobal,
      const Sequence<PaymentMethodData>& aMethodData,
      const PaymentDetailsInit& aDetails, const PaymentOptions& aOptions,
      ErrorResult& aRv);

  already_AddRefed<Promise> CanMakePayment(ErrorResult& aRv);
  void RespondCanMakePayment(bool aResult);

  already_AddRefed<Promise> Show(
      const Optional<OwningNonNull<Promise>>& detailsPromise, ErrorResult& aRv);
  void RespondShowPayment(const nsAString& aMethodName,
                          const ResponseData& aData,
                          const nsAString& aPayerName,
                          const nsAString& aPayerEmail,
                          const nsAString& aPayerPhone, ErrorResult&& aResult);
  void RejectShowPayment(ErrorResult&& aRejectReason);
  void RespondComplete();

  already_AddRefed<Promise> Abort(ErrorResult& aRv);
  void RespondAbortPayment(bool aResult);

  void RetryPayment(JSContext* aCx, const PaymentValidationErrors& aErrors,
                    ErrorResult& aRv);

  void GetId(nsAString& aRetVal) const;
  void GetInternalId(nsAString& aRetVal);
  void SetId(const nsAString& aId);

  bool Equals(const nsAString& aInternalId) const;

  bool ReadyForUpdate();
  bool IsUpdating() const { return mUpdating; }
  void SetUpdating(bool aUpdating);

  already_AddRefed<PaymentResponse> GetResponse() const;

  already_AddRefed<PaymentAddress> GetShippingAddress() const;
  // Update mShippingAddress and fire shippingaddresschange event
  nsresult UpdateShippingAddress(
      const nsAString& aCountry, const nsTArray<nsString>& aAddressLine,
      const nsAString& aRegion, const nsAString& aRegionCode,
      const nsAString& aCity, const nsAString& aDependentLocality,
      const nsAString& aPostalCode, const nsAString& aSortingCode,
      const nsAString& aOrganization, const nsAString& aRecipient,
      const nsAString& aPhone);

  void SetShippingOption(const nsAString& aShippingOption);
  void GetShippingOption(nsAString& aRetVal) const;
  void GetOptions(PaymentOptions& aRetVal) const;
  void SetOptions(const PaymentOptions& aOptions);
  nsresult UpdateShippingOption(const nsAString& aShippingOption);

  void UpdatePayment(JSContext* aCx, const PaymentDetailsUpdate& aDetails,
                     ErrorResult& aRv);
  void AbortUpdate(ErrorResult& aReason);

  void SetShippingType(const Nullable<PaymentShippingType>& aShippingType);
  Nullable<PaymentShippingType> GetShippingType() const;

  inline void ShippingWasRequested() { mRequestShipping = true; }

  nsresult UpdatePaymentMethod(const nsAString& aMethodName,
                               const ChangeDetails& aMethodDetails);

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;
  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  bool InFullyActiveDocument();

  IMPL_EVENT_HANDLER(merchantvalidation);
  IMPL_EVENT_HANDLER(shippingaddresschange);
  IMPL_EVENT_HANDLER(shippingoptionchange);
  IMPL_EVENT_HANDLER(paymentmethodchange);

  void SetIPC(PaymentRequestChild* aChild) { mIPC = aChild; }

  PaymentRequestChild* GetIPC() const { return mIPC; }

 private:
  PaymentOptions mOptions;

 protected:
  ~PaymentRequest();

  void RegisterActivityObserver();
  void UnregisterActivityObserver();

  nsresult DispatchUpdateEvent(const nsAString& aType);

  nsresult DispatchMerchantValidationEvent(const nsAString& aType);

  nsresult DispatchPaymentMethodChangeEvent(const nsAString& aMethodName,
                                            const ChangeDetails& aMethodDatils);

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
  // Hold a reference to the document to allow unregistering the activity
  // observer.
  RefPtr<Document> mDocument;
  // It is populated when the user chooses a shipping option.
  nsString mShippingOption;

  Nullable<PaymentShippingType> mShippingType;

  // "true" when there is a pending updateWith() call to update the payment
  // request and "false" otherwise.
  bool mUpdating;

  // Whether shipping was requested. This models [[options]].requestShipping,
  // but we don't actually store the full [[options]] internal slot.
  bool mRequestShipping;

  // The error is set in AbortUpdate(). The value is not-failed by default.
  ErrorResult mUpdateError;

  enum { eUnknown, eCreated, eInteractive, eClosed } mState;

  PaymentRequestChild* mIPC;
};
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PaymentRequest_h
