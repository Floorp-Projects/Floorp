/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicCardPayment.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/PaymentRequest.h"
#include "mozilla/dom/PaymentRequestChild.h"
#include "mozilla/dom/PaymentRequestManager.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/intl/MozLocale.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/StaticPrefs.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsIURLParser.h"
#include "nsNetCID.h"
#include "mozilla/dom/MerchantValidationEvent.h"
#include "PaymentResponse.h"

using mozilla::intl::LocaleService;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(PaymentRequest)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(PaymentRequest,
                                               DOMEventTargetHelper)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER because
  // DOMEventTargetHelper does it for us.
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PaymentRequest,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResultPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAcceptPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAbortPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResponse)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mShippingAddress)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFullShippingAddress)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PaymentRequest,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResultPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAcceptPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAbortPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResponse)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mShippingAddress)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFullShippingAddress)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PaymentRequest)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentActivity)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(PaymentRequest, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PaymentRequest, DOMEventTargetHelper)

bool
PaymentRequest::PrefEnabled(JSContext* aCx, JSObject* aObj)
{
#if defined(NIGHTLY_BUILD)
  if (!XRE_IsContentProcess()) {
    return false;
  }
  if (!StaticPrefs::dom_payments_request_enabled()) {
    return false;
  }
  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  nsAutoString region;
  Preferences::GetString("browser.search.region", region);
  if (!manager->IsRegionSupported(region)) {
    return false;
  }
  nsAutoCString locale;
  LocaleService::GetInstance()->GetAppLocaleAsLangTag(locale);
  mozilla::intl::Locale loc = mozilla::intl::Locale(locale);
  if (!(loc.GetLanguage() == "en" && loc.GetRegion() == "US")) {
    return false;
  }

  return true;
#else
  return false;
#endif
}

nsresult
PaymentRequest::IsValidStandardizedPMI(const nsAString& aIdentifier,
                                       nsAString& aErrorMsg)
{
  /*
   *   The syntax of a standardized payment method identifier is given by the
   *   following [ABNF]:
   *
   *       stdpmi = part *( "-" part )
   *       part = 1loweralpha *( DIGIT / loweralpha )
   *       loweralpha =  %x61-7A
   */
  nsString::const_iterator start, end;
  aIdentifier.BeginReading(start);
  aIdentifier.EndReading(end);
  while (start != end) {
    // the first char must be in the range %x61-7A
    if ((*start < 'a' || *start > 'z')) {
      aErrorMsg.AssignLiteral("'");
      aErrorMsg.Append(aIdentifier);
      aErrorMsg.AppendLiteral("' is not valid. The character '");
      aErrorMsg.Append(*start);
      aErrorMsg.AppendLiteral("' at the beginning or after the '-' must be in the range [a-z].");
      return NS_ERROR_RANGE_ERR;
    }
    ++start;
    // the rest can be in the range %x61-7A + DIGITs
    while (start != end && *start != '-' &&
           ((*start >= 'a' && *start <= 'z') || (*start >= '0' && *start <= '9'))) {
      ++start;
    }
    // if the char is not in the range %x61-7A + DIGITs, it must be '-'
    if (start != end && *start != '-') {
      aErrorMsg.AssignLiteral("'");
      aErrorMsg.Append(aIdentifier);
      aErrorMsg.AppendLiteral("' is not valid. The character '");
      aErrorMsg.Append(*start);
      aErrorMsg.AppendLiteral("' must be in the range [a-zA-z0-9-].");
      return NS_ERROR_RANGE_ERR;
    }
    if (*start == '-') {
      ++start;
      // the last char can not be '-'
      if (start == end) {
        aErrorMsg.AssignLiteral("'");
        aErrorMsg.Append(aIdentifier);
        aErrorMsg.AppendLiteral("' is not valid. The last character '");
        aErrorMsg.Append(*start);
        aErrorMsg.AppendLiteral("' must be in the range [a-z0-9].");
        return NS_ERROR_RANGE_ERR;
      }
    }
  }
  return NS_OK;
}

