/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorAnimationStorage.h"

#include "AnimationHelper.h"
#include "mozilla/layers/CompositorThread.h"  // for CompositorThreadHolder
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/webrender/WebRenderTypes.h"  // for ToWrTransformProperty, etc
#include "nsDeviceContext.h"  // for AppUnitsPerCSSPixel
#include "nsDisplayList.h"    // for nsDisplayTransform, etc

namespace mozilla {
namespace layers {

void CompositorAnimationStorage::Clear() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  mAnimatedValues.Clear();
  mAnimations.clear();
}

void CompositorAnimationStorage::ClearById(const uint64_t& aId) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  mAnimatedValues.Remove(aId);
  mAnimations.erase(aId);
}

AnimatedValue* CompositorAnimationStorage::GetAnimatedValue(
    const uint64_t& aId) const {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mAnimatedValues.Get(aId);
}

OMTAValue CompositorAnimationStorage::GetOMTAValue(const uint64_t& aId) const {
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

void CompositorAnimationStorage::SetAnimatedValue(
    uint64_t aId, AnimatedValue* aPreviousValue,
    gfx::Matrix4x4&& aTransformInDevSpace, gfx::Matrix4x4&& aFrameTransform,
    const TransformData& aData) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!aPreviousValue) {
    MOZ_ASSERT(!mAnimatedValues.Contains(aId));
    mAnimatedValues.Put(
        aId, MakeUnique<AnimatedValue>(std::move(aTransformInDevSpace),
                                       std::move(aFrameTransform), aData));
    return;
  }
  MOZ_ASSERT(aPreviousValue->Is<AnimationTransform>());
  MOZ_ASSERT(aPreviousValue == GetAnimatedValue(aId));

  aPreviousValue->SetTransform(std::move(aTransformInDevSpace),
                               std::move(aFrameTransform), aData);
}

void CompositorAnimationStorage::SetAnimatedValue(uint64_t aId,
                                                  AnimatedValue* aPreviousValue,
                                                  nscolor aColor) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!aPreviousValue) {
    MOZ_ASSERT(!mAnimatedValues.Contains(aId));
    mAnimatedValues.Put(aId, MakeUnique<AnimatedValue>(aColor));
    return;
  }

  MOZ_ASSERT(aPreviousValue->Is<nscolor>());
  MOZ_ASSERT(aPreviousValue == GetAnimatedValue(aId));
  aPreviousValue->SetColor(aColor);
}

void CompositorAnimationStorage::SetAnimatedValue(uint64_t aId,
                                                  AnimatedValue* aPreviousValue,
                                                  float aOpacity) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!aPreviousValue) {
    MOZ_ASSERT(!mAnimatedValues.Contains(aId));
    mAnimatedValues.Put(aId, MakeUnique<AnimatedValue>(aOpacity));
    return;
  }

  MOZ_ASSERT(aPreviousValue->Is<float>());
  MOZ_ASSERT(aPreviousValue == GetAnimatedValue(aId));
  aPreviousValue->SetOpacity(aOpacity);
}

void CompositorAnimationStorage::SetAnimations(uint64_t aId,
                                               const AnimationArray& aValue) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mAnimations[aId] = std::make_unique<AnimationStorageData>(
      AnimationHelper::ExtractAnimations(aValue));
}

bool CompositorAnimationStorage::SampleAnimations(TimeStamp aPreviousFrameTime,
                                                  TimeStamp aCurrentFrameTime) {
  bool isAnimating = false;

  // Do nothing if there are no compositor animations
  if (mAnimations.empty()) {
    return isAnimating;
  }

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

        gfx::Matrix4x4 transform =
            AnimationHelper::ServoAnimationValueToMatrix4x4(
                animationValues, transformData,
                animationStorageData->mCachedMotionPath);
        gfx::Matrix4x4 frameTransform = transform;

        transform.PostScale(transformData.inheritedXScale(),
                            transformData.inheritedYScale(), 1);

        SetAnimatedValue(iter.first, previousValue, std::move(transform),
                         std::move(frameTransform), transformData);
        break;
      }
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled animated property");
    }
  }

  return isAnimating;
}

WrAnimations CompositorAnimationStorage::CollectWebRenderAnimations() const {
  WrAnimations animations;

  for (auto iter = mAnimatedValues.ConstIter(); !iter.Done(); iter.Next()) {
    AnimatedValue* value = iter.UserData();
    value->Value().match(
        [&](const AnimationTransform& aTransform) {
          animations.mTransformArrays.AppendElement(wr::ToWrTransformProperty(
              iter.Key(), aTransform.mTransformInDevSpace));
        },
        [&](const float& aOpacity) {
          animations.mOpacityArrays.AppendElement(
              wr::ToWrOpacityProperty(iter.Key(), aOpacity));
        },
        [&](const nscolor& aColor) {
          animations.mColorArrays.AppendElement(wr::ToWrColorProperty(
              iter.Key(), ToDeviceColor(gfx::sRGBColor::FromABGR(aColor))));
        });
  }

  return animations;
}

}  // namespace layers
}  // namespace mozilla
