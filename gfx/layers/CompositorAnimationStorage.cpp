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
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/webrender/WebRenderTypes.h"  // for ToWrTransformProperty, etc
#include "nsDeviceContext.h"                   // for AppUnitsPerCSSPixel
#include "nsDisplayList.h"                     // for nsDisplayTransform, etc
#include "nsLayoutUtils.h"
#include "TreeTraversal.h"  // for ForEachNode, BreadthFirstSearch

namespace geckoprofiler::markers {

using namespace mozilla;

struct CompositorAnimationMarker {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("CompositorAnimation");
  }
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   uint64_t aId, nsCSSPropertyID aProperty) {
    aWriter.IntProperty("pid", int64_t(aId >> 32));
    aWriter.IntProperty("id", int64_t(aId & 0xffffffff));
    aWriter.StringProperty("property", nsCSSProps::GetStringValue(aProperty));
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
    schema.AddKeyLabelFormat("pid", "Process Id", MS::Format::Integer);
    schema.AddKeyLabelFormat("id", "Animation Id", MS::Format::Integer);
    schema.AddKeyLabelFormat("property", "Animated Property",
                             MS::Format::String);
    schema.SetTableLabel("{marker.name} - {marker.data.property}");
    return schema;
  }
};

}  // namespace geckoprofiler::markers

namespace mozilla {
namespace layers {

using gfx::Matrix4x4;

void CompositorAnimationStorage::ClearById(const uint64_t& aId) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MutexAutoLock lock(mLock);

