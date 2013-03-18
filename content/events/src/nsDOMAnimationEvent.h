/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDOMAnimationEvent_h_
#define nsDOMAnimationEvent_h_

#include "nsDOMEvent.h"
#include "nsIDOMAnimationEvent.h"
#include "nsString.h"
#include "mozilla/dom/AnimationEventBinding.h"

class nsAnimationEvent;

class nsDOMAnimationEvent : public nsDOMEvent,
                            public nsIDOMAnimationEvent
{
public:
  nsDOMAnimationEvent(mozilla::dom::EventTarget* aOwner,
                      nsPresContext *aPresContext,
                      nsAnimationEvent *aEvent);
  ~nsDOMAnimationEvent();

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMEVENT
  NS_DECL_NSIDOMANIMATIONEVENT

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope)
  {
    return mozilla::dom::AnimationEventBinding::Wrap(aCx, aScope, this);
  }

  // xpidl implementation
  // GetAnimationName(nsAString& aAnimationName);

  float ElapsedTime()
  {
    return AnimationEvent()->elapsedTime;
  }

  void InitAnimationEvent(const nsAString& aType,
                          bool aCanBubble,
                          bool aCancelable,
                          const nsAString& aAnimationName,
                          float aElapsedTime,
                          mozilla::ErrorResult& aRv)
  {
    aRv = InitAnimationEvent(aType, aCanBubble, aCancelable, aAnimationName,
                             aElapsedTime);
  }
private:
  nsAnimationEvent* AnimationEvent() {
    NS_ABORT_IF_FALSE(mEvent->eventStructType == NS_ANIMATION_EVENT,
                      "unexpected struct type");
    return static_cast<nsAnimationEvent*>(mEvent);
  }
};

#endif /* !defined(nsDOMAnimationEvent_h_) */
