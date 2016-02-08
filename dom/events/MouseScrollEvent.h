/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MouseScrollEvent_h_
#define mozilla_dom_MouseScrollEvent_h_

#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/MouseScrollEventBinding.h"

namespace mozilla {
namespace dom {

class MouseScrollEvent : public MouseEvent
{
public:
  MouseScrollEvent(EventTarget* aOwner,
                   nsPresContext* aPresContext,
                   WidgetMouseScrollEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to base class
  NS_FORWARD_TO_MOUSEEVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return MouseScrollEventBinding::Wrap(aCx, this, aGivenProto);
  }

  int32_t Axis();

  void InitMouseScrollEvent(const nsAString& aType, bool aCanBubble,
                            bool aCancelable, nsGlobalWindow* aView,
                            int32_t aDetail, int32_t aScreenX, int32_t aScreenY,
                            int32_t aClientX, int32_t aClientY,
                            bool aCtrlKey, bool aAltKey, bool aShiftKey,
                            bool aMetaKey, uint16_t aButton,
                            EventTarget* aRelatedTarget,
                            int32_t aAxis);

protected:
  ~MouseScrollEvent() {}
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::MouseScrollEvent>
NS_NewDOMMouseScrollEvent(mozilla::dom::EventTarget* aOwner,
                          nsPresContext* aPresContext,
                          mozilla::WidgetMouseScrollEvent* aEvent);

#endif // mozilla_dom_MouseScrollEvent_h_
