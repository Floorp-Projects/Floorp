/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationInfo.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/AnimationHelper.h"
#include "mozilla/dom/Animation.h"

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

    // If the animation is doing an async update of its playback rate, then we
    // want to match whatever its current time would be at *aReadyTime*.
    if (!std::isnan(anim.previousPlaybackRate()) &&
        anim.startTime().type() == MaybeTimeDuration::TTimeDuration &&
        !anim.originTime().IsNull() && !anim.isNotPlaying()) {
      TimeDuration readyTime = aReadyTime - anim.originTime();
      anim.holdTime() = dom::Animation::CurrentTimeFromTimelineTime(
        readyTime,
        anim.startTime().get_TimeDuration(),
        anim.previousPlaybackRate());
      // Make start time null so that we know to update it below.
      anim.startTime() = null_t();
    }

    // If the animation is play-pending, resolve the start time.
    if (anim.startTime().type() == MaybeTimeDuration::Tnull_t &&
        !anim.originTime().IsNull() &&
        !anim.isNotPlaying()) {
      TimeDuration readyTime = aReadyTime - anim.originTime();
      anim.startTime() = dom::Animation::StartTimeFromTimelineTime(
        readyTime, anim.holdTime(), anim.playbackRate());
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

/* static */ Maybe<uint64_t>
AnimationInfo::GetGenerationFromFrame(nsIFrame* aFrame,
                                      DisplayItemType aDisplayItemKey)
{
  MOZ_ASSERT(aFrame->IsPrimaryFrame() ||
             nsLayoutUtils::IsFirstContinuationOrIBSplitSibling(aFrame));

  layers::Layer* layer =
    FrameLayerBuilder::GetDedicatedLayer(aFrame, aDisplayItemKey);
  if (layer) {
    return Some(layer->GetAnimationInfo().GetAnimationGeneration());
  }

  // In case of continuation, KeyframeEffectReadOnly uses its first frame,
  // whereas nsDisplayItem uses its last continuation, so we have to use the
  // last continuation frame here.
  if (nsLayoutUtils::IsFirstContinuationOrIBSplitSibling(aFrame)) {
    aFrame = nsLayoutUtils::LastContinuationOrIBSplitSibling(aFrame);
  }
  RefPtr<WebRenderAnimationData> animationData =
      GetWebRenderUserData<WebRenderAnimationData>(aFrame, (uint32_t)aDisplayItemKey);
  if (animationData) {
    return Some(animationData->GetAnimationInfo().GetAnimationGeneration());
  }

  return Nothing();
}

} // namespace layers
} // namespace mozilla
