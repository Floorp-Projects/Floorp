/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nullptr; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/NativeLayerCA.h"

#import <AppKit/NSAnimationContext.h>
#import <AppKit/NSColor.h>
#import <OpenGL/gl.h>
#import <QuartzCore/QuartzCore.h>

#include <utility>
#include <algorithm>

#include "gfxUtils.h"
#include "GLBlitHelper.h"
#include "GLContextCGL.h"
#include "GLContextProvider.h"
#include "MozFramebuffer.h"
#include "mozilla/layers/SurfacePoolCA.h"
#include "mozilla/webrender/RenderMacIOSurfaceTextureHostOGL.h"
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
using gfx::Matrix4x4;
using gfx::SurfaceFormat;
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

// Returns an autoreleased CALayer* object.
static CALayer* MakeOffscreenRootCALayer() {
  // This layer should behave similarly to the backing layer of a flipped NSView.
  // It will never be rendered on the screen and it will never be attached to an NSView's layer;
  // instead, it will be the root layer of a "local" CAContext.
  // Setting geometryFlipped to YES causes the orientation of descendant CALayers' contents (such as
  // IOSurfaces) to be consistent with what happens in a layer subtree that is attached to a flipped
  // NSView. Setting it to NO would cause the surfaces in individual leaf layers to render upside
  // down (rather than just flipping the entire layer tree upside down).
  AutoCATransaction transaction;
  CALayer* layer = [CALayer layer];
  layer.position = NSZeroPoint;
  layer.bounds = NSZeroRect;
  layer.anchorPoint = NSZeroPoint;
  layer.contentsGravity = kCAGravityTopLeft;
  layer.masksToBounds = YES;
  layer.geometryFlipped = YES;
  return layer;
}

NativeLayerRootCA::NativeLayerRootCA(CALayer* aLayer)
    : mMutex("NativeLayerRootCA"),
      mOnscreenRepresentation(aLayer),
      mOffscreenRepresentation(MakeOffscreenRootCALayer()) {}

NativeLayerRootCA::~NativeLayerRootCA() {
  MOZ_RELEASE_ASSERT(mSublayers.IsEmpty(),
                     "Please clear all layers before destroying the layer root.");
}

already_AddRefed<NativeLayer> NativeLayerRootCA::CreateLayer(
    const IntSize& aSize, bool aIsOpaque, SurfacePoolHandle* aSurfacePoolHandle) {
  RefPtr<NativeLayer> layer =
      new NativeLayerCA(aSize, aIsOpaque, aSurfacePoolHandle->AsSurfacePoolHandleCA());
  return layer.forget();
}

already_AddRefed<NativeLayer> NativeLayerRootCA::CreateLayerForExternalTexture(bool aIsOpaque) {
  RefPtr<NativeLayer> layer = new NativeLayerCA(aIsOpaque);
  return layer.forget();
}

void NativeLayerRootCA::AppendLayer(NativeLayer* aLayer) {
  MutexAutoLock lock(mMutex);

  RefPtr<NativeLayerCA> layerCA = aLayer->AsNativeLayerCA();
  MOZ_RELEASE_ASSERT(layerCA);

  mSublayers.AppendElement(layerCA);
  layerCA->SetBackingScale(mBackingScale);
  ForAllRepresentations([&](Representation& r) { r.mMutated = true; });
}

void NativeLayerRootCA::RemoveLayer(NativeLayer* aLayer) {
  MutexAutoLock lock(mMutex);

  RefPtr<NativeLayerCA> layerCA = aLayer->AsNativeLayerCA();
  MOZ_RELEASE_ASSERT(layerCA);

  mSublayers.RemoveElement(layerCA);
  ForAllRepresentations([&](Representation& r) { r.mMutated = true; });
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
    ForAllRepresentations([&](Representation& r) { r.mMutated = true; });
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

  mOnscreenRepresentation.Commit(WhichRepresentation::ONSCREEN, mSublayers);

  mCommitPending = false;

  return true;
}

UniquePtr<NativeLayerRootSnapshotter> NativeLayerRootCA::CreateSnapshotter() {
  MutexAutoLock lock(mMutex);
  MOZ_RELEASE_ASSERT(
      !mWeakSnapshotter,
      "No NativeLayerRootSnapshotter for this NativeLayerRoot should exist when this is called");

  auto cr = NativeLayerRootSnapshotterCA::Create(this, mOffscreenRepresentation.mRootCALayer);
  if (cr) {
    mWeakSnapshotter = cr.get();
  }
  return cr;
}

