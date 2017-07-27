/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BasicCardPayment_h
#define mozilla_dom_BasicCardPayment_h

#include "mozilla/dom/BasicCardPaymentBinding.h"
#include "nsPIDOMWindow.h"
#include "nsIPaymentAddress.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class BasicCardService final
{
public:
  NS_INLINE_DECL_REFCOUNTING(BasicCardService)

  static already_AddRefed<BasicCardService> GetService();

  bool IsBasicCardPayment(const nsAString& aSupportedMethods);
  bool IsValidBasicCardRequest(JSContext* aCx, JSObject* aData, nsAString& aErrorMsg);
  bool IsValidExpiryMonth(const nsAString& aExpiryMonth);
  bool IsValidExpiryYear(const nsAString& aExpiryYear);

/*
  To let BasicCardResponse using the same data type with non-BasicCard response
  in IPC transferring, following two methods is used to Encode/Decode the raw
  data of BasicCardResponse.
*/
  nsresult EncodeBasicCardData(const nsAString& aCardholderName,
                               const nsAString& aCardNumber,
                               const nsAString& aExpiryMonth,
                               const nsAString& aExpiryYear,
                               const nsAString& aCardSecurityCode,
                               nsIPaymentAddress* aBillingAddress,
                               nsAString& aResult);

  nsresult DecodeBasicCardData(const nsAString& aData,
                               nsPIDOMWindowInner* aWindow,
                               BasicCardResponse& aResponse);
private:
  BasicCardService() = default;
  ~BasicCardService() = default;
};

} // end of namespace dom
} // end of namespace mozilla

#endif
