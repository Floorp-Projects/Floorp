/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicCardPayment.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/PaymentMethodChangeEvent.h"
#include "mozilla/dom/PaymentRequest.h"
#include "mozilla/dom/PaymentRequestChild.h"
#include "mozilla/dom/PaymentRequestManager.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/intl/MozLocale.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsContentUtils.h"
#include "nsIDUtils.h"
#include "nsImportModule.h"
#include "nsIRegion.h"
#include "nsIScriptError.h"
#include "nsIURLParser.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/MerchantValidationEvent.h"
#include "PaymentResponse.h"

using mozilla::intl::LocaleService;

namespace mozilla::dom {

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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PaymentRequest,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResultPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAcceptPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAbortPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResponse)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mShippingAddress)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFullShippingAddress)
  tmp->UnregisterActivityObserver();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PaymentRequest)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentActivity)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(PaymentRequest, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PaymentRequest, DOMEventTargetHelper)

bool PaymentRequest::PrefEnabled(JSContext* aCx, JSObject* aObj) {
#if defined(NIGHTLY_BUILD)
  if (!XRE_IsContentProcess()) {
    return false;
  }
  if (!StaticPrefs::dom_payments_request_enabled()) {
    return false;
  }
  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);

  nsCOMPtr<nsIRegion> regionJsm =
      do_ImportModule("resource://gre/modules/Region.jsm", "Region");
  nsAutoString region;
  nsresult rv = regionJsm->GetHome(region);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (!manager->IsRegionSupported(region)) {
    return false;
  }
  nsAutoCString locale;
  LocaleService::GetInstance()->GetAppLocaleAsBCP47(locale);
  mozilla::intl::Locale loc = mozilla::intl::Locale(locale);
  if (!(loc.GetLanguage() == "en" && loc.GetRegion() == "US")) {
    return false;
  }

  return true;
#else
  return false;
#endif
}

void PaymentRequest::IsValidStandardizedPMI(const nsAString& aIdentifier,
                                            ErrorResult& aRv) {
  /*
   *   The syntax of a standardized payment method identifier is given by the
   *   following [ABNF]:
   *
   *       stdpmi = part *( "-" part )
   *       part = 1loweralpha *( DIGIT / loweralpha )
   *       loweralpha =  %x61-7A
   */
  const char16_t* start = aIdentifier.BeginReading();
  const char16_t* end = aIdentifier.EndReading();
  while (start != end) {
    // the first char must be in the range %x61-7A
    if ((*start < 'a' || *start > 'z')) {
      nsAutoCString error;
      error.AssignLiteral("'");
      error.Append(NS_ConvertUTF16toUTF8(aIdentifier));
      error.AppendLiteral("' is not valid. The character '");
      error.Append(NS_ConvertUTF16toUTF8(start, 1));
      error.AppendLiteral(
          "' at the beginning or after the '-' must be in the range [a-z].");
      aRv.ThrowRangeError(error);
      return;
    }
    ++start;
    // the rest can be in the range %x61-7A + DIGITs
    while (start != end && *start != '-' &&
           ((*start >= 'a' && *start <= 'z') ||
            (*start >= '0' && *start <= '9'))) {
      ++start;
    }
    // if the char is not in the range %x61-7A + DIGITs, it must be '-'
    if (start != end && *start != '-') {
      nsAutoCString error;
      error.AssignLiteral("'");
      error.Append(NS_ConvertUTF16toUTF8(aIdentifier));
      error.AppendLiteral("' is not valid. The character '");
      error.Append(NS_ConvertUTF16toUTF8(start, 1));
      error.AppendLiteral("' must be in the range [a-zA-z0-9-].");
      aRv.ThrowRangeError(error);
      return;
    }
    if (*start == '-') {
      ++start;
      // the last char can not be '-'
      if (start == end) {
        nsAutoCString error;
        error.AssignLiteral("'");
        error.Append(NS_ConvertUTF16toUTF8(aIdentifier));
        error.AppendLiteral("' is not valid. The last character '");
        error.Append(NS_ConvertUTF16toUTF8(start, 1));
        error.AppendLiteral("' must be in the range [a-z0-9].");
        aRv.ThrowRangeError(error);
        return;
      }
    }
  }
}

