/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nullptr; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozilla/layers/NativeLayerCA.h"

#import <QuartzCore/QuartzCore.h>
#import <CoreVideo/CVPixelBuffer.h>

#include <utility>
#include <algorithm>

@interface CALayer (PrivateSetContentsOpaque)
- (void)setContentsOpaque:(BOOL)opaque;
@end

namespace mozilla {
namespace layers {

using gfx::IntPoint;
using gfx::IntSize;
using gfx::IntRect;
using gfx::IntRegion;

/* static */ already_AddRefed<NativeLayerRootCA> NativeLayerRootCA::CreateForCALayer(
    CALayer* aLayer) {
  RefPtr<NativeLayerRootCA> layerRoot = new NativeLayerRootCA(aLayer);
  return layerRoot.forget();
}

NativeLayerRootCA::NativeLayerRootCA(CALayer* aLayer)
    : mMutex("NativeLayerRootCA"), mRootCALayer([aLayer retain]) {}

NativeLayerRootCA::~NativeLayerRootCA() {
  MOZ_RELEASE_ASSERT(mSublayers.IsEmpty(),
                     "Please clear all layers before destroying the layer root.");

  // FIXME: mMutated might be true at this point, which would indicate that, even
  // though mSublayers is empty now, this state may not yet have been synced to
  // the underlying CALayer. In other words, mRootCALayer might still have sublayers.
  // Should we do anything about that?
  // We could just clear mRootCALayer's sublayers now, but doing so would be a
  // layer tree transformation outside of a transaction, which we want to avoid.
  // But we also don't want to trigger a transaction just for clearing the
  // window's layers. And we wouldn't expect a NativeLayerRootCA to be destroyed
  // while the window is still open and visible. Are layer tree modifications
  // outside of CATransactions allowed while the window is closed? Who knows.

  [mRootCALayer release];
}

already_AddRefed<NativeLayer> NativeLayerRootCA::CreateLayer() {
  RefPtr<NativeLayer> layer = new NativeLayerCA();
  return layer.forget();
}

void NativeLayerRootCA::AppendLayer(NativeLayer* aLayer) {
  MutexAutoLock lock(mMutex);

  RefPtr<NativeLayerCA> layerCA = aLayer->AsNativeLayerCA();
  MOZ_RELEASE_ASSERT(layerCA);

  mSublayers.AppendElement(layerCA);
  layerCA->SetBackingScale(mBackingScale);
  mMutated = true;
}

void NativeLayerRootCA::RemoveLayer(NativeLayer* aLayer) {
  MutexAutoLock lock(mMutex);

  RefPtr<NativeLayerCA> layerCA = aLayer->AsNativeLayerCA();
  MOZ_RELEASE_ASSERT(layerCA);

  mSublayers.RemoveElement(layerCA);
  mMutated = true;
}

// Must be called within a current CATransaction on the transaction's thread.
void NativeLayerRootCA::ApplyChanges() {
  MutexAutoLock lock(mMutex);

  [CATransaction setDisableActions:YES];

  // Call ApplyChanges on our sublayers first, and then update the root layer's
  // list of sublayers. The order is important because we need layer->UnderlyingCALayer()
  // to be non-null, and the underlying CALayer gets lazily initialized in ApplyChanges().
  for (auto layer : mSublayers) {
    layer->ApplyChanges();
  }

  if (mMutated) {
    NSMutableArray<CALayer*>* sublayers = [NSMutableArray arrayWithCapacity:mSublayers.Length()];
    for (auto layer : mSublayers) {
      [sublayers addObject:layer->UnderlyingCALayer()];
    }
    mRootCALayer.sublayers = sublayers;
    mMutated = false;
  }
}

void NativeLayerRootCA::SetBackingScale(float aBackingScale) {
  MutexAutoLock lock(mMutex);

  mBackingScale = aBackingScale;
  for (auto layer : mSublayers) {
    layer->SetBackingScale(aBackingScale);
  }
}

NativeLayerCA::NativeLayerCA() : mMutex("NativeLayerCA") {}

NativeLayerCA::~NativeLayerCA() {
  SetSurfaceRegistry(nullptr);  // or maybe MOZ_RELEASE_ASSERT(!mSurfaceRegistry) would be better?

  if (mInProgressSurface) {
    IOSurfaceDecrementUseCount(mInProgressSurface->mSurface.get());
  }
  if (mReadySurface) {
    IOSurfaceDecrementUseCount(mReadySurface->mSurface.get());
  }

  for (CALayer* contentLayer : mContentCALayers) {
    [contentLayer release];
  }
  [mWrappingCALayer release];
}

void NativeLayerCA::SetSurfaceRegistry(RefPtr<IOSurfaceRegistry> aSurfaceRegistry) {
  MutexAutoLock lock(mMutex);

  if (mSurfaceRegistry) {
    for (auto surf : mSurfaces) {
      mSurfaceRegistry->UnregisterSurface(surf.mSurface);
    }
    if (mInProgressSurface) {
      mSurfaceRegistry->UnregisterSurface(mInProgressSurface->mSurface);
    }
    if (mReadySurface) {
      mSurfaceRegistry->UnregisterSurface(mReadySurface->mSurface);
    }
  }
  mSurfaceRegistry = aSurfaceRegistry;
  if (mSurfaceRegistry) {
    for (auto surf : mSurfaces) {
      mSurfaceRegistry->RegisterSurface(surf.mSurface);
    }
    if (mInProgressSurface) {
      mSurfaceRegistry->RegisterSurface(mInProgressSurface->mSurface);
    }
    if (mReadySurface) {
      mSurfaceRegistry->RegisterSurface(mReadySurface->mSurface);
    }
  }
}

RefPtr<IOSurfaceRegistry> NativeLayerCA::GetSurfaceRegistry() {
  MutexAutoLock lock(mMutex);

  return mSurfaceRegistry;
}

void NativeLayerCA::SetSurfaceIsFlipped(bool aIsFlipped) {
  MutexAutoLock lock(mMutex);

  if (aIsFlipped != mSurfaceIsFlipped) {
    mSurfaceIsFlipped = aIsFlipped;
    mMutatedGeometry = true;
  }
}

bool NativeLayerCA::SurfaceIsFlipped() {
  MutexAutoLock lock(mMutex);

  return mSurfaceIsFlipped;
}

void NativeLayerCA::SetRect(const IntRect& aRect) {
  MutexAutoLock lock(mMutex);

  if (aRect.TopLeft() != mPosition) {
    mPosition = aRect.TopLeft();
    mMutatedPosition = true;
  }
  if (aRect.Size() != mSize) {
    mSize = aRect.Size();
    mMutatedGeometry = true;
  }
}

IntRect NativeLayerCA::GetRect() {
  MutexAutoLock lock(mMutex);
  return IntRect(mPosition, mSize);
}

void NativeLayerCA::SetBackingScale(float aBackingScale) {
  MutexAutoLock lock(mMutex);

  if (aBackingScale != mBackingScale) {
    mBackingScale = aBackingScale;
    mMutatedGeometry = true;
  }
}

void NativeLayerCA::SetOpaqueRegion(const gfx::IntRegion& aRegion) {
  MutexAutoLock lock(mMutex);

  if (aRegion != mOpaqueRegion) {
    mOpaqueRegion = aRegion;
    mMutatedGeometry = true;
  }
}

gfx::IntRegion NativeLayerCA::OpaqueRegion() {
  MutexAutoLock lock(mMutex);
  return mOpaqueRegion;
}

IntRegion NativeLayerCA::CurrentSurfaceInvalidRegion() {
  MutexAutoLock lock(mMutex);

  MOZ_RELEASE_ASSERT(
      mInProgressSurface,
      "Only call currentSurfaceInvalidRegion after a call to NextSurface and before the call "
      "to notifySurfaceIsReady.");
  return mInProgressSurface->mInvalidRegion;
}

void NativeLayerCA::InvalidateRegionThroughoutSwapchain(const IntRegion& aRegion) {
  MutexAutoLock lock(mMutex);

  IntRegion r = aRegion;
  r.AndWith(IntRect(IntPoint(0, 0), mSize));
  if (mInProgressSurface) {
    mInProgressSurface->mInvalidRegion.OrWith(r);
  }
  if (mReadySurface) {
    mReadySurface->mInvalidRegion.OrWith(r);
  }
  for (auto& surf : mSurfaces) {
    surf.mInvalidRegion.OrWith(r);
  }
}

CFTypeRefPtr<IOSurfaceRef> NativeLayerCA::NextSurface() {
  MutexAutoLock lock(mMutex);

  IntSize surfaceSize = mSize;
  if (surfaceSize.IsEmpty()) {
    NSLog(@"NextSurface returning nullptr because of invalid surfaceSize (%d, %d).",
          surfaceSize.width, surfaceSize.height);
    return nullptr;
  }

  MOZ_RELEASE_ASSERT(
      !mInProgressSurface,
      "ERROR: Do not call NextSurface twice in sequence. Call NotifySurfaceReady before the "
      "next call to NextSurface.");

  // Find the last surface in unusedSurfaces which has the right size. If such
  // a surface exists, it is the surface we will recycle.
  std::vector<SurfaceWithInvalidRegion> unusedSurfaces = RemoveExcessUnusedSurfaces(lock);
  auto surfIter = std::find_if(
      unusedSurfaces.rbegin(), unusedSurfaces.rend(),
      [surfaceSize](const SurfaceWithInvalidRegion& s) { return s.mSize == surfaceSize; });

  Maybe<SurfaceWithInvalidRegion> surf;
  if (surfIter != unusedSurfaces.rend()) {
    // We found the surface we want to recycle.
    surf = Some(*surfIter);

    // Remove surf from unusedSurfaces.
    // The reverse iterator makes this a bit cumbersome.
    unusedSurfaces.erase(std::next(surfIter).base());
  } else {
    CFTypeRefPtr<IOSurfaceRef> newSurf = CFTypeRefPtr<IOSurfaceRef>::WrapUnderCreateRule(
        IOSurfaceCreate((__bridge CFDictionaryRef) @{
          (__bridge NSString*)kIOSurfaceWidth : @(surfaceSize.width),
          (__bridge NSString*)kIOSurfaceHeight : @(surfaceSize.height),
          (__bridge NSString*)kIOSurfacePixelFormat : @(kCVPixelFormatType_32BGRA),
          (__bridge NSString*)kIOSurfaceBytesPerElement : @(4),
        }));
    if (!newSurf) {
      NSLog(@"NextSurface returning nullptr because IOSurfaceCreate failed to create the surface.");
      return nullptr;
    }
    if (mSurfaceRegistry) {
      mSurfaceRegistry->RegisterSurface(newSurf);
    }
    surf =
        Some(SurfaceWithInvalidRegion{newSurf, IntRect(IntPoint(0, 0), surfaceSize), surfaceSize});
  }

  // Delete all other unused surfaces.
  if (mSurfaceRegistry) {
    for (auto unusedSurf : unusedSurfaces) {
      mSurfaceRegistry->UnregisterSurface(unusedSurf.mSurface);
    }
  }
  unusedSurfaces.clear();

  MOZ_RELEASE_ASSERT(surf);
  mInProgressSurface = std::move(surf);
  IOSurfaceIncrementUseCount(mInProgressSurface->mSurface.get());
  return mInProgressSurface->mSurface;
}

void NativeLayerCA::NotifySurfaceReady() {
  MutexAutoLock lock(mMutex);

  MOZ_RELEASE_ASSERT(mInProgressSurface,
                     "NotifySurfaceReady called without preceding call to NextSurface");
  if (mReadySurface) {
    IOSurfaceDecrementUseCount(mReadySurface->mSurface.get());
    mSurfaces.push_back(*mReadySurface);
    mReadySurface = Nothing();
  }
  mReadySurface = std::move(mInProgressSurface);
  mReadySurface->mInvalidRegion = IntRect();
}

void NativeLayerCA::ApplyChanges() {
  MutexAutoLock lock(mMutex);

  if (!mWrappingCALayer) {
    mWrappingCALayer = [[CALayer layer] retain];
    mWrappingCALayer.position = NSZeroPoint;
    mWrappingCALayer.bounds = NSZeroRect;
    mWrappingCALayer.anchorPoint = NSZeroPoint;
    mWrappingCALayer.contentsGravity = kCAGravityTopLeft;
  }

  if (mMutatedPosition || mMutatedGeometry) {
    mWrappingCALayer.position =
        CGPointMake(mPosition.x / mBackingScale, mPosition.y / mBackingScale);
    mMutatedPosition = false;
  }

  if (mMutatedGeometry) {
    mWrappingCALayer.bounds =
        CGRectMake(0, 0, mSize.width / mBackingScale, mSize.height / mBackingScale);

    // Assemble opaque and transparent sublayers to cover the respective regions.
    // mContentCALayers has the current sublayers. We will try to re-use layers
    // as much as possible.
    IntRegion opaqueRegion;
    opaqueRegion.And(IntRect(IntPoint(), mSize), mOpaqueRegion);
    IntRegion transparentRegion;
    transparentRegion.Sub(IntRect(IntPoint(), mSize), opaqueRegion);
    std::deque<CALayer*> layersToRecycle = std::move(mContentCALayers);
    PlaceContentLayers(lock, opaqueRegion, true, &layersToRecycle);
    PlaceContentLayers(lock, transparentRegion, false, &layersToRecycle);
    for (CALayer* unusedLayer : layersToRecycle) {
      [unusedLayer release];
    }
    NSMutableArray<CALayer*>* sublayers =
        [NSMutableArray arrayWithCapacity:mContentCALayers.size()];
    for (auto layer : mContentCALayers) {
      [sublayers addObject:layer];
    }
    mWrappingCALayer.sublayers = sublayers;
    mMutatedGeometry = false;
  }

  if (mReadySurface) {
    for (CALayer* layer : mContentCALayers) {
      layer.contents = (id)mReadySurface->mSurface.get();
    }
    IOSurfaceDecrementUseCount(mReadySurface->mSurface.get());
    mSurfaces.push_back(*mReadySurface);
    mReadySurface = Nothing();
  }
}

void NativeLayerCA::PlaceContentLayers(const MutexAutoLock&, const IntRegion& aRegion, bool aOpaque,
                                       std::deque<CALayer*>* aLayersToRecycle) {
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    IntRect r = iter.Get();

    CALayer* layer;
    if (aLayersToRecycle->empty()) {
      layer = [[CALayer layer] retain];
      layer.anchorPoint = NSZeroPoint;
      layer.contentsGravity = kCAGravityTopLeft;
    } else {
      layer = aLayersToRecycle->front();
      aLayersToRecycle->pop_front();
    }
    layer.position = CGPointMake(r.x / mBackingScale, r.y / mBackingScale);
    layer.bounds = CGRectMake(0, 0, r.width / mBackingScale, r.height / mBackingScale);
    layer.contentsScale = mBackingScale;
    CGRect unitContentsRect =
        CGRectMake(CGFloat(r.x) / mSize.width, CGFloat(r.y) / mSize.height,
                   CGFloat(r.width) / mSize.width, CGFloat(r.height) / mSize.height);
    if (mSurfaceIsFlipped) {
      CGFloat height = r.height / mBackingScale;
      layer.affineTransform = CGAffineTransformMake(1.0, 0.0, 0.0, -1.0, 0.0, height);
      unitContentsRect.origin.y = 1.0 - (unitContentsRect.origin.y + unitContentsRect.size.height);
      layer.contentsRect = unitContentsRect;
    } else {
      layer.affineTransform = CGAffineTransformIdentity;
      layer.contentsRect = unitContentsRect;
    }
    layer.opaque = aOpaque;
    if ([layer respondsToSelector:@selector(setContentsOpaque:)]) {
      // The opaque property seems to not be enough when using IOSurface contents.
      // Additionally, call the private method setContentsOpaque.
      [layer setContentsOpaque:aOpaque];
    }
    mContentCALayers.push_back(layer);
  }
}

// Called when mMutex is already being held by the current thread.
std::vector<NativeLayerCA::SurfaceWithInvalidRegion> NativeLayerCA::RemoveExcessUnusedSurfaces(
    const MutexAutoLock&) {
  std::vector<SurfaceWithInvalidRegion> usedSurfaces;
  std::vector<SurfaceWithInvalidRegion> unusedSurfaces;

  // Separate mSurfaces into used and unused surfaces, leaving 2 surfaces behind.
  while (mSurfaces.size() > 2) {
    auto surf = std::move(mSurfaces.front());
    mSurfaces.pop_front();
    if (IOSurfaceIsInUse(surf.mSurface.get())) {
      usedSurfaces.push_back(std::move(surf));
    } else {
      unusedSurfaces.push_back(std::move(surf));
    }
  }
  // Put the used surfaces back into mSurfaces, at the beginning.
  mSurfaces.insert(mSurfaces.begin(), usedSurfaces.begin(), usedSurfaces.end());

  return unusedSurfaces;
}

}  // namespace layers
}  // namespace mozilla
