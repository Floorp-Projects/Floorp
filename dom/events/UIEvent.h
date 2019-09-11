/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UIEvent_h_
#define mozilla_dom_UIEvent_h_

#include "mozilla/Attributes.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/UIEventBinding.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "nsDeviceContext.h"
#include "nsDocShell.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"

class nsINode;

namespace mozilla {
namespace dom {

class UIEvent : public Event {
 public:
  UIEvent(EventTarget* aOwner, nsPresContext* aPresContext,
          WidgetGUIEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(UIEvent, Event)

  void DuplicatePrivateData() override;
  void Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType) override;
  bool Deserialize(const IPC::Message* aMsg, PickleIterator* aIter) override;

  static already_AddRefed<UIEvent> Constructor(const GlobalObject& aGlobal,
                                               const nsAString& aType,
                                               const UIEventInit& aParam);

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override {
    return UIEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  UIEvent* AsUIEvent() override { return this; }

  void InitUIEvent(const nsAString& typeArg, bool canBubbleArg,
                   bool cancelableArg, nsGlobalWindowInner* viewArg,
                   int32_t detailArg);

  Nullable<WindowProxyHolder> GetView() const {
    if (!mView) {
      return nullptr;
    }
    return WindowProxyHolder(mView->GetBrowsingContext());
  }

  int32_t Detail() const { return mDetail; }

  int32_t LayerX() const { return GetLayerPoint().x; }

  int32_t LayerY() const { return GetLayerPoint().y; }

  virtual uint32_t Which(CallerType aCallerType = CallerType::System) {
    MOZ_ASSERT(mEvent->mClass != eKeyboardEventClass,
               "Key events should override Which()");
    MOZ_ASSERT(mEvent->mClass != eMouseEventClass,
               "Mouse events should override Which()");
    return 0;
  }

  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<nsINode> GetRangeParent();

  MOZ_CAN_RUN_SCRIPT
  int32_t RangeOffset() const;

 protected:
  ~UIEvent() {}

  // Internal helper functions
  nsIntPoint GetMovementPoint();
  nsIntPoint GetLayerPoint() const;

  nsCOMPtr<nsPIDOMWindowOuter> mView;
  int32_t mDetail;
  CSSIntPoint mClientPoint;
  // Screenpoint is mEvent->mRefPoint.
  nsIntPoint mLayerPoint;
  CSSIntPoint mPagePoint;
  nsIntPoint mMovementPoint;
  bool mIsPointerLocked;
  CSSIntPoint mLastClientPoint;

  static Modifiers ComputeModifierState(const nsAString& aModifiersList);
  bool GetModifierStateInternal(const nsAString& aKey);
  void InitModifiers(const EventModifierInit& aParam);
};

}  // namespace dom
}  // namespace mozilla

already_AddRefed<mozilla::dom::UIEvent> NS_NewDOMUIEvent(
    mozilla::dom::EventTarget* aOwner, nsPresContext* aPresContext,
    mozilla::WidgetGUIEvent* aEvent);

#endif  // mozilla_dom_UIEvent_h_