void PaymentRequest::IsValidPaymentMethodIdentifier(
    const nsAString& aIdentifier, ErrorResult& aRv) {
  if (aIdentifier.IsEmpty()) {
    aRv.ThrowTypeError("Payment method identifier is required.");
    return;
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
  nsresult rv =
      urlParser->ParseURL(url.get(), url.Length(), &schemePos, &schemeLen,
                          &authorityPos, &authorityLen, nullptr, nullptr);
  if (NS_FAILED(rv)) {
    nsAutoCString error;
    error.AppendLiteral("Error parsing payment method identifier '");
    error.Append(NS_ConvertUTF16toUTF8(aIdentifier));
    error.AppendLiteral("'as a URL.");
    aRv.ThrowRangeError(error);
    return;
  }

  if (schemeLen == -1) {
    // The PMI is not a URL-based PMI, check if it is a standardized PMI
    IsValidStandardizedPMI(aIdentifier, aRv);
    return;
  }
  if (!Substring(aIdentifier, schemePos, schemeLen).EqualsASCII("https")) {
    nsAutoCString error;
    error.AssignLiteral("'");
    error.Append(NS_ConvertUTF16toUTF8(aIdentifier));
    error.AppendLiteral("' is not valid. The scheme must be 'https'.");
    aRv.ThrowRangeError(error);
    return;
  }
  if (Substring(aIdentifier, authorityPos, authorityLen).IsEmpty()) {
    nsAutoCString error;
    error.AssignLiteral("'");
    error.Append(NS_ConvertUTF16toUTF8(aIdentifier));
    error.AppendLiteral("' is not valid. hostname can not be empty.");
    aRv.ThrowRangeError(error);
    return;
  }

  uint32_t usernamePos = 0;
  int32_t usernameLen = 0;
  uint32_t passwordPos = 0;
  int32_t passwordLen = 0;
  uint32_t hostnamePos = 0;
  int32_t hostnameLen = 0;
  int32_t port = 0;

  NS_ConvertUTF16toUTF8 authority(
      Substring(aIdentifier, authorityPos, authorityLen));
  rv = urlParser->ParseAuthority(
      authority.get(), authority.Length(), &usernamePos, &usernameLen,
      &passwordPos, &passwordLen, &hostnamePos, &hostnameLen, &port);
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
    // Parse server information when both username and password are empty or do
    // not exist.
    if ((usernameLen <= 0) && (passwordLen <= 0)) {
      if (authority.Length() - atPos - 1 == 0) {
        nsAutoCString error;
        error.AssignLiteral("'");
        error.Append(NS_ConvertUTF16toUTF8(aIdentifier));
        error.AppendLiteral("' is not valid. hostname can not be empty.");
        aRv.ThrowRangeError(error);
        return;
      }
      // Re-using nsIURLParser::ParseServerInfo to extract the hostname and port
      // information. This can help us to handle complicated IPv6 cases.
      nsAutoCString serverInfo(
          Substring(authority, atPos + 1, authority.Length() - atPos - 1));
      rv = urlParser->ParseServerInfo(serverInfo.get(), serverInfo.Length(),
                                      &hostnamePos, &hostnameLen, &port);
      if (NS_FAILED(rv)) {
        // ParseServerInfo returns NS_ERROR_MALFORMED_URI in all fail cases, we
        // probably need a followup bug to figure out the fail reason.
        nsAutoCString error;
        error.AssignLiteral("Error extracting hostname from '");
        error.Append(serverInfo);
        error.AppendLiteral("'.");
        aRv.ThrowRangeError(error);
        return;
      }
    }
  }
  // PMI is valid when usernameLen/passwordLen equals to -1 or 0.
  if (usernameLen > 0 || passwordLen > 0) {
    nsAutoCString error;
    error.AssignLiteral("'");
    error.Append(NS_ConvertUTF16toUTF8(aIdentifier));
    error.AssignLiteral("' is not valid. Username and password must be empty.");
    aRv.ThrowRangeError(error);
    return;
  }

  // PMI is valid when hostnameLen is larger than 0
  if (hostnameLen <= 0) {
    nsAutoCString error;
    error.AssignLiteral("'");
    error.Append(NS_ConvertUTF16toUTF8(aIdentifier));
    error.AppendLiteral("' is not valid. hostname can not be empty.");
    aRv.ThrowRangeError(error);
    return;
  }
}

void PaymentRequest::IsValidMethodData(
    JSContext* aCx, const Sequence<PaymentMethodData>& aMethodData,
    ErrorResult& aRv) {
  if (!aMethodData.Length()) {
    aRv.ThrowTypeError("At least one payment method is required.");
    return;
  }

  nsTArray<nsString> methods;
  for (const PaymentMethodData& methodData : aMethodData) {
    IsValidPaymentMethodIdentifier(methodData.mSupportedMethods, aRv);
    if (aRv.Failed()) {
      return;
    }

    RefPtr<BasicCardService> service = BasicCardService::GetService();
    MOZ_ASSERT(service);
    if (service->IsBasicCardPayment(methodData.mSupportedMethods)) {
      if (!methodData.mData.WasPassed()) {
        continue;
      }
      MOZ_ASSERT(aCx);
      nsAutoString error;
      if (!service->IsValidBasicCardRequest(aCx, methodData.mData.Value(),
                                            error)) {
        aRv.ThrowTypeError(NS_ConvertUTF16toUTF8(error));
        return;
      }
    }
    if (!methods.Contains(methodData.mSupportedMethods)) {
      methods.AppendElement(methodData.mSupportedMethods);
    } else {
      aRv.ThrowRangeError(nsPrintfCString(
          "Duplicate payment method '%s'",
          NS_ConvertUTF16toUTF8(methodData.mSupportedMethods).get()));
      return;
    }
  }
}

