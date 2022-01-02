/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorAnimationStorage.h"

#include "AnimationHelper.h"
#include "mozilla/gfx/MatrixFwd.h"
#include "mozilla/layers/APZSampler.h"              // for APZSampler
#include "mozilla/layers/CompositorBridgeParent.h"  // for CompositorBridgeParent
#include "mozilla/layers/CompositorThread.h"  // for CompositorThreadHolder
#include "mozilla/layers/OMTAController.h"    // for OMTAController
#include "mozilla/ScopeExit.h"
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/webrender/WebRenderTypes.h"  // for ToWrTransformProperty, etc
#include "nsDeviceContext.h"                   // for AppUnitsPerCSSPixel
#include "nsDisplayList.h"                     // for nsDisplayTransform, etc
#include "nsLayoutUtils.h"
#include "TreeTraversal.h"  // for ForEachNode, BreadthFirstSearch

namespace mozilla {
namespace layers {

using gfx::Matrix4x4;

void CompositorAnimationStorage::Clear() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  // This function should only be called via the non Webrender version of
  // SampleAnimations.
  mLock.AssertCurrentThreadOwns();

  mAnimatedValues.Clear();
  mAnimations.clear();
}

void CompositorAnimationStorage::ClearById(const uint64_t& aId) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MutexAutoLock lock(mLock);

  mAnimatedValues.Remove(aId);
  mAnimations.erase(aId);
}

bool CompositorAnimationStorage::HasAnimations() const {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MutexAutoLock lock(mLock);

  return !mAnimations.empty();
}

AnimatedValue* CompositorAnimationStorage::GetAnimatedValue(
    const uint64_t& aId) const {
  mLock.AssertCurrentThreadOwns();

  return mAnimatedValues.Get(aId);
}

OMTAValue CompositorAnimationStorage::GetOMTAValue(const uint64_t& aId) const {
  MutexAutoLock lock(mLock);

  OMTAValue omtaValue = mozilla::null_t();
  auto animatedValue = GetAnimatedValue(aId);
  if (!animatedValue) {
    return omtaValue;
  }

  animatedValue->Value().match(
      [&](const AnimationTransform& aTransform) {
        gfx::Matrix4x4 transform = aTransform.mFrameTransform;
        const TransformData& data = aTransform.mData;
        float scale = data.appUnitsPerDevPixel();
        gfx::Point3D transformOrigin = data.transformOrigin();

        // Undo the rebasing applied by
        // nsDisplayTransform::GetResultingTransformMatrixInternal
        transform.ChangeBasis(-transformOrigin);

        // Convert to CSS pixels (this undoes the operations performed by
        // nsStyleTransformMatrix::ProcessTranslatePart which is called from
        // nsDisplayTransform::GetResultingTransformMatrix)
        double devPerCss = double(scale) / double(AppUnitsPerCSSPixel());
        transform._41 *= devPerCss;
        transform._42 *= devPerCss;
        transform._43 *= devPerCss;
        omtaValue = transform;
      },
      [&](const float& aOpacity) { omtaValue = aOpacity; },
      [&](const nscolor& aColor) { omtaValue = aColor; });
  return omtaValue;
}

void CompositorAnimationStorage::SetAnimatedValueForWebRender(
    uint64_t aId, AnimatedValue* aPreviousValue,
    const gfx::Matrix4x4& aFrameTransform, const TransformData& aData) {
  mLock.AssertCurrentThreadOwns();

  if (!aPreviousValue) {
    MOZ_ASSERT(!mAnimatedValues.Contains(aId));
    mAnimatedValues.InsertOrUpdate(
        aId,
        MakeUnique<AnimatedValue>(gfx::Matrix4x4(), aFrameTransform, aData));
    return;
  }
  MOZ_ASSERT(aPreviousValue->Is<AnimationTransform>());
  MOZ_ASSERT(aPreviousValue == GetAnimatedValue(aId));

  aPreviousValue->SetTransformForWebRender(aFrameTransform, aData);
}

