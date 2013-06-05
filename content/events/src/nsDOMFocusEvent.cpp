/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMFocusEvent.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS_INHERITED1(nsDOMFocusEvent, nsDOMUIEvent, nsIDOMFocusEvent)

nsDOMFocusEvent::nsDOMFocusEvent(mozilla::dom::EventTarget* aOwner,
                                 nsPresContext* aPresContext, nsFocusEvent* aEvent)
  : nsDOMUIEvent(aOwner, aPresContext, aEvent ?
                 static_cast<nsGUIEvent*>(aEvent) :
                 static_cast<nsGUIEvent*>(new nsFocusEvent(false, NS_FOCUS_CONTENT)))
{
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
  SetIsDOMBinding();
}

nsDOMFocusEvent::~nsDOMFocusEvent()
{
  if (mEventIsInternal && mEvent) {
    delete static_cast<nsFocusEvent*>(mEvent);
    mEvent = nullptr;
  }
}

/* readonly attribute nsIDOMEventTarget relatedTarget; */
NS_IMETHODIMP
nsDOMFocusEvent::GetRelatedTarget(nsIDOMEventTarget** aRelatedTarget)
{
  NS_ENSURE_ARG_POINTER(aRelatedTarget);
  NS_IF_ADDREF(*aRelatedTarget = GetRelatedTarget());
  return NS_OK;
}

mozilla::dom::EventTarget*
nsDOMFocusEvent::GetRelatedTarget()
{
  return static_cast<nsFocusEvent*>(mEvent)->relatedTarget;
}

nsresult
nsDOMFocusEvent::InitFocusEvent(const nsAString& aType,
                                bool aCanBubble,
                                bool aCancelable,
                                nsIDOMWindow* aView,
                                int32_t aDetail,
                                mozilla::dom::EventTarget* aRelatedTarget)
{
  nsresult rv = nsDOMUIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, aDetail);
  NS_ENSURE_SUCCESS(rv, rv);
  static_cast<nsFocusEvent*>(mEvent)->relatedTarget = aRelatedTarget;
  return NS_OK;
}

already_AddRefed<nsDOMFocusEvent>
nsDOMFocusEvent::Constructor(const mozilla::dom::GlobalObject& aGlobal,
                             const nsAString& aType,
                             const mozilla::dom::FocusEventInit& aParam,
                             mozilla::ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> t = do_QueryInterface(aGlobal.Get());
  nsRefPtr<nsDOMFocusEvent> e = new nsDOMFocusEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  aRv = e->InitFocusEvent(aType, aParam.mBubbles, aParam.mCancelable, aParam.mView,
                          aParam.mDetail, aParam.mRelatedTarget);
  e->SetTrusted(trusted);
  return e.forget();
}

nsresult NS_NewDOMFocusEvent(nsIDOMEvent** aInstancePtrResult,
                             mozilla::dom::EventTarget* aOwner,
                             nsPresContext* aPresContext,
                             nsFocusEvent* aEvent)
{
  nsDOMFocusEvent* it = new nsDOMFocusEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
