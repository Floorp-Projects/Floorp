/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMTransitionEvent.h"
#include "nsGUIEvent.h"
#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsIXPCScriptable.h"

nsDOMTransitionEvent::nsDOMTransitionEvent(mozilla::dom::EventTarget* aOwner,
                                           nsPresContext *aPresContext,
                                           nsTransitionEvent *aEvent)
  : nsDOMEvent(aOwner, aPresContext,
               aEvent ? aEvent : new nsTransitionEvent(false, 0,
                                                       EmptyString(),
                                                       0.0))
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

DOMCI_DATA(TransitionEvent, nsDOMTransitionEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMTransitionEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTransitionEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TransitionEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMTransitionEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMTransitionEvent, nsDOMEvent)

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
nsDOMTransitionEvent::InitTransitionEvent(const nsAString & typeArg,
                                          bool canBubbleArg,
                                          bool cancelableArg,
                                          const nsAString & propertyNameArg,
                                          float elapsedTimeArg)
{
  nsresult rv = nsDOMEvent::InitEvent(typeArg, canBubbleArg, cancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  TransitionEvent()->propertyName = propertyNameArg;
  TransitionEvent()->elapsedTime = elapsedTimeArg;

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
