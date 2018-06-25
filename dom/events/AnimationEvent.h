/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_AnimationEvent_h_
#define mozilla_dom_AnimationEvent_h_

#include "mozilla/EventForwards.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/AnimationEventBinding.h"
#include "nsStringFwd.h"

namespace mozilla {
namespace dom {

class AnimationEvent : public Event
{
public:
  AnimationEvent(EventTarget* aOwner,
                 nsPresContext* aPresContext,
                 InternalAnimationEvent* aEvent);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(AnimationEvent, Event)

  static already_AddRefed<AnimationEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const AnimationEventInit& aParam,
              ErrorResult& aRv);

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return AnimationEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  void GetAnimationName(nsAString& aAnimationName);

  float ElapsedTime();

  void GetPseudoElement(nsAString& aPseudoElement);

protected:
  ~AnimationEvent() {}
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::AnimationEvent>
NS_NewDOMAnimationEvent(mozilla::dom::EventTarget* aOwner,
                        nsPresContext* aPresContext,
                        mozilla::InternalAnimationEvent* aEvent);

#endif // mozilla_dom_AnimationEvent_h_
