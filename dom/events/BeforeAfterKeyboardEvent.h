/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BeforeAfterKeyboardEvent_h_
#define mozilla_dom_BeforeAfterKeyboardEvent_h_

#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/BeforeAfterKeyboardEventBinding.h"

namespace mozilla {
namespace dom {

class BeforeAfterKeyboardEvent : public KeyboardEvent
{
public:
  BeforeAfterKeyboardEvent(EventTarget* aOwner,
                           nsPresContext* aPresContext,
                           InternalBeforeAfterKeyboardEvent* aEvent);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return BeforeAfterKeyboardEventBinding::Wrap(aCx, this);
  }

  static already_AddRefed<BeforeAfterKeyboardEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const BeforeAfterKeyboardEventInit& aParam,
              ErrorResult& aRv);

  static already_AddRefed<BeforeAfterKeyboardEvent>
  Constructor(EventTarget* aOwner, const nsAString& aType,
              const BeforeAfterKeyboardEventInit& aEventInitDict);

  // This function returns a boolean value when event typs is either
  // "mozbrowserafterkeydown" or "mozbrowserafterkeyup".
  Nullable<bool> GetEmbeddedCancelled();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BeforeAfterKeyboardEvent_h_
