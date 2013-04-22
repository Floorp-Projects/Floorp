/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMMouseEvent_h__
#define nsDOMMouseEvent_h__

#include "nsIDOMMouseEvent.h"
#include "nsDOMUIEvent.h"
#include "mozilla/dom/MouseEventBinding.h"

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

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::MouseEventBinding::Wrap(aCx, aScope, this);
  }

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, JS::Value* aVal);

  // Web IDL binding methods
  virtual uint32_t Which() MOZ_OVERRIDE
  {
    uint32_t w = 0;
    Which(&w);
    return w;
  }

  int32_t ScreenX();
  int32_t ScreenY();
  int32_t ClientX();
  int32_t ClientY();
  bool CtrlKey()
  {
    return static_cast<nsInputEvent*>(mEvent)->IsControl();
  }
  bool ShiftKey()
  {
    return static_cast<nsInputEvent*>(mEvent)->IsShift();
  }
  bool AltKey()
  {
    return static_cast<nsInputEvent*>(mEvent)->IsAlt();
  }
  bool MetaKey()
  {
    return static_cast<nsInputEvent*>(mEvent)->IsMeta();
  }
  uint16_t Button();
  uint16_t Buttons();
  already_AddRefed<mozilla::dom::EventTarget> GetRelatedTarget();
  void InitMouseEvent(const nsAString & aType, bool aCanBubble, bool aCancelable,
                      nsIDOMWindow* aView, int32_t aDetail, int32_t aScreenX,
                      int32_t aScreenY, int32_t aClientX, int32_t aClientY,
                      bool aCtrlKey, bool aAltKey, bool aShiftKey,
                      bool aMetaKey, uint16_t aButton,
                      mozilla::dom::EventTarget *aRelatedTarget,
                      mozilla::ErrorResult& aRv)
  {
    aRv = InitMouseEvent(aType, aCanBubble, aCancelable,
                         aView, aDetail, aScreenX, aScreenY,
                         aClientX, aClientY, aCtrlKey, aAltKey,
                         aShiftKey, aMetaKey, aButton,
                         aRelatedTarget);
  }
  bool GetModifierState(const nsAString& aKeyArg)
  {
    return GetModifierStateInternal(aKeyArg);
  }
  static already_AddRefed<nsDOMMouseEvent> Constructor(const mozilla::dom::GlobalObject& aGlobal,
                                                       const nsAString& aType,
                                                       const mozilla::dom::MouseEventInit& aParam,
                                                       mozilla::ErrorResult& aRv);
  int32_t MozMovementX()
  {
    return GetMovementPoint().x;
  }
  int32_t MozMovementY()
  {
    return GetMovementPoint().y;
  }
  float MozPressure() const
  {
    return static_cast<nsMouseEvent_base*>(mEvent)->pressure;
  }
  uint16_t MozInputSource() const
  {
    return static_cast<nsMouseEvent_base*>(mEvent)->inputSource;
  }
  void InitNSMouseEvent(const nsAString & aType, bool aCanBubble, bool aCancelable,
                        nsIDOMWindow *aView, int32_t aDetail, int32_t aScreenX,
                        int32_t aScreenY, int32_t aClientX, int32_t aClientY,
                        bool aCtrlKey, bool aAltKey, bool aShiftKey,
                        bool aMetaKey, uint16_t aButton,
                        mozilla::dom::EventTarget *aRelatedTarget,
                        float aPressure, uint16_t aInputSource,
                        mozilla::ErrorResult& aRv)
  {
    aRv = InitNSMouseEvent(aType, aCanBubble, aCancelable,
                           aView, aDetail, aScreenX, aScreenY,
                           aClientX, aClientY, aCtrlKey, aAltKey,
                           aShiftKey, aMetaKey, aButton,
                           aRelatedTarget, aPressure, aInputSource);
  }

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
