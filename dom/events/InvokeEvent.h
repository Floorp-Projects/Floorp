/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_INVOKEEVENT_H_
#define DOM_INVOKEEVENT_H_

#include "mozilla/dom/Event.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/InvokeEventBinding.h"

struct JSContext;
namespace mozilla::dom {

class InvokeEvent : public Event {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(InvokeEvent, Event)

  virtual InvokeEvent* AsInvokeEvent() { return this; }

  explicit InvokeEvent(EventTarget* aOwner) : Event(aOwner, nullptr, nullptr) {}

  JSObject* WrapObjectInternal(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<InvokeEvent> Constructor(
      EventTarget* aOwner, const nsAString& aType,
      const InvokeEventInit& aEventInitDict);

  static already_AddRefed<InvokeEvent> Constructor(
      const GlobalObject& aGlobal, const nsAString& aType,
      const InvokeEventInit& aEventInitDict);

  Element* GetInvoker();

  void GetAction(nsAString& aAction) const { aAction = mAction; }

 protected:
  ~InvokeEvent() = default;

  RefPtr<Element> mInvoker;
  nsString mAction;
};

}  // namespace mozilla::dom

#endif  // DOM_INVOKEEVENT_H_
