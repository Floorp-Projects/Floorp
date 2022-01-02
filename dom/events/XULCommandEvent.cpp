/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XULCommandEvent.h"
#include "prtime.h"

namespace mozilla::dom {

XULCommandEvent::XULCommandEvent(EventTarget* aOwner,
                                 nsPresContext* aPresContext,
                                 WidgetInputEvent* aEvent)
    : UIEvent(
          aOwner, aPresContext,
          aEvent ? aEvent : new WidgetInputEvent(false, eVoidEvent, nullptr)),
      mInputSource(0) {
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->mTime = PR_Now();
  }
}

NS_IMPL_ADDREF_INHERITED(XULCommandEvent, UIEvent)
NS_IMPL_RELEASE_INHERITED(XULCommandEvent, UIEvent)

NS_IMPL_CYCLE_COLLECTION_INHERITED(XULCommandEvent, UIEvent, mSourceEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XULCommandEvent)
NS_INTERFACE_MAP_END_INHERITING(UIEvent)

bool XULCommandEvent::AltKey() { return mEvent->AsInputEvent()->IsAlt(); }

bool XULCommandEvent::CtrlKey() { return mEvent->AsInputEvent()->IsControl(); }

bool XULCommandEvent::ShiftKey() { return mEvent->AsInputEvent()->IsShift(); }

bool XULCommandEvent::MetaKey() { return mEvent->AsInputEvent()->IsMeta(); }

uint16_t XULCommandEvent::InputSource() { return mInputSource; }

void XULCommandEvent::InitCommandEvent(
    const nsAString& aType, bool aCanBubble, bool aCancelable,
    nsGlobalWindowInner* aView, int32_t aDetail, bool aCtrlKey, bool aAltKey,
    bool aShiftKey, bool aMetaKey, int16_t aButton, Event* aSourceEvent,
    uint16_t aInputSource, ErrorResult& aRv) {
  if (NS_WARN_IF(mEvent->mFlags.mIsBeingDispatched)) {
    return;
  }

  UIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, aDetail);

  mEvent->AsInputEvent()->InitBasicModifiers(aCtrlKey, aAltKey, aShiftKey,
                                             aMetaKey);
  mSourceEvent = aSourceEvent;
  mInputSource = aInputSource;
  mButton = aButton;
}

}  // namespace mozilla::dom

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<XULCommandEvent> NS_NewDOMXULCommandEvent(
    EventTarget* aOwner, nsPresContext* aPresContext,
    WidgetInputEvent* aEvent) {
  RefPtr<XULCommandEvent> it =
      new XULCommandEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
