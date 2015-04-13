/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UIEvent_h_
#define mozilla_dom_UIEvent_h_

#include "mozilla/Attributes.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/UIEventBinding.h"
#include "nsDeviceContext.h"
#include "nsIDOMUIEvent.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"

class nsINode;

namespace mozilla {
namespace dom {

class UIEvent : public Event,
                public nsIDOMUIEvent
{
public:
  UIEvent(EventTarget* aOwner,
          nsPresContext* aPresContext,
          WidgetGUIEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(UIEvent, Event)

  // nsIDOMUIEvent Interface
  NS_DECL_NSIDOMUIEVENT

  // Forward to Event
  NS_FORWARD_TO_EVENT_NO_SERIALIZATION_NO_DUPLICATION
  NS_IMETHOD DuplicatePrivateData() override;
  NS_IMETHOD_(void) Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType) override;
  NS_IMETHOD_(bool) Deserialize(const IPC::Message* aMsg, void** aIter) override;

  static LayoutDeviceIntPoint CalculateScreenPoint(nsPresContext* aPresContext,
                                                   WidgetEvent* aEvent)
  {
    if (!aEvent ||
        (aEvent->mClass != eMouseEventClass &&
         aEvent->mClass != eMouseScrollEventClass &&
         aEvent->mClass != eWheelEventClass &&
         aEvent->mClass != eDragEventClass &&
         aEvent->mClass != ePointerEventClass &&
         aEvent->mClass != eSimpleGestureEventClass)) {
      return LayoutDeviceIntPoint(0, 0);
    }

    WidgetGUIEvent* event = aEvent->AsGUIEvent();
    if (!event->widget) {
      return aEvent->refPoint;
    }

    LayoutDeviceIntPoint offset = aEvent->refPoint + event->widget->WidgetToScreenOffset();
    nscoord factor =
      aPresContext->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom();
    return LayoutDeviceIntPoint(nsPresContext::AppUnitsToIntCSSPixels(offset.x * factor),
                                nsPresContext::AppUnitsToIntCSSPixels(offset.y * factor));
  }

  static CSSIntPoint CalculateClientPoint(nsPresContext* aPresContext,
                                          WidgetEvent* aEvent,
                                          CSSIntPoint* aDefaultClientPoint)
  {
    if (!aEvent ||
        (aEvent->mClass != eMouseEventClass &&
         aEvent->mClass != eMouseScrollEventClass &&
         aEvent->mClass != eWheelEventClass &&
         aEvent->mClass != eDragEventClass &&
         aEvent->mClass != ePointerEventClass &&
         aEvent->mClass != eSimpleGestureEventClass) ||
        !aPresContext ||
        !aEvent->AsGUIEvent()->widget) {
      return aDefaultClientPoint
             ? *aDefaultClientPoint
             : CSSIntPoint(0, 0);
    }

    nsIPresShell* shell = aPresContext->GetPresShell();
    if (!shell) {
      return CSSIntPoint(0, 0);
    }
    nsIFrame* rootFrame = shell->GetRootFrame();
    if (!rootFrame) {
      return CSSIntPoint(0, 0);
    }
    nsPoint pt =
      nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, rootFrame);

    return CSSIntPoint::FromAppUnitsRounded(pt);
  }

  static already_AddRefed<UIEvent> Constructor(const GlobalObject& aGlobal,
                                               const nsAString& aType,
                                               const UIEventInit& aParam,
                                               ErrorResult& aRv);

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return UIEventBinding::Wrap(aCx, this, aGivenProto);
  }

  nsIDOMWindow* GetView() const
  {
    return mView;
  }

  int32_t Detail() const
  {
    return mDetail;
  }

  int32_t LayerX() const
  {
    return GetLayerPoint().x;
  }

  int32_t LayerY() const
  {
    return GetLayerPoint().y;
  }

  int32_t PageX() const;
  int32_t PageY() const;

  virtual uint32_t Which()
  {
    MOZ_ASSERT(mEvent->mClass != eKeyboardEventClass,
               "Key events should override Which()");
    MOZ_ASSERT(mEvent->mClass != eMouseEventClass,
               "Mouse events should override Which()");
    return 0;
  }

  already_AddRefed<nsINode> GetRangeParent();

  int32_t RangeOffset() const;

  bool CancelBubble() const
  {
    return mEvent->mFlags.mPropagationStopped;
  }

  bool IsChar() const;

protected:
  ~UIEvent() {}

  // Internal helper functions
  nsIntPoint GetMovementPoint();
  nsIntPoint GetLayerPoint() const;

  nsCOMPtr<nsIDOMWindow> mView;
  int32_t mDetail;
  CSSIntPoint mClientPoint;
  // Screenpoint is mEvent->refPoint.
  nsIntPoint mLayerPoint;
  CSSIntPoint mPagePoint;
  nsIntPoint mMovementPoint;
  bool mIsPointerLocked;
  CSSIntPoint mLastClientPoint;

  static Modifiers ComputeModifierState(const nsAString& aModifiersList);
  bool GetModifierStateInternal(const nsAString& aKey);
  void InitModifiers(const EventModifierInit& aParam);
};

} // namespace dom
} // namespace mozilla

#define NS_FORWARD_TO_UIEVENT                               \
  NS_FORWARD_NSIDOMUIEVENT(UIEvent::)                       \
  NS_FORWARD_TO_EVENT_NO_SERIALIZATION_NO_DUPLICATION       \
  NS_IMETHOD DuplicatePrivateData() override                \
  {                                                         \
    return UIEvent::DuplicatePrivateData();                 \
  }                                                         \
  NS_IMETHOD_(void) Serialize(IPC::Message* aMsg,           \
                              bool aSerializeInterfaceType) \
    override                                                \
  {                                                         \
    UIEvent::Serialize(aMsg, aSerializeInterfaceType);      \
  }                                                         \
  NS_IMETHOD_(bool) Deserialize(const IPC::Message* aMsg,   \
                                void** aIter) override      \
  {                                                         \
    return UIEvent::Deserialize(aMsg, aIter);               \
  }

#endif // mozilla_dom_UIEvent_h_
