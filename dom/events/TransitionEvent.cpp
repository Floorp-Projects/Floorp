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
          aEvent ? aEvent : new InternalTransitionEvent(false, eVoidEvent))
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

NS_INTERFACE_MAP_BEGIN(TransitionEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTransitionEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(TransitionEvent, Event)
NS_IMPL_RELEASE_INHERITED(TransitionEvent, Event)

// static
already_AddRefed<TransitionEvent>
TransitionEvent::Constructor(const GlobalObject& aGlobal,
                             const nsAString& aType,
                             const TransitionEventInit& aParam,
                             ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<TransitionEvent> e = new TransitionEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);

  aRv = e->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);

  InternalTransitionEvent* internalEvent = e->mEvent->AsTransitionEvent();
  internalEvent->propertyName = aParam.mPropertyName;
  internalEvent->elapsedTime = aParam.mElapsedTime;
  internalEvent->pseudoElement = aParam.mPseudoElement;

  e->SetTrusted(trusted);
  return e.forget();
}

NS_IMETHODIMP
TransitionEvent::GetPropertyName(nsAString& aPropertyName)
{
  aPropertyName = mEvent->AsTransitionEvent()->propertyName;
  return NS_OK;
}

NS_IMETHODIMP
TransitionEvent::GetElapsedTime(float* aElapsedTime)
{
  *aElapsedTime = ElapsedTime();
  return NS_OK;
}

float
TransitionEvent::ElapsedTime()
{
  return mEvent->AsTransitionEvent()->elapsedTime;
}

NS_IMETHODIMP
TransitionEvent::GetPseudoElement(nsAString& aPseudoElement)
{
  aPseudoElement = mEvent->AsTransitionEvent()->pseudoElement;
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<TransitionEvent>
NS_NewDOMTransitionEvent(EventTarget* aOwner,
                         nsPresContext* aPresContext,
                         InternalTransitionEvent* aEvent)
{
  nsRefPtr<TransitionEvent> it =
    new TransitionEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