void CompositorAnimationStorage::SetAnimatedValue(
    uint64_t aId, AnimatedValue* aPreviousValue,
    const gfx::Matrix4x4& aTransformInDevSpace,
    const gfx::Matrix4x4& aFrameTransform, const TransformData& aData) {
  mLock.AssertCurrentThreadOwns();

  if (!aPreviousValue) {
    MOZ_ASSERT(!mAnimatedValues.Contains(aId));
    mAnimatedValues.InsertOrUpdate(
        aId, MakeUnique<AnimatedValue>(aTransformInDevSpace, aFrameTransform,
                                       aData));
    return;
  }
  MOZ_ASSERT(aPreviousValue->Is<AnimationTransform>());
  MOZ_ASSERT(aPreviousValue == GetAnimatedValue(aId));

  aPreviousValue->SetTransform(aTransformInDevSpace, aFrameTransform, aData);
}

void CompositorAnimationStorage::SetAnimatedValue(uint64_t aId,
                                                  AnimatedValue* aPreviousValue,
                                                  nscolor aColor) {
  mLock.AssertCurrentThreadOwns();

  if (!aPreviousValue) {
    MOZ_ASSERT(!mAnimatedValues.Contains(aId));
    mAnimatedValues.InsertOrUpdate(aId, MakeUnique<AnimatedValue>(aColor));
    return;
  }

  MOZ_ASSERT(aPreviousValue->Is<nscolor>());
  MOZ_ASSERT(aPreviousValue == GetAnimatedValue(aId));
  aPreviousValue->SetColor(aColor);
}

void CompositorAnimationStorage::SetAnimatedValue(uint64_t aId,
                                                  AnimatedValue* aPreviousValue,
                                                  float aOpacity) {
  mLock.AssertCurrentThreadOwns();

  if (!aPreviousValue) {
    MOZ_ASSERT(!mAnimatedValues.Contains(aId));
    mAnimatedValues.InsertOrUpdate(aId, MakeUnique<AnimatedValue>(aOpacity));
    return;
  }

  MOZ_ASSERT(aPreviousValue->Is<float>());
  MOZ_ASSERT(aPreviousValue == GetAnimatedValue(aId));
  aPreviousValue->SetOpacity(aOpacity);
}

void CompositorAnimationStorage::SetAnimations(uint64_t aId,
                                               const LayersId& aLayersId,
                                               const AnimationArray& aValue) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MutexAutoLock lock(mLock);

  mAnimations[aId] = std::make_unique<AnimationStorageData>(
      AnimationHelper::ExtractAnimations(aLayersId, aValue));

  // If there is the last animated value, then we need to store the id to remove
  // the value if the new animation doesn't produce any animated data (i.e. in
  // the delay phase) when we sample this new animation.
  if (mAnimatedValues.Contains(aId)) {
    mNewAnimations.insert(aId);
  }
}

// Returns clip rect in the scroll frame's coordinate space.
static ParentLayerRect GetClipRectForPartialPrerender(
    const LayersId aLayersId, const PartialPrerenderData& aPartialPrerenderData,
    const RefPtr<APZSampler>& aSampler) {
  if (aSampler &&
      aPartialPrerenderData.scrollId() != ScrollableLayerGuid::NULL_SCROLL_ID) {
    return aSampler->GetCompositionBounds(aLayersId,
                                          aPartialPrerenderData.scrollId());
  }

  return aPartialPrerenderData.clipRect();
}

