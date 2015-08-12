/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return BeforeAfterKeyboardEventBinding::Wrap(aCx, this, aGivenProto);
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

already_AddRefed<mozilla::dom::BeforeAfterKeyboardEvent>
NS_NewDOMBeforeAfterKeyboardEvent(mozilla::dom::EventTarget* aOwner,
                                  nsPresContext* aPresContext,
                                  mozilla::InternalBeforeAfterKeyboardEvent* aEvent);

#endif // mozilla_dom_BeforeAfterKeyboardEvent_h_
