/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/InvokeEvent.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/InvokeEventBinding.h"
#include "nsContentUtils.h"

namespace mozilla::dom {
NS_IMPL_CYCLE_COLLECTION_INHERITED(InvokeEvent, Event, mInvoker)

NS_IMPL_ADDREF_INHERITED(InvokeEvent, Event)
NS_IMPL_RELEASE_INHERITED(InvokeEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InvokeEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

JSObject* InvokeEvent::WrapObjectInternal(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return InvokeEvent_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<InvokeEvent> InvokeEvent::Constructor(
    mozilla::dom::EventTarget* aOwner, const nsAString& aType,
    const InvokeEventInit& aEventInitDict) {
  RefPtr<InvokeEvent> e = new InvokeEvent(aOwner);
  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  e->mInvoker = aEventInitDict.mInvoker;
  e->mAction = aEventInitDict.mAction;
  e->SetTrusted(trusted);
  e->SetComposed(aEventInitDict.mComposed);
  return e.forget();
}

already_AddRefed<InvokeEvent> InvokeEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const InvokeEventInit& aEventInitDict) {
  nsCOMPtr<mozilla::dom::EventTarget> owner =
      do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(owner, aType, aEventInitDict);
}

Element* InvokeEvent::GetInvoker() {
  EventTarget* currentTarget = GetCurrentTarget();
  if (currentTarget) {
    nsINode* retargeted = nsContentUtils::Retarget(
        static_cast<nsINode*>(mInvoker), currentTarget->GetAsNode());
    return retargeted->AsElement();
  }
  MOZ_ASSERT(!mEvent->mFlags.mIsBeingDispatched);
  return mInvoker;
}

}  // namespace mozilla::dom