nsresult
PaymentRequest::IsValidPaymentMethodIdentifier(const nsAString& aIdentifier,
                                               nsAString& aErrorMsg)
{
  if (aIdentifier.IsEmpty()) {
    aErrorMsg.AssignLiteral("Payment method identifier is required.");
    return NS_ERROR_TYPE_ERR;
  }
  /*
   *  URL-based payment method identifier
   *
   *  1. If url's scheme is not "https", return false.
   *  2. If url's username or password is not the empty string, return false.
   *  3. Otherwise, return true.
   */
  nsCOMPtr<nsIURLParser> urlParser = do_GetService(NS_STDURLPARSER_CONTRACTID);
  MOZ_ASSERT(urlParser);
  uint32_t schemePos = 0;
  int32_t schemeLen = 0;
  uint32_t authorityPos = 0;
  int32_t authorityLen = 0;
  NS_ConvertUTF16toUTF8 url(aIdentifier);
  nsresult rv = urlParser->ParseURL(url.get(),
                                    url.Length(),
                                    &schemePos, &schemeLen,
                                    &authorityPos, &authorityLen,
                                    nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_RANGE_ERR);
  if (schemeLen == -1) {
    // The PMI is not a URL-based PMI, check if it is a standardized PMI
    return IsValidStandardizedPMI(aIdentifier, aErrorMsg);
  }
  if (!Substring(aIdentifier, schemePos, schemeLen).EqualsASCII("https")) {
    aErrorMsg.AssignLiteral("'");
    aErrorMsg.Append(aIdentifier);
    aErrorMsg.AppendLiteral("' is not valid. The scheme must be 'https'.");
    return NS_ERROR_RANGE_ERR;
  }
  if (Substring(aIdentifier, authorityPos, authorityLen).IsEmpty()) {
    aErrorMsg.AssignLiteral("'");
    aErrorMsg.Append(aIdentifier);
    aErrorMsg.AppendLiteral("' is not valid. hostname can not be empty.");
    return NS_ERROR_RANGE_ERR;
  }

  uint32_t usernamePos = 0;
  int32_t usernameLen = 0;
  uint32_t passwordPos = 0;
  int32_t passwordLen = 0;
  uint32_t hostnamePos = 0;
  int32_t hostnameLen = 0;
  int32_t port = 0;

  NS_ConvertUTF16toUTF8 authority(Substring(aIdentifier, authorityPos, authorityLen));
  rv = urlParser->ParseAuthority(authority.get(),
                                 authority.Length(),
                                 &usernamePos, &usernameLen,
                                 &passwordPos, &passwordLen,
                                 &hostnamePos, &hostnameLen,
                                 &port);
  if (NS_FAILED(rv)) {
    // Handle the special cases that URLParser treats it as an invalid URL, but
    // are used in web-platform-test
    // For exmaple:
    //     https://:@example.com             // should be considered as valid
    //     https://:password@example.com.    // should be considered as invalid
    int32_t atPos = authority.FindChar('@');
    if (atPos >= 0) {
      // only accept the case https://:@xxx
      if (atPos == 1 && authority.CharAt(0) == ':') {
        usernamePos = 0;
        usernameLen = 0;
        passwordPos = 0;
        passwordLen = 0;
      } else {
        // for the fail cases, don't care about what the actual length is.
        usernamePos = 0;
        usernameLen = INT32_MAX;
        passwordPos = 0;
        passwordLen = INT32_MAX;
      }
    } else {
      usernamePos = 0;
      usernameLen = -1;
      passwordPos = 0;
      passwordLen = -1;
    }
    // Parse server information when both username and password are empty or do not
    // exist.
    if ((usernameLen <= 0) && (passwordLen <= 0)) {
      if (authority.Length() - atPos - 1 == 0) {
        aErrorMsg.AssignLiteral("'");
        aErrorMsg.Append(aIdentifier);
        aErrorMsg.AppendLiteral("' is not valid. hostname can not be empty.");
        return NS_ERROR_RANGE_ERR;
      }
      // Re-using nsIURLParser::ParseServerInfo to extract the hostname and port
      // information. This can help us to handle complicated IPv6 cases.
      nsAutoCString serverInfo(Substring(authority,
                                         atPos + 1,
                                         authority.Length() - atPos - 1));
      rv = urlParser->ParseServerInfo(serverInfo.get(),
                                      serverInfo.Length(),
                                      &hostnamePos, &hostnameLen, &port);
      if (NS_FAILED(rv)) {
        // ParseServerInfo returns NS_ERROR_MALFORMED_URI in all fail cases, we
        // probably need a followup bug to figure out the fail reason.
        return NS_ERROR_RANGE_ERR;
      }
    }
  }
  // PMI is valid when usernameLen/passwordLen equals to -1 or 0.
  if (usernameLen > 0 || passwordLen > 0) {
    aErrorMsg.AssignLiteral("'");
    aErrorMsg.Append(aIdentifier);
    aErrorMsg.AssignLiteral("' is not valid. Username and password must be empty.");
    return NS_ERROR_RANGE_ERR;
  }

  // PMI is valid when hostnameLen is larger than 0
  if (hostnameLen <= 0) {
    aErrorMsg.AssignLiteral("'");
    aErrorMsg.Append(aIdentifier);
    aErrorMsg.AppendLiteral("' is not valid. hostname can not be empty.");
    return NS_ERROR_RANGE_ERR;
  }
  return NS_OK;
}

