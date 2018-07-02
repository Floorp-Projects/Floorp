/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentRequestUpdateEvent_h
#define mozilla_dom_PaymentRequestUpdateEvent_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/PaymentRequestUpdateEventBinding.h"
#include "mozilla/dom/PromiseNativeHandler.h"

namespace mozilla {
namespace dom {

class Promise;
class PaymentRequest;
class PaymentRequestUpdateEvent : public Event
                                , public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(PaymentRequestUpdateEvent, Event)

  explicit PaymentRequestUpdateEvent(EventTarget* aOwner);

  virtual JSObject*
  WrapObjectInternal(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;
  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  static already_AddRefed<PaymentRequestUpdateEvent>
  Constructor(EventTarget* aOwner,
              const nsAString& aType,
              const PaymentRequestUpdateEventInit& aEventInitDict);

  // Called by WebIDL constructor
  static already_AddRefed<PaymentRequestUpdateEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const PaymentRequestUpdateEventInit& aEventInitDict,
              ErrorResult& aRv);

  void UpdateWith(Promise& aPromise, ErrorResult& aRv);

  void SetRequest(PaymentRequest* aRequest);

protected:
  ~PaymentRequestUpdateEvent();

private:
  // Indicating whether an updateWith()-initiated update is currently in progress.
  bool mWaitForUpdate;
  RefPtr<PaymentRequest> mRequest;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PaymentRequestUpdateEvent_h
