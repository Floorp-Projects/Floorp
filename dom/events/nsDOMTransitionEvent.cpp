/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMTransitionEvent.h"
#include "prtime.h"
#include "mozilla/ContentEvents.h"

using namespace mozilla;

nsDOMTransitionEvent::nsDOMTransitionEvent(mozilla::dom::EventTarget* aOwner,
                                           nsPresContext *aPresContext,
                                           InternalTransitionEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext,
               aEvent ? aEvent : new InternalTransitionEvent(false, 0))
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

NS_INTERFACE_MAP_BEGIN(nsDOMTransitionEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTransitionEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMTransitionEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMTransitionEvent, nsDOMEvent)

//static
already_AddRefed<nsDOMTransitionEvent>
nsDOMTransitionEvent::Constructor(const mozilla::dom::GlobalObject& aGlobal,
                                  const nsAString& aType,
                                  const mozilla::dom::TransitionEventInit& aParam,
                                  mozilla::ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<nsDOMTransitionEvent> e = new nsDOMTransitionEvent(t, nullptr, nullptr);
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
nsDOMTransitionEvent::GetPropertyName(nsAString & aPropertyName)
{
  aPropertyName = mEvent->AsTransitionEvent()->propertyName;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTransitionEvent::GetElapsedTime(float *aElapsedTime)
{
  *aElapsedTime = ElapsedTime();
  return NS_OK;
}

float
nsDOMTransitionEvent::ElapsedTime()
{
  return mEvent->AsTransitionEvent()->elapsedTime;
}

NS_IMETHODIMP
nsDOMTransitionEvent::GetPseudoElement(nsAString& aPseudoElement)
{
  aPseudoElement = mEvent->AsTransitionEvent()->pseudoElement;
  return NS_OK;
}

nsresult
NS_NewDOMTransitionEvent(nsIDOMEvent **aInstancePtrResult,
                         mozilla::dom::EventTarget* aOwner,
                         nsPresContext *aPresContext,
                         InternalTransitionEvent* aEvent)
{
  nsDOMTransitionEvent *it =
    new nsDOMTransitionEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
