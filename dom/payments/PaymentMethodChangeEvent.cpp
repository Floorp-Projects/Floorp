/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PaymentMethodChangeEvent.h"
#include "mozilla/dom/PaymentRequestUpdateEvent.h"
#include "mozilla/HoldDropJSObjects.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(PaymentMethodChangeEvent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PaymentMethodChangeEvent,
                                                PaymentRequestUpdateEvent)
  tmp->mMethodDetails = nullptr;
  mozilla::DropJSObjects(this);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PaymentMethodChangeEvent,
                                                  PaymentRequestUpdateEvent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(PaymentMethodChangeEvent,
                                               PaymentRequestUpdateEvent)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mMethodDetails)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(PaymentMethodChangeEvent, PaymentRequestUpdateEvent)
NS_IMPL_RELEASE_INHERITED(PaymentMethodChangeEvent, PaymentRequestUpdateEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PaymentMethodChangeEvent)
NS_INTERFACE_MAP_END_INHERITING(PaymentRequestUpdateEvent)

already_AddRefed<PaymentMethodChangeEvent>
PaymentMethodChangeEvent::Constructor(
  mozilla::dom::EventTarget* aOwner,
  const nsAString& aType,
  const PaymentMethodChangeEventInit& aEventInitDict)
{
  RefPtr<PaymentMethodChangeEvent> e = new PaymentMethodChangeEvent(aOwner);
  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  e->SetTrusted(trusted);
  e->SetComposed(aEventInitDict.mComposed);
  e->init(aEventInitDict);
  return e.forget();
}

already_AddRefed<PaymentMethodChangeEvent>
PaymentMethodChangeEvent::Constructor(
  const GlobalObject& aGlobal,
  const nsAString& aType,
  const PaymentMethodChangeEventInit& aEventInitDict,
  ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> owner =
    do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(owner, aType, aEventInitDict);
}

PaymentMethodChangeEvent::PaymentMethodChangeEvent(EventTarget* aOwner)
  : PaymentRequestUpdateEvent(aOwner)
{
  MOZ_ASSERT(aOwner);
  mozilla::HoldJSObjects(this);
}

void
PaymentMethodChangeEvent::init(
  const PaymentMethodChangeEventInit& aEventInitDict)
{
  mMethodName.Assign(aEventInitDict.mMethodName);
  if (aEventInitDict.mMethodDetails.WasPassed()) {
    mMethodDetails = aEventInitDict.mMethodDetails.Value();
  }
}

void
PaymentMethodChangeEvent::GetMethodName(nsAString& aMethodName)
{
  aMethodName.Assign(mMethodName);
}

void
PaymentMethodChangeEvent::GetMethodDetails(JSContext* cx,
                                           JS::MutableHandle<JSObject*> retval)
{
  retval.set(mMethodDetails.get());
}

PaymentMethodChangeEvent::~PaymentMethodChangeEvent()
{
  mozilla::DropJSObjects(this);
}

JSObject*
PaymentMethodChangeEvent::WrapObjectInternal(JSContext* aCx,
                                             JS::Handle<JSObject*> aGivenProto)
{
  return PaymentMethodChangeEvent_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
