/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMWheelEvent.h"
#include "nsGUIEvent.h"
#include "nsIContent.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

DOMWheelEvent::DOMWheelEvent(EventTarget* aOwner,
                             nsPresContext* aPresContext,
                             widget::WheelEvent* aWheelEvent)
  : nsDOMMouseEvent(aOwner, aPresContext,
                    aWheelEvent ? aWheelEvent :
                                  new widget::WheelEvent(false, 0, nullptr))
{
  if (aWheelEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
    static_cast<widget::WheelEvent*>(mEvent)->inputSource =
      nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }
}

DOMWheelEvent::~DOMWheelEvent()
{
  if (mEventIsInternal && mEvent) {
    MOZ_ASSERT(mEvent->eventStructType == NS_WHEEL_EVENT,
               "The mEvent must be WheelEvent");
    delete static_cast<widget::WheelEvent*>(mEvent);
    mEvent = nullptr;
  }
}

NS_IMPL_ADDREF_INHERITED(DOMWheelEvent, nsDOMMouseEvent)
NS_IMPL_RELEASE_INHERITED(DOMWheelEvent, nsDOMMouseEvent)

NS_INTERFACE_MAP_BEGIN(DOMWheelEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWheelEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMMouseEvent)

NS_IMETHODIMP
DOMWheelEvent::InitWheelEvent(const nsAString & aType,
                              bool aCanBubble,
                              bool aCancelable,
                              nsIDOMWindow *aView,
                              int32_t aDetail,
                              int32_t aScreenX,
                              int32_t aScreenY,
                              int32_t aClientX,
                              int32_t aClientY, 
                              uint16_t aButton,
                              nsIDOMEventTarget *aRelatedTarget,
                              const nsAString& aModifiersList,
                              double aDeltaX,
                              double aDeltaY,
                              double aDeltaZ,
                              uint32_t aDeltaMode)
{
  nsresult rv =
    nsDOMMouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable, aView,
                                    aDetail, aScreenX, aScreenY,
                                    aClientX, aClientY, aButton,
                                    aRelatedTarget, aModifiersList);
  NS_ENSURE_SUCCESS(rv, rv);

  widget::WheelEvent* wheelEvent = static_cast<widget::WheelEvent*>(mEvent);
  wheelEvent->deltaX = aDeltaX;
  wheelEvent->deltaY = aDeltaY;
  wheelEvent->deltaZ = aDeltaZ;
  wheelEvent->deltaMode = aDeltaMode;

  return NS_OK;
}

NS_IMETHODIMP
DOMWheelEvent::GetDeltaX(double* aDeltaX)
{
  NS_ENSURE_ARG_POINTER(aDeltaX);

  *aDeltaX = DeltaX();
  return NS_OK;
}

NS_IMETHODIMP
DOMWheelEvent::GetDeltaY(double* aDeltaY)
{
  NS_ENSURE_ARG_POINTER(aDeltaY);

  *aDeltaY = DeltaY();
  return NS_OK;
}

NS_IMETHODIMP
DOMWheelEvent::GetDeltaZ(double* aDeltaZ)
{
  NS_ENSURE_ARG_POINTER(aDeltaZ);

  *aDeltaZ = DeltaZ();
  return NS_OK;
}

NS_IMETHODIMP
DOMWheelEvent::GetDeltaMode(uint32_t* aDeltaMode)
{
  NS_ENSURE_ARG_POINTER(aDeltaMode);

  *aDeltaMode = DeltaMode();
  return NS_OK;
}

static void
GetModifierList(bool aCtrl, bool aShift, bool aAlt, bool aMeta,
                nsAString& aModifierList)
{
  if (aCtrl) {
    aModifierList.AppendLiteral(NS_DOM_KEYNAME_CONTROL);
  }
  if (aShift) {
    if (!aModifierList.IsEmpty()) {
      aModifierList.AppendLiteral(" ");
    }
    aModifierList.AppendLiteral(NS_DOM_KEYNAME_SHIFT);
  }
  if (aAlt) {
    if (!aModifierList.IsEmpty()) {
      aModifierList.AppendLiteral(" ");
    }
    aModifierList.AppendLiteral(NS_DOM_KEYNAME_ALT);
  }
  if (aMeta) {
    if (!aModifierList.IsEmpty()) {
      aModifierList.AppendLiteral(" ");
    }
    aModifierList.AppendLiteral(NS_DOM_KEYNAME_META);
  }
}

already_AddRefed<DOMWheelEvent>
DOMWheelEvent::Constructor(const GlobalObject& aGlobal,
                           const nsAString& aType,
                           const WheelEventInit& aParam,
                           mozilla::ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.Get());
  nsRefPtr<DOMWheelEvent> e = new DOMWheelEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  nsAutoString modifierList;
  GetModifierList(aParam.mCtrlKey, aParam.mShiftKey,
                  aParam.mAltKey, aParam.mMetaKey,
                  modifierList);
  aRv = e->InitWheelEvent(aType, aParam.mBubbles, aParam.mCancelable,
                          aParam.mView, aParam.mDetail,
                          aParam.mScreenX, aParam.mScreenY,
                          aParam.mClientX, aParam.mClientY,
                          aParam.mButton, aParam.mRelatedTarget,
                          modifierList, aParam.mDeltaX,
                          aParam.mDeltaY, aParam.mDeltaZ, aParam.mDeltaMode);
  static_cast<widget::WheelEvent*>(e->mEvent)->buttons = aParam.mButtons;
  e->SetTrusted(trusted);
  return e.forget();
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;

nsresult NS_NewDOMWheelEvent(nsIDOMEvent** aInstancePtrResult,
                             mozilla::dom::EventTarget* aOwner,
                             nsPresContext* aPresContext,
                             widget::WheelEvent *aEvent)
{
  dom::DOMWheelEvent* it = new dom::DOMWheelEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
