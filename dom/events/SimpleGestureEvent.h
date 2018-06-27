/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SimpleGestureEvent_h_
#define mozilla_dom_SimpleGestureEvent_h_

#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/SimpleGestureEventBinding.h"
#include "mozilla/EventForwards.h"

class nsPresContext;

namespace mozilla {
namespace dom {

class SimpleGestureEvent : public MouseEvent
{
public:
  SimpleGestureEvent(EventTarget* aOwner,
                     nsPresContext* aPresContext,
                     WidgetSimpleGestureEvent* aEvent);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(SimpleGestureEvent, MouseEvent)

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return SimpleGestureEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  uint32_t AllowedDirections() const;
  void SetAllowedDirections(uint32_t aAllowedDirections);
  uint32_t Direction() const;
  double Delta() const;
  uint32_t ClickCount() const;

  void InitSimpleGestureEvent(const nsAString& aType,
                              bool aCanBubble,
                              bool aCancelable,
                              nsGlobalWindowInner* aView,
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
                              uint32_t aAllowedDirections,
                              uint32_t aDirection,
                              double aDelta,
                              uint32_t aClickCount);

protected:
  ~SimpleGestureEvent() {}
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::SimpleGestureEvent>
NS_NewDOMSimpleGestureEvent(mozilla::dom::EventTarget* aOwner,
                            nsPresContext* aPresContext,
                            mozilla::WidgetSimpleGestureEvent* aEvent);

#endif // mozilla_dom_SimpleGestureEvent_h_
