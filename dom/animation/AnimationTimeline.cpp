/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationTimeline.h"
#include "mozilla/AnimationComparator.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationTimeline, mWindow, mAnimations)

NS_IMPL_CYCLE_COLLECTING_ADDREF(AnimationTimeline)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AnimationTimeline)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AnimationTimeline)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

namespace {
  struct AddAnimationParams {
    AnimationTimeline::AnimationSequence& mSequence;
#ifdef DEBUG
    // This is only used for a pointer-equality assertion
    AnimationTimeline* mTimeline;
#endif
  };
}

static PLDHashOperator
AppendAnimationToSequence(nsRefPtrHashKey<dom::Animation>* aKey,
                          void* aParams)
{
  Animation* animation = aKey->GetKey();
  AddAnimationParams* params = static_cast<AddAnimationParams*>(aParams);

  MOZ_ASSERT(animation->IsRelevant(),
             "Animations registered with a timeline should be relevant");
  MOZ_ASSERT(animation->GetTimeline() == params->mTimeline,
             "Animation should refer to this timeline");

  params->mSequence.AppendElement(animation);

  return PL_DHASH_NEXT;
}

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

#ifdef DEBUG
  AddAnimationParams params{ aAnimations, this };
#else
  AddAnimationParams params{ aAnimations };
#endif
  mAnimations.EnumerateEntries(AppendAnimationToSequence, &params);

  // Sort animations by priority
  aAnimations.Sort(AnimationPtrComparator<nsRefPtr<Animation>>());
}

void
AnimationTimeline::AddAnimation(Animation& aAnimation)
{
  mAnimations.PutEntry(&aAnimation);
}

void
AnimationTimeline::RemoveAnimation(Animation& aAnimation)
{
  mAnimations.RemoveEntry(&aAnimation);
}

} // namespace dom
} // namespace mozilla
