/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nullptr; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/NativeLayerCA.h"

#import <AppKit/NSAnimationContext.h>
#import <AppKit/NSColor.h>
#import <QuartzCore/QuartzCore.h>

#include <utility>
#include <algorithm>

#include "GLBlitHelper.h"
#include "GLContextCGL.h"
#include "MozFramebuffer.h"
#include "mozilla/layers/SurfacePoolCA.h"
#include "ScopedGLHelpers.h"

@interface CALayer (PrivateSetContentsOpaque)
- (void)setContentsOpaque:(BOOL)opaque;
@end

namespace mozilla {
namespace layers {

using gfx::IntPoint;
using gfx::IntSize;
using gfx::IntRect;
using gfx::IntRegion;
using gl::GLContext;
using gl::GLContextCGL;

// Needs to be on the stack whenever CALayer mutations are performed.
// (Mutating CALayers outside of a transaction can result in permanently stuck rendering, because
// such mutations create an implicit transaction which never auto-commits if the current thread does
// not have a native runloop.)
// Uses NSAnimationContext, which wraps CATransaction with additional off-main-thread protection,
// see bug 1585523.
struct MOZ_STACK_CLASS AutoCATransaction final {
  AutoCATransaction() {
    [NSAnimationContext beginGrouping];
    // By default, mutating a CALayer property triggers an animation which smoothly transitions the
    // property to the new value. We don't need these animations, and this call turns them off:
    [CATransaction setDisableActions:YES];
  }
  ~AutoCATransaction() { [NSAnimationContext endGrouping]; }
};

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
  if (mMutated) {
    // Clear the root layer's sublayers. At this point the window is usually closed, so this
    // transaction does not cause any screen updates.
    AutoCATransaction transaction;
    mRootCALayer.sublayers = @[];
  }

