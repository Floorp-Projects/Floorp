/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nullptr; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozilla/layers/NativeLayerCA.h"

#import <QuartzCore/QuartzCore.h>
#import <CoreVideo/CVPixelBuffer.h>

#include <utility>
#include <algorithm>

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
    : mRootCALayer([aLayer retain]), mMutex("NativeLayerRootCA") {}

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

  [mCALayer release];
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

  mSurfaceIsFlipped = aIsFlipped;
  mMutated = true;
}

bool NativeLayerCA::SurfaceIsFlipped() {
  MutexAutoLock lock(mMutex);

  return mSurfaceIsFlipped;
}

void NativeLayerCA::SetRect(const IntRect& aRect) {
  MutexAutoLock lock(mMutex);

  mRect = aRect;
  mMutated = true;
}

IntRect NativeLayerCA::GetRect() {
  MutexAutoLock lock(mMutex);
  return mRect;
}

void NativeLayerCA::SetBackingScale(float aBackingScale) {
  MutexAutoLock lock(mMutex);

  mBackingScale = aBackingScale;
  mMutated = true;
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
  r.AndWith(IntRect(IntPoint(0, 0), mRect.Size()));
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

  IntSize surfaceSize = mRect.Size();
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
  mMutated = true;
}

void NativeLayerCA::ApplyChanges() {
  MutexAutoLock lock(mMutex);

  if (!mCALayer) {
    mCALayer = [[CALayer layer] retain];
    mCALayer.position = NSZeroPoint;
    mCALayer.bounds = NSZeroRect;
    mCALayer.anchorPoint = NSZeroPoint;
    mCALayer.contentsGravity = kCAGravityTopLeft;
  }

  if (!mMutated) {
    return;
  }

  mCALayer.contentsScale = mBackingScale;
  mCALayer.position = NSMakePoint(mRect.x / mBackingScale, mRect.y / mBackingScale);
  mCALayer.bounds = NSMakeRect(0, 0, mRect.width / mBackingScale, mRect.height / mBackingScale);

  if (mReadySurface) {
    mCALayer.contents = (id)mReadySurface->mSurface.get();
    IOSurfaceDecrementUseCount(mReadySurface->mSurface.get());
    mSurfaces.push_back(*mReadySurface);
    mReadySurface = Nothing();
  }

  if (mSurfaceIsFlipped) {
    CGFloat height = mCALayer.bounds.size.height;
    // FIXME: this probably needs adjusting if mCALayer.position is non-zero.
    mCALayer.affineTransform = CGAffineTransformMake(1.0, 0.0, 0.0, -1.0, 0.0, height);
  } else {
    mCALayer.affineTransform = CGAffineTransformIdentity;
  }

  mMutated = false;
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
