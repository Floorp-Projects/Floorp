/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMUIEvent_h
#define nsDOMUIEvent_h

#include "mozilla/Attributes.h"
#include "nsIDOMUIEvent.h"
#include "nsDOMEvent.h"
#include "nsLayoutUtils.h"
#include "mozilla/dom/UIEventBinding.h"
#include "nsPresContext.h"
#include "nsDeviceContext.h"

class nsINode;

class nsDOMUIEvent : public nsDOMEvent,
                     public nsIDOMUIEvent
{
  typedef mozilla::CSSIntPoint CSSIntPoint;
public:
  nsDOMUIEvent(mozilla::dom::EventTarget* aOwner,
               nsPresContext* aPresContext,
               mozilla::WidgetGUIEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMUIEvent, nsDOMEvent)

  // nsIDOMUIEvent Interface
  NS_DECL_NSIDOMUIEVENT
  
  // Forward to nsDOMEvent
  NS_FORWARD_TO_NSDOMEVENT_NO_SERIALIZATION_NO_DUPLICATION
  NS_IMETHOD DuplicatePrivateData() MOZ_OVERRIDE;
  NS_IMETHOD_(void) Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) Deserialize(const IPC::Message* aMsg, void** aIter) MOZ_OVERRIDE;

  static nsIntPoint
  CalculateScreenPoint(nsPresContext* aPresContext,
                       mozilla::WidgetEvent* aEvent)
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

    mozilla::WidgetGUIEvent* event = aEvent->AsGUIEvent();
    if (!event->widget) {
      return mozilla::LayoutDeviceIntPoint::ToUntyped(aEvent->refPoint);
    }

    mozilla::LayoutDeviceIntPoint offset = aEvent->refPoint +
      mozilla::LayoutDeviceIntPoint::FromUntyped(event->widget->WidgetToScreenOffset());
    nscoord factor = aPresContext->DeviceContext()->UnscaledAppUnitsPerDevPixel();
    return nsIntPoint(nsPresContext::AppUnitsToIntCSSPixels(offset.x * factor),
                      nsPresContext::AppUnitsToIntCSSPixels(offset.y * factor));
  }

  static CSSIntPoint CalculateClientPoint(nsPresContext* aPresContext,
                                          mozilla::WidgetEvent* aEvent,
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

  static already_AddRefed<nsDOMUIEvent> Constructor(const mozilla::dom::GlobalObject& aGlobal,
                                                    const nsAString& aType,
                                                    const mozilla::dom::UIEventInit& aParam,
                                                    mozilla::ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::UIEventBinding::Wrap(aCx, aScope, this);
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

  typedef mozilla::Modifiers Modifiers;
  static Modifiers ComputeModifierState(const nsAString& aModifiersList);
  bool GetModifierStateInternal(const nsAString& aKey);
};

#define NS_FORWARD_TO_NSDOMUIEVENT                          \
  NS_FORWARD_NSIDOMUIEVENT(nsDOMUIEvent::)                  \
  NS_FORWARD_TO_NSDOMEVENT_NO_SERIALIZATION_NO_DUPLICATION  \
  NS_IMETHOD DuplicatePrivateData()                         \
  {                                                         \
    return nsDOMUIEvent::DuplicatePrivateData();            \
  }                                                         \
  NS_IMETHOD_(void) Serialize(IPC::Message* aMsg,           \
                              bool aSerializeInterfaceType) \
  {                                                         \
    nsDOMUIEvent::Serialize(aMsg, aSerializeInterfaceType); \
  }                                                         \
  NS_IMETHOD_(bool) Deserialize(const IPC::Message* aMsg,   \
                                void** aIter)               \
  {                                                         \
    return nsDOMUIEvent::Deserialize(aMsg, aIter);          \
  }

#endif // nsDOMUIEvent_h
