/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/LayerManager.h"

#include <stdint.h>          // for uint64_t, uint8_t
#include <stdlib.h>          // for abort
#include <algorithm>         // for copy, copy_backward
#include <utility>           // for move, forward
#include "FrameMetrics.h"    // for FrameMetrics
#include "ImageContainer.h"  // for ImageContainer, ImageContainer::Mode
#include "LayerUserData.h"   // for LayerUserData
#include "Layers.h"          // for RecordCompositionPayloadsPresented, Layer
#include "TreeTraversal.h"   // for ForwardIterator, BreadthFirstSearch
#include "gfxPlatform.h"     // for gfxPlatform
#include "mozilla/AlreadyAddRefed.h"  // for already_AddRefed
#include "mozilla/ArrayIterator.h"    // for ArrayIterator
#include "mozilla/Assertions.h"  // for AssertionConditionType, MOZ_ASSERT, MOZ_ASSERT_HELPER1
#include "mozilla/EffectSet.h"  // for EffectSet
#include "mozilla/Logging.h"    // for LazyLogModule, LogModule (ptr only)
#include "mozilla/RefPtr.h"  // for RefPtr, getter_AddRefs, RefPtrGetterAddRefs
#include "mozilla/StaticPrefs_layers.h"  // for layers_componentalpha_enabled_AtStartup_DoNotUseDirectly
#include "mozilla/UniquePtr.h"            // for UniquePtr
#include "mozilla/dom/Animation.h"        // for Animation
#include "mozilla/dom/AnimationEffect.h"  // for AnimationEffect
#include "mozilla/gfx/Point.h"            // for IntSize
#include "mozilla/gfx/Types.h"            // for SurfaceFormat, gfx
#include "mozilla/gfx/UserData.h"  // for UserData, UserDataKey (ptr only)
#include "mozilla/layers/LayerMetricsWrapper.h"  // for LayerMetricsWrapper
#include "mozilla/layers/LayersTypes.h"          // for CompositionPayload
#include "mozilla/layers/PersistentBufferProvider.h"  // for PersistentBufferProviderBasic, PersistentBufferProvider (ptr only)
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ScrollableLayerGuid, ScrollableLayerGuid::NULL_SCROLL_ID, ScrollableLayerGu...
#include "nsHashKeys.h"                          // for nsUint64HashKey
#include "nsRefPtrHashtable.h"                   // for nsRefPtrHashtable
#include "nsTArray.h"                            // for nsTArray

uint8_t gLayerManagerLayerBuilder;

// Undo the damage done by mozzconf.h
#undef compress
#include "mozilla/Compression.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;
using namespace mozilla::Compression;

//--------------------------------------------------
// LayerManager

LayerManager::LayerManager()
    : mDestroyed(false),
      mSnapEffectiveTransforms(true),
      mId(0),
      mInTransaction(false),
      mContainsSVG(false),
      mPaintedPixelCount(0) {}

LayerManager::~LayerManager() = default;

void LayerManager::Destroy() {
  mDestroyed = true;
  mUserData.Destroy();
  mRoot = nullptr;
  mPartialPrerenderedAnimations.Clear();
}

/* static */ mozilla::LogModule* LayerManager::GetLog() {
  static LazyLogModule sLog("Layers");
  return sLog;
}

ScrollableLayerGuid::ViewID LayerManager::GetRootScrollableLayerId() {
  if (!mRoot) {
    return ScrollableLayerGuid::NULL_SCROLL_ID;
  }

  LayerMetricsWrapper layerMetricsRoot = LayerMetricsWrapper(mRoot);

  LayerMetricsWrapper rootScrollableLayerMetrics =
      BreadthFirstSearch<ForwardIterator>(
          layerMetricsRoot, [](LayerMetricsWrapper aLayerMetrics) {
            return aLayerMetrics.Metrics().IsScrollable();
          });

  return rootScrollableLayerMetrics.IsValid()
             ? rootScrollableLayerMetrics.Metrics().GetScrollId()
             : ScrollableLayerGuid::NULL_SCROLL_ID;
}

LayerMetricsWrapper LayerManager::GetRootContentLayer() {
  if (!mRoot) {
    return LayerMetricsWrapper();
  }

  LayerMetricsWrapper root(mRoot);

  return BreadthFirstSearch<ForwardIterator>(
      root, [](LayerMetricsWrapper aLayerMetrics) {
        return aLayerMetrics.Metrics().IsRootContent();
      });
}

already_AddRefed<DrawTarget> LayerManager::CreateOptimalDrawTarget(
    const gfx::IntSize& aSize, SurfaceFormat aFormat) {
  return gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(aSize,
                                                                      aFormat);
}

