/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationInfo.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/AnimationHelper.h"

namespace mozilla {
namespace layers {

AnimationInfo::AnimationInfo(LayerManager* aManager) :
  mManager(aManager),
  mCompositorAnimationsId(0),
  mAnimationGeneration(0),
  mMutated(false)
{
}

AnimationInfo::~AnimationInfo()
{
}

void
AnimationInfo::EnsureAnimationsId()
{
  if (!mCompositorAnimationsId) {
    mCompositorAnimationsId = AnimationHelper::GetNextCompositorAnimationsId();
  }
}

Animation*
AnimationInfo::AddAnimation()
{
  // Here generates a new id when the first animation is added and
  // this id is used to represent the animations in this layer.
  EnsureAnimationsId();

  MOZ_ASSERT(!mPendingAnimations, "should have called ClearAnimations first");

  Animation* anim = mAnimations.AppendElement();

  mMutated = true;

  return anim;
}

Animation*
AnimationInfo::AddAnimationForNextTransaction()
{
  MOZ_ASSERT(mPendingAnimations,
             "should have called ClearAnimationsForNextTransaction first");

  Animation* anim = mPendingAnimations->AppendElement();

  return anim;
}

void
AnimationInfo::ClearAnimations()
{
  mPendingAnimations = nullptr;

  if (mAnimations.IsEmpty() && mAnimationData.IsEmpty()) {
    return;
  }

  if (mManager->AsWebRenderLayerManager()) {
    mManager->AsWebRenderLayerManager()->
      AddCompositorAnimationsIdForDiscard(mCompositorAnimationsId);
  }

  mAnimations.Clear();
  mAnimationData.Clear();

  mMutated = true;
}

void
AnimationInfo::ClearAnimationsForNextTransaction()
{
  // Ensure we have a non-null mPendingAnimations to mark a future clear.
  if (!mPendingAnimations) {
    mPendingAnimations = new AnimationArray;
  }

  mPendingAnimations->Clear();
}

void
AnimationInfo::SetCompositorAnimations(const CompositorAnimations& aCompositorAnimations)
{
  mAnimations = aCompositorAnimations.animations();
  mCompositorAnimationsId = aCompositorAnimations.id();
  mAnimationData.Clear();
  AnimationHelper::SetAnimations(mAnimations,
                                 mAnimationData,
                                 mBaseAnimationStyle);
}

bool
AnimationInfo::StartPendingAnimations(const TimeStamp& aReadyTime)
{
  bool updated = false;
  for (size_t animIdx = 0, animEnd = mAnimations.Length();
       animIdx < animEnd; animIdx++) {
    Animation& anim = mAnimations[animIdx];

    // If the animation is play-pending, resolve the start time.
    // This mirrors the calculation in Animation::StartTimeFromReadyTime.
    if (anim.startTime().type() == MaybeTimeDuration::Tnull_t &&
        !anim.originTime().IsNull() &&
        !anim.isNotPlaying()) {
      TimeDuration readyTime = aReadyTime - anim.originTime();
      anim.startTime() =
        anim.playbackRate() == 0
        ? readyTime
        : readyTime - anim.holdTime().MultDouble(1.0 /
                                                 anim.playbackRate());
      updated = true;
    }
  }
  return updated;
}

void
AnimationInfo::TransferMutatedFlagToLayer(Layer* aLayer)
{
  if (mMutated) {
    aLayer->Mutated();
    mMutated = false;
  }
}

bool
AnimationInfo::ApplyPendingUpdatesForThisTransaction()
{
  if (mPendingAnimations) {
    mPendingAnimations->SwapElements(mAnimations);
    mPendingAnimations = nullptr;
    return true;
  }

  return false;
}

bool
AnimationInfo::HasOpacityAnimation() const
{
  for (uint32_t i = 0; i < mAnimations.Length(); i++) {
    if (mAnimations[i].property() == eCSSProperty_opacity) {
      return true;
    }
  }
  return false;
}

bool
AnimationInfo::HasTransformAnimation() const
{
  for (uint32_t i = 0; i < mAnimations.Length(); i++) {
    if (mAnimations[i].property() == eCSSProperty_transform) {
      return true;
    }
  }
  return false;
}

} // namespace layers
} // namespace mozilla
