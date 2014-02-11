/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMAnimationEvent.h"
#include "prtime.h"
#include "mozilla/ContentEvents.h"

using namespace mozilla;

nsDOMAnimationEvent::nsDOMAnimationEvent(mozilla::dom::EventTarget* aOwner,
                                         nsPresContext *aPresContext,
                                         InternalAnimationEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext,
               aEvent ? aEvent : new InternalAnimationEvent(false, 0))
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

NS_INTERFACE_MAP_BEGIN(nsDOMAnimationEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMAnimationEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMAnimationEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMAnimationEvent, nsDOMEvent)

//static
already_AddRefed<nsDOMAnimationEvent>
nsDOMAnimationEvent::Constructor(const mozilla::dom::GlobalObject& aGlobal,
                                 const nsAString& aType,
                                 const mozilla::dom::AnimationEventInit& aParam,
                                 mozilla::ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<nsDOMAnimationEvent> e = new nsDOMAnimationEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);

  aRv = e->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);

  InternalAnimationEvent* internalEvent = e->mEvent->AsAnimationEvent();
  internalEvent->animationName = aParam.mAnimationName;
  internalEvent->elapsedTime = aParam.mElapsedTime;
  internalEvent->pseudoElement = aParam.mPseudoElement;

  e->SetTrusted(trusted);
  return e.forget();
}

NS_IMETHODIMP
nsDOMAnimationEvent::GetAnimationName(nsAString & aAnimationName)
{
  aAnimationName = mEvent->AsAnimationEvent()->animationName;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAnimationEvent::GetElapsedTime(float *aElapsedTime)
{
  *aElapsedTime = ElapsedTime();
  return NS_OK;
}

float
nsDOMAnimationEvent::ElapsedTime()
{
  return mEvent->AsAnimationEvent()->elapsedTime;
}

NS_IMETHODIMP
nsDOMAnimationEvent::GetPseudoElement(nsAString& aPseudoElement)
{
  aPseudoElement = mEvent->AsAnimationEvent()->pseudoElement;
  return NS_OK;
}

nsresult
NS_NewDOMAnimationEvent(nsIDOMEvent **aInstancePtrResult,
                        mozilla::dom::EventTarget* aOwner,
                        nsPresContext *aPresContext,
                        InternalAnimationEvent *aEvent)
{
  nsDOMAnimationEvent* it =
    new nsDOMAnimationEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