nsresult
PaymentRequest::IsValidMethodData(JSContext* aCx,
                                  const Sequence<PaymentMethodData>& aMethodData,
                                  nsAString& aErrorMsg)
{
  if (!aMethodData.Length()) {
    aErrorMsg.AssignLiteral("At least one payment method is required.");
    return NS_ERROR_TYPE_ERR;
  }

  for (const PaymentMethodData& methodData : aMethodData) {
    nsresult rv = IsValidPaymentMethodIdentifier(methodData.mSupportedMethods,
                                                 aErrorMsg);
    if (NS_FAILED(rv)) {
      return rv;
    }

    RefPtr<BasicCardService> service = BasicCardService::GetService();
    MOZ_ASSERT(service);
    if (service->IsBasicCardPayment(methodData.mSupportedMethods)) {
      if (!methodData.mData.WasPassed()) {
        continue;
      }
      MOZ_ASSERT(aCx);
      if (!service->IsValidBasicCardRequest(aCx,
                                            methodData.mData.Value(),
                                            aErrorMsg)) {
        return NS_ERROR_TYPE_ERR;
      }
    }
  }

  return NS_OK;
}

nsresult
PaymentRequest::IsValidNumber(const nsAString& aItem,
                              const nsAString& aStr,
                              nsAString& aErrorMsg)
{
  nsresult error = NS_ERROR_FAILURE;

  if (!aStr.IsEmpty()) {
    nsAutoString aValue(aStr);

    // If the beginning character is '-', we will check the second one.
    int beginningIndex = (aValue.First() == '-') ? 1 : 0;

    // Ensure
    // - the beginning character is a digit in [0-9], and
    // - the last character is not '.'
    // to follow spec:
    //   https://w3c.github.io/browser-payment-api/#dfn-valid-decimal-monetary-value
    //
    // For example, ".1" is not valid for '.' is not in [0-9],
    // and " 0.1" either for beginning with ' '
    if (aValue.Last() != '.' &&
        aValue.CharAt(beginningIndex) >= '0' &&
        aValue.CharAt(beginningIndex) <= '9') {
      aValue.ToFloat(&error);
    }
  }

  if (NS_FAILED(error)) {
    aErrorMsg.AssignLiteral("The amount.value of \"");
    aErrorMsg.Append(aItem);
    aErrorMsg.AppendLiteral("\"(");
    aErrorMsg.Append(aStr);
    aErrorMsg.AppendLiteral(") must be a valid decimal monetary value.");
    return NS_ERROR_TYPE_ERR;
  }
  return NS_OK;
}

nsresult
PaymentRequest::IsNonNegativeNumber(const nsAString& aItem,
                                    const nsAString& aStr,
                                    nsAString& aErrorMsg)
{
  nsresult error = NS_ERROR_FAILURE;

  if (!aStr.IsEmpty()) {
    nsAutoString aValue(aStr);
    // Ensure
    // - the beginning character is a digit in [0-9], and
    // - the last character is not '.'
    if (aValue.Last() != '.' &&
        aValue.First() >= '0' &&
        aValue.First() <= '9') {
      aValue.ToFloat(&error);
    }
  }

  if (NS_FAILED(error)) {
    aErrorMsg.AssignLiteral("The amount.value of \"");
    aErrorMsg.Append(aItem);
    aErrorMsg.AppendLiteral("\"(");
    aErrorMsg.Append(aStr);
    aErrorMsg.AppendLiteral(") must be a valid and non-negative decimal monetary value.");
    return NS_ERROR_TYPE_ERR;
  }
  return NS_OK;
}