already_AddRefed<DrawTarget> LayerManager::CreateOptimalMaskDrawTarget(
    const gfx::IntSize& aSize) {
  return CreateOptimalDrawTarget(aSize, SurfaceFormat::A8);
}

already_AddRefed<DrawTarget> LayerManager::CreateDrawTarget(
    const IntSize& aSize, SurfaceFormat aFormat) {
  return gfxPlatform::GetPlatform()->CreateOffscreenCanvasDrawTarget(aSize,
                                                                     aFormat);
}

already_AddRefed<PersistentBufferProvider>
LayerManager::CreatePersistentBufferProvider(
    const mozilla::gfx::IntSize& aSize, mozilla::gfx::SurfaceFormat aFormat) {
  RefPtr<PersistentBufferProviderBasic> bufferProvider;
  // If we are using remote canvas we don't want to use acceleration in
  // non-remote layer managers, so we always use the fallback software one.
  if (!gfxPlatform::UseRemoteCanvas() ||
      !gfxPlatform::IsBackendAccelerated(
          gfxPlatform::GetPlatform()->GetPreferredCanvasBackend())) {
    bufferProvider = PersistentBufferProviderBasic::Create(
        aSize, aFormat,
        gfxPlatform::GetPlatform()->GetPreferredCanvasBackend());
  }

  if (!bufferProvider) {
    bufferProvider = PersistentBufferProviderBasic::Create(
        aSize, aFormat, gfxPlatform::GetPlatform()->GetFallbackCanvasBackend());
  }

  return bufferProvider.forget();
}

already_AddRefed<ImageContainer> LayerManager::CreateImageContainer(
    ImageContainer::Mode flag) {
  RefPtr<ImageContainer> container = new ImageContainer(flag);
  return container.forget();
}

bool LayerManager::LayersComponentAlphaEnabled() {
  // If MOZ_GFX_OPTIMIZE_MOBILE is defined, we force component alpha off
  // and ignore the preference.
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  return false;
#else
  return StaticPrefs::
      layers_componentalpha_enabled_AtStartup_DoNotUseDirectly();
#endif
}

bool LayerManager::AreComponentAlphaLayersEnabled() {
  return LayerManager::LayersComponentAlphaEnabled();
}

/*static*/
void LayerManager::LayerUserDataDestroy(void* data) {
  delete static_cast<LayerUserData*>(data);
}

UniquePtr<LayerUserData> LayerManager::RemoveUserData(void* aKey) {
  UniquePtr<LayerUserData> d(static_cast<LayerUserData*>(
      mUserData.Remove(static_cast<gfx::UserDataKey*>(aKey))));
  return d;
}

void LayerManager::PayloadPresented(const TimeStamp& aTimeStamp) {
  RecordCompositionPayloadsPresented(aTimeStamp, mPayload);
}

void LayerManager::AddPartialPrerenderedAnimation(
    uint64_t aCompositorAnimationId, dom::Animation* aAnimation) {
  mPartialPrerenderedAnimations.InsertOrUpdate(aCompositorAnimationId,
                                               RefPtr{aAnimation});
  aAnimation->SetPartialPrerendered(aCompositorAnimationId);
}

void LayerManager::RemovePartialPrerenderedAnimation(
    uint64_t aCompositorAnimationId, dom::Animation* aAnimation) {
  MOZ_ASSERT(aAnimation);
#ifdef DEBUG
  RefPtr<dom::Animation> animation;
  if (mPartialPrerenderedAnimations.Remove(aCompositorAnimationId,
                                           getter_AddRefs(animation)) &&
      // It may be possible that either animation's effect has already been
      // nulled out via Animation::SetEffect() so ignore such cases.
      aAnimation->GetEffect() && aAnimation->GetEffect()->AsKeyframeEffect() &&
      animation->GetEffect() && animation->GetEffect()->AsKeyframeEffect()) {
    MOZ_ASSERT(EffectSet::GetEffectSetForEffect(
                   aAnimation->GetEffect()->AsKeyframeEffect()) ==
               EffectSet::GetEffectSetForEffect(
                   animation->GetEffect()->AsKeyframeEffect()));
  }
#else
  mPartialPrerenderedAnimations.Remove(aCompositorAnimationId);
#endif
  aAnimation->ResetPartialPrerendered();
}

void LayerManager::UpdatePartialPrerenderedAnimations(
    const nsTArray<uint64_t>& aJankedAnimations) {
  for (uint64_t id : aJankedAnimations) {
    RefPtr<dom::Animation> animation;
    if (mPartialPrerenderedAnimations.Remove(id, getter_AddRefs(animation))) {
      animation->UpdatePartialPrerendered();
    }
  }
}

}  // namespace layers
}  // namespace mozilla
