/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MouseEvent_h_
#define mozilla_dom_MouseEvent_h_

#include "mozilla/dom/UIEvent.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/EventForwards.h"
#include "nsIDOMMouseEvent.h"

namespace mozilla {
namespace dom {

class MouseEvent : public UIEvent,
                   public nsIDOMMouseEvent
{
public:
  MouseEvent(EventTarget* aOwner,
             nsPresContext* aPresContext,
             WidgetMouseEventBase* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMMouseEvent Interface
  NS_DECL_NSIDOMMOUSEEVENT

  // Forward to base class
  NS_FORWARD_TO_UIEVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return MouseEventBinding::Wrap(aCx, this, aGivenProto);
  }

  // Web IDL binding methods
  virtual uint32_t Which() override
  {
    return Button() + 1;
  }

  int32_t ScreenX();
  int32_t ScreenY();
  int32_t ClientX();
  int32_t ClientY();
  int32_t OffsetX();
  int32_t OffsetY();
  bool CtrlKey();
  bool ShiftKey();
  bool AltKey();
  bool MetaKey();
  int16_t Button();
  uint16_t Buttons();
  already_AddRefed<EventTarget> GetRelatedTarget();
  void GetRegion(nsAString& aRegion);
  void InitMouseEvent(const nsAString& aType, bool aCanBubble, bool aCancelable,
                      nsIDOMWindow* aView, int32_t aDetail, int32_t aScreenX,
                      int32_t aScreenY, int32_t aClientX, int32_t aClientY,
                      bool aCtrlKey, bool aAltKey, bool aShiftKey,
                      bool aMetaKey, uint16_t aButton,
                      EventTarget* aRelatedTarget, ErrorResult& aRv)
  {
    aRv = InitMouseEvent(aType, aCanBubble, aCancelable,
                         aView, aDetail, aScreenX, aScreenY,
                         aClientX, aClientY, aCtrlKey, aAltKey,
                         aShiftKey, aMetaKey, aButton,
                         aRelatedTarget);
  }

  void InitializeExtraMouseEventDictionaryMembers(const MouseEventInit& aParam);

  bool GetModifierState(const nsAString& aKeyArg)
  {
    return GetModifierStateInternal(aKeyArg);
  }
  static already_AddRefed<MouseEvent> Constructor(const GlobalObject& aGlobal,
                                                  const nsAString& aType,
                                                  const MouseEventInit& aParam,
                                                  ErrorResult& aRv);
  int32_t MovementX()
  {
    return GetMovementPoint().x;
  }
  int32_t MovementY()
  {
    return GetMovementPoint().y;
  }
  float MozPressure() const;
  bool HitCluster() const;
  uint16_t MozInputSource() const;
  void InitNSMouseEvent(const nsAString& aType,
                        bool aCanBubble, bool aCancelable,
                        nsIDOMWindow* aView, int32_t aDetail, int32_t aScreenX,
                        int32_t aScreenY, int32_t aClientX, int32_t aClientY,
                        bool aCtrlKey, bool aAltKey, bool aShiftKey,
                        bool aMetaKey, uint16_t aButton,
                        EventTarget* aRelatedTarget,
                        float aPressure, uint16_t aInputSource,
                        ErrorResult& aRv)
  {
    aRv = InitNSMouseEvent(aType, aCanBubble, aCancelable,
                           aView, aDetail, aScreenX, aScreenY,
                           aClientX, aClientY, aCtrlKey, aAltKey,
                           aShiftKey, aMetaKey, aButton,
                           aRelatedTarget, aPressure, aInputSource);
  }

protected:
  ~MouseEvent() {}

  nsresult InitMouseEvent(const nsAString& aType,
                          bool aCanBubble,
                          bool aCancelable,
                          nsIDOMWindow* aView,
                          int32_t aDetail,
                          int32_t aScreenX,
                          int32_t aScreenY,
                          int32_t aClientX,
                          int32_t aClientY,
                          int16_t aButton,
                          nsIDOMEventTarget* aRelatedTarget,
                          const nsAString& aModifiersList);
};

} // namespace dom
} // namespace mozilla

#define NS_FORWARD_TO_MOUSEEVENT \
  NS_FORWARD_NSIDOMMOUSEEVENT(MouseEvent::) \
  NS_FORWARD_TO_UIEVENT

#endif // mozilla_dom_MouseEvent_h_
