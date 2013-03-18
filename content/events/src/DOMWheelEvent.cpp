/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMWheelEvent.h"
#include "nsGUIEvent.h"
#include "nsIContent.h"
#include "nsContentUtils.h"
#include "DictionaryHelpers.h"
#include "nsDOMClassInfoID.h"

DOMCI_DATA(WheelEvent, mozilla::dom::DOMWheelEvent)

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
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WheelEvent)
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

  *aDeltaX = static_cast<widget::WheelEvent*>(mEvent)->deltaX;
  return NS_OK;
}

NS_IMETHODIMP
DOMWheelEvent::GetDeltaY(double* aDeltaY)
{
  NS_ENSURE_ARG_POINTER(aDeltaY);

  *aDeltaY = static_cast<widget::WheelEvent*>(mEvent)->deltaY;
  return NS_OK;
}

NS_IMETHODIMP
DOMWheelEvent::GetDeltaZ(double* aDeltaZ)
{
  NS_ENSURE_ARG_POINTER(aDeltaZ);

  *aDeltaZ = static_cast<widget::WheelEvent*>(mEvent)->deltaZ;
  return NS_OK;
}

NS_IMETHODIMP
DOMWheelEvent::GetDeltaMode(uint32_t* aDeltaMode)
{
  NS_ENSURE_ARG_POINTER(aDeltaMode);

  *aDeltaMode = static_cast<widget::WheelEvent*>(mEvent)->deltaMode;
  return NS_OK;
}

nsresult
DOMWheelEvent::InitFromCtor(const nsAString& aType,
                            JSContext* aCx, JS::Value* aVal)
{
  mozilla::idl::WheelEventInit d;
  nsresult rv = d.Init(aCx, aVal);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString modifierList;
  if (d.ctrlKey) {
    modifierList.AppendLiteral(NS_DOM_KEYNAME_CONTROL);
  }
  if (d.shiftKey) {
    if (!modifierList.IsEmpty()) {
      modifierList.AppendLiteral(" ");
    }
    modifierList.AppendLiteral(NS_DOM_KEYNAME_SHIFT);
  }
  if (d.altKey) {
    if (!modifierList.IsEmpty()) {
      modifierList.AppendLiteral(" ");
    }
    modifierList.AppendLiteral(NS_DOM_KEYNAME_ALT);
  }
  if (d.metaKey) {
    if (!modifierList.IsEmpty()) {
      modifierList.AppendLiteral(" ");
    }
    modifierList.AppendLiteral(NS_DOM_KEYNAME_META);
  }

  rv = InitWheelEvent(aType, d.bubbles, d.cancelable,
                      d.view, d.detail, d.screenX, d.screenY,
                      d.clientX, d.clientY, d.button, d.relatedTarget,
                      modifierList, d.deltaX, d.deltaY, d.deltaZ, d.deltaMode);
  NS_ENSURE_SUCCESS(rv, rv);

  static_cast<widget::WheelEvent*>(mEvent)->buttons = d.buttons;

  return NS_OK;
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
