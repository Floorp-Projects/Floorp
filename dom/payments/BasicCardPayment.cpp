/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicCardPayment.h"
#include "PaymentAddress.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsArrayUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsDataHashtable.h"

namespace mozilla {
namespace dom {
#ifndef PaymentBasicCardMacros
#define PaymentBasicCardMacros

#define AMEX NS_LITERAL_STRING("amex")
#define CARTEBANCAIRE NS_LITERAL_STRING("cartebancaire")
#define DINERS NS_LITERAL_STRING("diners")
#define DISCOVER NS_LITERAL_STRING("discover")
#define JCB NS_LITERAL_STRING("jcb")
#define MASTERCARD NS_LITERAL_STRING("mastercard")
#define MIR NS_LITERAL_STRING("mir")
#define UNIONPAY NS_LITERAL_STRING("unionpay")
#define VISA NS_LITERAL_STRING("visa")

#define CardholderName NS_LITERAL_STRING("cardholderName")
#define CardNumber NS_LITERAL_STRING("cardNumber")
#define ExpiryMonth NS_LITERAL_STRING("expiryMonth")
#define ExpiryYear NS_LITERAL_STRING("expiryYear")
#define CardSecurityCode NS_LITERAL_STRING("cardSecurityCode")

#define Country NS_LITERAL_STRING("country")
#define AddressLine NS_LITERAL_STRING("addressLine")
#define Region NS_LITERAL_STRING("region")
#define City NS_LITERAL_STRING("city")
#define DependentLocality NS_LITERAL_STRING("dependentLocality")
#define PostalCode NS_LITERAL_STRING("postalCode")
#define SortingCode NS_LITERAL_STRING("sortingCode")
#define LanguageCode NS_LITERAL_STRING("languageCode")
#define Organization NS_LITERAL_STRING("organization")
#define Recipient NS_LITERAL_STRING("recipient")
#define Phone NS_LITERAL_STRING("phone")

#define PropertySpliter NS_LITERAL_STRING(";")
#define KeyValueSpliter NS_LITERAL_STRING(":")
#define AddressLineSpliter NS_LITERAL_STRING("%")

#define EncodeBasicCardProperty(aPropertyName, aPropertyValue, aResult)        \
  do {                                                                         \
    if (!(aPropertyValue).IsEmpty()) {                                         \
      (aResult) += (aPropertyName)                                             \
                 + KeyValueSpliter                                             \
                 + (aPropertyValue)                                            \
                 + PropertySpliter;                                            \
    }                                                                          \
  } while(0)

#define EncodeAddressProperty(aAddress, aPropertyName, aResult)                \
  do {                                                                         \
    nsAutoString propertyValue;                                                \
    NS_ENSURE_SUCCESS((aAddress)->Get##aPropertyName(propertyValue),           \
                                                     NS_ERROR_FAILURE);        \
    EncodeBasicCardProperty((aPropertyName) ,propertyValue , (aResult));       \
  } while(0)

#define DecodeBasicCardProperty(aPropertyName, aPropertyValue,                 \
                                aMatchPropertyName, aResponse)                 \
  do {                                                                         \
    if ((aPropertyName).Equals((aMatchPropertyName))) {                        \
      (aResponse).m##aMatchPropertyName.Construct();                           \
      (aResponse).m##aMatchPropertyName.Value() = (aPropertyValue);            \
    }                                                                          \
  } while(0)

#define DecodeAddressProperty(aPropertyName, aPropertyValue,                   \
                              aMatchPropertyName, aMatchPropertyValue)         \
  do {                                                                         \
    if ((aPropertyName).Equals((aMatchPropertyName))) {                        \
      (aMatchPropertyValue) = (aPropertyValue);                                \
    }                                                                          \
  } while(0)

#endif

namespace {

bool IsValidNetwork(const nsAString& aNetwork)
{
  return AMEX.Equals(aNetwork) ||
         CARTEBANCAIRE.Equals(aNetwork) ||
         DINERS.Equals(aNetwork) ||
         DISCOVER.Equals(aNetwork) ||
         JCB.Equals(aNetwork) ||
         MASTERCARD.Equals(aNetwork) ||
         MIR.Equals(aNetwork) ||
         UNIONPAY.Equals(aNetwork) ||
         VISA.Equals(aNetwork);
}

bool IsBasicCardKey(const nsAString& aKey)
{
  return CardholderName.Equals(aKey) ||
         CardNumber.Equals(aKey) ||
         ExpiryMonth.Equals(aKey) ||
         ExpiryYear.Equals(aKey) ||
         CardSecurityCode.Equals(aKey);
}

bool IsAddressKey(const nsAString& aKey)
{
  return Country.Equals(aKey) ||
         AddressLine.Equals(aKey) ||
         Region.Equals(aKey) ||
         City.Equals(aKey) ||
         DependentLocality.Equals(aKey) ||
         PostalCode.Equals(aKey) ||
         SortingCode.Equals(aKey) ||
         LanguageCode.Equals(aKey) ||
         Organization.Equals(aKey) ||
         Recipient.Equals(aKey) ||
         Phone.Equals(aKey);
}

} // end of namespace


