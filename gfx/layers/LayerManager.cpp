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
#include "mozilla/gfx/UserData.h"        // for UserData, UserDataKey (ptr only)
#include "mozilla/layers/LayersTypes.h"  // for CompositionPayload
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
  mPartialPrerenderedAnimations.Clear();
}

/* static */ mozilla::LogModule* LayerManager::GetLog() {
  static LazyLogModule sLog("Layers");
  return sLog;
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

already_AddRefed<ImageContainer> LayerManager::CreateImageContainer(
    ImageContainer::Mode flag) {
  RefPtr<ImageContainer> container = new ImageContainer(flag);
  return container.forget();
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

}  // namespace layers
}  // namespace mozilla
