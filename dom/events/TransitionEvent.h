/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_TransitionEvent_h_
#define mozilla_dom_TransitionEvent_h_

#include "mozilla/EventForwards.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/TransitionEventBinding.h"
#include "nsIDOMTransitionEvent.h"

class nsAString;

namespace mozilla {
namespace dom {

class TransitionEvent : public Event,
                        public nsIDOMTransitionEvent
{
public:
  TransitionEvent(EventTarget* aOwner,
                  nsPresContext* aPresContext,
                  InternalTransitionEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_EVENT
  NS_DECL_NSIDOMTRANSITIONEVENT

  static already_AddRefed<TransitionEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const TransitionEventInit& aParam,
              ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return TransitionEventBinding::Wrap(aCx, this);
  }

  // xpidl implementation
  // GetPropertyName(nsAString& aPropertyName)
  // GetPseudoElement(nsAString& aPreudoElement)

  float ElapsedTime();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TransitionEvent_h_
