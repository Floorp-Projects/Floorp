/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDOMFocusEvent_h_
#define nsDOMFocusEvent_h_

#include "nsDOMUIEvent.h"
#include "nsIDOMFocusEvent.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/FocusEventBinding.h"

class nsDOMFocusEvent : public nsDOMUIEvent,
                        public nsIDOMFocusEvent
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMFOCUSEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMUIEVENT

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::FocusEventBinding::Wrap(aCx, aScope, this);
  }

  nsDOMFocusEvent(mozilla::dom::EventTarget* aOwner,
                  nsPresContext* aPresContext,
                  mozilla::InternalFocusEvent* aEvent);

  mozilla::dom::EventTarget* GetRelatedTarget();

  static already_AddRefed<nsDOMFocusEvent> Constructor(const mozilla::dom::GlobalObject& aGlobal,
                                                       const nsAString& aType,
                                                       const mozilla::dom::FocusEventInit& aParam,
                                                       mozilla::ErrorResult& aRv);
protected:
  nsresult InitFocusEvent(const nsAString& aType,
                          bool aCanBubble,
                          bool aCancelable,
                          nsIDOMWindow* aView,
                          int32_t aDetail,
                          mozilla::dom::EventTarget* aRelatedTarget);
};

#endif /* !defined(nsDOMFocusEvent_h_) */