nsresult
PaymentRequest::IsValidCurrency(const nsAString& aItem,
                                const nsAString& aCurrency,
                                nsAString& aErrorMsg)
{
   /*
    *  According to spec in https://w3c.github.io/payment-request/#validity-checkers,
    *  perform currency validation with following criteria
    *  1. The currency length must be 3.
    *  2. The currency contains any character that must be in the range "A" to "Z"
    *     (U+0041 to U+005A) or the range "a" to "z" (U+0061 to U+007A)
    */
   if (aCurrency.Length() != 3) {
     aErrorMsg.AssignLiteral("The length amount.currency of \"");
     aErrorMsg.Append(aItem);
     aErrorMsg.AppendLiteral("\"(");
     aErrorMsg.Append(aCurrency);
     aErrorMsg.AppendLiteral(") must be 3.");
     return NS_ERROR_RANGE_ERR;
   }
   // Don't use nsUnicharUtils::ToUpperCase, it converts the invalid "ınr" PMI to
   // to the valid one "INR".
   for (uint32_t idx = 0; idx < aCurrency.Length(); ++idx) {
     if ((aCurrency.CharAt(idx) >= 'A' && aCurrency.CharAt(idx) <= 'Z') ||
         (aCurrency.CharAt(idx) >= 'a' && aCurrency.CharAt(idx) <= 'z')) {
       continue;
     }
     aErrorMsg.AssignLiteral("The character amount.currency of \"");
     aErrorMsg.Append(aItem);
     aErrorMsg.AppendLiteral("\"(");
     aErrorMsg.Append(aCurrency);
     aErrorMsg.AppendLiteral(") must be in the range 'A' to 'Z'(U+0041 to U+005A) or 'a' to 'z'(U+0061 to U+007A).");
     return NS_ERROR_RANGE_ERR;
   }
   return NS_OK;
}

nsresult
PaymentRequest::IsValidCurrencyAmount(const nsAString& aItem,
                                      const PaymentCurrencyAmount& aAmount,
                                      const bool aIsTotalItem,
                                      nsAString& aErrorMsg)
{
  nsresult rv;
  rv = IsValidCurrency(aItem, aAmount.mCurrency, aErrorMsg);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (aIsTotalItem) {
    rv = IsNonNegativeNumber(aItem, aAmount.mValue, aErrorMsg);
    if (NS_FAILED(rv)) {
      return rv;
    }
  } else {
    rv = IsValidNumber(aItem, aAmount.mValue, aErrorMsg);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
PaymentRequest::IsValidDetailsInit(const PaymentDetailsInit& aDetails,
                                   const bool aRequestShipping,
                                   nsAString& aErrorMsg)
{
  // Check the amount.value and amount.currency of detail.total
  nsresult rv = IsValidCurrencyAmount(NS_LITERAL_STRING("details.total"),
                                      aDetails.mTotal.mAmount,
                                      true, // isTotalItem
                                      aErrorMsg);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return IsValidDetailsBase(aDetails, aRequestShipping, aErrorMsg);
}

nsresult
PaymentRequest::IsValidDetailsUpdate(const PaymentDetailsUpdate& aDetails,
                                     const bool aRequestShipping)
{
  nsAutoString message;
  // Check the amount.value and amount.currency of detail.total
  nsresult rv = IsValidCurrencyAmount(NS_LITERAL_STRING("details.total"),
                                      aDetails.mTotal.mAmount,
                                      true, // isTotalItem
                                      message);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return IsValidDetailsBase(aDetails, aRequestShipping, message);
}

nsresult
PaymentRequest::IsValidDetailsBase(const PaymentDetailsBase& aDetails,
                                   const bool aRequestShipping,
                                   nsAString& aErrorMsg)
{
  nsresult rv;
  // Check the amount.value of each item in the display items
  if (aDetails.mDisplayItems.WasPassed()) {
    const Sequence<PaymentItem>& displayItems = aDetails.mDisplayItems.Value();
    for (const PaymentItem& displayItem : displayItems) {
      rv = IsValidCurrencyAmount(displayItem.mLabel,
                                 displayItem.mAmount,
                                 false,  // isTotalItem
                                 aErrorMsg);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  // Check the shipping option
  if (aDetails.mShippingOptions.WasPassed() && aRequestShipping) {
    const Sequence<PaymentShippingOption>& shippingOptions = aDetails.mShippingOptions.Value();
    nsTArray<nsString> seenIDs;
    for (const PaymentShippingOption& shippingOption : shippingOptions) {
      rv = IsValidCurrencyAmount(NS_LITERAL_STRING("details.shippingOptions"),
                                 shippingOption.mAmount,
                                 false,  // isTotalItem
                                 aErrorMsg);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (seenIDs.Contains(shippingOption.mId)) {
        aErrorMsg.AssignLiteral("Duplicate shippingOption id '");
        aErrorMsg.Append(shippingOption.mId);
        aErrorMsg.AppendLiteral("'");
        return NS_ERROR_TYPE_ERR;
      }
      seenIDs.AppendElement(shippingOption.mId);
    }
  }

  // Check payment details modifiers
  if (aDetails.mModifiers.WasPassed()) {
    const Sequence<PaymentDetailsModifier>& modifiers = aDetails.mModifiers.Value();
    for (const PaymentDetailsModifier& modifier : modifiers) {
      rv = IsValidPaymentMethodIdentifier(modifier.mSupportedMethods, aErrorMsg);
      if (NS_FAILED(rv)) {
        return rv;
      }
      rv = IsValidCurrencyAmount(NS_LITERAL_STRING("details.modifiers.total"),
                                 modifier.mTotal.mAmount,
                                 true, // isTotalItem
                                 aErrorMsg);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (modifier.mAdditionalDisplayItems.WasPassed()) {
        const Sequence<PaymentItem>& displayItems = modifier.mAdditionalDisplayItems.Value();
        for (const PaymentItem& displayItem : displayItems) {
          rv = IsValidCurrencyAmount(displayItem.mLabel,
                                     displayItem.mAmount,
                                     false,  // isTotalItem
                                     aErrorMsg);
          if (NS_FAILED(rv)) {
            return rv;
          }
        }
      }
    }
  }

  return NS_OK;
}

already_AddRefed<PaymentRequest>
PaymentRequest::Constructor(const GlobalObject& aGlobal,
                            const Sequence<PaymentMethodData>& aMethodData,
                            const PaymentDetailsInit& aDetails,
                            const PaymentOptions& aOptions,
                            ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  // the feature can only be used in an active document
  if (!window->IsCurrentInnerWindow()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }


  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  if (!FeaturePolicyUtils::IsFeatureAllowed(doc,
                                            NS_LITERAL_STRING("payment"))) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // Check if AllowPaymentRequest on the owner document
  if (!doc->AllowPaymentRequest()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // Get the top level principal
  nsCOMPtr<nsIDocument> topLevelDoc = doc->GetTopLevelContentDocument();
  MOZ_ASSERT(topLevelDoc);
  nsCOMPtr<nsIPrincipal> topLevelPrincipal = topLevelDoc->NodePrincipal();

  // Check payment methods and details
  nsAutoString message;
  nsresult rv = IsValidMethodData(aGlobal.Context(),
                                  aMethodData,
                                  message);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_TYPE_ERR) {
      aRv.ThrowTypeError<MSG_ILLEGAL_TYPE_PR_CONSTRUCTOR>(message);
    } else if (rv == NS_ERROR_RANGE_ERR) {
      aRv.ThrowRangeError<MSG_ILLEGAL_RANGE_PR_CONSTRUCTOR>(message);
    }
    return nullptr;
  }
  rv = IsValidDetailsInit(aDetails, aOptions.mRequestShipping, message);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_TYPE_ERR) {
      aRv.ThrowTypeError<MSG_ILLEGAL_TYPE_PR_CONSTRUCTOR>(message);
    } else if (rv == NS_ERROR_RANGE_ERR) {
      aRv.ThrowRangeError<MSG_ILLEGAL_RANGE_PR_CONSTRUCTOR>(message);
    }
    return nullptr;
  }

  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  if (NS_WARN_IF(!manager)) {
    return nullptr;
  }

  // Create PaymentRequest and set its |mId|
  RefPtr<PaymentRequest> request;
  rv = manager->CreatePayment(aGlobal.Context(), window, topLevelPrincipal, aMethodData,
                              aDetails, aOptions, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_TYPE_ERR);
    return nullptr;
  }
  return request.forget();
}

