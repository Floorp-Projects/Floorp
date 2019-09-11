/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentMethodChangeEvent_h
#define mozilla_dom_PaymentMethodChangeEvent_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/PaymentMethodChangeEventBinding.h"
#include "mozilla/dom/PaymentRequestUpdateEvent.h"
#include "mozilla/dom/PaymentRequest.h"

namespace mozilla {
namespace dom {
class PaymentRequestUpdateEvent;
struct PaymentMethodChangeEventInit;
class PaymentMethodChangeEvent final : public PaymentRequestUpdateEvent {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      PaymentMethodChangeEvent, PaymentRequestUpdateEvent)

  explicit PaymentMethodChangeEvent(EventTarget* aOwner);

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<PaymentMethodChangeEvent> Constructor(
      EventTarget* aOwner, const nsAString& aType,
      const PaymentRequestUpdateEventInit& aEventInitDict,
      const nsAString& aMethodName, const ChangeDetails& aMethodDetails);

  // Called by WebIDL constructor
  static already_AddRefed<PaymentMethodChangeEvent> Constructor(
      const GlobalObject& aGlobal, const nsAString& aType,
      const PaymentMethodChangeEventInit& aEventInitDict);

  void GetMethodName(nsAString& aMethodName);
  void SetMethodName(const nsAString& aMethodName);

  void GetMethodDetails(JSContext* cx, JS::MutableHandle<JSObject*> retval);
  void SetMethodDetails(const ChangeDetails& aMethodDetails);

 protected:
  void init(const PaymentMethodChangeEventInit& aEventInitDict);
  ~PaymentMethodChangeEvent();

 private:
  JS::Heap<JSObject*> mMethodDetails;
  ChangeDetails mInternalDetails;
  nsString mMethodName;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PaymentMethodChangeEvent_h
