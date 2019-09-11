/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DragEvent_h_
#define mozilla_dom_DragEvent_h_

#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/DragEventBinding.h"
#include "mozilla/EventForwards.h"

namespace mozilla {
namespace dom {

class DataTransfer;

class DragEvent : public MouseEvent {
 public:
  DragEvent(EventTarget* aOwner, nsPresContext* aPresContext,
            WidgetDragEvent* aEvent);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(DragEvent, MouseEvent)

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override {
    return DragEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  DragEvent* AsDragEvent() override { return this; }

  DataTransfer* GetDataTransfer();

  void InitDragEvent(const nsAString& aType, bool aCanBubble, bool aCancelable,
                     nsGlobalWindowInner* aView, int32_t aDetail,
                     int32_t aScreenX, int32_t aScreenY, int32_t aClientX,
                     int32_t aClientY, bool aCtrlKey, bool aAltKey,
                     bool aShiftKey, bool aMetaKey, uint16_t aButton,
                     EventTarget* aRelatedTarget, DataTransfer* aDataTransfer);

  static already_AddRefed<DragEvent> Constructor(const GlobalObject& aGlobal,
                                                 const nsAString& aType,
                                                 const DragEventInit& aParam);

 protected:
  ~DragEvent() {}
};

}  // namespace dom
}  // namespace mozilla

already_AddRefed<mozilla::dom::DragEvent> NS_NewDOMDragEvent(
    mozilla::dom::EventTarget* aOwner, nsPresContext* aPresContext,
    mozilla::WidgetDragEvent* aEvent);

#endif  // mozilla_dom_DragEvent_h_
