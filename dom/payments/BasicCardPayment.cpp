/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicCardPayment.h"
#include "PaymentAddress.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ErrorResult.h"
#include "nsArrayUtils.h"

namespace mozilla::dom {
namespace {
bool IsValidNetwork(const nsAString& aNetwork) {
  return aNetwork.Equals(u"amex"_ns) || aNetwork.Equals(u"cartebancaire"_ns) ||
         aNetwork.Equals(u"diners"_ns) || aNetwork.Equals(u"discover"_ns) ||
         aNetwork.Equals(u"jcb"_ns) || aNetwork.Equals(u"mastercard"_ns) ||
         aNetwork.Equals(u"mir"_ns) || aNetwork.Equals(u"unionpay"_ns) ||
         aNetwork.Equals(u"visa"_ns);
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
  return aSupportedMethods.Equals(u"basic-card"_ns);
}

bool BasicCardService::IsValidBasicCardRequest(JSContext* aCx, JSObject* aData,
                                               nsAString& aErrorMsg) {
  if (!aData) {
    return true;
  }
  JS::Rooted<JS::Value> data(aCx, JS::ObjectValue(*aData));

  BasicCardRequest request;
  if (!request.Init(aCx, data)) {
    aErrorMsg.AssignLiteral(
        "Fail to convert methodData.data to BasicCardRequest.");
    return false;
  }

  for (const nsString& network : request.mSupportedNetworks) {
    if (!IsValidNetwork(network)) {
      aErrorMsg.Assign(network + u" is not an valid network."_ns);
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

void BasicCardService::CheckForValidBasicCardErrors(JSContext* aCx,
                                                    JSObject* aData,
                                                    ErrorResult& aRv) {
  MOZ_ASSERT(aData, "Don't pass null data");
  JS::Rooted<JS::Value> data(aCx, JS::ObjectValue(*aData));

  // XXXbz Just because aData converts to BasicCardErrors right now doesn't mean
  // it will if someone tries again!  Should we be replacing aData with a
  // conversion of the BasicCardErrors dictionary to a JS object in a clean
  // compartment or something?
  BasicCardErrors bcError;
  if (!bcError.Init(aCx, data)) {
    aRv.NoteJSContextException(aCx);
  }
}
}  // namespace mozilla::dom
