/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationTimeline.h"
#include "mozilla/AnimationComparator.h"
#include "mozilla/dom/Animation.h"

namespace mozilla::dom {

AnimationTimeline::~AnimationTimeline() { mAnimationOrder.clear(); }

NS_IMPL_CYCLE_COLLECTION_CLASS(AnimationTimeline)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AnimationTimeline)
  tmp->mAnimationOrder.clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow, mAnimations)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AnimationTimeline)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow, mAnimations)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(AnimationTimeline)

NS_IMPL_CYCLE_COLLECTING_ADDREF(AnimationTimeline)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AnimationTimeline)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AnimationTimeline)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

void AnimationTimeline::NotifyAnimationUpdated(Animation& aAnimation) {
  if (mAnimations.EnsureInserted(&aAnimation)) {
    if (aAnimation.GetTimeline() && aAnimation.GetTimeline() != this) {
      aAnimation.GetTimeline()->RemoveAnimation(&aAnimation);
    }
    mAnimationOrder.insertBack(&aAnimation);
  }
}

void AnimationTimeline::RemoveAnimation(Animation* aAnimation) {
  MOZ_ASSERT(!aAnimation->GetTimeline() || aAnimation->GetTimeline() == this);
  if (static_cast<LinkedListElement<Animation>*>(aAnimation)->isInList()) {
    static_cast<LinkedListElement<Animation>*>(aAnimation)->remove();
  }
  mAnimations.Remove(aAnimation);
}

}  // namespace mozilla::dom