void NativeLayerRootCA::OnNativeLayerRootSnapshotterDestroyed(
    NativeLayerRootSnapshotterCA* aNativeLayerRootSnapshotter) {
  MutexAutoLock lock(mMutex);
  MOZ_RELEASE_ASSERT(mWeakSnapshotter == aNativeLayerRootSnapshotter);
  mWeakSnapshotter = nullptr;
}

void NativeLayerRootCA::CommitOffscreen() {
  MutexAutoLock lock(mMutex);
  mOffscreenRepresentation.Commit(WhichRepresentation::OFFSCREEN, mSublayers);
}

template <typename F>
void NativeLayerRootCA::ForAllRepresentations(F aFn) {
  aFn(mOnscreenRepresentation);
  aFn(mOffscreenRepresentation);
}

NativeLayerRootCA::Representation::Representation(CALayer* aRootCALayer)
    : mRootCALayer([aRootCALayer retain]) {}

NativeLayerRootCA::Representation::~Representation() {
  if (mMutated) {
    // Clear the root layer's sublayers. At this point the window is usually closed, so this
    // transaction does not cause any screen updates.
    AutoCATransaction transaction;
    mRootCALayer.sublayers = @[];
  }

  [mRootCALayer release];
}

void NativeLayerRootCA::Representation::Commit(WhichRepresentation aRepresentation,
                                               const nsTArray<RefPtr<NativeLayerCA>>& aSublayers) {
  AutoCATransaction transaction;

  // Call ApplyChanges on our sublayers first, and then update the root layer's
  // list of sublayers. The order is important because we need layer->UnderlyingCALayer()
  // to be non-null, and the underlying CALayer gets lazily initialized in ApplyChanges().
  for (auto layer : aSublayers) {
    layer->ApplyChanges(aRepresentation);
  }

  if (mMutated) {
    NSMutableArray<CALayer*>* sublayers = [NSMutableArray arrayWithCapacity:aSublayers.Length()];
    for (auto layer : aSublayers) {
      [sublayers addObject:layer->UnderlyingCALayer(aRepresentation)];
    }
    mRootCALayer.sublayers = sublayers;
    mMutated = false;
  }
}

/* static */ UniquePtr<NativeLayerRootSnapshotterCA> NativeLayerRootSnapshotterCA::Create(
    NativeLayerRootCA* aLayerRoot, CALayer* aRootCALayer) {
  if (NS_IsMainThread()) {
    // Disallow creating snapshotters on the main thread.
    // On the main thread, any explicit CATransaction / NSAnimationContext is nested within a global
    // implicit transaction. This makes it impossible to apply CALayer mutations synchronously such
    // that they become visible to CARenderer. As a result, the snapshotter would not capture
    // the right output on the main thread.
    return nullptr;
  }

  nsCString failureUnused;
  RefPtr<gl::GLContext> gl =
      gl::GLContextProvider::CreateHeadless({gl::CreateContextFlags::ALLOW_OFFLINE_RENDERER |
                                             gl::CreateContextFlags::REQUIRE_COMPAT_PROFILE},
                                            &failureUnused);
  if (!gl) {
    return nullptr;
  }

  return UniquePtr<NativeLayerRootSnapshotterCA>(
      new NativeLayerRootSnapshotterCA(aLayerRoot, std::move(gl), aRootCALayer));
}

NativeLayerRootSnapshotterCA::NativeLayerRootSnapshotterCA(NativeLayerRootCA* aLayerRoot,
                                                           RefPtr<GLContext>&& aGL,
                                                           CALayer* aRootCALayer)
    : mLayerRoot(aLayerRoot), mGL(aGL) {
  AutoCATransaction transaction;
  mRenderer = [[CARenderer rendererWithCGLContext:gl::GLContextCGL::Cast(mGL)->GetCGLContext()
                                          options:nil] retain];
  mRenderer.layer = aRootCALayer;
}

NativeLayerRootSnapshotterCA::~NativeLayerRootSnapshotterCA() {
  mLayerRoot->OnNativeLayerRootSnapshotterDestroyed(this);
  [mRenderer release];
}

