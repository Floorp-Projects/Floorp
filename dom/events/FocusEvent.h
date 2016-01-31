/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_FocusEvent_h_
#define mozilla_dom_FocusEvent_h_

#include "mozilla/dom/FocusEventBinding.h"
#include "mozilla/dom/UIEvent.h"
#include "mozilla/EventForwards.h"
#include "nsIDOMFocusEvent.h"

namespace mozilla {
namespace dom {

class FocusEvent : public UIEvent,
                   public nsIDOMFocusEvent
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMFOCUSEVENT

  // Forward to base class
  NS_FORWARD_TO_UIEVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return FocusEventBinding::Wrap(aCx, this, aGivenProto);
  }

  FocusEvent(EventTarget* aOwner,
             nsPresContext* aPresContext,
             InternalFocusEvent* aEvent);

  EventTarget* GetRelatedTarget();

  static already_AddRefed<FocusEvent> Constructor(const GlobalObject& aGlobal,
                                                  const nsAString& aType,
                                                  const FocusEventInit& aParam,
                                                  ErrorResult& aRv);
protected:
  ~FocusEvent() {}

  void InitFocusEvent(const nsAString& aType,
                      bool aCanBubble,
                      bool aCancelable,
                      nsGlobalWindow* aView,
                      int32_t aDetail,
                      EventTarget* aRelatedTarget);
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::FocusEvent>
NS_NewDOMFocusEvent(mozilla::dom::EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    mozilla::InternalFocusEvent* aEvent);

#endif // mozilla_dom_FocusEvent_h_
