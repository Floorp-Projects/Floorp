/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SMIL_TIMEEVENT_H_
#define DOM_SMIL_TIMEEVENT_H_

#include "nsDocShell.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/TimeEventBinding.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/WindowProxyHolder.h"

class nsGlobalWindowInner;

namespace mozilla::dom {

class TimeEvent final : public Event {
 public:
  TimeEvent(EventTarget* aOwner, nsPresContext* aPresContext,
            InternalSMILTimeEvent* aEvent);

  // nsISupports interface:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TimeEvent, Event)

  JSObject* WrapObjectInternal(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override {
    return TimeEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  void InitTimeEvent(const nsAString& aType, nsGlobalWindowInner* aView,
                     int32_t aDetail);

  int32_t Detail() const { return mDetail; }

  Nullable<WindowProxyHolder> GetView() const {
    if (!mView) {
      return nullptr;
    }
    return WindowProxyHolder(mView->GetBrowsingContext());
  }

  TimeEvent* AsTimeEvent() final { return this; }

 private:
  ~TimeEvent() = default;

  nsCOMPtr<nsPIDOMWindowOuter> mView;
  int32_t mDetail;
};

}  // namespace mozilla::dom

already_AddRefed<mozilla::dom::TimeEvent> NS_NewDOMTimeEvent(
    mozilla::dom::EventTarget* aOwner, nsPresContext* aPresContext,
    mozilla::InternalSMILTimeEvent* aEvent);

#endif  // DOM_SMIL_TIMEEVENT_H_