bool NativeLayerRootSnapshotterCA::ReadbackPixels(const IntSize& aReadbackSize,
                                                  SurfaceFormat aReadbackFormat,
                                                  const Range<uint8_t>& aReadbackBuffer) {
  if (aReadbackFormat != SurfaceFormat::B8G8R8A8) {
    return false;
  }

  CGRect bounds = CGRectMake(0, 0, aReadbackSize.width, aReadbackSize.height);

  {
    // Set the correct bounds and scale on the renderer and its root layer. CARenderer always
    // renders at unit scale, i.e. the coordinates on the root layer must map 1:1 to render target
    // pixels. But the coordinates on our content layers are in "points", where 1 point maps to 2
    // device pixels on HiDPI. So in order to render at the full device pixel resolution, we set a
    // scale transform on the root offscreen layer.
    AutoCATransaction transaction;
    mRenderer.layer.bounds = bounds;
    float scale = mLayerRoot->BackingScale();
    mRenderer.layer.sublayerTransform = CATransform3DMakeScale(scale, scale, 1);
    mRenderer.bounds = bounds;
  }

  mLayerRoot->CommitOffscreen();

  mGL->MakeCurrent();

  bool needToRedrawEverything = false;
  if (!mFB || mFB->mSize != aReadbackSize) {
    mFB = gl::MozFramebuffer::Create(mGL, aReadbackSize, 0, false);
    if (!mFB) {
      return false;
    }
    needToRedrawEverything = true;
  }

  const gl::ScopedBindFramebuffer bindFB(mGL, mFB->mFB);
  mGL->fViewport(0.0, 0.0, aReadbackSize.width, aReadbackSize.height);

  // These legacy OpenGL function calls are part of CARenderer's API contract, see CARenderer.h.
  // The size passed to glOrtho must be the device pixel size of the render target, otherwise
  // CARenderer will produce incorrect results.
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, aReadbackSize.width, 0.0, aReadbackSize.height, -1, 1);

  float mediaTime = CACurrentMediaTime();
  [mRenderer beginFrameAtTime:mediaTime timeStamp:nullptr];
  if (needToRedrawEverything) {
    [mRenderer addUpdateRect:bounds];
  }
  if (!CGRectIsEmpty([mRenderer updateBounds])) {
    // CARenderer assumes the layer tree is opaque. It only ever paints over existing content, it
    // never erases anything. However, our layer tree is not necessarily opaque. So we manually
    // erase the area that's going to be redrawn. This ensures correct rendering in the transparent
    // areas.
    //
    // Since we erase the bounds of the update area, this will erase more than necessary if the
    // update area is not a single rectangle. Unfortunately we cannot get the precise update region
    // from CARenderer, we can only get the bounds.
    CGRect updateBounds = [mRenderer updateBounds];
    gl::ScopedGLState scopedScissorTestState(mGL, LOCAL_GL_SCISSOR_TEST, true);
    gl::ScopedScissorRect scissor(mGL, updateBounds.origin.x, updateBounds.origin.y,
                                  updateBounds.size.width, updateBounds.size.height);
    mGL->fClearColor(0.0, 0.0, 0.0, 0.0);
    mGL->fClear(LOCAL_GL_COLOR_BUFFER_BIT);
    // We erased the update region's bounds. Make sure the entire update bounds get repainted.
    [mRenderer addUpdateRect:updateBounds];
  }
  [mRenderer render];
  [mRenderer endFrame];

  gl::ScopedPackState safePackState(mGL);
  mGL->fReadPixels(0.0f, 0.0f, aReadbackSize.width, aReadbackSize.height, LOCAL_GL_BGRA,
                   LOCAL_GL_UNSIGNED_BYTE, &aReadbackBuffer[0]);

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

NativeLayerCA::NativeLayerCA(bool aIsOpaque)
    : mMutex("NativeLayerCA"), mSurfacePoolHandle(nullptr), mIsOpaque(aIsOpaque) {}

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
}

void NativeLayerCA::AttachExternalImage(wr::RenderTextureHost* aExternalImage) {
  wr::RenderMacIOSurfaceTextureHostOGL* texture =
      aExternalImage->AsRenderMacIOSurfaceTextureHostOGL();
  MOZ_ASSERT(texture);
  mTextureHost = texture;
  mSize = texture->GetSize(0);
  mDisplayRect = IntRect(IntPoint{}, mSize);

  ForAllRepresentations([&](Representation& r) {
    r.mMutatedFrontSurface = true;
    r.mMutatedDisplayRect = true;
    r.mMutatedSize = true;
  });
}

