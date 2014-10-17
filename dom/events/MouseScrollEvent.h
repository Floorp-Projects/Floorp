/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MouseScrollEvent_h_
#define mozilla_dom_MouseScrollEvent_h_

#include "nsIDOMMouseScrollEvent.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/MouseScrollEventBinding.h"

namespace mozilla {
namespace dom {

class MouseScrollEvent : public MouseEvent,
                         public nsIDOMMouseScrollEvent
{
public:
  MouseScrollEvent(EventTarget* aOwner,
                   nsPresContext* aPresContext,
                   WidgetMouseScrollEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMMouseScrollEvent Interface
  NS_DECL_NSIDOMMOUSESCROLLEVENT

  // Forward to base class
  NS_FORWARD_TO_MOUSEEVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx) MOZ_OVERRIDE
  {
    return MouseScrollEventBinding::Wrap(aCx, this);
  }

  int32_t Axis();

  void InitMouseScrollEvent(const nsAString& aType, bool aCanBubble,
                            bool aCancelable, nsIDOMWindow* aView,
                            int32_t aDetail, int32_t aScreenX, int32_t aScreenY,
                            int32_t aClientX, int32_t aClientY,
                            bool aCtrlKey, bool aAltKey, bool aShiftKey,
                            bool aMetaKey, uint16_t aButton,
                            nsIDOMEventTarget* aRelatedTarget, int32_t aAxis,
                            ErrorResult& aRv)
  {
    aRv = InitMouseScrollEvent(aType, aCanBubble, aCancelable, aView,
                               aDetail, aScreenX, aScreenY, aClientX, aClientY,
                               aCtrlKey, aAltKey, aShiftKey, aMetaKey, aButton,
                               aRelatedTarget, aAxis);
  }

protected:
  ~MouseScrollEvent() {}
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MouseScrollEvent_h_