void PaymentRequest::IsValidNumber(const nsAString& aItem,
                                   const nsAString& aStr, ErrorResult& aRv) {
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
    if (aValue.Last() != '.' && aValue.CharAt(beginningIndex) >= '0' &&
        aValue.CharAt(beginningIndex) <= '9') {
      aValue.ToFloat(&error);
    }
  }

  if (NS_FAILED(error)) {
    nsAutoCString errorMsg;
    errorMsg.AssignLiteral("The amount.value of \"");
    errorMsg.Append(NS_ConvertUTF16toUTF8(aItem));
    errorMsg.AppendLiteral("\"(");
    errorMsg.Append(NS_ConvertUTF16toUTF8(aStr));
    errorMsg.AppendLiteral(") must be a valid decimal monetary value.");
    aRv.ThrowTypeError(errorMsg);
    return;
  }
}

void PaymentRequest::IsNonNegativeNumber(const nsAString& aItem,
                                         const nsAString& aStr,
                                         ErrorResult& aRv) {
  nsresult error = NS_ERROR_FAILURE;

  if (!aStr.IsEmpty()) {
    nsAutoString aValue(aStr);
    // Ensure
    // - the beginning character is a digit in [0-9], and
    // - the last character is not '.'
    if (aValue.Last() != '.' && aValue.First() >= '0' &&
        aValue.First() <= '9') {
      aValue.ToFloat(&error);
    }
  }

  if (NS_FAILED(error)) {
    nsAutoCString errorMsg;
    errorMsg.AssignLiteral("The amount.value of \"");
    errorMsg.Append(NS_ConvertUTF16toUTF8(aItem));
    errorMsg.AppendLiteral("\"(");
    errorMsg.Append(NS_ConvertUTF16toUTF8(aStr));
    errorMsg.AppendLiteral(
        ") must be a valid and non-negative decimal monetary value.");
    aRv.ThrowTypeError(errorMsg);
    return;
  }
}

void PaymentRequest::IsValidCurrency(const nsAString& aItem,
                                     const nsAString& aCurrency,
                                     ErrorResult& aRv) {
  /*
   *  According to spec in
   * https://w3c.github.io/payment-request/#validity-checkers, perform currency
   * validation with following criteria
   *  1. The currency length must be 3.
   *  2. The currency contains any character that must be in the range "A" to
   * "Z" (U+0041 to U+005A) or the range "a" to "z" (U+0061 to U+007A)
   */
  if (aCurrency.Length() != 3) {
    nsAutoCString error;
    error.AssignLiteral("The length amount.currency of \"");
    error.Append(NS_ConvertUTF16toUTF8(aItem));
    error.AppendLiteral("\"(");
    error.Append(NS_ConvertUTF16toUTF8(aCurrency));
    error.AppendLiteral(") must be 3.");
    aRv.ThrowRangeError(error);
    return;
  }
  // Don't use nsUnicharUtils::ToUpperCase, it converts the invalid "ınr" PMI to
  // to the valid one "INR".
  for (uint32_t idx = 0; idx < aCurrency.Length(); ++idx) {
    if ((aCurrency.CharAt(idx) >= 'A' && aCurrency.CharAt(idx) <= 'Z') ||
        (aCurrency.CharAt(idx) >= 'a' && aCurrency.CharAt(idx) <= 'z')) {
      continue;
    }
    nsAutoCString error;
    error.AssignLiteral("The character amount.currency of \"");
    error.Append(NS_ConvertUTF16toUTF8(aItem));
    error.AppendLiteral("\"(");
    error.Append(NS_ConvertUTF16toUTF8(aCurrency));
    error.AppendLiteral(
        ") must be in the range 'A' to 'Z'(U+0041 to U+005A) or 'a' to "
        "'z'(U+0061 to U+007A).");
    aRv.ThrowRangeError(error);
    return;
  }
}