void NativeLayerCA::SetSurfaceIsFlipped(bool aIsFlipped) {
  MutexAutoLock lock(mMutex);

  if (aIsFlipped != mSurfaceIsFlipped) {
    mSurfaceIsFlipped = aIsFlipped;
    ForAllRepresentations([&](Representation& r) { r.mMutatedSurfaceIsFlipped = true; });
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
    ForAllRepresentations([&](Representation& r) { r.mMutatedPosition = true; });
  }
}

IntPoint NativeLayerCA::GetPosition() {
  MutexAutoLock lock(mMutex);
  return mPosition;
}

void NativeLayerCA::SetTransform(const Matrix4x4& aTransform) {
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(aTransform.IsRectilinear());

  if (aTransform != mTransform) {
    mTransform = aTransform;
    ForAllRepresentations([&](Representation& r) { r.mMutatedTransform = true; });
  }
}

void NativeLayerCA::SetSamplingFilter(gfx::SamplingFilter aSamplingFilter) {
  MutexAutoLock lock(mMutex);

  if (aSamplingFilter != mSamplingFilter) {
    mSamplingFilter = aSamplingFilter;
    ForAllRepresentations([&](Representation& r) { r.mMutatedSamplingFilter = true; });
  }
}

Matrix4x4 NativeLayerCA::GetTransform() {
  MutexAutoLock lock(mMutex);
  return mTransform;
}

IntRect NativeLayerCA::GetRect() {
  MutexAutoLock lock(mMutex);
  return IntRect(mPosition, mSize);
}

void NativeLayerCA::SetBackingScale(float aBackingScale) {
  MutexAutoLock lock(mMutex);

  if (aBackingScale != mBackingScale) {
    mBackingScale = aBackingScale;
    ForAllRepresentations([&](Representation& r) { r.mMutatedBackingScale = true; });
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
    ForAllRepresentations([&](Representation& r) { r.mMutatedClipRect = true; });
  }
}

Maybe<gfx::IntRect> NativeLayerCA::ClipRect() {
  MutexAutoLock lock(mMutex);
  return mClipRect;
}

gfx::IntRect NativeLayerCA::CurrentSurfaceDisplayRect() {
  MutexAutoLock lock(mMutex);
  return mDisplayRect;
}