  const auto& animationStorageData = mAnimations[aId];
  if (animationStorageData) {
    PROFILER_MARKER("ClearAnimation", GRAPHICS,
                    MarkerInnerWindowId(mCompositorBridge->GetInnerWindowId()),
                    CompositorAnimationMarker, aId,
                    animationStorageData->mAnimation.LastElement().mProperty);
  }

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

void CompositorAnimationStorage::SetAnimatedValue(
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

  aPreviousValue->SetTransform(aFrameTransform, aData);
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

  PROFILER_MARKER("SetAnimation", GRAPHICS,
                  MarkerInnerWindowId(mCompositorBridge->GetInnerWindowId()),
                  CompositorAnimationMarker, aId,
                  mAnimations[aId]->mAnimation.LastElement().mProperty);

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
    const RefPtr<APZSampler>& aSampler, const MutexAutoLock& aProofOfMapLock) {
  if (aSampler &&
      aPartialPrerenderData.scrollId() != ScrollableLayerGuid::NULL_SCROLL_ID) {
    return aSampler->GetCompositionBounds(
        aLayersId, aPartialPrerenderData.scrollId(), aProofOfMapLock);
  }

  return aPartialPrerenderData.clipRect();
}

void CompositorAnimationStorage::StoreAnimatedValue(
    nsCSSPropertyID aProperty, uint64_t aId,
    const std::unique_ptr<AnimationStorageData>& aAnimationStorageData,
    const AutoTArray<RefPtr<StyleAnimationValue>, 1>& aAnimationValues,
    const MutexAutoLock& aProofOfMapLock, const RefPtr<APZSampler>& aApzSampler,
    AnimatedValue* aAnimatedValueEntry,
    JankedAnimationMap& aJankedAnimationMap) {
  switch (aProperty) {
    case eCSSProperty_background_color: {
      SetAnimatedValue(aId, aAnimatedValueEntry,
                       Servo_AnimationValue_GetColor(aAnimationValues[0],
                                                     NS_RGBA(0, 0, 0, 0)));
      break;
    }
    case eCSSProperty_opacity: {
      MOZ_ASSERT(aAnimationValues.Length() == 1);
      SetAnimatedValue(aId, aAnimatedValueEntry,
                       Servo_AnimationValue_GetOpacity(aAnimationValues[0]));
      break;
    }
    case eCSSProperty_rotate:
    case eCSSProperty_scale:
    case eCSSProperty_translate:
    case eCSSProperty_transform:
    case eCSSProperty_offset_path:
    case eCSSProperty_offset_distance:
    case eCSSProperty_offset_rotate:
    case eCSSProperty_offset_anchor:
    case eCSSProperty_offset_position: {
      MOZ_ASSERT(aAnimationStorageData->mTransformData);

      const TransformData& transformData =
          *aAnimationStorageData->mTransformData;
      MOZ_ASSERT(transformData.origin() == nsPoint());

      gfx::Matrix4x4 frameTransform =
          AnimationHelper::ServoAnimationValueToMatrix4x4(
              aAnimationValues, transformData,
              aAnimationStorageData->mCachedMotionPath);

      if (const Maybe<PartialPrerenderData>& partialPrerenderData =
              transformData.partialPrerenderData()) {
        gfx::Matrix4x4 transform = frameTransform;
        transform.PostTranslate(
            partialPrerenderData->position().ToUnknownPoint());

        gfx::Matrix4x4 transformInClip =
            partialPrerenderData->transformInClip();
        if (aApzSampler && partialPrerenderData->scrollId() !=
                               ScrollableLayerGuid::NULL_SCROLL_ID) {
          AsyncTransform asyncTransform = aApzSampler->GetCurrentAsyncTransform(
              aAnimationStorageData->mLayersId,
              partialPrerenderData->scrollId(), LayoutAndVisual,
              aProofOfMapLock);
          transformInClip.PostTranslate(
              asyncTransform.mTranslation.ToUnknownPoint());
        }
        transformInClip = transform * transformInClip;

        ParentLayerRect clipRect = GetClipRectForPartialPrerender(
            aAnimationStorageData->mLayersId, *partialPrerenderData,
            aApzSampler, aProofOfMapLock);
        if (AnimationHelper::ShouldBeJank(
                partialPrerenderData->rect(),
                partialPrerenderData->overflowedSides(), transformInClip,
                clipRect)) {
          if (aAnimatedValueEntry) {
            frameTransform = aAnimatedValueEntry->Transform().mFrameTransform;
          }
          aJankedAnimationMap[aAnimationStorageData->mLayersId].AppendElement(
              aId);
        }
      }

      SetAnimatedValue(aId, aAnimatedValueEntry, frameTransform, transformData);
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled animated property");
  }
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

  JankedAnimationMap janked;
  RefPtr<APZSampler> apzSampler = mCompositorBridge->GetAPZSampler();

  auto callback = [&](const MutexAutoLock& aProofOfMapLock) {
    for (const auto& iter : mAnimations) {
      const auto& animationStorageData = iter.second;
      if (animationStorageData->mAnimation.IsEmpty()) {
        continue;
      }

      const nsCSSPropertyID lastPropertyAnimationGroupProperty =
          animationStorageData->mAnimation.LastElement().mProperty;
      isAnimating = true;
      AutoTArray<RefPtr<StyleAnimationValue>, 1> animationValues;
      AnimatedValue* previousValue = GetAnimatedValue(iter.first);
      AnimationHelper::SampleResult sampleResult =
          AnimationHelper::SampleAnimationForEachNode(
              apzSampler, animationStorageData->mLayersId, aProofOfMapLock,
              aPreviousFrameTime, aCurrentFrameTime, previousValue,
              animationStorageData->mAnimation, animationValues);

      PROFILER_MARKER(
          "SampleAnimation", GRAPHICS,
          MarkerOptions(
              MarkerThreadId(CompositorThreadHolder::GetThreadId()),
              MarkerInnerWindowId(mCompositorBridge->GetInnerWindowId())),
          CompositorAnimationMarker, iter.first,
          lastPropertyAnimationGroupProperty);

      if (!sampleResult.IsSampled()) {
        // Note: Checking new animations first. If new animations arrive and we
        // scroll back to delay phase in the meantime for scroll-driven
        // animations, removing the previous animated value is still the
        // preferable way because the newly animation case would probably more
        // often than the scroll timeline. Besides, we expect the display items
        // get sync with main thread at this moment, so dropping the old
        // animation sampled result is more suitable.
        // FIXME: Bug 1791884: We might have to revisit this to make sure we
        // respect animation composition order.
        if (mNewAnimations.find(iter.first) != mNewAnimations.end()) {
          mAnimatedValues.Remove(iter.first);
        } else if (sampleResult.mReason ==
                   AnimationHelper::SampleResult::Reason::ScrollToDelayPhase) {
          // For the scroll-driven animations, its animation effect phases may
          // be changed between the active phase and the before/after phase.
          // Basically we don't produce any sampled animation value for
          // before/after phase (if we don't have fills). In this case, we have
          // to make sure the animations are not applied on the compositor.
          // Removing the previous animated value is not enough because the
          // display item in webrender may be out-of-date. Therefore, we should
          // not just skip these animation values. Instead, storing their base
          // styles (which are in |animationValues| already) to simulate these
          // delayed animations.
          //
          // There are two possible situations:
          // 1. If there is just a new animation arrived but there is no sampled
          //    animation value when we go from active phase, we remove the
          //    previous AnimatedValue. This is done in the above condition.
          // 2. If |animation| is not replaced by a new arrived one, we detect
          //    it in SampleAnimationForProperty(), which sets
          //    ScrollToDelayPhase if it goes from the active phase to the
          //    before/after phase.
          //
          // For the 2nd case, we store the base styles until we have some other
          // new sampled results or the new animations arrived (i.e. case 1).
          StoreAnimatedValue(lastPropertyAnimationGroupProperty, iter.first,
                             animationStorageData, animationValues,
                             aProofOfMapLock, apzSampler, previousValue,
                             janked);
        }
        continue;
      }

      // Store the normal sampled result.
      StoreAnimatedValue(lastPropertyAnimationGroupProperty, iter.first,
                         animationStorageData, animationValues, aProofOfMapLock,
                         apzSampler, previousValue, janked);
    }
  };

  if (apzSampler) {
    // Hold APZCTreeManager::mMapLock for all the animations inside this
    // CompositorBridgeParent. This ensures that APZ cannot process a
    // transaction on the updater thread in between sampling different
    // animations.
    apzSampler->CallWithMapLock(callback);
  } else {
    // A fallback way if we don't have |apzSampler|. We don't care about
    // APZCTreeManager::mMapLock in this case because we don't use any APZ
    // interface.
    mozilla::Mutex dummy("DummyAPZMapLock");
    MutexAutoLock lock(dummy);
    callback(lock);
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