  [mRootCALayer release];
}

already_AddRefed<NativeLayer> NativeLayerRootCA::CreateLayer(
    const IntSize& aSize, bool aIsOpaque, SurfacePoolHandle* aSurfacePoolHandle) {
  RefPtr<NativeLayer> layer =
      new NativeLayerCA(aSize, aIsOpaque, aSurfacePoolHandle->AsSurfacePoolHandleCA());
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

void NativeLayerRootCA::SetLayers(const nsTArray<RefPtr<NativeLayer>>& aLayers) {
  MutexAutoLock lock(mMutex);

  // Ideally, we'd just be able to do mSublayers = std::move(aLayers).
  // However, aLayers has a different type: it carries NativeLayer objects, whereas mSublayers
  // carries NativeLayerCA objects, so we have to downcast all the elements first. There's one other
  // reason to look at all the elements in aLayers first: We need to make sure any new layers know
  // about our current backing scale.

  nsTArray<RefPtr<NativeLayerCA>> layersCA(aLayers.Length());
  for (auto& layer : aLayers) {
    RefPtr<NativeLayerCA> layerCA = layer->AsNativeLayerCA();
    MOZ_RELEASE_ASSERT(layerCA);
    layerCA->SetBackingScale(mBackingScale);
    layersCA.AppendElement(std::move(layerCA));
  }

  if (layersCA != mSublayers) {
    mSublayers = std::move(layersCA);
    mMutated = true;
  }
}

void NativeLayerRootCA::SetBackingScale(float aBackingScale) {
  MutexAutoLock lock(mMutex);

  mBackingScale = aBackingScale;
  for (auto layer : mSublayers) {
    layer->SetBackingScale(aBackingScale);
  }
}

float NativeLayerRootCA::BackingScale() {
  MutexAutoLock lock(mMutex);
  return mBackingScale;
}

void NativeLayerRootCA::SuspendOffMainThreadCommits() {
  MutexAutoLock lock(mMutex);
  mOffMainThreadCommitsSuspended = true;
}

bool NativeLayerRootCA::UnsuspendOffMainThreadCommits() {
  MutexAutoLock lock(mMutex);
  mOffMainThreadCommitsSuspended = false;
  return mCommitPending;
}

bool NativeLayerRootCA::AreOffMainThreadCommitsSuspended() {
  MutexAutoLock lock(mMutex);
  return mOffMainThreadCommitsSuspended;
}

bool NativeLayerRootCA::CommitToScreen() {
  MutexAutoLock lock(mMutex);

  if (!NS_IsMainThread() && mOffMainThreadCommitsSuspended) {
    mCommitPending = true;
    return false;
  }

  // Force a CoreAnimation layer tree update from this thread.
  AutoCATransaction transaction;

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

  mCommitPending = false;

  return true;
}

NativeLayerCA::NativeLayerCA(const IntSize& aSize, bool aIsOpaque,
                             SurfacePoolHandleCA* aSurfacePoolHandle)
    : mMutex("NativeLayerCA"),
      mSurfacePoolHandle(aSurfacePoolHandle),
      mSize(aSize),
      mIsOpaque(aIsOpaque) {
  MOZ_RELEASE_ASSERT(mSurfacePoolHandle, "Need a non-null surface pool handle.");
}

NativeLayerCA::~NativeLayerCA() {
  if (mInProgressLockedIOSurface) {
    mInProgressLockedIOSurface->Unlock(false);
    mInProgressLockedIOSurface = nullptr;
  }
  if (mInProgressSurface) {
    IOSurfaceDecrementUseCount(mInProgressSurface->mSurface.get());
    mSurfacePoolHandle->ReturnSurfaceToPool(mInProgressSurface->mSurface);
  }
  if (mFrontSurface) {
    mSurfacePoolHandle->ReturnSurfaceToPool(mFrontSurface->mSurface);
  }
  for (const auto& surf : mSurfaces) {
    mSurfacePoolHandle->ReturnSurfaceToPool(surf.mEntry.mSurface);
  }

  [mContentCALayer release];
  [mOpaquenessTintLayer release];
  [mWrappingCALayer release];
}

void NativeLayerCA::SetSurfaceIsFlipped(bool aIsFlipped) {
  MutexAutoLock lock(mMutex);

  if (aIsFlipped != mSurfaceIsFlipped) {
    mSurfaceIsFlipped = aIsFlipped;
    mMutatedSurfaceIsFlipped = true;
  }
}

bool NativeLayerCA::SurfaceIsFlipped() {
  MutexAutoLock lock(mMutex);

  return mSurfaceIsFlipped;
}

IntSize NativeLayerCA::GetSize() {
  MutexAutoLock lock(mMutex);
  return mSize;
}
void NativeLayerCA::SetPosition(const IntPoint& aPosition) {
  MutexAutoLock lock(mMutex);

  if (aPosition != mPosition) {
    mPosition = aPosition;
    mMutatedPosition = true;
  }
}

IntPoint NativeLayerCA::GetPosition() {
  MutexAutoLock lock(mMutex);
  return mPosition;
}

IntRect NativeLayerCA::GetRect() {
  MutexAutoLock lock(mMutex);
  return IntRect(mPosition, mSize);
}

void NativeLayerCA::SetBackingScale(float aBackingScale) {
  MutexAutoLock lock(mMutex);

  if (aBackingScale != mBackingScale) {
    mBackingScale = aBackingScale;
    mMutatedBackingScale = true;
  }
}

bool NativeLayerCA::IsOpaque() {
  MutexAutoLock lock(mMutex);
  return mIsOpaque;
}

void NativeLayerCA::SetClipRect(const Maybe<gfx::IntRect>& aClipRect) {
  MutexAutoLock lock(mMutex);

  if (aClipRect != mClipRect) {
    mClipRect = aClipRect;
    mMutatedClipRect = true;
  }
}

Maybe<gfx::IntRect> NativeLayerCA::ClipRect() {
  MutexAutoLock lock(mMutex);
  return mClipRect;
}

void NativeLayerCA::InvalidateRegionThroughoutSwapchain(const MutexAutoLock&,
                                                        const IntRegion& aRegion) {
  IntRegion r = aRegion;
  if (mInProgressSurface) {
    mInProgressSurface->mInvalidRegion.OrWith(r);
  }
  if (mFrontSurface) {
    mFrontSurface->mInvalidRegion.OrWith(r);
  }
  for (auto& surf : mSurfaces) {
    surf.mEntry.mInvalidRegion.OrWith(r);
  }
}

bool NativeLayerCA::NextSurface(const MutexAutoLock& aLock) {
  if (mSize.IsEmpty()) {
    NSLog(@"NextSurface returning false because of invalid mSize (%d, %d).", mSize.width,
          mSize.height);
    return false;
  }

  MOZ_RELEASE_ASSERT(
      !mInProgressSurface,
      "ERROR: Do not call NextSurface twice in sequence. Call NotifySurfaceReady before the "
      "next call to NextSurface.");

  Maybe<SurfaceWithInvalidRegion> surf = GetUnusedSurfaceAndCleanUp(aLock);
  if (!surf) {
    CFTypeRefPtr<IOSurfaceRef> newSurf = mSurfacePoolHandle->ObtainSurfaceFromPool(mSize);
    if (!newSurf) {
      NSLog(@"NextSurface returning false because IOSurfaceCreate failed to create the surface.");
      return false;
    }
    surf = Some(SurfaceWithInvalidRegion{newSurf, IntRect({}, mSize)});
  }

  MOZ_RELEASE_ASSERT(surf);
  mInProgressSurface = std::move(surf);
  IOSurfaceIncrementUseCount(mInProgressSurface->mSurface.get());
  return true;
}

template <typename F>
void NativeLayerCA::HandlePartialUpdate(const MutexAutoLock& aLock,
                                        const gfx::IntRegion& aUpdateRegion, F&& aCopyFn) {
  MOZ_RELEASE_ASSERT(IntRect({}, mSize).Contains(aUpdateRegion.GetBounds()),
                     "The update region should be within the surface bounds.");

  InvalidateRegionThroughoutSwapchain(aLock, aUpdateRegion);

  gfx::IntRegion copyRegion;
  copyRegion.Sub(mInProgressSurface->mInvalidRegion, aUpdateRegion);
  if (!copyRegion.IsEmpty()) {
    // There are parts in mInProgressSurface which are invalid but which are not included in
    // aUpdateRegion. We will obtain valid content for those parts by copying from a previous
    // surface.
    MOZ_RELEASE_ASSERT(
        mFrontSurface,
        "The first call to NextSurface* must always update the entire layer. If this "
        "is the second call, mFrontSurface will be Some().");

    // NotifySurfaceReady marks the entirety of mFrontSurface as valid.
    MOZ_RELEASE_ASSERT(mFrontSurface->mInvalidRegion.Intersect(copyRegion).IsEmpty(),
                       "mFrontSurface should have valid content in the entire copy region, because "
                       "the only invalidation since NotifySurfaceReady was aUpdateRegion, and "
                       "aUpdateRegion has no overlap with copyRegion.");

    // Now copy the valid content, using a caller-provided copy function.
    aCopyFn(mFrontSurface->mSurface, copyRegion);
    mInProgressSurface->mInvalidRegion.SubOut(copyRegion);
  }

  MOZ_RELEASE_ASSERT(mInProgressSurface->mInvalidRegion == aUpdateRegion);
}

RefPtr<gfx::DrawTarget> NativeLayerCA::NextSurfaceAsDrawTarget(const gfx::IntRegion& aUpdateRegion,
                                                               gfx::BackendType aBackendType) {
  MutexAutoLock lock(mMutex);
  if (!NextSurface(lock)) {
    return nullptr;
  }

  mInProgressLockedIOSurface = new MacIOSurface(mInProgressSurface->mSurface);
  mInProgressLockedIOSurface->Lock(false);
  RefPtr<gfx::DrawTarget> dt = mInProgressLockedIOSurface->GetAsDrawTargetLocked(aBackendType);

  HandlePartialUpdate(
      lock, aUpdateRegion,
      [&](CFTypeRefPtr<IOSurfaceRef> validSource, const gfx::IntRegion& copyRegion) {
        RefPtr<MacIOSurface> source = new MacIOSurface(validSource);
        source->Lock(true);
        {
          RefPtr<gfx::DrawTarget> sourceDT = source->GetAsDrawTargetLocked(aBackendType);
          RefPtr<gfx::SourceSurface> sourceSurface = sourceDT->Snapshot();

          for (auto iter = copyRegion.RectIter(); !iter.Done(); iter.Next()) {
            const gfx::IntRect& r = iter.Get();
            dt->CopySurface(sourceSurface, r, r.TopLeft());
          }
        }
        source->Unlock(true);
      });

  return dt;
}

Maybe<GLuint> NativeLayerCA::NextSurfaceAsFramebuffer(const gfx::IntRegion& aUpdateRegion,
                                                      bool aNeedsDepth) {
  MutexAutoLock lock(mMutex);
  if (!NextSurface(lock)) {
    return Nothing();
  }

  Maybe<GLuint> fbo =
      mSurfacePoolHandle->GetFramebufferForSurface(mInProgressSurface->mSurface, aNeedsDepth);
  if (!fbo) {
    return Nothing();
  }

  HandlePartialUpdate(
      lock, aUpdateRegion,
      [&](CFTypeRefPtr<IOSurfaceRef> validSource, const gfx::IntRegion& copyRegion) {
        // Copy copyRegion from validSource to fbo.
        MOZ_RELEASE_ASSERT(mSurfacePoolHandle->gl());
        mSurfacePoolHandle->gl()->MakeCurrent();
        Maybe<GLuint> sourceFBO = mSurfacePoolHandle->GetFramebufferForSurface(validSource, false);
        if (!sourceFBO) {
          return;
        }
        for (auto iter = copyRegion.RectIter(); !iter.Done(); iter.Next()) {
          gfx::IntRect r = iter.Get();
          if (mSurfaceIsFlipped) {
            r.y = mSize.height - r.YMost();
          }
          mSurfacePoolHandle->gl()->BlitHelper()->BlitFramebufferToFramebuffer(*sourceFBO, *fbo, r,
                                                                               r, LOCAL_GL_NEAREST);
        }
      });

  return fbo;
}

void NativeLayerCA::NotifySurfaceReady() {
  MutexAutoLock lock(mMutex);

  MOZ_RELEASE_ASSERT(mInProgressSurface,
                     "NotifySurfaceReady called without preceding call to NextSurface");

  if (mInProgressLockedIOSurface) {
    mInProgressLockedIOSurface->Unlock(false);
    mInProgressLockedIOSurface = nullptr;
  }

  if (mFrontSurface) {
    mSurfaces.push_back({*mFrontSurface, 0});
    mFrontSurface = Nothing();
  }

  IOSurfaceDecrementUseCount(mInProgressSurface->mSurface.get());
  mFrontSurface = std::move(mInProgressSurface);
  mFrontSurface->mInvalidRegion = IntRect();
  mMutatedFrontSurface = true;
}

void NativeLayerCA::DiscardBackbuffers() {
  MutexAutoLock lock(mMutex);

  for (const auto& surf : mSurfaces) {
    mSurfacePoolHandle->ReturnSurfaceToPool(surf.mEntry.mSurface);
  }
  mSurfaces.clear();
}

void NativeLayerCA::ApplyChanges() {
  MutexAutoLock lock(mMutex);

  if (!mWrappingCALayer) {
    mWrappingCALayer = [[CALayer layer] retain];
    mWrappingCALayer.position = NSZeroPoint;
    mWrappingCALayer.bounds = NSZeroRect;
    mWrappingCALayer.anchorPoint = NSZeroPoint;
    mWrappingCALayer.contentsGravity = kCAGravityTopLeft;
    mContentCALayer = [[CALayer layer] retain];
    mContentCALayer.position = NSZeroPoint;
    mContentCALayer.anchorPoint = NSZeroPoint;
    mContentCALayer.contentsGravity = kCAGravityTopLeft;
    mContentCALayer.contentsScale = 1;
    mContentCALayer.bounds = CGRectMake(0, 0, mSize.width, mSize.height);
    mContentCALayer.opaque = mIsOpaque;
    if ([mContentCALayer respondsToSelector:@selector(setContentsOpaque:)]) {
      // The opaque property seems to not be enough when using IOSurface contents.
      // Additionally, call the private method setContentsOpaque.
      [mContentCALayer setContentsOpaque:mIsOpaque];
    }
    [mWrappingCALayer addSublayer:mContentCALayer];
  }

  bool shouldTintOpaqueness = StaticPrefs::gfx_core_animation_tint_opaque();
  if (shouldTintOpaqueness && !mOpaquenessTintLayer) {
    mOpaquenessTintLayer = [[CALayer layer] retain];
    mOpaquenessTintLayer.position = mContentCALayer.position;
    mOpaquenessTintLayer.bounds = mContentCALayer.bounds;
    mOpaquenessTintLayer.anchorPoint = NSZeroPoint;
    mOpaquenessTintLayer.contentsGravity = kCAGravityTopLeft;
    if (mIsOpaque) {
      mOpaquenessTintLayer.backgroundColor =
          [[[NSColor greenColor] colorWithAlphaComponent:0.5] CGColor];
    } else {
      mOpaquenessTintLayer.backgroundColor =
          [[[NSColor redColor] colorWithAlphaComponent:0.5] CGColor];
    }
    [mWrappingCALayer addSublayer:mOpaquenessTintLayer];
  } else if (!shouldTintOpaqueness && mOpaquenessTintLayer) {
    [mOpaquenessTintLayer removeFromSuperlayer];
    [mOpaquenessTintLayer release];
    mOpaquenessTintLayer = nullptr;
  }

  // CALayers have a position and a size, specified through the position and the bounds properties.
  // layer.bounds.origin must always be (0, 0).
  // A layer's position affects the layer's entire layer subtree. In other words, each layer's
  // position is relative to its superlayer's position. We implement the clip rect using
  // masksToBounds on mWrappingCALayer. So mContentCALayer's position is relative to the clip rect
  // position.
  // Note: The Core Animation docs on "Positioning and Sizing Sublayers" say:
  //  Important: Always use integral numbers for the width and height of your layer.
  // We hope that this refers to integral physical pixels, and not to integral logical coordinates.

  auto globalClipOrigin = mClipRect ? mClipRect->TopLeft() : gfx::IntPoint{};
  auto globalLayerOrigin = mPosition;
  auto clipToLayerOffset = globalLayerOrigin - globalClipOrigin;

  if (mMutatedBackingScale) {
    mContentCALayer.bounds =
        CGRectMake(0, 0, mSize.width / mBackingScale, mSize.height / mBackingScale);
    if (mOpaquenessTintLayer) {
      mOpaquenessTintLayer.bounds = mContentCALayer.bounds;
    }
    mContentCALayer.contentsScale = mBackingScale;
  }

  if (mMutatedBackingScale || mMutatedClipRect) {
    mWrappingCALayer.position =
        CGPointMake(globalClipOrigin.x / mBackingScale, globalClipOrigin.y / mBackingScale);
    if (mClipRect) {
      mWrappingCALayer.masksToBounds = YES;
      mWrappingCALayer.bounds =
          CGRectMake(0, 0, mClipRect->Width() / mBackingScale, mClipRect->Height() / mBackingScale);
    } else {
      mWrappingCALayer.masksToBounds = NO;
    }
  }

  if (mMutatedBackingScale || mMutatedPosition || mMutatedClipRect) {
    mContentCALayer.position =
        CGPointMake(clipToLayerOffset.x / mBackingScale, clipToLayerOffset.y / mBackingScale);
    if (mOpaquenessTintLayer) {
      mOpaquenessTintLayer.position = mContentCALayer.position;
    }
  }

  if (mMutatedBackingScale || mMutatedSurfaceIsFlipped) {
    if (mSurfaceIsFlipped) {
      CGFloat height = mSize.height / mBackingScale;
      mContentCALayer.affineTransform = CGAffineTransformMake(1.0, 0.0, 0.0, -1.0, 0.0, height);
    } else {
      mContentCALayer.affineTransform = CGAffineTransformIdentity;
    }
  }

  if (mMutatedFrontSurface) {
    mContentCALayer.contents = (id)mFrontSurface->mSurface.get();
  }

  mMutatedPosition = false;
  mMutatedBackingScale = false;
  mMutatedSurfaceIsFlipped = false;
  mMutatedClipRect = false;
  mMutatedFrontSurface = false;
}

// Called when mMutex is already being held by the current thread.
Maybe<NativeLayerCA::SurfaceWithInvalidRegion> NativeLayerCA::GetUnusedSurfaceAndCleanUp(
    const MutexAutoLock&) {
  std::vector<SurfaceWithInvalidRegionAndCheckCount> usedSurfaces;
  Maybe<SurfaceWithInvalidRegion> unusedSurface;

  // Separate mSurfaces into used and unused surfaces.
  for (auto& surf : mSurfaces) {
    if (IOSurfaceIsInUse(surf.mEntry.mSurface.get())) {
      surf.mCheckCount++;
      if (surf.mCheckCount < 10) {
        usedSurfaces.push_back(std::move(surf));
      } else {
        // The window server has been holding on to this surface for an unreasonably long time. This
        // is known to happen sometimes, for example in occluded windows or after a GPU switch. In
        // that case, release our references to the surface so that it doesn't look like we're
        // trying to keep it alive.
        mSurfacePoolHandle->ReturnSurfaceToPool(std::move(surf.mEntry.mSurface));
      }
    } else {
      if (unusedSurface) {
        // Multiple surfaces are unused. Keep the most recent one and release any earlier ones. The
        // most recent one requires the least amount of copying during partial repaints.
        mSurfacePoolHandle->ReturnSurfaceToPool(std::move(unusedSurface->mSurface));
      }
      unusedSurface = Some(std::move(surf.mEntry));
    }
  }

  // Put the used surfaces back into mSurfaces.
  mSurfaces = std::move(usedSurfaces);

  return unusedSurface;
}

}  // namespace layers
}  // namespace mozilla
