/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMFOCUSEVENT

  // Forward to base class
  NS_FORWARD_TO_UIEVENT

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return FocusEventBinding::Wrap(aCx, this);
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

  nsresult InitFocusEvent(const nsAString& aType,
                          bool aCanBubble,
                          bool aCancelable,
                          nsIDOMWindow* aView,
                          int32_t aDetail,
                          EventTarget* aRelatedTarget);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FocusEvent_h_
