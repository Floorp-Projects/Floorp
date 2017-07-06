/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XULCommandEvent.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

XULCommandEvent::XULCommandEvent(EventTarget* aOwner,
                                 nsPresContext* aPresContext,
                                 WidgetInputEvent* aEvent)
  : UIEvent(aOwner, aPresContext,
            aEvent ? aEvent :
                     new WidgetInputEvent(false, eVoidEvent, nullptr))
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->mTime = PR_Now();
  }
}

NS_IMPL_ADDREF_INHERITED(XULCommandEvent, UIEvent)
NS_IMPL_RELEASE_INHERITED(XULCommandEvent, UIEvent)

NS_IMPL_CYCLE_COLLECTION_INHERITED(XULCommandEvent, UIEvent,
                                   mSourceEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(XULCommandEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXULCommandEvent)
NS_INTERFACE_MAP_END_INHERITING(UIEvent)

bool
XULCommandEvent::AltKey()
{
  return mEvent->AsInputEvent()->IsAlt();
}

NS_IMETHODIMP
XULCommandEvent::GetAltKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = AltKey();
  return NS_OK;
}

bool
XULCommandEvent::CtrlKey()
{
  return mEvent->AsInputEvent()->IsControl();
}

NS_IMETHODIMP
XULCommandEvent::GetCtrlKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = CtrlKey();
  return NS_OK;
}

bool
XULCommandEvent::ShiftKey()
{
  return mEvent->AsInputEvent()->IsShift();
}

NS_IMETHODIMP
XULCommandEvent::GetShiftKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = ShiftKey();
  return NS_OK;
}

bool
XULCommandEvent::MetaKey()
{
  return mEvent->AsInputEvent()->IsMeta();
}

NS_IMETHODIMP
XULCommandEvent::GetMetaKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = MetaKey();
  return NS_OK;
}

NS_IMETHODIMP
XULCommandEvent::GetSourceEvent(nsIDOMEvent** aSourceEvent)
{
  NS_ENSURE_ARG_POINTER(aSourceEvent);
  nsCOMPtr<nsIDOMEvent> event = GetSourceEvent();
  event.forget(aSourceEvent);
  return NS_OK;
}

NS_IMETHODIMP
XULCommandEvent::InitCommandEvent(const nsAString& aType,
                                  bool aCanBubble,
                                  bool aCancelable,
                                  mozIDOMWindow* aView,
                                  int32_t aDetail,
                                  bool aCtrlKey,
                                  bool aAltKey,
                                  bool aShiftKey,
                                  bool aMetaKey,
                                  nsIDOMEvent* aSourceEvent)
{
  NS_ENSURE_TRUE(!mEvent->mFlags.mIsBeingDispatched, NS_OK);

  auto* view = nsGlobalWindow::Cast(nsPIDOMWindowInner::From(aView));
  UIEvent::InitUIEvent(aType, aCanBubble, aCancelable, view, aDetail);

  mEvent->AsInputEvent()->InitBasicModifiers(aCtrlKey, aAltKey,
                                             aShiftKey, aMetaKey);
  mSourceEvent = aSourceEvent;

  return NS_OK;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<XULCommandEvent>
NS_NewDOMXULCommandEvent(EventTarget* aOwner,
                         nsPresContext* aPresContext,
                         WidgetInputEvent* aEvent)
{
  RefPtr<XULCommandEvent> it =
    new XULCommandEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