bool CompositorAnimationStorage::SampleAnimations(
    const OMTAController* aOMTAController, TimeStamp aPreviousFrameTime,
    TimeStamp aCurrentFrameTime) {
  MutexAutoLock lock(mLock);

  bool isAnimating = false;
  auto cleanup = MakeScopeExit([&] { mNewAnimations.clear(); });

  // Do nothing if there are no compositor animations
  if (mAnimations.empty()) {
    return isAnimating;
  }

  std::unordered_map<LayersId, nsTArray<uint64_t>, LayersId::HashFn> janked;

  RefPtr<APZSampler> apzSampler = mCompositorBridge->GetAPZSampler();

  for (const auto& iter : mAnimations) {
    const auto& animationStorageData = iter.second;
    if (animationStorageData->mAnimation.IsEmpty()) {
      continue;
    }

    isAnimating = true;
    AutoTArray<RefPtr<RawServoAnimationValue>, 1> animationValues;
    AnimatedValue* previousValue = GetAnimatedValue(iter.first);
    AnimationHelper::SampleResult sampleResult =
        AnimationHelper::SampleAnimationForEachNode(
            aPreviousFrameTime, aCurrentFrameTime, previousValue,
            animationStorageData->mAnimation, animationValues);

    if (sampleResult != AnimationHelper::SampleResult::Sampled) {
      if (mNewAnimations.find(iter.first) != mNewAnimations.end()) {
        mAnimatedValues.Remove(iter.first);
      }
      continue;
    }

    const PropertyAnimationGroup& lastPropertyAnimationGroup =
        animationStorageData->mAnimation.LastElement();

    // Store the AnimatedValue
    switch (lastPropertyAnimationGroup.mProperty) {
      case eCSSProperty_background_color: {
        SetAnimatedValue(iter.first, previousValue,
                         Servo_AnimationValue_GetColor(animationValues[0],
                                                       NS_RGBA(0, 0, 0, 0)));
        break;
      }
      case eCSSProperty_opacity: {
        MOZ_ASSERT(animationValues.Length() == 1);
        SetAnimatedValue(iter.first, previousValue,
                         Servo_AnimationValue_GetOpacity(animationValues[0]));
        break;
      }
      case eCSSProperty_rotate:
      case eCSSProperty_scale:
      case eCSSProperty_translate:
      case eCSSProperty_transform:
      case eCSSProperty_offset_path:
      case eCSSProperty_offset_distance:
      case eCSSProperty_offset_rotate:
      case eCSSProperty_offset_anchor: {
        MOZ_ASSERT(animationStorageData->mTransformData);

        const TransformData& transformData =
            *animationStorageData->mTransformData;
        MOZ_ASSERT(transformData.origin() == nsPoint());

        gfx::Matrix4x4 frameTransform =
            AnimationHelper::ServoAnimationValueToMatrix4x4(
                animationValues, transformData,
                animationStorageData->mCachedMotionPath);

        if (const Maybe<PartialPrerenderData>& partialPrerenderData =
                transformData.partialPrerenderData()) {
          gfx::Matrix4x4 transform = frameTransform;
          transform.PostTranslate(
              partialPrerenderData->position().ToUnknownPoint());

          gfx::Matrix4x4 transformInClip =
              partialPrerenderData->transformInClip();
          if (apzSampler && partialPrerenderData->scrollId() !=
                                ScrollableLayerGuid::NULL_SCROLL_ID) {
            AsyncTransform asyncTransform =
                apzSampler->GetCurrentAsyncTransform(
                    animationStorageData->mLayersId,
                    partialPrerenderData->scrollId(), LayoutAndVisual);
            transformInClip.PostTranslate(
                asyncTransform.mTranslation.ToUnknownPoint());
          }
          transformInClip = transform * transformInClip;

          ParentLayerRect clipRect =
              GetClipRectForPartialPrerender(animationStorageData->mLayersId,
                                             *partialPrerenderData, apzSampler);
          if (AnimationHelper::ShouldBeJank(
                  partialPrerenderData->rect(),
                  partialPrerenderData->overflowedSides(), transformInClip,
                  clipRect)) {
            if (previousValue) {
              frameTransform = previousValue->Transform().mFrameTransform;
            }
            janked[animationStorageData->mLayersId].AppendElement(iter.first);
          }
        }

        SetAnimatedValueForWebRender(iter.first, previousValue, frameTransform,
                                     transformData);
        break;
      }
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled animated property");
    }
  }

  if (!janked.empty() && aOMTAController) {
    aOMTAController->NotifyJankedAnimations(std::move(janked));
  }

  return isAnimating;
}

WrAnimations CompositorAnimationStorage::CollectWebRenderAnimations() const {
  MutexAutoLock lock(mLock);

  WrAnimations animations;

  for (const auto& animatedValueEntry : mAnimatedValues) {
    AnimatedValue* value = animatedValueEntry.GetWeak();
    value->Value().match(
        [&](const AnimationTransform& aTransform) {
          animations.mTransformArrays.AppendElement(wr::ToWrTransformProperty(
              animatedValueEntry.GetKey(), aTransform.mFrameTransform));
        },
        [&](const float& aOpacity) {
          animations.mOpacityArrays.AppendElement(
              wr::ToWrOpacityProperty(animatedValueEntry.GetKey(), aOpacity));
        },
        [&](const nscolor& aColor) {
          animations.mColorArrays.AppendElement(wr::ToWrColorProperty(
              animatedValueEntry.GetKey(),
              ToDeviceColor(gfx::sRGBColor::FromABGR(aColor))));
        });
  }

  return animations;
}

}  // namespace layers
}  // namespace mozilla
