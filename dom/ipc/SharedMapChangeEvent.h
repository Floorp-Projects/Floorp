/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_ipc_SharedMapChangeEvent_h
#define dom_ipc_SharedMapChangeEvent_h

#include "mozilla/dom/MozSharedMapBinding.h"

#include "mozilla/dom/Event.h"
#include "nsTArray.h"

namespace mozilla::dom::ipc {

class SharedMapChangeEvent final : public Event {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(SharedMapChangeEvent, Event)

  JSObject* WrapObjectInternal(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override {
    return MozSharedMapChangeEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  static already_AddRefed<SharedMapChangeEvent> Constructor(
      EventTarget* aEventTarget, const nsAString& aType,
      const MozSharedMapChangeEventInit& aInit);

  void GetChangedKeys(nsTArray<nsString>& aChangedKeys) const {
    aChangedKeys.AppendElements(mChangedKeys);
  }

 protected:
  ~SharedMapChangeEvent() override = default;

 private:
  explicit SharedMapChangeEvent(EventTarget* aEventTarget)
      : Event(aEventTarget, nullptr, nullptr) {}

  nsTArray<nsString> mChangedKeys;
};

}  // namespace mozilla::dom::ipc

#endif  // dom_ipc_SharedMapChangeEvent_h
