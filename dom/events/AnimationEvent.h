/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_AnimationEvent_h_
#define mozilla_dom_AnimationEvent_h_

#include "mozilla/EventForwards.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/AnimationEventBinding.h"
#include "nsIDOMAnimationEvent.h"

class nsAString;

namespace mozilla {
namespace dom {

class AnimationEvent : public Event,
                       public nsIDOMAnimationEvent
{
public:
  AnimationEvent(EventTarget* aOwner,
                 nsPresContext* aPresContext,
                 InternalAnimationEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_EVENT
  NS_DECL_NSIDOMANIMATIONEVENT

  static already_AddRefed<AnimationEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const AnimationEventInit& aParam,
              ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return AnimationEventBinding::Wrap(aCx, this);
  }

  // xpidl implementation
  // GetAnimationName(nsAString& aAnimationName);
  // GetPseudoElement(nsAString& aPseudoElement);

  float ElapsedTime();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationEvent_h_
