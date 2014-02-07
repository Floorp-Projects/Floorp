/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Portions Copyright 2013 Microsoft Open Technologies, Inc. */

#ifndef PointerEvent_h__
#define PointerEvent_h__

#include "nsDOMMouseEvent.h"
#include "mozilla/dom/PointerEventBinding.h"

class nsPresContext;

namespace mozilla {
namespace dom {

class PointerEvent : public nsDOMMouseEvent
{
public:
  PointerEvent(EventTarget* aOwner,
               nsPresContext* aPresContext,
               WidgetPointerEvent* aEvent);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::PointerEventBinding::Wrap(aCx, aScope, this);
  }

  static already_AddRefed<PointerEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const PointerEventInit& aParam,
              mozilla::ErrorResult& aRv);

  int32_t PointerId();
  int32_t Width();
  int32_t Height();
  float Pressure();
  int32_t TiltX();
  int32_t TiltY();
  bool IsPrimary();
  void GetPointerType(nsAString& aPointerType);
};

} // namespace dom
} // namespace mozilla

#endif
