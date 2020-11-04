/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TransitionEvent.h"
#include "mozilla/ContentEvents.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

TransitionEvent::TransitionEvent(EventTarget* aOwner,
                                 nsPresContext* aPresContext,
                                 InternalTransitionEvent* aEvent)
    : Event(aOwner, aPresContext,
            aEvent ? aEvent : new InternalTransitionEvent(false, eVoidEvent)) {
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->mTime = PR_Now();
  }
}

// static
already_AddRefed<TransitionEvent> TransitionEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const TransitionEventInit& aParam) {
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<TransitionEvent> e = new TransitionEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);

  e->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);

  InternalTransitionEvent* internalEvent = e->mEvent->AsTransitionEvent();
  internalEvent->mPropertyName = aParam.mPropertyName;
  internalEvent->mElapsedTime = aParam.mElapsedTime;
  internalEvent->mPseudoElement = aParam.mPseudoElement;

  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

void TransitionEvent::GetPropertyName(nsAString& aPropertyName) const {
  aPropertyName = mEvent->AsTransitionEvent()->mPropertyName;
}

float TransitionEvent::ElapsedTime() {
  return mEvent->AsTransitionEvent()->mElapsedTime;
}

void TransitionEvent::GetPseudoElement(nsAString& aPseudoElement) const {
  aPseudoElement = mEvent->AsTransitionEvent()->mPseudoElement;
}

}  // namespace dom
}  // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<TransitionEvent> NS_NewDOMTransitionEvent(
    EventTarget* aOwner, nsPresContext* aPresContext,
    InternalTransitionEvent* aEvent) {
  RefPtr<TransitionEvent> it =
      new TransitionEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
