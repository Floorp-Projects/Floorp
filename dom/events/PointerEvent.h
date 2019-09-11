/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Portions Copyright 2013 Microsoft Open Technologies, Inc. */

#ifndef mozilla_dom_PointerEvent_h_
#define mozilla_dom_PointerEvent_h_

#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/PointerEventBinding.h"

class nsPresContext;

namespace mozilla {
namespace dom {

struct PointerEventInit;

class PointerEvent : public MouseEvent {
 public:
  PointerEvent(EventTarget* aOwner, nsPresContext* aPresContext,
               WidgetPointerEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PointerEvent, MouseEvent)

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<PointerEvent> Constructor(
      const GlobalObject& aGlobal, const nsAString& aType,
      const PointerEventInit& aParam);

  static already_AddRefed<PointerEvent> Constructor(
      EventTarget* aOwner, const nsAString& aType,
      const PointerEventInit& aParam);

  int32_t PointerId(CallerType aCallerType);
  int32_t Width(CallerType aCallerType);
  int32_t Height(CallerType aCallerType);
  float Pressure(CallerType aCallerType);
  float TangentialPressure(CallerType aCallerType);
  int32_t TiltX(CallerType aCallerType);
  int32_t TiltY(CallerType aCallerType);
  int32_t Twist(CallerType aCallerType);
  bool IsPrimary();
  void GetPointerType(nsAString& aPointerType, CallerType aCallerType);
  void GetCoalescedEvents(nsTArray<RefPtr<PointerEvent>>& aPointerEvents);

 protected:
  ~PointerEvent() {}

 private:
  // This method returns the boolean to indicate whether spoofing pointer
  // event for fingerprinting resistance.
  bool ShouldResistFingerprinting(CallerType aCallerType);

  nsTArray<RefPtr<PointerEvent>> mCoalescedEvents;
};

void ConvertPointerTypeToString(uint16_t aPointerTypeSrc,
                                nsAString& aPointerTypeDest);

}  // namespace dom
}  // namespace mozilla

already_AddRefed<mozilla::dom::PointerEvent> NS_NewDOMPointerEvent(
    mozilla::dom::EventTarget* aOwner, nsPresContext* aPresContext,
    mozilla::WidgetPointerEvent* aEvent);

#endif  // mozilla_dom_PointerEvent_h_
