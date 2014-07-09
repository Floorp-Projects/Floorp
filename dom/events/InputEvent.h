/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputEvent_h_
#define mozilla_dom_InputEvent_h_

#include "mozilla/dom/UIEvent.h"
#include "mozilla/dom/InputEventBinding.h"
#include "mozilla/EventForwards.h"

namespace mozilla {
namespace dom {

class InputEvent : public UIEvent
{
public:
  InputEvent(EventTarget* aOwner,
             nsPresContext* aPresContext,
             InternalEditorInputEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to base class
  NS_FORWARD_TO_UIEVENT


  static already_AddRefed<InputEvent> Constructor(const GlobalObject& aGlobal,
                                                  const nsAString& aType,
                                                  const InputEventInit& aParam,
                                                  ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return InputEventBinding::Wrap(aCx, this);
  }

  bool IsComposing();

protected:
  ~InputEvent() {}
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_InputEvent_h_