already_AddRefed<PaymentRequest>
PaymentRequest::CreatePaymentRequest(nsPIDOMWindowInner* aWindow, nsresult& aRv)
{
  // Generate a unique id for identification
  nsID uuid;
  aRv = nsContentUtils::GenerateUUIDInPlace(uuid);
  if (NS_WARN_IF(NS_FAILED(aRv))) {
    return nullptr;
  }

  // Build a string in {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} format
  char buffer[NSID_LENGTH];
  uuid.ToProvidedString(buffer);

  // Remove {} and the null terminator
  nsAutoString id;
  id.AssignASCII(&buffer[1], NSID_LENGTH - 3);

  // Create payment request with generated id
  RefPtr<PaymentRequest> request = new PaymentRequest(aWindow, id);
  return request.forget();
}

PaymentRequest::PaymentRequest(nsPIDOMWindowInner* aWindow, const nsAString& aInternalId)
  : DOMEventTargetHelper(aWindow)
  , mInternalId(aInternalId)
  , mShippingAddress(nullptr)
  , mUpdating(false)
  , mRequestShipping(false)
  , mDeferredShow(false)
  , mUpdateError(NS_OK)
  , mState(eCreated)
  , mIPC(nullptr)
{
  MOZ_ASSERT(aWindow);
  RegisterActivityObserver();
}

