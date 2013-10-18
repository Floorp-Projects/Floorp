/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDOMAnimationEvent_h_
#define nsDOMAnimationEvent_h_

#include "nsDOMEvent.h"
#include "nsIDOMAnimationEvent.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/AnimationEventBinding.h"

class nsAString;

class nsDOMAnimationEvent : public nsDOMEvent,
                            public nsIDOMAnimationEvent
{
public:
  nsDOMAnimationEvent(mozilla::dom::EventTarget* aOwner,
                      nsPresContext *aPresContext,
                      mozilla::InternalAnimationEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMEVENT
  NS_DECL_NSIDOMANIMATIONEVENT

  static already_AddRefed<nsDOMAnimationEvent>
  Constructor(const mozilla::dom::GlobalObject& aGlobal,
              const nsAString& aType,
              const mozilla::dom::AnimationEventInit& aParam,
              mozilla::ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::AnimationEventBinding::Wrap(aCx, aScope, this);
  }

  // xpidl implementation
  // GetAnimationName(nsAString& aAnimationName);
  // GetPseudoElement(nsAString& aPseudoElement);

  float ElapsedTime();
};

#endif /* !defined(nsDOMAnimationEvent_h_) */
