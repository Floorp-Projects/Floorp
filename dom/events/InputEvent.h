/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return InputEventBinding::Wrap(aCx, this, aGivenProto);
  }

  bool IsComposing();

protected:
  ~InputEvent() {}
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::InputEvent>
NS_NewDOMInputEvent(mozilla::dom::EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    mozilla::InternalEditorInputEvent* aEvent);

#endif // mozilla_dom_InputEvent_h_
