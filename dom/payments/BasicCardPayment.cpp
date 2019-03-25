/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
namespace {
bool IsValidNetwork(const nsAString& aNetwork) {
  return aNetwork.Equals(NS_LITERAL_STRING("amex")) ||
         aNetwork.Equals(NS_LITERAL_STRING("cartebancaire")) ||
         aNetwork.Equals(NS_LITERAL_STRING("diners")) ||
         aNetwork.Equals(NS_LITERAL_STRING("discover")) ||
         aNetwork.Equals(NS_LITERAL_STRING("jcb")) ||
         aNetwork.Equals(NS_LITERAL_STRING("mastercard")) ||
         aNetwork.Equals(NS_LITERAL_STRING("mir")) ||
         aNetwork.Equals(NS_LITERAL_STRING("unionpay")) ||
         aNetwork.Equals(NS_LITERAL_STRING("visa"));
}
}  // end of namespace

StaticRefPtr<BasicCardService> gBasicCardService;

already_AddRefed<BasicCardService> BasicCardService::GetService() {
  if (!gBasicCardService) {
    gBasicCardService = new BasicCardService();
    ClearOnShutdown(&gBasicCardService);
  }
  RefPtr<BasicCardService> service = gBasicCardService;
  return service.forget();
}

bool BasicCardService::IsBasicCardPayment(const nsAString& aSupportedMethods) {
  return aSupportedMethods.Equals(NS_LITERAL_STRING("basic-card"));
}

bool BasicCardService::IsValidBasicCardRequest(JSContext* aCx, JSObject* aData,
                                               nsAString& aErrorMsg) {
  if (!aData) {
    return true;
  }
  JS::RootedValue data(aCx, JS::ObjectValue(*aData));

  BasicCardRequest request;
  if (!request.Init(aCx, data)) {
    aErrorMsg.AssignLiteral(
        "Fail to convert methodData.data to BasicCardRequest.");
    return false;
  }

  for (const nsString& network : request.mSupportedNetworks) {
    if (!IsValidNetwork(network)) {
      aErrorMsg.Assign(network +
                       NS_LITERAL_STRING(" is not an valid network."));
      return false;
    }
  }
  return true;
}

bool BasicCardService::IsValidExpiryMonth(const nsAString& aExpiryMonth) {
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

bool BasicCardService::IsValidExpiryYear(const nsAString& aExpiryYear) {
  // ExpiryYear can only be
  //   1. empty string
  //   2. 0000 ~ 9999
  if (!aExpiryYear.IsEmpty()) {
    if (aExpiryYear.Length() != 4) {
      return false;
    }
    for (uint32_t index = 0; index < 4; ++index) {
      if (aExpiryYear.CharAt(index) < '0' || aExpiryYear.CharAt(index) > '9') {
        return false;
      }
    }
  }
  return true;
}

bool BasicCardService::IsValidBasicCardErrors(JSContext* aCx, JSObject* aData) {
  if (!aData) {
    return true;
  }
  JS::RootedValue data(aCx, JS::ObjectValue(*aData));

  BasicCardErrors bcError;
  return !bcError.Init(aCx, data);
}
}  // end of namespace dom
}  // end of namespace mozilla
