/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This class implements a XUL "command" event.  See XULCommandEvent.webidl

#ifndef mozilla_dom_XULCommandEvent_h_
#define mozilla_dom_XULCommandEvent_h_

#include "mozilla/RefPtr.h"
#include "mozilla/dom/UIEvent.h"
#include "mozilla/dom/XULCommandEventBinding.h"

namespace mozilla {
namespace dom {

class XULCommandEvent : public UIEvent {
 public:
  XULCommandEvent(EventTarget* aOwner, nsPresContext* aPresContext,
                  WidgetInputEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULCommandEvent, UIEvent)

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override {
    return XULCommandEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  virtual XULCommandEvent* AsXULCommandEvent() override { return this; }

  bool AltKey();
  bool CtrlKey();
  bool ShiftKey();
  bool MetaKey();
  uint16_t InputSource();
  int16_t Button() { return mButton; }

  already_AddRefed<Event> GetSourceEvent() {
    RefPtr<Event> e = mSourceEvent;
    return e.forget();
  }

  void InitCommandEvent(const nsAString& aType, bool aCanBubble,
                        bool aCancelable, nsGlobalWindowInner* aView,
                        int32_t aDetail, bool aCtrlKey, bool aAltKey,
                        bool aShiftKey, bool aMetaKey, int16_t aButton,
                        Event* aSourceEvent, uint16_t aInputSource,
                        ErrorResult& aRv);

 protected:
  ~XULCommandEvent() = default;

  RefPtr<Event> mSourceEvent;
  uint16_t mInputSource;
  int16_t mButton = 0;
};

}  // namespace dom
}  // namespace mozilla

already_AddRefed<mozilla::dom::XULCommandEvent> NS_NewDOMXULCommandEvent(
    mozilla::dom::EventTarget* aOwner, nsPresContext* aPresContext,
    mozilla::WidgetInputEvent* aEvent);

#endif  // mozilla_dom_XULCommandEvent_h_