NativeLayerCA::Representation::~Representation() {
  [mContentCALayer release];
  [mOpaquenessTintLayer release];
  [mWrappingCALayer release];
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
void NativeLayerCA::HandlePartialUpdate(const MutexAutoLock& aLock, const IntRect& aDisplayRect,
                                        const IntRegion& aUpdateRegion, F&& aCopyFn) {
  MOZ_RELEASE_ASSERT(IntRect({}, mSize).Contains(aUpdateRegion.GetBounds()),
                     "The update region should be within the surface bounds.");
  MOZ_RELEASE_ASSERT(IntRect({}, mSize).Contains(aDisplayRect),
                     "The display rect should be within the surface bounds.");

  MOZ_RELEASE_ASSERT(!mInProgressUpdateRegion);
  MOZ_RELEASE_ASSERT(!mInProgressDisplayRect);
  mInProgressUpdateRegion = Some(aUpdateRegion);
  mInProgressDisplayRect = Some(aDisplayRect);

  InvalidateRegionThroughoutSwapchain(aLock, aUpdateRegion);

  if (mFrontSurface) {
    // Copy not-overwritten valid content from mFrontSurface so that valid content never gets lost.
    gfx::IntRegion copyRegion;
    copyRegion.Sub(mInProgressSurface->mInvalidRegion, aUpdateRegion);
    copyRegion.SubOut(mFrontSurface->mInvalidRegion);

    if (!copyRegion.IsEmpty()) {
      // Now copy the valid content, using a caller-provided copy function.
      aCopyFn(mFrontSurface->mSurface, copyRegion);
      mInProgressSurface->mInvalidRegion.SubOut(copyRegion);
    }
  }
}

RefPtr<gfx::DrawTarget> NativeLayerCA::NextSurfaceAsDrawTarget(const IntRect& aDisplayRect,
                                                               const IntRegion& aUpdateRegion,
                                                               gfx::BackendType aBackendType) {
  MutexAutoLock lock(mMutex);
  if (!NextSurface(lock)) {
    return nullptr;
  }

  mInProgressLockedIOSurface = new MacIOSurface(mInProgressSurface->mSurface);
  mInProgressLockedIOSurface->Lock(false);
  RefPtr<gfx::DrawTarget> dt = mInProgressLockedIOSurface->GetAsDrawTargetLocked(aBackendType);

  HandlePartialUpdate(
      lock, aDisplayRect, aUpdateRegion,
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

Maybe<GLuint> NativeLayerCA::NextSurfaceAsFramebuffer(const IntRect& aDisplayRect,
                                                      const IntRegion& aUpdateRegion,
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
      lock, aDisplayRect, aUpdateRegion,
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

  MOZ_RELEASE_ASSERT(mInProgressUpdateRegion);
  IOSurfaceDecrementUseCount(mInProgressSurface->mSurface.get());
  mFrontSurface = std::move(mInProgressSurface);
  mFrontSurface->mInvalidRegion.SubOut(mInProgressUpdateRegion.extract());
  ForAllRepresentations([&](Representation& r) { r.mMutatedFrontSurface = true; });

  MOZ_RELEASE_ASSERT(mInProgressDisplayRect);
  if (!mDisplayRect.IsEqualInterior(*mInProgressDisplayRect)) {
    mDisplayRect = *mInProgressDisplayRect;
    ForAllRepresentations([&](Representation& r) { r.mMutatedDisplayRect = true; });
  }
  mInProgressDisplayRect = Nothing();
  MOZ_RELEASE_ASSERT(mFrontSurface->mInvalidRegion.Intersect(mDisplayRect).IsEmpty(),
                     "Parts of the display rect are invalid! This shouldn't happen.");
}

void NativeLayerCA::DiscardBackbuffers() {
  MutexAutoLock lock(mMutex);

  for (const auto& surf : mSurfaces) {
    mSurfacePoolHandle->ReturnSurfaceToPool(surf.mEntry.mSurface);
  }
  mSurfaces.clear();
}

NativeLayerCA::Representation& NativeLayerCA::GetRepresentation(
    WhichRepresentation aRepresentation) {
  switch (aRepresentation) {
    case WhichRepresentation::ONSCREEN:
      return mOnscreenRepresentation;
    case WhichRepresentation::OFFSCREEN:
      return mOffscreenRepresentation;
  }
}

template <typename F>
void NativeLayerCA::ForAllRepresentations(F aFn) {
  aFn(mOnscreenRepresentation);
  aFn(mOffscreenRepresentation);
}

void NativeLayerCA::ApplyChanges(WhichRepresentation aRepresentation) {
  MutexAutoLock lock(mMutex);
  CFTypeRefPtr<IOSurfaceRef> surface;
  if (mFrontSurface) {
    surface = mFrontSurface->mSurface;
  } else if (mTextureHost) {
    surface = mTextureHost->GetSurface()->GetIOSurfaceRef();
  }
  GetRepresentation(aRepresentation)
      .ApplyChanges(mSize, mIsOpaque, mPosition, mTransform, mDisplayRect, mClipRect, mBackingScale,
                    mSurfaceIsFlipped, mSamplingFilter, surface);
}

CALayer* NativeLayerCA::UnderlyingCALayer(WhichRepresentation aRepresentation) {
  MutexAutoLock lock(mMutex);
  return GetRepresentation(aRepresentation).UnderlyingCALayer();
}

void NativeLayerCA::Representation::ApplyChanges(
    const IntSize& aSize, bool aIsOpaque, const IntPoint& aPosition, const Matrix4x4& aTransform,
    const IntRect& aDisplayRect, const Maybe<IntRect>& aClipRect, float aBackingScale,
    bool aSurfaceIsFlipped, gfx::SamplingFilter aSamplingFilter,
    CFTypeRefPtr<IOSurfaceRef> aFrontSurface) {
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
    mContentCALayer.bounds = CGRectMake(0, 0, aSize.width, aSize.height);
    mContentCALayer.opaque = aIsOpaque;
    if ([mContentCALayer respondsToSelector:@selector(setContentsOpaque:)]) {
      // The opaque property seems to not be enough when using IOSurface contents.
      // Additionally, call the private method setContentsOpaque.
      [mContentCALayer setContentsOpaque:aIsOpaque];
    }
    [mWrappingCALayer addSublayer:mContentCALayer];
  }

  bool shouldTintOpaqueness = StaticPrefs::gfx_core_animation_tint_opaque();
  if (shouldTintOpaqueness && !mOpaquenessTintLayer) {
    mOpaquenessTintLayer = [[CALayer layer] retain];
    mOpaquenessTintLayer.position = NSZeroPoint;
    mOpaquenessTintLayer.bounds = mContentCALayer.bounds;
    mOpaquenessTintLayer.anchorPoint = NSZeroPoint;
    mOpaquenessTintLayer.contentsGravity = kCAGravityTopLeft;
    if (aIsOpaque) {
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

  if (mMutatedBackingScale || mMutatedSize) {
    mContentCALayer.bounds =
        CGRectMake(0, 0, aSize.width / aBackingScale, aSize.height / aBackingScale);
    if (mOpaquenessTintLayer) {
      mOpaquenessTintLayer.bounds = mContentCALayer.bounds;
    }
    mContentCALayer.contentsScale = aBackingScale;
  }

  if (mMutatedBackingScale || mMutatedPosition || mMutatedDisplayRect || mMutatedClipRect ||
      mMutatedTransform || mMutatedSurfaceIsFlipped || mMutatedSize) {
    Maybe<IntRect> clipFromDisplayRect;
    if (!aDisplayRect.IsEqualInterior(IntRect({}, aSize))) {
      // When the display rect is a subset of the layer, then we want to guarantee that no
      // pixels outside that rect are sampled, since they might be uninitialized.
      // Transforming the display rect into a post-transform clip only maintains this if
      // it's an integer translation, which is all we support for this case currently.
      MOZ_ASSERT(aTransform.Is2DIntegerTranslation());
      clipFromDisplayRect =
          Some(RoundedToInt(aTransform.TransformBounds(IntRectToRect(aDisplayRect + aPosition))));
    }

    auto effectiveClip = IntersectMaybeRects(aClipRect, clipFromDisplayRect);
    auto globalClipOrigin = effectiveClip ? effectiveClip->TopLeft() : IntPoint();
    auto clipToLayerOffset = -globalClipOrigin;

    mWrappingCALayer.position =
        CGPointMake(globalClipOrigin.x / aBackingScale, globalClipOrigin.y / aBackingScale);

    if (effectiveClip) {
      mWrappingCALayer.masksToBounds = YES;
      mWrappingCALayer.bounds = CGRectMake(0, 0, effectiveClip->Width() / aBackingScale,
                                           effectiveClip->Height() / aBackingScale);
    } else {
      mWrappingCALayer.masksToBounds = NO;
    }

    Matrix4x4 transform = aTransform;
    transform.PreTranslate(aPosition.x, aPosition.y, 0);
    transform.PostTranslate(clipToLayerOffset.x, clipToLayerOffset.y, 0);

    if (aSurfaceIsFlipped) {
      transform.PreTranslate(0, aSize.height, 0).PreScale(1, -1, 1);
    }

    CATransform3D transformCA{transform._11,
                              transform._12,
                              transform._13,
                              transform._14,
                              transform._21,
                              transform._22,
                              transform._23,
                              transform._24,
                              transform._31,
                              transform._32,
                              transform._33,
                              transform._34,
                              transform._41 / aBackingScale,
                              transform._42 / aBackingScale,
                              transform._43,
                              transform._44};
    mContentCALayer.transform = transformCA;
    if (mOpaquenessTintLayer) {
      mOpaquenessTintLayer.transform = mContentCALayer.transform;
    }
  }

  if (mMutatedFrontSurface) {
    mContentCALayer.contents = (id)aFrontSurface.get();
  }

  if (mMutatedSamplingFilter) {
    if (aSamplingFilter == gfx::SamplingFilter::POINT) {
      mContentCALayer.minificationFilter = kCAFilterNearest;
      mContentCALayer.magnificationFilter = kCAFilterNearest;
    } else {
      mContentCALayer.minificationFilter = kCAFilterLinear;
      mContentCALayer.magnificationFilter = kCAFilterLinear;
    }
  }

  mMutatedPosition = false;
  mMutatedTransform = false;
  mMutatedBackingScale = false;
  mMutatedSize = false;
  mMutatedSurfaceIsFlipped = false;
  mMutatedDisplayRect = false;
  mMutatedClipRect = false;
  mMutatedFrontSurface = false;
  mMutatedSamplingFilter = false;
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
