/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nullptr; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozilla/layers/NativeLayerCA.h"

#import <QuartzCore/QuartzCore.h>
#import <CoreVideo/CVPixelBuffer.h>

#include <utility>
#include <algorithm>

#include "GLContextCGL.h"
#include "MozFramebuffer.h"
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

already_AddRefed<NativeLayer> NativeLayerRootCA::CreateLayer(const IntSize& aSize, bool aIsOpaque) {
  RefPtr<NativeLayer> layer = new NativeLayerCA(aSize, aIsOpaque);
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

NativeLayerCA::NativeLayerCA(const IntSize& aSize, bool aIsOpaque)
    : mMutex("NativeLayerCA"), mSize(aSize), mIsOpaque(aIsOpaque) {}

NativeLayerCA::~NativeLayerCA() {
  if (mInProgressLockedIOSurface) {
    mInProgressLockedIOSurface->Unlock(false);
    mInProgressLockedIOSurface = nullptr;
  }
  if (mInProgressSurface) {
    IOSurfaceDecrementUseCount(mInProgressSurface->mSurface.get());
  }
  if (mReadySurface) {
    IOSurfaceDecrementUseCount(mReadySurface->mSurface.get());
  }

  [mContentCALayer release];
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

IntRegion NativeLayerCA::CurrentSurfaceInvalidRegion() {
  MutexAutoLock lock(mMutex);

  MOZ_RELEASE_ASSERT(
      mInProgressSurface,
      "Only call currentSurfaceInvalidRegion after a call to NextSurface and before the call "
      "to notifySurfaceIsReady.");
  return mInProgressSurface->mInvalidRegion;
}

void NativeLayerCA::InvalidateRegionThroughoutSwapchain(const MutexAutoLock&,
                                                        const IntRegion& aRegion) {
  IntRegion r = aRegion;
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

CFTypeRefPtr<IOSurfaceRef> NativeLayerCA::NextSurface(const MutexAutoLock& aLock) {
  if (mSize.IsEmpty()) {
    NSLog(@"NextSurface returning nullptr because of invalid mSize (%d, %d).", mSize.width,
          mSize.height);
    return nullptr;
  }

  MOZ_RELEASE_ASSERT(
      !mInProgressSurface,
      "ERROR: Do not call NextSurface twice in sequence. Call NotifySurfaceReady before the "
      "next call to NextSurface.");

  // Find the last surface in unusedSurfaces. If such
  // a surface exists, it is the surface we will recycle.
  std::vector<SurfaceWithInvalidRegion> unusedSurfaces = RemoveExcessUnusedSurfaces(aLock);

  Maybe<SurfaceWithInvalidRegion> surf;
  if (!unusedSurfaces.empty()) {
    // We found the surface we want to recycle.
    surf = Some(unusedSurfaces.back());

    // Remove surf from unusedSurfaces.
    unusedSurfaces.pop_back();
  } else {
    CFTypeRefPtr<IOSurfaceRef> newSurf = CFTypeRefPtr<IOSurfaceRef>::WrapUnderCreateRule(
        IOSurfaceCreate((__bridge CFDictionaryRef) @{
          (__bridge NSString*)kIOSurfaceWidth : @(mSize.width),
          (__bridge NSString*)kIOSurfaceHeight : @(mSize.height),
          (__bridge NSString*)kIOSurfacePixelFormat : @(kCVPixelFormatType_32BGRA),
          (__bridge NSString*)kIOSurfaceBytesPerElement : @(4),
        }));
    if (!newSurf) {
      NSLog(@"NextSurface returning nullptr because IOSurfaceCreate failed to create the surface.");
      return nullptr;
    }
    surf = Some(SurfaceWithInvalidRegion{newSurf, IntRect({}, mSize)});
  }

  // Delete all other unused surfaces.
  for (auto unusedSurf : unusedSurfaces) {
    mFramebuffers.erase(unusedSurf.mSurface);
  }
  unusedSurfaces.clear();

  MOZ_RELEASE_ASSERT(surf);
  mInProgressSurface = std::move(surf);
  IOSurfaceIncrementUseCount(mInProgressSurface->mSurface.get());
  return mInProgressSurface->mSurface;
}

RefPtr<gfx::DrawTarget> NativeLayerCA::NextSurfaceAsDrawTarget(const gfx::IntRegion& aUpdateRegion,
                                                               gfx::BackendType aBackendType) {
  MutexAutoLock lock(mMutex);
  CFTypeRefPtr<IOSurfaceRef> surface = NextSurface(lock);
  if (!surface) {
    return nullptr;
  }

  MOZ_RELEASE_ASSERT(IntRect({}, mSize).Contains(aUpdateRegion.GetBounds()));
  InvalidateRegionThroughoutSwapchain(lock, aUpdateRegion);

  mInProgressLockedIOSurface = new MacIOSurface(std::move(surface));
  mInProgressLockedIOSurface->Lock(false);
  return mInProgressLockedIOSurface->GetAsDrawTargetLocked(aBackendType);
}

void NativeLayerCA::SetGLContext(gl::GLContext* aContext) {
  MutexAutoLock lock(mMutex);

  RefPtr<gl::GLContextCGL> glContextCGL = gl::GLContextCGL::Cast(aContext);
  MOZ_RELEASE_ASSERT(glContextCGL, "Unexpected GLContext type");

  if (glContextCGL != mGLContext) {
    mFramebuffers.clear();
    mGLContext = glContextCGL;
  }
}

gl::GLContext* NativeLayerCA::GetGLContext() {
  MutexAutoLock lock(mMutex);
  return mGLContext;
}

Maybe<GLuint> NativeLayerCA::NextSurfaceAsFramebuffer(const gfx::IntRegion& aUpdateRegion,
                                                      bool aNeedsDepth) {
  MutexAutoLock lock(mMutex);
  CFTypeRefPtr<IOSurfaceRef> surface = NextSurface(lock);
  if (!surface) {
    return Nothing();
  }

  MOZ_RELEASE_ASSERT(IntRect({}, mSize).Contains(aUpdateRegion.GetBounds()));
  InvalidateRegionThroughoutSwapchain(lock, aUpdateRegion);
  return Some(GetOrCreateFramebufferForSurface(lock, std::move(surface), aNeedsDepth));
}

GLuint NativeLayerCA::GetOrCreateFramebufferForSurface(const MutexAutoLock&,
                                                       CFTypeRefPtr<IOSurfaceRef> aSurface,
                                                       bool aNeedsDepth) {
  auto fbCursor = mFramebuffers.find(aSurface);
  if (fbCursor != mFramebuffers.end()) {
    return fbCursor->second->mFB;
  }

  MOZ_RELEASE_ASSERT(
      mGLContext, "Only call NextSurfaceAsFramebuffer when a GLContext is set on this NativeLayer");
  mGLContext->MakeCurrent();
  GLuint tex = mGLContext->CreateTexture();
  {
    const gl::ScopedBindTexture bindTex(mGLContext, tex, LOCAL_GL_TEXTURE_RECTANGLE_ARB);
    CGLTexImageIOSurface2D(mGLContext->GetCGLContext(), LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                           LOCAL_GL_RGBA, mSize.width, mSize.height, LOCAL_GL_BGRA,
                           LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV, aSurface.get(), 0);
  }

  auto fb = gl::MozFramebuffer::CreateWith(mGLContext, mSize, 0, aNeedsDepth,
                                           LOCAL_GL_TEXTURE_RECTANGLE_ARB, tex);
  GLuint fbo = fb->mFB;
  mFramebuffers.insert({aSurface, std::move(fb)});

  return fbo;
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

  if (mInProgressLockedIOSurface) {
    mInProgressLockedIOSurface->Unlock(false);
    mInProgressLockedIOSurface = nullptr;
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
  }

  if (mMutatedBackingScale || mMutatedSurfaceIsFlipped) {
    if (mSurfaceIsFlipped) {
      CGFloat height = mSize.height / mBackingScale;
      mContentCALayer.affineTransform = CGAffineTransformMake(1.0, 0.0, 0.0, -1.0, 0.0, height);
    } else {
      mContentCALayer.affineTransform = CGAffineTransformIdentity;
    }
  }

  mMutatedPosition = false;
  mMutatedBackingScale = false;
  mMutatedSurfaceIsFlipped = false;
  mMutatedClipRect = false;

  if (mReadySurface) {
    mContentCALayer.contents = (id)mReadySurface->mSurface.get();
    IOSurfaceDecrementUseCount(mReadySurface->mSurface.get());
    mSurfaces.push_back(*mReadySurface);
    mReadySurface = Nothing();
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