void PaymentRequest::IsValidCurrencyAmount(const nsAString& aItem,
                                           const PaymentCurrencyAmount& aAmount,
                                           const bool aIsTotalItem,
                                           ErrorResult& aRv) {
  IsValidCurrency(aItem, aAmount.mCurrency, aRv);
  if (aRv.Failed()) {
    return;
  }
  if (aIsTotalItem) {
    IsNonNegativeNumber(aItem, aAmount.mValue, aRv);
    if (aRv.Failed()) {
      return;
    }
  } else {
    IsValidNumber(aItem, aAmount.mValue, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

void PaymentRequest::IsValidDetailsInit(const PaymentDetailsInit& aDetails,
                                        const bool aRequestShipping,
                                        ErrorResult& aRv) {
  // Check the amount.value and amount.currency of detail.total
  IsValidCurrencyAmount(u"details.total"_ns, aDetails.mTotal.mAmount,
                        true,  // isTotalItem
                        aRv);
  if (aRv.Failed()) {
    return;
  }
  return IsValidDetailsBase(aDetails, aRequestShipping, aRv);
}

void PaymentRequest::IsValidDetailsUpdate(const PaymentDetailsUpdate& aDetails,
                                          const bool aRequestShipping,
                                          ErrorResult& aRv) {
  // Check the amount.value and amount.currency of detail.total
  if (aDetails.mTotal.WasPassed()) {
    IsValidCurrencyAmount(u"details.total"_ns, aDetails.mTotal.Value().mAmount,
                          true,  // isTotalItem
                          aRv);
    if (aRv.Failed()) {
      return;
    }
  }
  IsValidDetailsBase(aDetails, aRequestShipping, aRv);
}

void PaymentRequest::IsValidDetailsBase(const PaymentDetailsBase& aDetails,
                                        const bool aRequestShipping,
                                        ErrorResult& aRv) {
  // Check the amount.value of each item in the display items
  if (aDetails.mDisplayItems.WasPassed()) {
    const Sequence<PaymentItem>& displayItems = aDetails.mDisplayItems.Value();
    for (const PaymentItem& displayItem : displayItems) {
      IsValidCurrencyAmount(displayItem.mLabel, displayItem.mAmount,
                            false,  // isTotalItem
                            aRv);
      if (aRv.Failed()) {
        return;
      }
    }
  }

  // Check the shipping option
  if (aDetails.mShippingOptions.WasPassed() && aRequestShipping) {
    const Sequence<PaymentShippingOption>& shippingOptions =
        aDetails.mShippingOptions.Value();
    nsTArray<nsString> seenIDs;
    for (const PaymentShippingOption& shippingOption : shippingOptions) {
      IsValidCurrencyAmount(u"details.shippingOptions"_ns,
                            shippingOption.mAmount,
                            false,  // isTotalItem
                            aRv);
      if (aRv.Failed()) {
        return;
      }
      if (seenIDs.Contains(shippingOption.mId)) {
        nsAutoCString error;
        error.AssignLiteral("Duplicate shippingOption id '");
        error.Append(NS_ConvertUTF16toUTF8(shippingOption.mId));
        error.AppendLiteral("'");
        aRv.ThrowTypeError(error);
        return;
      }
      seenIDs.AppendElement(shippingOption.mId);
    }
  }

  // Check payment details modifiers
  if (aDetails.mModifiers.WasPassed()) {
    const Sequence<PaymentDetailsModifier>& modifiers =
        aDetails.mModifiers.Value();
    for (const PaymentDetailsModifier& modifier : modifiers) {
      IsValidPaymentMethodIdentifier(modifier.mSupportedMethods, aRv);
      if (aRv.Failed()) {
        return;
      }
      if (modifier.mTotal.WasPassed()) {
        IsValidCurrencyAmount(u"details.modifiers.total"_ns,
                              modifier.mTotal.Value().mAmount,
                              true,  // isTotalItem
                              aRv);
        if (aRv.Failed()) {
          return;
        }
      }
      if (modifier.mAdditionalDisplayItems.WasPassed()) {
        const Sequence<PaymentItem>& displayItems =
            modifier.mAdditionalDisplayItems.Value();
        for (const PaymentItem& displayItem : displayItems) {
          IsValidCurrencyAmount(displayItem.mLabel, displayItem.mAmount,
                                false,  // isTotalItem
                                aRv);
          if (aRv.Failed()) {
            return;
          }
        }
      }
    }
  }
}

already_AddRefed<PaymentRequest> PaymentRequest::Constructor(
    const GlobalObject& aGlobal, const Sequence<PaymentMethodData>& aMethodData,
    const PaymentDetailsInit& aDetails, const PaymentOptions& aOptions,
    ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.ThrowAbortError("No global object for creating PaymentRequest");
    return nullptr;
  }

  nsCOMPtr<Document> doc = window->GetExtantDoc();
  if (!doc) {
    aRv.ThrowAbortError("No document for creating PaymentRequest");
    return nullptr;
  }

  // the feature can only be used in an active document
  if (!doc->IsCurrentActiveDocument()) {
    aRv.ThrowSecurityError(
        "Can't create a PaymentRequest for an inactive document");
    return nullptr;
  }

  if (!FeaturePolicyUtils::IsFeatureAllowed(doc, u"payment"_ns)) {
    aRv.ThrowSecurityError(
        "Document's Feature Policy does not allow to create a PaymentRequest");
    return nullptr;
  }

  // Get the top level principal
  RefPtr<Document> topLevelDoc = doc->GetTopLevelContentDocumentIfSameProcess();
  MOZ_ASSERT(topLevelDoc);
  nsCOMPtr<nsIPrincipal> topLevelPrincipal = topLevelDoc->NodePrincipal();

  // Check payment methods and details
  IsValidMethodData(aGlobal.Context(), aMethodData, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  IsValidDetailsInit(aDetails, aOptions.mRequestShipping, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  if (NS_WARN_IF(!manager)) {
    return nullptr;
  }

  // Create PaymentRequest and set its |mId|
  RefPtr<PaymentRequest> request;
  manager->CreatePayment(aGlobal.Context(), window, topLevelPrincipal,
                         aMethodData, aDetails, aOptions,
                         getter_AddRefs(request), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return request.forget();
}

already_AddRefed<PaymentRequest> PaymentRequest::CreatePaymentRequest(
    nsPIDOMWindowInner* aWindow, ErrorResult& aRv) {
  // Generate a unique id for identification
  nsID uuid;
  if (NS_WARN_IF(NS_FAILED(nsContentUtils::GenerateUUIDInPlace(uuid)))) {
    aRv.ThrowAbortError(
        "Failed to create an internal UUID for the PaymentRequest");
    return nullptr;
  }

  NSID_TrimBracketsUTF16 id(uuid);

  // Create payment request with generated id
  RefPtr<PaymentRequest> request = new PaymentRequest(aWindow, id);
  return request.forget();
}

PaymentRequest::PaymentRequest(nsPIDOMWindowInner* aWindow,
                               const nsAString& aInternalId)
    : DOMEventTargetHelper(aWindow),
      mInternalId(aInternalId),
      mShippingAddress(nullptr),
      mUpdating(false),
      mRequestShipping(false),
      mState(eCreated),
      mIPC(nullptr) {
  MOZ_ASSERT(aWindow);
  RegisterActivityObserver();
}

already_AddRefed<Promise> PaymentRequest::CanMakePayment(ErrorResult& aRv) {
  if (!InFullyActiveDocument()) {
    aRv.ThrowAbortError("The owner document is not fully active");
    return nullptr;
  }

  if (mState != eCreated) {
    aRv.ThrowInvalidStateError(
        "The PaymentRequest's state should be 'Created'");
    return nullptr;
  }

  if (mResultPromise) {
    // XXX This doesn't match the spec but does match Chromium.
    aRv.ThrowNotAllowedError(
        "PaymentRequest.CanMakePayment() has already been called");
    return nullptr;
  }

  nsIGlobalObject* global = GetOwnerGlobal();
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  manager->CanMakePayment(this, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  mResultPromise = promise;
  return promise.forget();
}

void PaymentRequest::RespondCanMakePayment(bool aResult) {
  MOZ_ASSERT(mResultPromise);
  mResultPromise->MaybeResolve(aResult);
  mResultPromise = nullptr;
}

already_AddRefed<Promise> PaymentRequest::Show(
    const Optional<OwningNonNull<Promise>>& aDetailsPromise, ErrorResult& aRv) {
  if (!InFullyActiveDocument()) {
    aRv.ThrowAbortError("The owner document is not fully active");
    return nullptr;
  }

  nsIGlobalObject* global = GetOwnerGlobal();
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(global);
  Document* doc = win->GetExtantDoc();

  if (!UserActivation::IsHandlingUserInput()) {
    nsString msg = nsLiteralString(
        u"User activation is now required to call PaymentRequest.show()");
    nsContentUtils::ReportToConsoleNonLocalized(
        msg, nsIScriptError::warningFlag, "Security"_ns, doc);
    if (StaticPrefs::dom_payments_request_user_interaction_required()) {
      aRv.ThrowSecurityError(NS_ConvertUTF16toUTF8(msg));
      return nullptr;
    }
  }

  if (mState != eCreated) {
    aRv.ThrowInvalidStateError(
        "The PaymentRequest's state should be 'Created'");
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    mState = eClosed;
    return nullptr;
  }

  if (aDetailsPromise.WasPassed()) {
    aDetailsPromise.Value().AppendNativeHandler(this);
    mUpdating = true;
  }

  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  manager->ShowPayment(this, aRv);
  if (aRv.Failed()) {
    mState = eClosed;
    return nullptr;
  }

  mAcceptPromise = promise;
  mState = eInteractive;
  return promise.forget();
}

void PaymentRequest::RejectShowPayment(ErrorResult&& aRejectReason) {
  MOZ_ASSERT(mAcceptPromise || mResponse);
  MOZ_ASSERT(mState == eInteractive);

  if (mResponse) {
    mResponse->RejectRetry(std::move(aRejectReason));
  } else {
    mAcceptPromise->MaybeReject(std::move(aRejectReason));
  }
  mState = eClosed;
  mAcceptPromise = nullptr;
}

void PaymentRequest::RespondShowPayment(const nsAString& aMethodName,
                                        const ResponseData& aDetails,
                                        const nsAString& aPayerName,
                                        const nsAString& aPayerEmail,
                                        const nsAString& aPayerPhone,
                                        ErrorResult&& aResult) {
  MOZ_ASSERT(mAcceptPromise || mResponse);
  MOZ_ASSERT(mState == eInteractive);

  if (aResult.Failed()) {
    RejectShowPayment(std::move(aResult));
    return;
  }

  // https://github.com/w3c/payment-request/issues/692
  mShippingAddress.swap(mFullShippingAddress);
  mFullShippingAddress = nullptr;

  if (mResponse) {
    mResponse->RespondRetry(aMethodName, mShippingOption, mShippingAddress,
                            aDetails, aPayerName, aPayerEmail, aPayerPhone);
  } else {
    RefPtr<PaymentResponse> paymentResponse = new PaymentResponse(
        GetOwner(), this, mId, aMethodName, mShippingOption, mShippingAddress,
        aDetails, aPayerName, aPayerEmail, aPayerPhone);
    mResponse = paymentResponse;
    mAcceptPromise->MaybeResolve(paymentResponse);
  }

  mState = eClosed;
  mAcceptPromise = nullptr;
}

void PaymentRequest::RespondComplete() {
  MOZ_ASSERT(mResponse);
  mResponse->RespondComplete();
}

already_AddRefed<Promise> PaymentRequest::Abort(ErrorResult& aRv) {
  if (!InFullyActiveDocument()) {
    aRv.ThrowAbortError("The owner document is not fully active");
    return nullptr;
  }

  if (mState != eInteractive) {
    aRv.ThrowSecurityError(
        "The PaymentRequest's state should be 'Interactive'");
    return nullptr;
  }

  if (mAbortPromise) {
    aRv.ThrowInvalidStateError(
        "PaymentRequest.abort() has already been called");
    return nullptr;
  }

  nsIGlobalObject* global = GetOwnerGlobal();
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  manager->AbortPayment(this, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  mAbortPromise = promise;
  return promise.forget();
}

void PaymentRequest::RespondAbortPayment(bool aSuccess) {
  // Check whether we are aborting the update:
  //
  // - If |mUpdateError| is failed, we are aborting the update as
  //   |mUpdateError| was set in method |AbortUpdate|.
  //   => Reject |mAcceptPromise| and reset |mUpdateError| to complete
  //      the action, regardless of |aSuccess|.
  //
  // - Otherwise, we are handling |Abort| method call from merchant.
  //   => Resolve/Reject |mAbortPromise| based on |aSuccess|.
  if (mUpdateError.Failed()) {
    // Respond show with mUpdateError, set mUpdating to false.
    mUpdating = false;
    RespondShowPayment(u""_ns, ResponseData(), u""_ns, u""_ns, u""_ns,
                       std::move(mUpdateError));
    return;
  }

  MOZ_ASSERT(mAbortPromise);
  MOZ_ASSERT(mState == eInteractive);

  if (aSuccess) {
    mAbortPromise->MaybeResolve(JS::UndefinedHandleValue);
    mAbortPromise = nullptr;
    ErrorResult abortResult;
    abortResult.ThrowAbortError("The PaymentRequest is aborted");
    RejectShowPayment(std::move(abortResult));
  } else {
    mAbortPromise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    mAbortPromise = nullptr;
  }
}

void PaymentRequest::UpdatePayment(JSContext* aCx,
                                   const PaymentDetailsUpdate& aDetails,
                                   ErrorResult& aRv) {
  MOZ_ASSERT(aCx);
  if (mState != eInteractive) {
    aRv.ThrowInvalidStateError(
        "The PaymentRequest state should be 'Interactive'");
    return;
  }
  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  manager->UpdatePayment(aCx, this, aDetails, mRequestShipping, aRv);
}

void PaymentRequest::AbortUpdate(ErrorResult& aReason) {
  // AbortUpdate has the responsiblity to call aReason.SuppressException() when
  // fail to update.

  MOZ_ASSERT(aReason.Failed());

  // Completely ignoring the call when the owner document is not fully active.
  if (!InFullyActiveDocument()) {
    aReason.SuppressException();
    return;
  }

  // Completely ignoring the call when the PaymentRequest state is not
  // eInteractive.
  if (mState != eInteractive) {
    aReason.SuppressException();
    return;
  }
  // Try to close down any remaining user interface. Should recevie
  // RespondAbortPayment from chrome process.
  // Completely ignoring the call when failed to send action to chrome process.
  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  IgnoredErrorResult result;
  manager->AbortPayment(this, result);
  if (result.Failed()) {
    aReason.SuppressException();
    return;
  }

  // Remember update error |aReason| and do the following steps in
  // RespondShowPayment.
  // 1. Set target.state to closed
  // 2. Reject the promise target.acceptPromise with exception "aRv"
  // 3. Abort the algorithm with update error
  mUpdateError = std::move(aReason);
}

void PaymentRequest::RetryPayment(JSContext* aCx,
                                  const PaymentValidationErrors& aErrors,
                                  ErrorResult& aRv) {
  if (mState == eInteractive) {
    aRv.ThrowInvalidStateError(
        "Call Retry() when the PaymentReqeust state is 'Interactive'");
    return;
  }
  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  manager->RetryPayment(aCx, this, aErrors, aRv);
  if (aRv.Failed()) {
    return;
  }
  mState = eInteractive;
}

void PaymentRequest::GetId(nsAString& aRetVal) const { aRetVal = mId; }

void PaymentRequest::GetInternalId(nsAString& aRetVal) {
  aRetVal = mInternalId;
}

void PaymentRequest::SetId(const nsAString& aId) { mId = aId; }

bool PaymentRequest::Equals(const nsAString& aInternalId) const {
  return mInternalId.Equals(aInternalId);
}

bool PaymentRequest::ReadyForUpdate() {
  return mState == eInteractive && !mUpdating;
}

void PaymentRequest::SetUpdating(bool aUpdating) { mUpdating = aUpdating; }

already_AddRefed<PaymentResponse> PaymentRequest::GetResponse() const {
  RefPtr<PaymentResponse> response = mResponse;
  return response.forget();
}

nsresult PaymentRequest::DispatchUpdateEvent(const nsAString& aType) {
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

nsresult PaymentRequest::DispatchMerchantValidationEvent(
    const nsAString& aType) {
  MOZ_ASSERT(ReadyForUpdate());

  MerchantValidationEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mValidationURL.Truncate();

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

nsresult PaymentRequest::DispatchPaymentMethodChangeEvent(
    const nsAString& aMethodName, const ChangeDetails& aMethodDetails) {
  MOZ_ASSERT(ReadyForUpdate());

  PaymentRequestUpdateEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<PaymentMethodChangeEvent> event =
      PaymentMethodChangeEvent::Constructor(this, u"paymentmethodchange"_ns,
                                            init, aMethodName, aMethodDetails);
  event->SetTrusted(true);
  event->SetRequest(this);

  ErrorResult rv;
  DispatchEvent(*event, rv);
  return rv.StealNSResult();
}

already_AddRefed<PaymentAddress> PaymentRequest::GetShippingAddress() const {
  RefPtr<PaymentAddress> address = mShippingAddress;
  return address.forget();
}

nsresult PaymentRequest::UpdateShippingAddress(
    const nsAString& aCountry, const nsTArray<nsString>& aAddressLine,
    const nsAString& aRegion, const nsAString& aRegionCode,
    const nsAString& aCity, const nsAString& aDependentLocality,
    const nsAString& aPostalCode, const nsAString& aSortingCode,
    const nsAString& aOrganization, const nsAString& aRecipient,
    const nsAString& aPhone) {
  nsTArray<nsString> emptyArray;
  mShippingAddress = new PaymentAddress(
      GetOwner(), aCountry, emptyArray, aRegion, aRegionCode, aCity,
      aDependentLocality, aPostalCode, aSortingCode, u""_ns, u""_ns, u""_ns);
  mFullShippingAddress =
      new PaymentAddress(GetOwner(), aCountry, aAddressLine, aRegion,
                         aRegionCode, aCity, aDependentLocality, aPostalCode,
                         aSortingCode, aOrganization, aRecipient, aPhone);
  // Fire shippingaddresschange event
  return DispatchUpdateEvent(u"shippingaddresschange"_ns);
}

void PaymentRequest::SetShippingOption(const nsAString& aShippingOption) {
  mShippingOption = aShippingOption;
}

void PaymentRequest::GetShippingOption(nsAString& aRetVal) const {
  aRetVal = mShippingOption;
}

nsresult PaymentRequest::UpdateShippingOption(
    const nsAString& aShippingOption) {
  mShippingOption = aShippingOption;

  // Fire shippingaddresschange event
  return DispatchUpdateEvent(u"shippingoptionchange"_ns);
}

nsresult PaymentRequest::UpdatePaymentMethod(
    const nsAString& aMethodName, const ChangeDetails& aMethodDetails) {
  return DispatchPaymentMethodChangeEvent(aMethodName, aMethodDetails);
}

void PaymentRequest::SetShippingType(
    const Nullable<PaymentShippingType>& aShippingType) {
  mShippingType = aShippingType;
}

Nullable<PaymentShippingType> PaymentRequest::GetShippingType() const {
  return mShippingType;
}

void PaymentRequest::GetOptions(PaymentOptions& aRetVal) const {
  aRetVal = mOptions;
}

void PaymentRequest::SetOptions(const PaymentOptions& aOptions) {
  mOptions = aOptions;
}

void PaymentRequest::ResolvedCallback(JSContext* aCx,
                                      JS::Handle<JS::Value> aValue) {
  if (!InFullyActiveDocument()) {
    return;
  }

  MOZ_ASSERT(aCx);
  mUpdating = false;
  if (NS_WARN_IF(!aValue.isObject())) {
    return;
  }

  ErrorResult result;
  // Converting value to a PaymentDetailsUpdate dictionary
  RootedDictionary<PaymentDetailsUpdate> details(aCx);
  if (!details.Init(aCx, aValue)) {
    result.StealExceptionFromJSContext(aCx);
    AbortUpdate(result);
    return;
  }

  IsValidDetailsUpdate(details, mRequestShipping, result);
  if (result.Failed()) {
    AbortUpdate(result);
    return;
  }

  // Update the PaymentRequest with the new details
  UpdatePayment(aCx, details, result);
  if (result.Failed()) {
    AbortUpdate(result);
    return;
  }
}

void PaymentRequest::RejectedCallback(JSContext* aCx,
                                      JS::Handle<JS::Value> aValue) {
  if (!InFullyActiveDocument()) {
    return;
  }

  mUpdating = false;
  ErrorResult result;
  result.ThrowAbortError(
      "Details promise for PaymentRequest.show() is rejected by merchant");
  AbortUpdate(result);
}

bool PaymentRequest::InFullyActiveDocument() {
  nsIGlobalObject* global = GetOwnerGlobal();
  if (!global) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(global);

  Document* doc = win->GetExtantDoc();
  if (!doc || !doc->IsCurrentActiveDocument()) {
    return false;
  }

  WindowContext* winContext = win->GetWindowContext();
  if (!winContext) {
    return false;
  }

  while (winContext) {
    if (winContext->IsCached()) {
      return false;
    }
    winContext = winContext->GetParentWindowContext();
  }

  return true;
}

void PaymentRequest::RegisterActivityObserver() {
  if (nsPIDOMWindowInner* window = GetOwner()) {
    mDocument = window->GetExtantDoc();
    if (mDocument) {
      mDocument->RegisterActivityObserver(
          NS_ISUPPORTS_CAST(nsIDocumentActivity*, this));
    }
  }
}

void PaymentRequest::UnregisterActivityObserver() {
  if (mDocument) {
    mDocument->UnregisterActivityObserver(
        NS_ISUPPORTS_CAST(nsIDocumentActivity*, this));
  }
}

void PaymentRequest::NotifyOwnerDocumentActivityChanged() {
  nsPIDOMWindowInner* window = GetOwner();
  NS_ENSURE_TRUE_VOID(window);
  Document* doc = window->GetExtantDoc();
  NS_ENSURE_TRUE_VOID(doc);

  if (!InFullyActiveDocument()) {
    if (mState == eInteractive) {
      if (mAcceptPromise) {
        mAcceptPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
        mAcceptPromise = nullptr;
      }
      if (mResponse) {
        ErrorResult rejectReason;
        rejectReason.ThrowAbortError("The owner documnet is not fully active");
        mResponse->RejectRetry(std::move(rejectReason));
      }
      if (mAbortPromise) {
        mAbortPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
        mAbortPromise = nullptr;
      }
    }
    if (mState == eCreated) {
      if (mResultPromise) {
        mResultPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
        mResultPromise = nullptr;
      }
    }
    RefPtr<PaymentRequestManager> mgr = PaymentRequestManager::GetSingleton();
    mgr->ClosePayment(this);
  }
}

PaymentRequest::~PaymentRequest() {
  // Suppress any pending unreported exception on mUpdateError.  We don't use
  // IgnoredErrorResult for mUpdateError because that doesn't play very nice
  // with move assignment operators.
  mUpdateError.SuppressException();

  if (mIPC) {
    // If we're being destroyed, the PaymentRequestManager isn't holding any
    // references to us and we can't be waiting for any replies.
    mIPC->MaybeDelete(false);
  }
  UnregisterActivityObserver();
}

JSObject* PaymentRequest::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return PaymentRequest_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