already_AddRefed<Promise>
PaymentRequest::CanMakePayment(ErrorResult& aRv)
{
  if (mState != eCreated) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  if (mResultPromise) {
    // XXX This doesn't match the spec but does match Chromium.
    aRv.Throw(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return nullptr;
  }

  nsIGlobalObject* global = GetOwnerGlobal();
  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(global, result);
  if (result.Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  nsresult rv = manager->CanMakePayment(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_FAILURE);
    return promise.forget();
  }
  mResultPromise = promise;
  return promise.forget();
}

void
PaymentRequest::RespondCanMakePayment(bool aResult)
{
  MOZ_ASSERT(mResultPromise);
  mResultPromise->MaybeResolve(aResult);
  mResultPromise = nullptr;
}

already_AddRefed<Promise>
PaymentRequest::Show(const Optional<OwningNonNull<Promise>>& aDetailsPromise,
                     ErrorResult& aRv)
{
  nsIGlobalObject* global = GetOwnerGlobal();
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(global);
  MOZ_ASSERT(win);
  nsIDocument* doc = win->GetExtantDoc();

  if (!EventStateManager::IsHandlingUserInput()) {
    nsString msg = NS_LITERAL_STRING("User activation is now required to call PaymentRequest.show()");
    nsContentUtils::ReportToConsoleNonLocalized(msg,
                                                nsIScriptError::warningFlag,
                                                NS_LITERAL_CSTRING("Security"),
                                                doc);
    if (StaticPrefs::dom_payments_request_user_interaction_required()) {
      aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }
  }

  if (!doc || !doc->IsCurrentActiveDocument()) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return nullptr;
  }

  if (mState != eCreated) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(global, result);
  if (result.Failed()) {
    mState = eClosed;
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (aDetailsPromise.WasPassed()) {
    aDetailsPromise.Value().AppendNativeHandler(this);
    mUpdating = true;
    mDeferredShow = true;
  }

  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  nsresult rv = manager->ShowPayment(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (rv == NS_ERROR_ABORT) {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    } else {
      promise->MaybeReject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    }
    mState = eClosed;
    return promise.forget();
  }

  mAcceptPromise = promise;
  mState = eInteractive;
  return promise.forget();
}

void
PaymentRequest::RejectShowPayment(nsresult aRejectReason)
{
  MOZ_ASSERT(mAcceptPromise || mResponse);
  MOZ_ASSERT(mState == eInteractive);

  if (mResponse) {
    mResponse->RejectRetry(aRejectReason);
  } else {
    mAcceptPromise->MaybeReject(aRejectReason);
  }
  mState = eClosed;
  mAcceptPromise = nullptr;
}

void
PaymentRequest::RespondShowPayment(const nsAString& aMethodName,
                                   const ResponseData& aDetails,
                                   const nsAString& aPayerName,
                                   const nsAString& aPayerEmail,
                                   const nsAString& aPayerPhone,
                                   nsresult aRv)
{
  MOZ_ASSERT(mAcceptPromise || mResponse);
  MOZ_ASSERT(mState == eInteractive);

  if (NS_FAILED(aRv)) {
    RejectShowPayment(aRv);
    return;
  }

  // https://github.com/w3c/payment-request/issues/692
  mShippingAddress.swap(mFullShippingAddress);
  mFullShippingAddress = nullptr;

  if (mResponse) {
    mResponse->RespondRetry(aMethodName, mShippingOption, mShippingAddress,
                            aDetails, aPayerName, aPayerEmail, aPayerPhone);
  } else {
    RefPtr<PaymentResponse> paymentResponse =
      new PaymentResponse(GetOwner(), this, mId, aMethodName,
                          mShippingOption, mShippingAddress, aDetails,
                          aPayerName, aPayerEmail, aPayerPhone);
    mResponse = paymentResponse;
    mAcceptPromise->MaybeResolve(paymentResponse);
  }

  mState = eClosed;
  mAcceptPromise = nullptr;
}

void
PaymentRequest::RespondComplete()
{
  MOZ_ASSERT(mResponse);
  mResponse->RespondComplete();
}

already_AddRefed<Promise>
PaymentRequest::Abort(ErrorResult& aRv)
{
  if (mState != eInteractive) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  if (mAbortPromise) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsIGlobalObject* global = GetOwnerGlobal();
  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(global, result);
  if (result.Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  // It's possible to be called between show and its promise resolving.
  nsresult rv = manager->AbortPayment(this, mDeferredShow);
  mDeferredShow = false;
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  mAbortPromise = promise;
  return promise.forget();
}

void
PaymentRequest::RespondAbortPayment(bool aSuccess)
{
  // Check whether we are aborting the update:
  //
  // - If |mUpdateError| is not NS_OK, we are aborting the update as
  //   |mUpdateError| was set in method |AbortUpdate|.
  //   => Reject |mAcceptPromise| and reset |mUpdateError| to complete
  //      the action, regardless of |aSuccess|.
  //
  // - Otherwise, we are handling |Abort| method call from merchant.
  //   => Resolve/Reject |mAbortPromise| based on |aSuccess|.
  if (NS_FAILED(mUpdateError)) {
    // Respond show with mUpdateError, set mUpdating to false.
    mUpdating = false;
    RespondShowPayment(EmptyString(), ResponseData(), EmptyString(),
                       EmptyString(), EmptyString(), mUpdateError);
    mUpdateError = NS_OK;
    return;
  }

  MOZ_ASSERT(mAbortPromise);
  MOZ_ASSERT(mState == eInteractive);

  if (aSuccess) {
    mAbortPromise->MaybeResolve(JS::UndefinedHandleValue);
    mAbortPromise = nullptr;
    RejectShowPayment(NS_ERROR_DOM_ABORT_ERR);
  } else {
    mAbortPromise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    mAbortPromise = nullptr;
  }
}

nsresult
PaymentRequest::UpdatePayment(JSContext* aCx, const PaymentDetailsUpdate& aDetails,
                              bool aDeferredShow)
{
  NS_ENSURE_ARG_POINTER(aCx);
  if (mState != eInteractive) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }
  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  if (NS_WARN_IF(!manager)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = manager->UpdatePayment(aCx, this, aDetails, mRequestShipping,
                                       aDeferredShow);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

void
PaymentRequest::AbortUpdate(nsresult aRv, bool aDeferredShow)
{
  MOZ_ASSERT(NS_FAILED(aRv));

  if (mState != eInteractive) {
    return;
  }
  // Close down any remaining user interface.
  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  nsresult rv = manager->AbortPayment(this, aDeferredShow);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Remember update error |aRv| and do the following steps in RespondShowPayment.
  // 1. Set target.state to closed
  // 2. Reject the promise target.acceptPromise with exception "aRv"
  // 3. Abort the algorithm with update error
  mUpdateError = aRv;
}

nsresult
PaymentRequest::RetryPayment(JSContext* aCx, const PaymentValidationErrors& aErrors)
{
  if (mState == eInteractive) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }
  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  nsresult rv = manager->RetryPayment(aCx, this, aErrors);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  mState = eInteractive;
  return NS_OK;
}

void
PaymentRequest::GetId(nsAString& aRetVal) const
{
  aRetVal = mId;
}

void
PaymentRequest::GetInternalId(nsAString& aRetVal)
{
  aRetVal = mInternalId;
}

void
PaymentRequest::SetId(const nsAString& aId)
{
  mId = aId;
}

bool
PaymentRequest::Equals(const nsAString& aInternalId) const
{
  return mInternalId.Equals(aInternalId);
}

bool
PaymentRequest::ReadyForUpdate()
{
  return mState == eInteractive && !mUpdating;
}

void
PaymentRequest::SetUpdating(bool aUpdating)
{
  mUpdating = aUpdating;
}

already_AddRefed<PaymentResponse>
PaymentRequest::GetResponse() const
{
  RefPtr<PaymentResponse> response = mResponse;
  return response.forget();
}

nsresult
PaymentRequest::DispatchUpdateEvent(const nsAString& aType)
{
  MOZ_ASSERT(ReadyForUpdate());

  PaymentRequestUpdateEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<PaymentRequestUpdateEvent> event =
    PaymentRequestUpdateEvent::Constructor(this, aType, init);
  event->SetTrusted(true);
  event->SetRequest(this);

  ErrorResult rv;
  DispatchEvent(*event, rv);
  return rv.StealNSResult();
}

nsresult
PaymentRequest::DispatchMerchantValidationEvent(const nsAString& aType)
{
  MOZ_ASSERT(ReadyForUpdate());

  MerchantValidationEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mValidationURL = EmptyString();

  ErrorResult rv;
  RefPtr<MerchantValidationEvent> event =
    MerchantValidationEvent::Constructor(this, aType, init, rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }
  event->SetTrusted(true);
  event->SetRequest(this);

  DispatchEvent(*event, rv);
  return rv.StealNSResult();
}

already_AddRefed<PaymentAddress>
PaymentRequest::GetShippingAddress() const
{
  RefPtr<PaymentAddress> address = mShippingAddress;
  return address.forget();
}

nsresult
PaymentRequest::UpdateShippingAddress(const nsAString& aCountry,
                                      const nsTArray<nsString>& aAddressLine,
                                      const nsAString& aRegion,
                                      const nsAString& aRegionCode,
                                      const nsAString& aCity,
                                      const nsAString& aDependentLocality,
                                      const nsAString& aPostalCode,
                                      const nsAString& aSortingCode,
                                      const nsAString& aOrganization,
                                      const nsAString& aRecipient,
                                      const nsAString& aPhone)
{
  nsTArray<nsString> emptyArray;
  mShippingAddress = new PaymentAddress(GetOwner(), aCountry, emptyArray,
                                        aRegion, aRegionCode, aCity,
                                        aDependentLocality, aPostalCode,
                                        aSortingCode, EmptyString(),
                                        EmptyString(), EmptyString());
  mFullShippingAddress = new PaymentAddress(GetOwner(), aCountry, aAddressLine,
                                            aRegion, aRegionCode, aCity,
                                            aDependentLocality,
                                            aPostalCode, aSortingCode,
                                            aOrganization, aRecipient, aPhone);
  // Fire shippingaddresschange event
  return DispatchUpdateEvent(NS_LITERAL_STRING("shippingaddresschange"));
}

void
PaymentRequest::SetShippingOption(const nsAString& aShippingOption)
{
  mShippingOption = aShippingOption;
}

void
PaymentRequest::GetShippingOption(nsAString& aRetVal) const
{
  aRetVal = mShippingOption;
}

nsresult
PaymentRequest::UpdateShippingOption(const nsAString& aShippingOption)
{
  mShippingOption = aShippingOption;

  // Fire shippingaddresschange event
  return DispatchUpdateEvent(NS_LITERAL_STRING("shippingoptionchange"));
}

void
PaymentRequest::SetShippingType(const Nullable<PaymentShippingType>& aShippingType)
{
  mShippingType = aShippingType;
}

Nullable<PaymentShippingType>
PaymentRequest::GetShippingType() const
{
  return mShippingType;
}

void PaymentRequest::GetOptions(PaymentOptions& aRetVal) const
{
  aRetVal = mOptions;
}

void PaymentRequest::SetOptions(const PaymentOptions& aOptions)
{
  mOptions = aOptions;
}

void
PaymentRequest::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  MOZ_ASSERT(aCx);
  mUpdating = false;
  if (NS_WARN_IF(!aValue.isObject())) {
    return;
  }

  // Converting value to a PaymentDetailsUpdate dictionary
  PaymentDetailsUpdate details;
  if (!details.Init(aCx, aValue)) {
    AbortUpdate(NS_ERROR_DOM_TYPE_ERR, mDeferredShow);
    JS_ClearPendingException(aCx);
    return;
  }

  nsresult rv = IsValidDetailsUpdate(details, mRequestShipping);
  if (NS_FAILED(rv)) {
    AbortUpdate(rv, mDeferredShow);
    return;
  }

  // Update the PaymentRequest with the new details
  if (NS_FAILED(UpdatePayment(aCx, details, mDeferredShow))) {
    AbortUpdate(NS_ERROR_DOM_ABORT_ERR, mDeferredShow);
    return;
  }

  mDeferredShow = false;
}

void
PaymentRequest::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  MOZ_ASSERT(mDeferredShow);
  mUpdating = false;
  AbortUpdate(NS_ERROR_DOM_ABORT_ERR, mDeferredShow);
  mDeferredShow = false;
}

