/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMMouseEvent_h__
#define nsDOMMouseEvent_h__

#include "nsIDOMMouseEvent.h"
#include "nsDOMUIEvent.h"

class nsEvent;

class nsDOMMouseEvent : public nsDOMUIEvent,
                        public nsIDOMMouseEvent
{
public:
  nsDOMMouseEvent(mozilla::dom::EventTarget* aOwner,
                  nsPresContext* aPresContext, nsInputEvent* aEvent);
  virtual ~nsDOMMouseEvent();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMMouseEvent Interface
  NS_DECL_NSIDOMMOUSEEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMUIEVENT

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, JS::Value* aVal);
protected:
  // Specific implementation for a mouse event.
  virtual nsresult Which(uint32_t* aWhich);

  nsresult InitMouseEvent(const nsAString& aType,
                          bool aCanBubble,
                          bool aCancelable,
                          nsIDOMWindow* aView,
                          int32_t aDetail,
                          int32_t aScreenX,
                          int32_t aScreenY,
                          int32_t aClientX,
                          int32_t aClientY,
                          uint16_t aButton,
                          nsIDOMEventTarget *aRelatedTarget,
                          const nsAString& aModifiersList);
};

#define NS_FORWARD_TO_NSDOMMOUSEEVENT         \
  NS_FORWARD_NSIDOMMOUSEEVENT(nsDOMMouseEvent::) \
  NS_FORWARD_TO_NSDOMUIEVENT

#endif // nsDOMMouseEvent_h__
