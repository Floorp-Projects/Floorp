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
  NS_IMETHOD DuplicatePrivateData() MOZ_OVERRIDE;
  NS_IMETHOD_(void) Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) Deserialize(const IPC::Message* aMsg, void** aIter) MOZ_OVERRIDE;

  static nsIntPoint CalculateScreenPoint(nsPresContext* aPresContext,
                                         WidgetEvent* aEvent)
  {
    if (!aEvent ||
        (aEvent->eventStructType != NS_MOUSE_EVENT &&
         aEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
         aEvent->eventStructType != NS_WHEEL_EVENT &&
         aEvent->eventStructType != NS_DRAG_EVENT &&
         aEvent->eventStructType != NS_POINTER_EVENT &&
         aEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT)) {
      return nsIntPoint(0, 0);
    }

    WidgetGUIEvent* event = aEvent->AsGUIEvent();
    if (!event->widget) {
      return LayoutDeviceIntPoint::ToUntyped(aEvent->refPoint);
    }

    LayoutDeviceIntPoint offset = aEvent->refPoint +
      LayoutDeviceIntPoint::FromUntyped(event->widget->WidgetToScreenOffset());
    nscoord factor =
      aPresContext->DeviceContext()->UnscaledAppUnitsPerDevPixel();
    return nsIntPoint(nsPresContext::AppUnitsToIntCSSPixels(offset.x * factor),
                      nsPresContext::AppUnitsToIntCSSPixels(offset.y * factor));
  }

  static CSSIntPoint CalculateClientPoint(nsPresContext* aPresContext,
                                          WidgetEvent* aEvent,
                                          CSSIntPoint* aDefaultClientPoint)
  {
    if (!aEvent ||
        (aEvent->eventStructType != NS_MOUSE_EVENT &&
         aEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
         aEvent->eventStructType != NS_WHEEL_EVENT &&
         aEvent->eventStructType != NS_DRAG_EVENT &&
         aEvent->eventStructType != NS_POINTER_EVENT &&
         aEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT) ||
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

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return UIEventBinding::Wrap(aCx, aScope, this);
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
    MOZ_ASSERT(mEvent->eventStructType != NS_KEY_EVENT,
               "Key events should override Which()");
    MOZ_ASSERT(mEvent->eventStructType != NS_MOUSE_EVENT,
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
};

} // namespace dom
} // namespace mozilla

#define NS_FORWARD_TO_UIEVENT                               \
  NS_FORWARD_NSIDOMUIEVENT(UIEvent::)                       \
  NS_FORWARD_TO_EVENT_NO_SERIALIZATION_NO_DUPLICATION       \
  NS_IMETHOD DuplicatePrivateData()                         \
  {                                                         \
    return UIEvent::DuplicatePrivateData();                 \
  }                                                         \
  NS_IMETHOD_(void) Serialize(IPC::Message* aMsg,           \
                              bool aSerializeInterfaceType) \
  {                                                         \
    UIEvent::Serialize(aMsg, aSerializeInterfaceType);      \
  }                                                         \
  NS_IMETHOD_(bool) Deserialize(const IPC::Message* aMsg,   \
                                void** aIter)               \
  {                                                         \
    return UIEvent::Deserialize(aMsg, aIter);               \
  }

#endif // mozilla_dom_UIEvent_h_
