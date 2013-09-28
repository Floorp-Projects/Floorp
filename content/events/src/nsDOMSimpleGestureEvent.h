/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMSimpleGestureEvent_h__
#define nsDOMSimpleGestureEvent_h__

#include "nsIDOMSimpleGestureEvent.h"
#include "nsDOMMouseEvent.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/dom/SimpleGestureEventBinding.h"

class nsPresContext;

class nsDOMSimpleGestureEvent : public nsDOMMouseEvent,
                                public nsIDOMSimpleGestureEvent
{
public:
  nsDOMSimpleGestureEvent(mozilla::dom::EventTarget* aOwner,
                          nsPresContext*, mozilla::WidgetSimpleGestureEvent*);
  virtual ~nsDOMSimpleGestureEvent();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMSIMPLEGESTUREEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMMOUSEEVENT

  virtual JSObject* WrapObject(JSContext* aCx,
			       JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::SimpleGestureEventBinding::Wrap(aCx, aScope, this);
  }

  uint32_t AllowedDirections()
  {
    return static_cast<mozilla::WidgetSimpleGestureEvent*>(mEvent)->
             allowedDirections;
  }

  uint32_t Direction()
  {
    return static_cast<mozilla::WidgetSimpleGestureEvent*>(mEvent)->direction;
  }

  double Delta()
  {
    return static_cast<mozilla::WidgetSimpleGestureEvent*>(mEvent)->delta;
  }

  uint32_t ClickCount()
  {
    return static_cast<mozilla::WidgetSimpleGestureEvent*>(mEvent)->clickCount;
  }

  void InitSimpleGestureEvent(const nsAString& aType,
                              bool aCanBubble,
                              bool aCancelable,
                              nsIDOMWindow* aView,
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
                              mozilla::dom::EventTarget* aRelatedTarget,
                              uint32_t aAllowedDirections,
                              uint32_t aDirection,
                              double aDelta,
                              uint32_t aClickCount,
                              mozilla::ErrorResult& aRv)
  {
    aRv = InitSimpleGestureEvent(aType, aCanBubble, aCancelable,
                                 aView, aDetail, aScreenX, aScreenY,
                                 aClientX, aClientY, aCtrlKey, aAltKey,
                                 aShiftKey, aMetaKey, aButton,
                                 aRelatedTarget, aAllowedDirections,
                                 aDirection, aDelta, aClickCount);
  }
};

#endif
