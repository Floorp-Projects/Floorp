/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMUIEvent_h
#define nsDOMUIEvent_h

#include "nsIDOMUIEvent.h"
#include "nsDOMEvent.h"
#include "nsLayoutUtils.h"
#include "nsEvent.h"
#include "mozilla/dom/UIEventBinding.h"

class nsDOMUIEvent : public nsDOMEvent,
                     public nsIDOMUIEvent
{
public:
  nsDOMUIEvent(mozilla::dom::EventTarget* aOwner,
               nsPresContext* aPresContext, nsGUIEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMUIEvent, nsDOMEvent)

  // nsIDOMUIEvent Interface
  NS_DECL_NSIDOMUIEVENT
  
  // Forward to nsDOMEvent
  NS_FORWARD_TO_NSDOMEVENT_NO_SERIALIZATION_NO_DUPLICATION
  NS_IMETHOD DuplicatePrivateData();
  NS_IMETHOD_(void) Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType);
  NS_IMETHOD_(bool) Deserialize(const IPC::Message* aMsg, void** aIter);

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, JS::Value* aVal);

  static nsIntPoint CalculateScreenPoint(nsPresContext* aPresContext,
                                         nsEvent* aEvent)
  {
    if (!aEvent ||
        (aEvent->eventStructType != NS_MOUSE_EVENT &&
         aEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
         aEvent->eventStructType != NS_WHEEL_EVENT &&
         aEvent->eventStructType != NS_DRAG_EVENT &&
         aEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT)) {
      return nsIntPoint(0, 0);
    }

    if (!((nsGUIEvent*)aEvent)->widget ) {
      return aEvent->refPoint;
    }

    nsIntPoint offset = aEvent->refPoint +
                        ((nsGUIEvent*)aEvent)->widget->WidgetToScreenOffset();
    nscoord factor = aPresContext->DeviceContext()->UnscaledAppUnitsPerDevPixel();
    return nsIntPoint(nsPresContext::AppUnitsToIntCSSPixels(offset.x * factor),
                      nsPresContext::AppUnitsToIntCSSPixels(offset.y * factor));
  }

  static nsIntPoint CalculateClientPoint(nsPresContext* aPresContext,
                                         nsEvent* aEvent,
                                         nsIntPoint* aDefaultClientPoint)
  {
    if (!aEvent ||
        (aEvent->eventStructType != NS_MOUSE_EVENT &&
         aEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
         aEvent->eventStructType != NS_WHEEL_EVENT &&
         aEvent->eventStructType != NS_DRAG_EVENT &&
         aEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT) ||
        !aPresContext ||
        !((nsGUIEvent*)aEvent)->widget) {
      return (nullptr == aDefaultClientPoint ? nsIntPoint(0, 0) :
        nsIntPoint(aDefaultClientPoint->x, aDefaultClientPoint->y));
    }

    nsPoint pt(0, 0);
    nsIPresShell* shell = aPresContext->GetPresShell();
    if (!shell) {
      return nsIntPoint(0, 0);
    }
    nsIFrame* rootFrame = shell->GetRootFrame();
    if (rootFrame) {
      pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, rootFrame);
    }

    return nsIntPoint(nsPresContext::AppUnitsToIntCSSPixels(pt.x),
                      nsPresContext::AppUnitsToIntCSSPixels(pt.y));
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

  already_AddRefed<nsIDOMWindow> GetView()
  {
    nsCOMPtr<nsIDOMWindow> view = mView;
    return view.forget();
  }

  int32_t Detail()
  {
    return mDetail;
  }

  int32_t LayerX()
  {
    return GetLayerPoint().x;
  }

  int32_t LayerY()
  {
    return GetLayerPoint().y;
  }

  int32_t PageX()
  {
    int32_t x;
    GetPageX(&x);
    return x;
  }

  int32_t PageY()
  {
    int32_t y;
    GetPageY(&y);
    return y;
  }

  virtual uint32_t Which()
  {
    uint32_t w;
    GetWhich(&w);
    return w;
  }

  already_AddRefed<nsINode> GetRangeParent();

  int32_t RangeOffset()
  {
    int32_t offset;
    GetRangeOffset(&offset);
    return offset;
  }

  bool CancelBubble()
  {
    return mEvent->mFlags.mPropagationStopped;
  }

  bool IsChar()
  {
    bool isChar;
    GetIsChar(&isChar);
    return isChar;
  }

protected:
  // Internal helper functions
  nsIntPoint GetClientPoint();
  nsIntPoint GetMovementPoint();
  nsIntPoint GetLayerPoint();
  nsIntPoint GetPagePoint();

  // Allow specializations.
  virtual nsresult Which(uint32_t* aWhich)
  {
    NS_ENSURE_ARG_POINTER(aWhich);
    // Usually we never reach here, as this is reimplemented for mouse and keyboard events.
    *aWhich = 0;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMWindow> mView;
  int32_t mDetail;
  nsIntPoint mClientPoint;
  // Screenpoint is mEvent->refPoint.
  nsIntPoint mLayerPoint;
  nsIntPoint mPagePoint;
  nsIntPoint mMovementPoint;
  bool mIsPointerLocked;
  nsIntPoint mLastClientPoint;

  typedef mozilla::widget::Modifiers Modifiers;
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
