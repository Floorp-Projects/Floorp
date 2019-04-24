/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationInfo.h"
#include "mozilla/LayerAnimationInfo.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/AnimationHelper.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/dom/Animation.h"
#include "nsIContent.h"
#include "PuppetWidget.h"

namespace mozilla {
namespace layers {

AnimationInfo::AnimationInfo() : mCompositorAnimationsId(0), mMutated(false) {}

AnimationInfo::~AnimationInfo() {}

void AnimationInfo::EnsureAnimationsId() {
  if (!mCompositorAnimationsId) {
    mCompositorAnimationsId = AnimationHelper::GetNextCompositorAnimationsId();
  }
}

Animation* AnimationInfo::AddAnimation() {
  MOZ_ASSERT(!CompositorThreadHolder::IsInCompositorThread());
  // Here generates a new id when the first animation is added and
  // this id is used to represent the animations in this layer.
  EnsureAnimationsId();

  MOZ_ASSERT(!mPendingAnimations, "should have called ClearAnimations first");

  Animation* anim = mAnimations.AppendElement();

  mMutated = true;

  return anim;
}

Animation* AnimationInfo::AddAnimationForNextTransaction() {
  MOZ_ASSERT(!CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(mPendingAnimations,
             "should have called ClearAnimationsForNextTransaction first");

  Animation* anim = mPendingAnimations->AppendElement();

  return anim;
}

void AnimationInfo::ClearAnimations() {
  mPendingAnimations = nullptr;

  if (mAnimations.IsEmpty() && mPropertyAnimationGroups.IsEmpty()) {
    return;
  }

  mAnimations.Clear();
  mPropertyAnimationGroups.Clear();

  mMutated = true;
}

void AnimationInfo::ClearAnimationsForNextTransaction() {
  // Ensure we have a non-null mPendingAnimations to mark a future clear.
  if (!mPendingAnimations) {
    mPendingAnimations = new AnimationArray;
  }

  mPendingAnimations->Clear();
}

void AnimationInfo::SetCompositorAnimations(
    const CompositorAnimations& aCompositorAnimations) {
  mCompositorAnimationsId = aCompositorAnimations.id();
  mPropertyAnimationGroups =
      AnimationHelper::ExtractAnimations(aCompositorAnimations.animations());
}

bool AnimationInfo::StartPendingAnimations(const TimeStamp& aReadyTime) {
  bool updated = false;
  for (size_t animIdx = 0, animEnd = mAnimations.Length(); animIdx < animEnd;
       animIdx++) {
    Animation& anim = mAnimations[animIdx];

    // If the animation is doing an async update of its playback rate, then we
    // want to match whatever its current time would be at *aReadyTime*.
    if (!std::isnan(anim.previousPlaybackRate()) && anim.startTime().isSome() &&
        !anim.originTime().IsNull() && !anim.isNotPlaying()) {
      TimeDuration readyTime = aReadyTime - anim.originTime();
      anim.holdTime() = dom::Animation::CurrentTimeFromTimelineTime(
          readyTime, anim.startTime().ref(), anim.previousPlaybackRate());
      // Make start time null so that we know to update it below.
      anim.startTime() = Nothing();
    }

    // If the animation is play-pending, resolve the start time.
    if (anim.startTime().isNothing() && !anim.originTime().IsNull() &&
        !anim.isNotPlaying()) {
      TimeDuration readyTime = aReadyTime - anim.originTime();
      anim.startTime() = Some(dom::Animation::StartTimeFromTimelineTime(
          readyTime, anim.holdTime(), anim.playbackRate()));
      updated = true;
    }
  }
  return updated;
}

void AnimationInfo::TransferMutatedFlagToLayer(Layer* aLayer) {
  if (mMutated) {
    aLayer->Mutated();
    mMutated = false;
  }
}

bool AnimationInfo::ApplyPendingUpdatesForThisTransaction() {
  if (mPendingAnimations) {
    mPendingAnimations->SwapElements(mAnimations);
    mPendingAnimations = nullptr;
    return true;
  }

  return false;
}

bool AnimationInfo::HasTransformAnimation() const {
  const nsCSSPropertyIDSet& transformSet =
      LayerAnimationInfo::GetCSSPropertiesFor(DisplayItemType::TYPE_TRANSFORM);
  for (uint32_t i = 0; i < mAnimations.Length(); i++) {
    if (transformSet.HasProperty(mAnimations[i].property())) {
      return true;
    }
  }
  return false;
}

/* static */
Maybe<uint64_t> AnimationInfo::GetGenerationFromFrame(
    nsIFrame* aFrame, DisplayItemType aDisplayItemKey) {
  MOZ_ASSERT(aFrame->IsPrimaryFrame() ||
             nsLayoutUtils::IsFirstContinuationOrIBSplitSibling(aFrame));

  layers::Layer* layer =
      FrameLayerBuilder::GetDedicatedLayer(aFrame, aDisplayItemKey);
  if (layer) {
    return layer->GetAnimationInfo().GetAnimationGeneration();
  }

  // In case of continuation, KeyframeEffectReadOnly uses its first frame,
  // whereas nsDisplayItem uses its last continuation, so we have to use the
  // last continuation frame here.
  if (nsLayoutUtils::IsFirstContinuationOrIBSplitSibling(aFrame)) {
    aFrame = nsLayoutUtils::LastContinuationOrIBSplitSibling(aFrame);
  }
  RefPtr<WebRenderAnimationData> animationData =
      GetWebRenderUserData<WebRenderAnimationData>(aFrame,
                                                   (uint32_t)aDisplayItemKey);
  if (animationData) {
    return animationData->GetAnimationInfo().GetAnimationGeneration();
  }

  return Nothing();
}

/* static */
void AnimationInfo::EnumerateGenerationOnFrame(
    const nsIFrame* aFrame, const nsIContent* aContent,
    const CompositorAnimatableDisplayItemTypes& aDisplayItemTypes,
    const AnimationGenerationCallback& aCallback) {
  if (XRE_IsContentProcess()) {
    if (nsIWidget* widget = nsContentUtils::WidgetForContent(aContent)) {
      // In case of child processes, we might not have yet created the layer
      // manager.  That means there is no animation generation we have, thus
      // we call the callback function with |Nothing()| for the generation.
      //
      // Note that we need to use nsContentUtils::WidgetForContent() instead of
      // BrowserChild::GetFrom(aFrame->PresShell())->WebWidget() because in the
      // case of child popup content PuppetWidget::mBrowserChild is the same as
      // the parent's one, which means mBrowserChild->IsLayersConnected() check
      // in PuppetWidget::GetLayerManager queries the parent state, it results
      // the assertion in the function failure.
      if (widget->GetOwningBrowserChild() &&
          !static_cast<widget::PuppetWidget*>(widget)->HasLayerManager()) {
        for (auto displayItem : LayerAnimationInfo::sDisplayItemTypes) {
          aCallback(Nothing(), displayItem);
        }
        return;
      }
    }
  }

  RefPtr<LayerManager> layerManager =
      nsContentUtils::LayerManagerForContent(aContent);

  if (layerManager &&
      layerManager->GetBackendType() == layers::LayersBackend::LAYERS_WR) {
    // In case of continuation, nsDisplayItem uses its last continuation, so we
    // have to use the last continuation frame here.
    if (nsLayoutUtils::IsFirstContinuationOrIBSplitSibling(aFrame)) {
      aFrame = nsLayoutUtils::LastContinuationOrIBSplitSibling(aFrame);
    }

    for (auto displayItem : LayerAnimationInfo::sDisplayItemTypes) {
      // For transform animations, the animation is on the primary frame but
      // |aFrame| is the style frame.
      const nsIFrame* frameToQuery =
          displayItem == DisplayItemType::TYPE_TRANSFORM
              ? nsLayoutUtils::GetPrimaryFrameFromStyleFrame(aFrame)
              : aFrame;
      RefPtr<WebRenderAnimationData> animationData =
          GetWebRenderUserData<WebRenderAnimationData>(frameToQuery,
                                                       (uint32_t)displayItem);
      Maybe<uint64_t> generation;
      if (animationData) {
        generation = animationData->GetAnimationInfo().GetAnimationGeneration();
      }
      aCallback(generation, displayItem);
    }
    return;
  }

  FrameLayerBuilder::EnumerateGenerationForDedicatedLayers(aFrame, aCallback);
}

}  // namespace layers
}  // namespace mozilla