StaticRefPtr<BasicCardService> gBasicCardService;

already_AddRefed<BasicCardService>
BasicCardService::GetService()
{
  if (!gBasicCardService) {
    gBasicCardService = new BasicCardService();
    ClearOnShutdown(&gBasicCardService);
  }
  RefPtr<BasicCardService> service = gBasicCardService;
  return service.forget();
}

bool
BasicCardService::IsBasicCardPayment(const nsAString& aSupportedMethods)
{
  return aSupportedMethods.Equals(NS_LITERAL_STRING("basic-card"));
}

bool
BasicCardService::IsValidBasicCardRequest(JSContext* aCx,
                                          JSObject* aData,
                                          nsAString& aErrorMsg)
{
  if (!aData) {
    return true;
  }
  JS::RootedValue data(aCx, JS::ObjectValue(*aData));

  BasicCardRequest request;
  if (!request.Init(aCx, data)) {
    aErrorMsg.AssignLiteral("Fail to convert methodData.data to BasicCardRequest.");
    return false;
  }

  if (request.mSupportedNetworks.WasPassed()) {
    for (const nsString& network : request.mSupportedNetworks.Value()) {
      if (!IsValidNetwork(network)) {
        aErrorMsg.Assign(network + NS_LITERAL_STRING(" is not an valid network."));
        return false;
      }
    }
  }
  return true;
}

bool
BasicCardService::IsValidExpiryMonth(const nsAString& aExpiryMonth)
{
  // ExpiryMonth can only be
  //   1. empty string
  //   2. 01 ~ 12
  if (aExpiryMonth.IsEmpty()) {
    return true;
  }
  if (aExpiryMonth.Length() != 2) {
    return false;
  }
  // can only be 00 ~ 09
  if (aExpiryMonth.CharAt(0) == '0') {
    if (aExpiryMonth.CharAt(1) < '0' || aExpiryMonth.CharAt(1) > '9') {
      return false;
    }
    return true;
  }
  // can only be 11 or 12
  if (aExpiryMonth.CharAt(0) == '1') {
    if (aExpiryMonth.CharAt(1) != '1' && aExpiryMonth.CharAt(1) != '2') {
      return false;
    }
    return true;
  }
  return false;
}

bool
BasicCardService::IsValidExpiryYear(const nsAString& aExpiryYear)
{
  // ExpiryYear can only be
  //   1. empty string
  //   2. 0000 ~ 9999
  if (!aExpiryYear.IsEmpty()) {
    if (aExpiryYear.Length() != 4) {
      return false;
    }
    for (uint32_t index = 0; index < 4; ++index) {
      if (aExpiryYear.CharAt(index) < '0' ||
          aExpiryYear.CharAt(index) > '9') {
        return false;
      }
    }
  }
  return true;
}

nsresult
BasicCardService::EncodeBasicCardData(const nsAString& aCardholderName,
                                      const nsAString& aCardNumber,
                                      const nsAString& aExpiryMonth,
                                      const nsAString& aExpiryYear,
                                      const nsAString& aCardSecurityCode,
                                      nsIPaymentAddress* aBillingAddress,
                                      nsAString& aResult)
{
  // aBillingAddress can be nullptr
  if (aCardNumber.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }
  EncodeBasicCardProperty(CardholderName, aCardholderName, aResult);
  EncodeBasicCardProperty(CardNumber, aCardNumber, aResult);
  EncodeBasicCardProperty(ExpiryMonth, aExpiryMonth, aResult);
  EncodeBasicCardProperty(ExpiryYear, aExpiryYear, aResult);
  EncodeBasicCardProperty(CardSecurityCode, aCardSecurityCode, aResult);
  if (!aBillingAddress) {
    return NS_OK;
  }
  EncodeAddressProperty(aBillingAddress, Country, aResult);
  nsCOMPtr<nsIArray> addressLine;
  NS_ENSURE_SUCCESS(aBillingAddress->GetAddressLine(getter_AddRefs(addressLine)),
                                                    NS_ERROR_FAILURE);
  uint32_t length;
  nsAutoString addressLineString;
  NS_ENSURE_SUCCESS(addressLine->GetLength(&length), NS_ERROR_FAILURE);
  for (uint32_t index = 0; index < length; ++index) {
    nsCOMPtr<nsISupportsString> address = do_QueryElementAt(addressLine, index);
    MOZ_ASSERT(address);
    nsAutoString addressString;
    NS_ENSURE_SUCCESS(address->GetData(addressString), NS_ERROR_FAILURE);
    addressLineString += addressString + AddressLineSpliter;
  }
  EncodeBasicCardProperty(AddressLine ,addressLineString , aResult);
  EncodeAddressProperty(aBillingAddress, Region, aResult);
  EncodeAddressProperty(aBillingAddress, City, aResult);
  EncodeAddressProperty(aBillingAddress, DependentLocality, aResult);
  EncodeAddressProperty(aBillingAddress, PostalCode, aResult);
  EncodeAddressProperty(aBillingAddress, SortingCode, aResult);
  EncodeAddressProperty(aBillingAddress, LanguageCode, aResult);
  EncodeAddressProperty(aBillingAddress, Organization, aResult);
  EncodeAddressProperty(aBillingAddress, Recipient, aResult);
  EncodeAddressProperty(aBillingAddress, Phone, aResult);
  return NS_OK;
}

