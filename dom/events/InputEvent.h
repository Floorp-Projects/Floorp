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

class DataTransfer;

class InputEvent : public UIEvent {
 public:
  InputEvent(EventTarget* aOwner, nsPresContext* aPresContext,
             InternalEditorInputEvent* aEvent);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(InputEvent, UIEvent)

  static already_AddRefed<InputEvent> Constructor(const GlobalObject& aGlobal,
                                                  const nsAString& aType,
                                                  const InputEventInit& aParam);

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override {
    return InputEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  void GetInputType(nsAString& aInputType);
  void GetData(nsAString& aData, CallerType aCallerType = CallerType::System);
  already_AddRefed<DataTransfer> GetDataTransfer(
      CallerType aCallerType = CallerType::System);
  bool IsComposing();

 protected:
  ~InputEvent() {}

  // mInputTypeValue stores inputType attribute value if the instance is
  // created by script and not initialized with known inputType value.
  nsString mInputTypeValue;
};

}  // namespace dom
}  // namespace mozilla

already_AddRefed<mozilla::dom::InputEvent> NS_NewDOMInputEvent(
    mozilla::dom::EventTarget* aOwner, nsPresContext* aPresContext,
    mozilla::InternalEditorInputEvent* aEvent);

#endif  // mozilla_dom_InputEvent_h_
