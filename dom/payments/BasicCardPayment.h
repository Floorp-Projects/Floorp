/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BasicCardPayment_h
#define mozilla_dom_BasicCardPayment_h

#include "mozilla/dom/BasicCardPaymentBinding.h"
#include "nsPIDOMWindow.h"
#include "nsTArray.h"

namespace mozilla::dom {

class BasicCardService final {
 public:
  NS_INLINE_DECL_REFCOUNTING(BasicCardService)

  static already_AddRefed<BasicCardService> GetService();

  bool IsBasicCardPayment(const nsAString& aSupportedMethods);
  bool IsValidBasicCardRequest(JSContext* aCx, JSObject* aData,
                               nsAString& aErrorMsg);
  void CheckForValidBasicCardErrors(JSContext* aCx, JSObject* aData,
                                    ErrorResult& aRv);
  bool IsValidExpiryMonth(const nsAString& aExpiryMonth);
  bool IsValidExpiryYear(const nsAString& aExpiryYear);

 private:
  BasicCardService() = default;
  ~BasicCardService() = default;
};

}  // namespace mozilla::dom

#endif