nsresult
BasicCardService::DecodeBasicCardData(const nsAString& aData,
                                      nsPIDOMWindowInner* aWindow,
                                      BasicCardResponse& aResponse)
{
  // aWindow can be nullptr
  bool isBillingAddressPassed = false;
  nsTArray<nsString> addressLine;
  nsAutoString country;
  nsAutoString region;
  nsAutoString city;
  nsAutoString dependentLocality;
  nsAutoString postalCode;
  nsAutoString sortingCode;
  nsAutoString languageCode;
  nsAutoString organization;
  nsAutoString recipient;
  nsAutoString phone;

  nsCharSeparatedTokenizer propertyTokenizer(aData, PropertySpliter.CharAt(0));
  while (propertyTokenizer.hasMoreTokens()) {
    nsDependentSubstring property = propertyTokenizer.nextToken();
    nsCharSeparatedTokenizer keyValueTokenizer(property, KeyValueSpliter.CharAt(0));
    MOZ_ASSERT(keyValueTokenizer.hasMoreTokens());
    nsDependentSubstring key = keyValueTokenizer.nextToken();
    nsDependentSubstring value = keyValueTokenizer.nextToken();
    if (IsAddressKey(key) && !isBillingAddressPassed) {
      isBillingAddressPassed = true;
    }
    if (!IsAddressKey(key) && !IsBasicCardKey(key)) {
      return NS_ERROR_FAILURE;
    }

    if (key.Equals(CardNumber)) {
      aResponse.mCardNumber = (value);
    }

    DecodeBasicCardProperty(key, value, CardholderName, aResponse);
    DecodeBasicCardProperty(key, value, ExpiryMonth, aResponse);
    DecodeBasicCardProperty(key, value, ExpiryYear, aResponse);
    DecodeBasicCardProperty(key, value, CardSecurityCode, aResponse);

    DecodeAddressProperty(key, value, Country, country);
    DecodeAddressProperty(key, value, Region, region);
    DecodeAddressProperty(key, value, City, city);
    DecodeAddressProperty(key, value, DependentLocality, dependentLocality);
    DecodeAddressProperty(key, value, PostalCode, postalCode);
    DecodeAddressProperty(key, value, SortingCode, sortingCode);
    DecodeAddressProperty(key, value, LanguageCode, languageCode);
    DecodeAddressProperty(key, value, Organization, organization);
    DecodeAddressProperty(key, value, Recipient, recipient);
    DecodeAddressProperty(key, value, Phone, phone);

    if ((key).Equals(AddressLine)) {
      nsCharSeparatedTokenizer addressTokenizer(value, AddressLineSpliter.CharAt(0));
      while (addressTokenizer.hasMoreTokens()) {
        addressLine.AppendElement(addressTokenizer.nextToken());
      }
    }
  }
  if (isBillingAddressPassed) {
    aResponse.mBillingAddress.Construct();
    aResponse.mBillingAddress.Value() = new PaymentAddress(aWindow,
                                                           country,
                                                           addressLine,
                                                           region,
                                                           city,
                                                           dependentLocality,
                                                           postalCode,
                                                           sortingCode,
                                                           languageCode,
                                                           organization,
                                                           recipient,
                                                           phone);
  }
  return NS_OK;
}

#ifdef PaymentBasicCardMacros
#undef PaymentBasicCardMacros
#undef EncodeBasicCardProperty
#undef EncodeAddressProperty
#undef DecodeBasicCardProperty
#undef DecodeBasicCardCardNumber
#undef DecodeAddressProperty
#undef DecodeAddressLine
#undef AMEX
#undef CARTEBANCAIRE
#undef DINERS
#undef DISCOVER
#undef JCB
#undef MASTERCARD
#undef MIR
#undef UNIONPAY
#undef VISA
#undef CardholderName
#undef CardNumber
#undef ExpiryMonth
#undef ExpiryYear
#undef CardSecurityCode
#undef Country
#undef AddressLine
#undef Region
#undef City
#undef DependentLocality
#undef PostalCode
#undef SortingCode
#undef LanguageCode
#undef Organization
#undef Recipient
#undef Phone
#undef PropertySpliter
#undef KeyValueSpliter
#undef AddressLineSpliter
#endif

} // end of namespace dom
} // end of namespace mozilla
