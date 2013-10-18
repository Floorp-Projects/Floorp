/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMMouseScrollEvent_h__
#define nsDOMMouseScrollEvent_h__

#include "nsIDOMMouseScrollEvent.h"
#include "nsDOMMouseEvent.h"
#include "mozilla/dom/MouseScrollEventBinding.h"

class nsDOMMouseScrollEvent : public nsDOMMouseEvent,
                              public nsIDOMMouseScrollEvent
{
public:
  nsDOMMouseScrollEvent(mozilla::dom::EventTarget* aOwner,
                        nsPresContext* aPresContext,
                        mozilla::WidgetMouseScrollEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMMouseScrollEvent Interface
  NS_DECL_NSIDOMMOUSESCROLLEVENT
  
  // Forward to base class
  NS_FORWARD_TO_NSDOMMOUSEEVENT

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::MouseScrollEventBinding::Wrap(aCx, aScope, this);
  }

  int32_t Axis();

  void InitMouseScrollEvent(const nsAString& aType, bool aCanBubble,
                            bool aCancelable, nsIDOMWindow* aView,
                            int32_t aDetail, int32_t aScreenX, int32_t aScreenY,
                            int32_t aClientX, int32_t aClientY,
                            bool aCtrlKey, bool aAltKey, bool aShiftKey,
                            bool aMetaKey, uint16_t aButton,
                            nsIDOMEventTarget* aRelatedTarget, int32_t aAxis,
                            mozilla::ErrorResult& aRv)
  {
    aRv = InitMouseScrollEvent(aType, aCanBubble, aCancelable, aView,
                               aDetail, aScreenX, aScreenY, aClientX, aClientY,
                               aCtrlKey, aAltKey, aShiftKey, aMetaKey, aButton,
                               aRelatedTarget, aAxis);
  }
};

#endif // nsDOMMouseScrollEvent_h__
