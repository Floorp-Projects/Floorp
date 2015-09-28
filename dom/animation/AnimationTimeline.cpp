/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationTimeline.h"
#include "mozilla/AnimationComparator.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationTimeline, mWindow,
                                      mAnimationOrder)

NS_IMPL_CYCLE_COLLECTING_ADDREF(AnimationTimeline)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AnimationTimeline)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AnimationTimeline)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

void
AnimationTimeline::GetAnimations(AnimationSequence& aAnimations)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mWindow);
  if (mWindow) {
    nsIDocument* doc = window->GetDoc();
    if (doc) {
      doc->FlushPendingNotifications(Flush_Style);
    }
  }

  aAnimations.SetCapacity(mAnimationOrder.Length());

  for (Animation* animation : mAnimationOrder) {

    // Skip animations which are no longer relevant or which have been
    // associated with another timeline. These animations will be removed
    // on the next tick.
    if (!animation->IsRelevant() || animation->GetTimeline() != this) {
      continue;
    }

    // Bug 1174575: Until we implement a suitable PseudoElement interface we
    // don't have anything to return for the |target| attribute of
    // KeyframeEffect(ReadOnly) objects that refer to pseudo-elements.
    // Rather than return some half-baked version of these objects (e.g.
    // we a null effect attribute) we simply don't provide access to animations
    // whose effect refers to a pseudo-element until we can support them
    // properly.
    Element* target;
    nsCSSPseudoElements::Type pseudoType;
    animation->GetEffect()->GetTarget(target, pseudoType);
    if (pseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement) {
      aAnimations.AppendElement(animation);
    }
  }

  // Sort animations by priority
  aAnimations.Sort(AnimationPtrComparator<nsRefPtr<Animation>>());
}

void
AnimationTimeline::NotifyAnimationUpdated(Animation& aAnimation)
{
  if (mAnimations.Contains(&aAnimation)) {
    return;
  }

  mAnimations.PutEntry(&aAnimation);
  mAnimationOrder.AppendElement(&aAnimation);
}

} // namespace dom
} // namespace mozilla