void
PaymentRequest::RegisterActivityObserver()
{
  if (nsPIDOMWindowInner* window = GetOwner()) {
    nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
    if (doc) {
      doc->RegisterActivityObserver(
        NS_ISUPPORTS_CAST(nsIDocumentActivity*, this));
    }
  }
}

void
PaymentRequest::UnregisterActivityObserver()
{
  if (nsPIDOMWindowInner* window = GetOwner()) {
    nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
    if (doc) {
      doc->UnregisterActivityObserver(
        NS_ISUPPORTS_CAST(nsIDocumentActivity*, this));
    }
  }
}

void
PaymentRequest::NotifyOwnerDocumentActivityChanged()
{
  nsPIDOMWindowInner* window = GetOwner();
  NS_ENSURE_TRUE_VOID(window);
  nsIDocument* doc = window->GetExtantDoc();
  NS_ENSURE_TRUE_VOID(doc);

  if (!doc->IsCurrentActiveDocument()) {
    RefPtr<PaymentRequestManager> mgr = PaymentRequestManager::GetSingleton();
    mgr->ClosePayment(this);
  }
}

PaymentRequest::~PaymentRequest()
{
  if (mIPC) {
    // If we're being destroyed, the PaymentRequestManager isn't holding any
    // references to us and we can't be waiting for any replies.
    mIPC->MaybeDelete(false);
  }
  UnregisterActivityObserver();
}

JSObject*
PaymentRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PaymentRequest_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
