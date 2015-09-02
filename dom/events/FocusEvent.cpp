/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FocusEvent.h"
#include "mozilla/ContentEvents.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED(FocusEvent, UIEvent, nsIDOMFocusEvent)

FocusEvent::FocusEvent(EventTarget* aOwner,
                       nsPresContext* aPresContext,
                       InternalFocusEvent* aEvent)
  : UIEvent(aOwner, aPresContext,
            aEvent ? aEvent : new InternalFocusEvent(false, eFocus))
{
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

NS_IMETHODIMP
FocusEvent::GetRelatedTarget(nsIDOMEventTarget** aRelatedTarget)
{
  NS_ENSURE_ARG_POINTER(aRelatedTarget);
  NS_IF_ADDREF(*aRelatedTarget = GetRelatedTarget());
  return NS_OK;
}

EventTarget*
FocusEvent::GetRelatedTarget()
{
  return mEvent->AsFocusEvent()->relatedTarget;
}

nsresult
FocusEvent::InitFocusEvent(const nsAString& aType,
                           bool aCanBubble,
                           bool aCancelable,
                           nsIDOMWindow* aView,
                           int32_t aDetail,
                           EventTarget* aRelatedTarget)
{
  nsresult rv =
    UIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, aDetail);
  NS_ENSURE_SUCCESS(rv, rv);
  mEvent->AsFocusEvent()->relatedTarget = aRelatedTarget;
  return NS_OK;
}

already_AddRefed<FocusEvent>
FocusEvent::Constructor(const GlobalObject& aGlobal,
                        const nsAString& aType,
                        const FocusEventInit& aParam,
                        ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<FocusEvent> e = new FocusEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  aRv = e->InitFocusEvent(aType, aParam.mBubbles, aParam.mCancelable, aParam.mView,
                          aParam.mDetail, aParam.mRelatedTarget);
  e->SetTrusted(trusted);
  return e.forget();
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<FocusEvent>
NS_NewDOMFocusEvent(EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    InternalFocusEvent* aEvent)
{
  nsRefPtr<FocusEvent> it = new FocusEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
