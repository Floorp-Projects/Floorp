/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MouseScrollEvent.h"
#include "mozilla/MouseEvents.h"
#include "prtime.h"
#include "nsIDOMMouseScrollEvent.h"

namespace mozilla {
namespace dom {

MouseScrollEvent::MouseScrollEvent(EventTarget* aOwner,
                                   nsPresContext* aPresContext,
                                   WidgetMouseScrollEvent* aEvent)
  : MouseEvent(aOwner, aPresContext,
               aEvent ? aEvent :
                        new WidgetMouseScrollEvent(false, eVoidEvent, nullptr))
{
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->mTime = PR_Now();
    mEvent->mRefPoint = LayoutDeviceIntPoint(0, 0);
    static_cast<WidgetMouseEventBase*>(mEvent)->inputSource =
      nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }

  mDetail = mEvent->AsMouseScrollEvent()->mDelta;
}

NS_IMPL_ADDREF_INHERITED(MouseScrollEvent, MouseEvent)
NS_IMPL_RELEASE_INHERITED(MouseScrollEvent, MouseEvent)

NS_INTERFACE_MAP_BEGIN(MouseScrollEvent)
NS_INTERFACE_MAP_END_INHERITING(MouseEvent)

void
MouseScrollEvent::InitMouseScrollEvent(const nsAString& aType,
                                       bool aCanBubble,
                                       bool aCancelable,
                                       nsGlobalWindow* aView,
                                       int32_t aDetail,
                                       int32_t aScreenX,
                                       int32_t aScreenY,
                                       int32_t aClientX,
                                       int32_t aClientY,
                                       bool aCtrlKey,
                                       bool aAltKey,
                                       bool aShiftKey,
                                       bool aMetaKey,
                                       uint16_t aButton,
                                       EventTarget* aRelatedTarget,
                                       int32_t aAxis)
{
  MouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable, aView, aDetail,
                             aScreenX, aScreenY, aClientX, aClientY,
                             aCtrlKey, aAltKey, aShiftKey, aMetaKey, aButton,
                             aRelatedTarget);
  mEvent->AsMouseScrollEvent()->mIsHorizontal =
    (aAxis == nsIDOMMouseScrollEvent::HORIZONTAL_AXIS);
}

int32_t
MouseScrollEvent::Axis()
{
  return mEvent->AsMouseScrollEvent()->mIsHorizontal ?
          static_cast<int32_t>(nsIDOMMouseScrollEvent::HORIZONTAL_AXIS) :
          static_cast<int32_t>(nsIDOMMouseScrollEvent::VERTICAL_AXIS);
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace dom;

already_AddRefed<MouseScrollEvent>
NS_NewDOMMouseScrollEvent(EventTarget* aOwner,
                          nsPresContext* aPresContext,
                          WidgetMouseScrollEvent* aEvent)
{
  RefPtr<MouseScrollEvent> it =
    new MouseScrollEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
