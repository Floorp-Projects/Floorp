/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMTransitionEvent.h"
#include "nsGUIEvent.h"

nsDOMTransitionEvent::nsDOMTransitionEvent(mozilla::dom::EventTarget* aOwner,
                                           nsPresContext *aPresContext,
                                           nsTransitionEvent *aEvent)
  : nsDOMEvent(aOwner, aPresContext,
               aEvent ? aEvent : new nsTransitionEvent(false, 0,
                                                       EmptyString(),
                                                       0.0,
                                                       EmptyString()))
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
  SetIsDOMBinding();
}

nsDOMTransitionEvent::~nsDOMTransitionEvent()
{
  if (mEventIsInternal) {
    delete TransitionEvent();
    mEvent = nullptr;
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
  nsCOMPtr<mozilla::dom::EventTarget> t = do_QueryInterface(aGlobal.Get());
  nsRefPtr<nsDOMTransitionEvent> e = new nsDOMTransitionEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);

  aRv = e->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);

  e->TransitionEvent()->propertyName = aParam.mPropertyName;
  e->TransitionEvent()->elapsedTime = aParam.mElapsedTime;
  e->TransitionEvent()->pseudoElement = aParam.mPseudoElement;

  e->SetTrusted(trusted);
  return e.forget();
}

NS_IMETHODIMP
nsDOMTransitionEvent::GetPropertyName(nsAString & aPropertyName)
{
  aPropertyName = TransitionEvent()->propertyName;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTransitionEvent::GetElapsedTime(float *aElapsedTime)
{
  *aElapsedTime = ElapsedTime();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTransitionEvent::GetPseudoElement(nsAString& aPseudoElement)
{
  aPseudoElement = TransitionEvent()->pseudoElement;
  return NS_OK;
}

nsresult
NS_NewDOMTransitionEvent(nsIDOMEvent **aInstancePtrResult,
                         mozilla::dom::EventTarget* aOwner,
                         nsPresContext *aPresContext,
                         nsTransitionEvent *aEvent)
{
  nsDOMTransitionEvent *it =
    new nsDOMTransitionEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
