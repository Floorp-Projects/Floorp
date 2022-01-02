/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nullptr; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/NativeLayerCA.h"

#import <AppKit/NSAnimationContext.h>
#import <AppKit/NSColor.h>
#import <AVFoundation/AVFoundation.h>
#import <OpenGL/gl.h>
#import <QuartzCore/QuartzCore.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include "gfxUtils.h"
#include "GLBlitHelper.h"
#include "GLContextCGL.h"
#include "GLContextProvider.h"
#include "MozFramebuffer.h"
#include "mozilla/gfx/Swizzle.h"
#include "mozilla/layers/ScreenshotGrabber.h"
#include "mozilla/layers/SurfacePoolCA.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/webrender/RenderMacIOSurfaceTextureHost.h"
#include "nsCocoaFeatures.h"
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
using gfx::DataSourceSurface;
using gfx::Matrix4x4;
using gfx::SurfaceFormat;
using gl::GLContext;
using gl::GLContextCGL;

// Utility classes for NativeLayerRootSnapshotter (NLRS) profiler screenshots.

class RenderSourceNLRS : public profiler_screenshots::RenderSource {
 public:
  explicit RenderSourceNLRS(UniquePtr<gl::MozFramebuffer>&& aFramebuffer)
      : RenderSource(aFramebuffer->mSize), mFramebuffer(std::move(aFramebuffer)) {}
  auto& FB() { return *mFramebuffer; }

 protected:
  UniquePtr<gl::MozFramebuffer> mFramebuffer;
};

class DownscaleTargetNLRS : public profiler_screenshots::DownscaleTarget {
 public:
  DownscaleTargetNLRS(gl::GLContext* aGL, UniquePtr<gl::MozFramebuffer>&& aFramebuffer)
      : profiler_screenshots::DownscaleTarget(aFramebuffer->mSize),
        mGL(aGL),
        mRenderSource(new RenderSourceNLRS(std::move(aFramebuffer))) {}
  already_AddRefed<profiler_screenshots::RenderSource> AsRenderSource() override {
    return do_AddRef(mRenderSource);
  };
  bool DownscaleFrom(profiler_screenshots::RenderSource* aSource, const IntRect& aSourceRect,
                     const IntRect& aDestRect) override;

 protected:
  RefPtr<gl::GLContext> mGL;
  RefPtr<RenderSourceNLRS> mRenderSource;
};

class AsyncReadbackBufferNLRS : public profiler_screenshots::AsyncReadbackBuffer {
 public:
  AsyncReadbackBufferNLRS(gl::GLContext* aGL, const IntSize& aSize, GLuint aBufferHandle)
      : profiler_screenshots::AsyncReadbackBuffer(aSize), mGL(aGL), mBufferHandle(aBufferHandle) {}
  void CopyFrom(profiler_screenshots::RenderSource* aSource) override;
  bool MapAndCopyInto(DataSourceSurface* aSurface, const IntSize& aReadSize) override;

 protected:
  virtual ~AsyncReadbackBufferNLRS();
  RefPtr<gl::GLContext> mGL;
  GLuint mBufferHandle = 0;
};

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
      mOffscreenRepresentation(MakeOffscreenRootCALayer()) {
  mLastMouseMoveTime = TimeStamp::NowLoRes();
}

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
  layerCA->SetRootWindowIsFullscreen(mWindowIsFullscreen);
  ForAllRepresentations([&](Representation& r) { r.mMutatedLayerStructure = true; });
}

void NativeLayerRootCA::RemoveLayer(NativeLayer* aLayer) {
  MutexAutoLock lock(mMutex);

  RefPtr<NativeLayerCA> layerCA = aLayer->AsNativeLayerCA();
  MOZ_RELEASE_ASSERT(layerCA);

  mSublayers.RemoveElement(layerCA);
  ForAllRepresentations([&](Representation& r) { r.mMutatedLayerStructure = true; });
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
    layerCA->SetRootWindowIsFullscreen(mWindowIsFullscreen);
    layersCA.AppendElement(std::move(layerCA));
  }

  if (layersCA != mSublayers) {
    mSublayers = std::move(layersCA);
    ForAllRepresentations([&](Representation& r) { r.mMutatedLayerStructure = true; });
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
  {
    MutexAutoLock lock(mMutex);

    if (!NS_IsMainThread() && mOffMainThreadCommitsSuspended) {
      mCommitPending = true;
      return false;
    }

    UpdateMouseMovedRecently(lock);
    mOnscreenRepresentation.Commit(WhichRepresentation::ONSCREEN, mSublayers, mWindowIsFullscreen,
                                   mMouseMovedRecently);

    mCommitPending = false;
  }

  if (StaticPrefs::gfx_webrender_debug_dump_native_layer_tree_to_file()) {
    static uint32_t sFrameID = 0;
    uint32_t frameID = sFrameID++;

    NSString* dirPath =
        [NSString stringWithFormat:@"%@/Desktop/nativelayerdumps-%d", NSHomeDirectory(), getpid()];
    if ([NSFileManager.defaultManager createDirectoryAtPath:dirPath
                                withIntermediateDirectories:YES
                                                 attributes:nil
                                                      error:nullptr]) {
      NSString* filename = [NSString stringWithFormat:@"frame-%d.html", frameID];
      NSString* filePath = [dirPath stringByAppendingPathComponent:filename];
      DumpLayerTreeToFile([filePath UTF8String]);
    } else {
      NSLog(@"Failed to create directory %@", dirPath);
    }
  }

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
  mOffscreenRepresentation.Commit(WhichRepresentation::OFFSCREEN, mSublayers, mWindowIsFullscreen,
                                  false);
}

template <typename F>
void NativeLayerRootCA::ForAllRepresentations(F aFn) {
  aFn(mOnscreenRepresentation);
  aFn(mOffscreenRepresentation);
}

NativeLayerRootCA::Representation::Representation(CALayer* aRootCALayer)
    : mRootCALayer([aRootCALayer retain]) {}

NativeLayerRootCA::Representation::~Representation() {
  if (mMutatedLayerStructure) {
    // Clear the root layer's sublayers. At this point the window is usually closed, so this
    // transaction does not cause any screen updates.
    AutoCATransaction transaction;
    mRootCALayer.sublayers = @[];
  }

  [mRootCALayer release];
}

void NativeLayerRootCA::Representation::Commit(WhichRepresentation aRepresentation,
                                               const nsTArray<RefPtr<NativeLayerCA>>& aSublayers,
                                               bool aWindowIsFullscreen, bool aMouseMovedRecently) {
  bool mightIsolate = (aRepresentation == WhichRepresentation::ONSCREEN &&
                       StaticPrefs::gfx_core_animation_specialize_video());
  bool mustRebuild = (mMutatedLayerStructure || (mightIsolate && mMutatedMouseMovedRecently));
  if (!mustRebuild) {
    // Check which type of update we need to do, if any.
    NativeLayerCA::UpdateType updateRequired = NativeLayerCA::UpdateType::None;

    for (auto layer : aSublayers) {
      // Use the ordering of our UpdateType enums to build a maximal update type.
      updateRequired = std::max(updateRequired, layer->HasUpdate(aRepresentation));
      if (updateRequired == NativeLayerCA::UpdateType::All) {
        break;
      }
    }

    if (updateRequired == NativeLayerCA::UpdateType::None) {
      // Nothing more needed, so early exit.
      return;
    }

    if (updateRequired == NativeLayerCA::UpdateType::OnlyVideo) {
      bool allUpdatesSucceeded = std::all_of(
          aSublayers.begin(), aSublayers.end(), [=](const RefPtr<NativeLayerCA>& layer) {
            return layer->ApplyChanges(aRepresentation, NativeLayerCA::UpdateType::OnlyVideo);
          });

      if (allUpdatesSucceeded) {
        // Nothing more needed, so early exit;
        return;
      }
    }
  }

  // We're going to do a full update now, which requires a transaction.
  AutoCATransaction transaction;
  for (auto layer : aSublayers) {
    mustRebuild |= layer->WillUpdateAffectLayers(aRepresentation);
    layer->ApplyChanges(aRepresentation, NativeLayerCA::UpdateType::All);
  }

  if (mustRebuild) {
    // Bug 1731821 should eliminate this most of this logic and allow us to unconditionally
    // accept aSublayers.

    // We're going to check for an opportunity to isolate the topmost video layer. We'll avoid
    // modifying mRootCALayer.sublayers unless we absolutely must, which will avoid flickering
    // in the CATranscation. We check aSublayers with 3 possible outcomes.
    // 1) We can't isolate video, so accept the provided sublayers.
    // 2) We can isolate video, and we weren't isolating that video before, so create our own
    //    sublayers with the proper structure.
    // 3) We can isolate video, and we were already doing that. The sublayers underneath the
    //    video might have changed in some way that doesn't prevent isolation, so ignore them.
    //    Leave our sublayers unchanged.

    // Define a block we'll use to accept the provided sublayers if we must. In the different
    // cases, we'll call this at different times.
    void (^acceptProvidedSublayers)() = ^() {
      NSMutableArray<CALayer*>* sublayers = [NSMutableArray arrayWithCapacity:aSublayers.Length()];
      for (auto layer : aSublayers) {
        [sublayers addObject:layer->UnderlyingCALayer(aRepresentation)];
      }
      mRootCALayer.sublayers = sublayers;
    };

    // See if the top layer is already rooted in our mRootCALayer. If it is, then we can check
    // for isolation without first disrupting our sublayers.
    bool topLayerIsRooted =
        aSublayers.Length() &&
        (aSublayers.LastElement()->UnderlyingCALayer(aRepresentation).superlayer == mRootCALayer);

    if (!topLayerIsRooted) {
      // We have to accept the provided sublayers. We may still isolate, but it's because the
      // new topmost layer was not already isolated. This is an acceptable time to potentially
      // flicker as the sublayers are changed.
      acceptProvidedSublayers();
    }

    // Now that we've confirmed these layer relationships, we check to see if we should break
    // them and isolate a single video layer. It's important that the topmost layer is a
    // child of mRootCALayer for this logic to work.
    MOZ_DIAGNOSTIC_ASSERT(
        !aSublayers.Length() ||
            (aSublayers.LastElement()->UnderlyingCALayer(aRepresentation).superlayer ==
             mRootCALayer),
        "The topmost layer must be a child of mRootCALayer.");

    bool didIsolate = false;
    if (mightIsolate && aWindowIsFullscreen && !aMouseMovedRecently) {
      CALayer* isolatedLayer = FindVideoLayerToIsolate(aRepresentation, aSublayers);
      if (isolatedLayer) {
        // No matter what happens next, we did choose to isolate.
        didIsolate = true;

        // We only need to change our sublayers if we weren't already isolating, or
        // if the isolatedLayer does not match our current top layer.
        if (!mIsIsolatingVideo || isolatedLayer != mRootCALayer.sublayers.lastObject) {
          // Create a full coverage black layer behind the isolated layer.
          CGFloat rootWidth = mRootCALayer.bounds.size.width;
          CGFloat rootHeight = mRootCALayer.bounds.size.height;

          // Reaching the low-power mode requires that there is a single black layer
          // covering the entire window behind the video layer. Create that layer.
          CALayer* blackLayer = [CALayer layer];
          blackLayer.position = NSZeroPoint;
          blackLayer.anchorPoint = NSZeroPoint;
          blackLayer.bounds = CGRectMake(0, 0, rootWidth, rootHeight);
          blackLayer.backgroundColor = [[NSColor blackColor] CGColor];

          mRootCALayer.sublayers = @[ blackLayer, isolatedLayer ];
        }
      }
    }

    // If we didn't accept the sublayers earlier, and we decided we couldn't isolate,
    // accept them now.
    if (topLayerIsRooted && !didIsolate) {
      acceptProvidedSublayers();
    }

    mIsIsolatingVideo = didIsolate;
  }

  mMutatedLayerStructure = false;
  mMutatedMouseMovedRecently = false;
}

CALayer* NativeLayerRootCA::Representation::FindVideoLayerToIsolate(
    WhichRepresentation aRepresentation, const nsTArray<RefPtr<NativeLayerCA>>& aSublayers) {
  // Run a heuristic to determine if any one of aSublayers is a video layer that should be
  // isolated. These layers are ordered back-to-front. This function will return a candidate
  // CALayer if all of the following are true:
  // 1) The candidate layer is the topmost layer, and is a video layer.
  // 2) The candidate layer bounds covers at least 80% of the bounds of the root layer.
  // 3) The candidate layer center is "near" the center of the root layer.
  // 4) There are no other video layers other than the candidate layer.
  // Notably, this heuristic doesn't check the contents or bounds of layers that appear
  // before the candidate layer. This means that video layers on top of other content
  // may be selected for isolation and that other content will be replaced with a black
  // background.
  // Bug 1731136 will make this heuristic simpler, or completely unnecessary.

  auto topLayer = aSublayers.LastElement();
  if (!topLayer || !topLayer->IsVideo()) {
    // FAIL Step 1: the topmost layer is not video.
    return nil;
  }

  CALayer* candidateLayer = topLayer->UnderlyingCALayer(aRepresentation);
  MOZ_ASSERT(candidateLayer);

  // Check coverage of the candidate layer's bounds. We need the size of the root
  // layer to do this.
  CGFloat rootWidth = mRootCALayer.bounds.size.width;
  CGFloat rootHeight = mRootCALayer.bounds.size.height;
  CGFloat rootArea = rootWidth * rootHeight;
  CGFloat minimumRootArea = rootArea * 0.8;

  // Translate the candidate layer bounds into root layer space.
  CGRect candidateBoundsInRoot = [mRootCALayer convertRect:candidateLayer.bounds
                                                 fromLayer:candidateLayer];
  CGFloat candidateArea = candidateBoundsInRoot.size.width * candidateBoundsInRoot.size.height;
  if (candidateArea < minimumRootArea) {
    // FAIL Step 2: the candidate layer is not big enough.
    return nil;
  }

  // Check center of the candidate layer, relative to the root layer's center.
  CGFloat centerZoneWidth = rootWidth * 0.05;
  CGFloat centerZoneHeight = rootHeight * 0.05;
  CGRect centerZone =
      CGRectMake((rootWidth * 0.5) - (centerZoneWidth * 0.5),
                 (rootHeight * 0.5) - (centerZoneHeight * 0.5), centerZoneWidth, centerZoneHeight);
  CGPoint candidateCenterInRoot =
      CGPointMake(candidateBoundsInRoot.origin.x + (candidateBoundsInRoot.size.width * 0.5),
                  candidateBoundsInRoot.origin.y + (candidateBoundsInRoot.size.height * 0.5));
  if (!CGRectContainsPoint(centerZone, candidateCenterInRoot)) {
    // FAIL Step 3: the candidate layer is off-center.
    return nil;
  }

  // See if there are any other video layers behind the candidate layer.
  for (auto layer : aSublayers) {
    if (layer->IsVideo() && layer != topLayer) {
      // FAIL Step 4: there are multiple video layers.
      return nil;
    }
  }

  return candidateLayer;
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

void NativeLayerRootCA::DumpLayerTreeToFile(const char* aPath) {
  MutexAutoLock lock(mMutex);
  NSLog(@"Dumping NativeLayer contents to %s", aPath);
  std::ofstream fileOutput(aPath);
  if (fileOutput.fail()) {
    NSLog(@"Opening %s for writing failed.", aPath);
  }

  // Make sure floating point values use a period for the decimal separator.
  fileOutput.imbue(std::locale("C"));

  fileOutput << "<html>\n";
  for (const auto& layer : mSublayers) {
    layer->DumpLayer(fileOutput);
  }
  fileOutput << "</html>\n";
  fileOutput.close();
}

void NativeLayerRootCA::SetWindowIsFullscreen(bool aFullscreen) {
  MutexAutoLock lock(mMutex);

  if (mWindowIsFullscreen != aFullscreen) {
    mWindowIsFullscreen = aFullscreen;

    for (auto layer : mSublayers) {
      layer->SetRootWindowIsFullscreen(mWindowIsFullscreen);
    }
  }

  // Treat this as a mouse move, for purposes of resetting our timer.
  mLastMouseMoveTime = TimeStamp::NowLoRes();
}

void NativeLayerRootCA::NoteMouseMoveAtTime(const TimeStamp& aTime) {
  MutexAutoLock lock(mMutex);
  mLastMouseMoveTime = aTime;
}

void NativeLayerRootCA::UpdateMouseMovedRecently(const MutexAutoLock& aProofOfLock) {
  static const double SECONDS_TO_WAIT = 2.0;

  bool newMouseMovedRecently =
      ((TimeStamp::NowLoRes() - mLastMouseMoveTime).ToSeconds() < SECONDS_TO_WAIT);

  if (newMouseMovedRecently != mMouseMovedRecently) {
    mMouseMovedRecently = newMouseMovedRecently;

    ForAllRepresentations([&](Representation& r) { r.mMutatedMouseMovedRecently = true; });
  }
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

already_AddRefed<profiler_screenshots::RenderSource>
NativeLayerRootSnapshotterCA::GetWindowContents(const IntSize& aWindowSize) {
  UpdateSnapshot(aWindowSize);
  return do_AddRef(mSnapshot);
}

void NativeLayerRootSnapshotterCA::UpdateSnapshot(const IntSize& aSize) {
  CGRect bounds = CGRectMake(0, 0, aSize.width, aSize.height);

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
  if (!mSnapshot || mSnapshot->Size() != aSize) {
    mSnapshot = nullptr;
    auto fb = gl::MozFramebuffer::Create(mGL, aSize, 0, false);
    if (!fb) {
      return;
    }
    mSnapshot = new RenderSourceNLRS(std::move(fb));
    needToRedrawEverything = true;
  }

  const gl::ScopedBindFramebuffer bindFB(mGL, mSnapshot->FB().mFB);
  mGL->fViewport(0.0, 0.0, aSize.width, aSize.height);

  // These legacy OpenGL function calls are part of CARenderer's API contract, see CARenderer.h.
  // The size passed to glOrtho must be the device pixel size of the render target, otherwise
  // CARenderer will produce incorrect results.
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, aSize.width, 0.0, aSize.height, -1, 1);

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
}

bool NativeLayerRootSnapshotterCA::ReadbackPixels(const IntSize& aReadbackSize,
                                                  SurfaceFormat aReadbackFormat,
                                                  const Range<uint8_t>& aReadbackBuffer) {
  if (aReadbackFormat != SurfaceFormat::B8G8R8A8) {
    return false;
  }

  UpdateSnapshot(aReadbackSize);
  if (!mSnapshot) {
    return false;
  }

  const gl::ScopedBindFramebuffer bindFB(mGL, mSnapshot->FB().mFB);
  gl::ScopedPackState safePackState(mGL);
  mGL->fReadPixels(0.0f, 0.0f, aReadbackSize.width, aReadbackSize.height, LOCAL_GL_BGRA,
                   LOCAL_GL_UNSIGNED_BYTE, &aReadbackBuffer[0]);

  return true;
}

already_AddRefed<profiler_screenshots::DownscaleTarget>
NativeLayerRootSnapshotterCA::CreateDownscaleTarget(const IntSize& aSize) {
  auto fb = gl::MozFramebuffer::Create(mGL, aSize, 0, false);
  if (!fb) {
    return nullptr;
  }
  RefPtr<profiler_screenshots::DownscaleTarget> dt = new DownscaleTargetNLRS(mGL, std::move(fb));
  return dt.forget();
}

already_AddRefed<profiler_screenshots::AsyncReadbackBuffer>
NativeLayerRootSnapshotterCA::CreateAsyncReadbackBuffer(const IntSize& aSize) {
  size_t bufferByteCount = aSize.width * aSize.height * 4;
  GLuint bufferHandle = 0;
  mGL->fGenBuffers(1, &bufferHandle);

  gl::ScopedPackState scopedPackState(mGL);
  mGL->fBindBuffer(LOCAL_GL_PIXEL_PACK_BUFFER, bufferHandle);
  mGL->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 1);
  mGL->fBufferData(LOCAL_GL_PIXEL_PACK_BUFFER, bufferByteCount, nullptr, LOCAL_GL_STREAM_READ);
  return MakeAndAddRef<AsyncReadbackBufferNLRS>(mGL, aSize, bufferHandle);
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
  MutexAutoLock lock(mMutex);

  wr::RenderMacIOSurfaceTextureHost* texture = aExternalImage->AsRenderMacIOSurfaceTextureHost();
  MOZ_ASSERT(texture);
  mTextureHost = texture;

  gfx::IntSize oldSize = mSize;
  mSize = texture->GetSize(0);
  bool changedSizeAndDisplayRect = (mSize != oldSize);

  mDisplayRect = IntRect(IntPoint{}, mSize);

  bool oldSpecializeVideo = mSpecializeVideo;
  mSpecializeVideo = ShouldSpecializeVideo(lock);
  bool changedSpecializeVideo = (mSpecializeVideo != oldSpecializeVideo);

  ForAllRepresentations([&](Representation& r) {
    r.mMutatedFrontSurface = true;
    r.mMutatedDisplayRect |= changedSizeAndDisplayRect;
    r.mMutatedSize |= changedSizeAndDisplayRect;
    r.mMutatedSpecializeVideo |= changedSpecializeVideo;
  });
}

bool NativeLayerCA::IsVideo() {
  // Anything with a texture host is considered a video source.
  return mTextureHost;
}

bool NativeLayerCA::IsVideoAndLocked(const MutexAutoLock& aProofOfLock) {
  // Anything with a texture host is considered a video source.
  return mTextureHost;
}

bool NativeLayerCA::ShouldSpecializeVideo(const MutexAutoLock& aProofOfLock) {
  return StaticPrefs::gfx_core_animation_specialize_video() &&
         nsCocoaFeatures::OnHighSierraOrLater() && mRootWindowIsFullscreen &&
         IsVideoAndLocked(aProofOfLock);
}

void NativeLayerCA::SetRootWindowIsFullscreen(bool aFullscreen) {
  MutexAutoLock lock(mMutex);

  mRootWindowIsFullscreen = aFullscreen;

  bool oldSpecializeVideo = mSpecializeVideo;
  mSpecializeVideo = ShouldSpecializeVideo(lock);

  if (mSpecializeVideo != oldSpecializeVideo) {
    ForAllRepresentations([&](Representation& r) { r.mMutatedSpecializeVideo = true; });
  }
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
  // mIsOpaque is const, so no need for a lock.
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

void NativeLayerCA::DumpLayer(std::ostream& aOutputStream) {
  MutexAutoLock lock(mMutex);

  auto size = gfx::Size(mSize) / mBackingScale;

  Maybe<IntRect> clipFromDisplayRect;
  if (!mDisplayRect.IsEqualInterior(IntRect({}, mSize))) {
    // When the display rect is a subset of the layer, then we want to guarantee that no
    // pixels outside that rect are sampled, since they might be uninitialized.
    // Transforming the display rect into a post-transform clip only maintains this if
    // it's an integer translation, which is all we support for this case currently.
    MOZ_ASSERT(mTransform.Is2DIntegerTranslation());
    clipFromDisplayRect =
        Some(RoundedToInt(mTransform.TransformBounds(IntRectToRect(mDisplayRect + mPosition))));
  }

  auto effectiveClip = IntersectMaybeRects(mClipRect, clipFromDisplayRect);
  auto globalClipOrigin = effectiveClip ? effectiveClip->TopLeft() : IntPoint();
  auto clipToLayerOffset = -globalClipOrigin;

  auto wrappingDivPosition = gfx::Point(globalClipOrigin) / mBackingScale;

  aOutputStream << "<div style=\"";
  aOutputStream << "position: absolute; ";
  aOutputStream << "left: " << wrappingDivPosition.x << "px; ";
  aOutputStream << "top: " << wrappingDivPosition.y << "px; ";

  if (effectiveClip) {
    auto wrappingDivSize = gfx::Size(effectiveClip->Size()) / mBackingScale;
    aOutputStream << "overflow: hidden; ";
    aOutputStream << "width: " << wrappingDivSize.width << "px; ";
    aOutputStream << "height: " << wrappingDivSize.height << "px; ";
  }

  Matrix4x4 transform = mTransform;
  transform.PreTranslate(mPosition.x, mPosition.y, 0);
  transform.PostTranslate(clipToLayerOffset.x, clipToLayerOffset.y, 0);

  if (mSurfaceIsFlipped) {
    transform.PreTranslate(0, mSize.height, 0).PreScale(1, -1, 1);
  }

  aOutputStream << "\">";
  aOutputStream << "<img style=\"";
  aOutputStream << "width: " << size.width << "px; ";
  aOutputStream << "height: " << size.height << "px; ";

  if (mSamplingFilter == gfx::SamplingFilter::POINT) {
    aOutputStream << "image-rendering: crisp-edges; ";
  }

  if (!transform.IsIdentity()) {
    const auto& m = transform;
    aOutputStream << "transform-origin: top left; ";
    aOutputStream << "transform: matrix3d(";
    aOutputStream << m._11 << ", " << m._12 << ", " << m._13 << ", " << m._14 << ", ";
    aOutputStream << m._21 << ", " << m._22 << ", " << m._23 << ", " << m._24 << ", ";
    aOutputStream << m._31 << ", " << m._32 << ", " << m._33 << ", " << m._34 << ", ";
    aOutputStream << m._41 / mBackingScale << ", " << m._42 / mBackingScale << ", " << m._43 << ", "
                  << m._44;
    aOutputStream << "); ";
  }
  aOutputStream << "\" ";

  CFTypeRefPtr<IOSurfaceRef> surface;
  if (mFrontSurface) {
    surface = mFrontSurface->mSurface;
    aOutputStream << "alt=\"regular surface 0x" << std::hex << int(IOSurfaceGetID(surface.get()))
                  << "\" ";
  } else if (mTextureHost) {
    surface = mTextureHost->GetSurface()->GetIOSurfaceRef();
    aOutputStream << "alt=\"TextureHost surface 0x" << std::hex
                  << int(IOSurfaceGetID(surface.get())) << "\" ";
  } else {
    aOutputStream << "alt=\"no surface 0x\" ";
  }

  aOutputStream << "src=\"";

  if (surface) {
    RefPtr<MacIOSurface> surf = new MacIOSurface(surface);
    surf->Lock(true);
    {
      RefPtr<gfx::DrawTarget> dt = surf->GetAsDrawTargetLocked(gfx::BackendType::SKIA);
      if (dt) {
        RefPtr<gfx::SourceSurface> sourceSurf = dt->Snapshot();
        nsCString dataUrl;
        gfxUtils::EncodeSourceSurface(sourceSurf, ImageType::PNG, u""_ns, gfxUtils::eDataURIEncode,
                                      nullptr, &dataUrl);
        aOutputStream << dataUrl.get();
      }
    }
    surf->Unlock(true);
  }

  aOutputStream << "\"/></div>\n";
}

gfx::IntRect NativeLayerCA::CurrentSurfaceDisplayRect() {
  MutexAutoLock lock(mMutex);
  return mDisplayRect;
}

NativeLayerCA::Representation::Representation()
    : mMutatedPosition(true),
      mMutatedTransform(true),
      mMutatedDisplayRect(true),
      mMutatedClipRect(true),
      mMutatedBackingScale(true),
      mMutatedSize(true),
      mMutatedSurfaceIsFlipped(true),
      mMutatedFrontSurface(true),
      mMutatedSamplingFilter(true),
      mMutatedSpecializeVideo(true) {}

NativeLayerCA::Representation::~Representation() {
  [mContentCALayer release];
  [mOpaquenessTintLayer release];
  [mWrappingCALayer release];
}

void NativeLayerCA::InvalidateRegionThroughoutSwapchain(const MutexAutoLock& aProofOfLock,
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

bool NativeLayerCA::NextSurface(const MutexAutoLock& aProofOfLock) {
  if (mSize.IsEmpty()) {
    gfxCriticalError() << "NextSurface returning false because of invalid mSize (" << mSize.width
                       << ", " << mSize.height << ").";
    return false;
  }

  MOZ_RELEASE_ASSERT(
      !mInProgressSurface,
      "ERROR: Do not call NextSurface twice in sequence. Call NotifySurfaceReady before the "
      "next call to NextSurface.");

  Maybe<SurfaceWithInvalidRegion> surf = GetUnusedSurfaceAndCleanUp(aProofOfLock);
  if (!surf) {
    CFTypeRefPtr<IOSurfaceRef> newSurf = mSurfacePoolHandle->ObtainSurfaceFromPool(mSize);
    MOZ_RELEASE_ASSERT(newSurf, "NextSurface IOSurfaceCreate failed to create the surface.");
    surf = Some(SurfaceWithInvalidRegion{newSurf, IntRect({}, mSize)});
  }

  mInProgressSurface = std::move(surf);
  IOSurfaceIncrementUseCount(mInProgressSurface->mSurface.get());
  return true;
}

template <typename F>
void NativeLayerCA::HandlePartialUpdate(const MutexAutoLock& aProofOfLock,
                                        const IntRect& aDisplayRect, const IntRegion& aUpdateRegion,
                                        F&& aCopyFn) {
  MOZ_RELEASE_ASSERT(IntRect({}, mSize).Contains(aUpdateRegion.GetBounds()),
                     "The update region should be within the surface bounds.");
  MOZ_RELEASE_ASSERT(IntRect({}, mSize).Contains(aDisplayRect),
                     "The display rect should be within the surface bounds.");

  MOZ_RELEASE_ASSERT(!mInProgressUpdateRegion);
  MOZ_RELEASE_ASSERT(!mInProgressDisplayRect);
  mInProgressUpdateRegion = Some(aUpdateRegion);
  mInProgressDisplayRect = Some(aDisplayRect);

  InvalidateRegionThroughoutSwapchain(aProofOfLock, aUpdateRegion);

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
  MOZ_RELEASE_ASSERT(NextSurface(lock), "NextSurfaceAsFramebuffer needs a surface.");

  Maybe<GLuint> fbo =
      mSurfacePoolHandle->GetFramebufferForSurface(mInProgressSurface->mSurface, aNeedsDepth);
  MOZ_RELEASE_ASSERT(fbo, "GetFramebufferForSurface failed.");

  HandlePartialUpdate(
      lock, aDisplayRect, aUpdateRegion,
      [&](CFTypeRefPtr<IOSurfaceRef> validSource, const gfx::IntRegion& copyRegion) {
        // Copy copyRegion from validSource to fbo.
        MOZ_RELEASE_ASSERT(mSurfacePoolHandle->gl());
        mSurfacePoolHandle->gl()->MakeCurrent();
        Maybe<GLuint> sourceFBO = mSurfacePoolHandle->GetFramebufferForSurface(validSource, false);
        MOZ_RELEASE_ASSERT(sourceFBO,
                           "GetFramebufferForSurface failed during HandlePartialUpdate.");
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

NativeLayerCA::UpdateType NativeLayerCA::HasUpdate(WhichRepresentation aRepresentation) {
  MutexAutoLock lock(mMutex);
  return GetRepresentation(aRepresentation).HasUpdate(IsVideoAndLocked(lock));
}

bool NativeLayerCA::ApplyChanges(WhichRepresentation aRepresentation,
                                 NativeLayerCA::UpdateType aUpdate) {
  MutexAutoLock lock(mMutex);
  CFTypeRefPtr<IOSurfaceRef> surface;
  if (mFrontSurface) {
    surface = mFrontSurface->mSurface;
  } else if (mTextureHost) {
    surface = mTextureHost->GetSurface()->GetIOSurfaceRef();
  }
  return GetRepresentation(aRepresentation)
      .ApplyChanges(aUpdate, mSize, mIsOpaque, mPosition, mTransform, mDisplayRect, mClipRect,
                    mBackingScale, mSurfaceIsFlipped, mSamplingFilter, mSpecializeVideo, surface);
}

CALayer* NativeLayerCA::UnderlyingCALayer(WhichRepresentation aRepresentation) {
  MutexAutoLock lock(mMutex);
  return GetRepresentation(aRepresentation).UnderlyingCALayer();
}

bool NativeLayerCA::Representation::EnqueueSurface(IOSurfaceRef aSurfaceRef) {
  MOZ_ASSERT([mContentCALayer isKindOfClass:[AVSampleBufferDisplayLayer class]]);

  // Convert the IOSurfaceRef into a CMSampleBuffer, so we can enqueue it in mContentCALayer
  CVPixelBufferRef pixelBuffer = nullptr;
  CVReturn cvValue =
      CVPixelBufferCreateWithIOSurface(kCFAllocatorDefault, aSurfaceRef, nullptr, &pixelBuffer);
  if (cvValue != kCVReturnSuccess) {
    MOZ_ASSERT(pixelBuffer == nullptr, "Failed call shouldn't allocate memory.");
    return false;
  }
  CFTypeRefPtr<CVPixelBufferRef> pixelBufferDeallocator =
      CFTypeRefPtr<CVPixelBufferRef>::WrapUnderCreateRule(pixelBuffer);

  CMVideoFormatDescriptionRef formatDescription = nullptr;
  OSStatus osValue = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer,
                                                                  &formatDescription);
  if (osValue != noErr) {
    MOZ_ASSERT(formatDescription == nullptr, "Failed call shouldn't allocate memory.");
    return false;
  }
  CFTypeRefPtr<CMVideoFormatDescriptionRef> formatDescriptionDeallocator =
      CFTypeRefPtr<CMVideoFormatDescriptionRef>::WrapUnderCreateRule(formatDescription);

  CMSampleBufferRef sampleBuffer = nullptr;
  osValue = CMSampleBufferCreateReadyWithImageBuffer(
      kCFAllocatorDefault, pixelBuffer, formatDescription, &kCMTimingInfoInvalid, &sampleBuffer);
  if (osValue != noErr) {
    MOZ_ASSERT(sampleBuffer == nullptr, "Failed call shouldn't allocate memory.");
    return false;
  }
  CFTypeRefPtr<CMSampleBufferRef> sampleBufferDeallocator =
      CFTypeRefPtr<CMSampleBufferRef>::WrapUnderCreateRule(sampleBuffer);

  // Since we don't have timing information for the sample, before we enqueue it, we
  // attach an attribute that specifies that the sample should be played immediately.
  CFArrayRef attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, YES);
  if (!attachmentsArray || CFArrayGetCount(attachmentsArray) == 0) {
    // No dictionary to alter.
    return false;
  }
  CFMutableDictionaryRef sample0Dictionary =
      (__bridge CFMutableDictionaryRef)CFArrayGetValueAtIndex(attachmentsArray, 0);
  CFDictionarySetValue(sample0Dictionary, kCMSampleAttachmentKey_DisplayImmediately,
                       kCFBooleanTrue);

  [(AVSampleBufferDisplayLayer*)mContentCALayer enqueueSampleBuffer:sampleBuffer];

  return true;
}

bool NativeLayerCA::Representation::ApplyChanges(
    NativeLayerCA::UpdateType aUpdate, const IntSize& aSize, bool aIsOpaque,
    const IntPoint& aPosition, const Matrix4x4& aTransform, const IntRect& aDisplayRect,
    const Maybe<IntRect>& aClipRect, float aBackingScale, bool aSurfaceIsFlipped,
    gfx::SamplingFilter aSamplingFilter, bool aSpecializeVideo,
    CFTypeRefPtr<IOSurfaceRef> aFrontSurface) {
  // If we have an OnlyVideo update, handle it and early exit.
  if (aUpdate == UpdateType::OnlyVideo) {
    // If we don't have any updates to do, exit early with success. This is
    // important to do so that the overall OnlyVideo pass will succeed as long
    // as the video layers are successful.
    if (HasUpdate(true) == UpdateType::None) {
      return true;
    }

    MOZ_ASSERT(!mMutatedSpecializeVideo && mMutatedFrontSurface,
               "Shouldn't attempt a OnlyVideo update in this case.");

    bool updateSucceeded = false;
    if (aSpecializeVideo) {
      IOSurfaceRef surface = aFrontSurface.get();
      if (CanSpecializeSurface(surface)) {
        IOSurfaceRef surface = aFrontSurface.get();
        updateSucceeded = EnqueueSurface(surface);

        if (updateSucceeded) {
          mMutatedFrontSurface = false;
        }
      }
    }

    return updateSucceeded;
  }

  MOZ_ASSERT(aUpdate == UpdateType::All);

  if (mWrappingCALayer && mMutatedSpecializeVideo) {
    // Since specialize video changes the way we construct our wrapping and content layers,
    // we have to scrap them if this value has changed. We can leave mOpaquenessTintLayer alone.
    [mContentCALayer release];
    mContentCALayer = nil;
    [mWrappingCALayer removeFromSuperlayer];
    [mWrappingCALayer release];
    mWrappingCALayer = nil;
  }

  bool layerNeedsInitialization = false;
  if (!mWrappingCALayer) {
    layerNeedsInitialization = true;
    mWrappingCALayer = [[CALayer layer] retain];
    mWrappingCALayer.position = NSZeroPoint;
    mWrappingCALayer.bounds = NSZeroRect;
    mWrappingCALayer.anchorPoint = NSZeroPoint;
    mWrappingCALayer.contentsGravity = kCAGravityTopLeft;
    mWrappingCALayer.edgeAntialiasingMask = 0;
    if (aSpecializeVideo) {
      mContentCALayer = [[AVSampleBufferDisplayLayer layer] retain];
    } else {
      mContentCALayer = [[CALayer layer] retain];
    }
    mContentCALayer.position = NSZeroPoint;
    mContentCALayer.anchorPoint = NSZeroPoint;
    mContentCALayer.contentsGravity = kCAGravityTopLeft;
    mContentCALayer.contentsScale = 1;
    mContentCALayer.bounds = CGRectMake(0, 0, aSize.width, aSize.height);
    mContentCALayer.edgeAntialiasingMask = 0;
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

  if (mMutatedBackingScale || mMutatedSize || layerNeedsInitialization) {
    mContentCALayer.bounds =
        CGRectMake(0, 0, aSize.width / aBackingScale, aSize.height / aBackingScale);
    if (mOpaquenessTintLayer) {
      mOpaquenessTintLayer.bounds = mContentCALayer.bounds;
    }
    mContentCALayer.contentsScale = aBackingScale;
  }

  if (mMutatedBackingScale || mMutatedPosition || mMutatedDisplayRect || mMutatedClipRect ||
      mMutatedTransform || mMutatedSurfaceIsFlipped || mMutatedSize || layerNeedsInitialization) {
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
    bool isEnqueued = false;
    IOSurfaceRef surface = aFrontSurface.get();
    if (aSpecializeVideo && CanSpecializeSurface(surface)) {
      // Attempt to enqueue this as a video frame. If we fail, we'll fall back to image case.
      isEnqueued = EnqueueSurface(surface);
    }

    if (!isEnqueued) {
      mContentCALayer.contents = (id)surface;
    }
  }

  if (mMutatedSamplingFilter || layerNeedsInitialization) {
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
  mMutatedSpecializeVideo = false;

  return true;
}

NativeLayerCA::UpdateType NativeLayerCA::Representation::HasUpdate(bool aIsVideo) {
  if (!mWrappingCALayer) {
    return UpdateType::All;
  }

  // This check intentionally skips mMutatedFrontSurface. We'll check it later to see
  // if we can attempt an OnlyVideo update.
  if (mMutatedPosition || mMutatedTransform || mMutatedDisplayRect || mMutatedClipRect ||
      mMutatedBackingScale || mMutatedSize || mMutatedSurfaceIsFlipped || mMutatedSamplingFilter ||
      mMutatedSpecializeVideo) {
    return UpdateType::All;
  }

  // Check if we should try an OnlyVideo update. We know from the above check that our
  // specialize video is stable (we don't know what value we'll receive, though), so
  // we just have to check that we have a surface to display.
  if (mMutatedFrontSurface) {
    return (aIsVideo ? UpdateType::OnlyVideo : UpdateType::All);
  }

  return UpdateType::None;
}

bool NativeLayerCA::WillUpdateAffectLayers(WhichRepresentation aRepresentation) {
  MutexAutoLock lock(mMutex);
  return GetRepresentation(aRepresentation).mMutatedSpecializeVideo;
}

bool NativeLayerCA::Representation::CanSpecializeSurface(IOSurfaceRef surface) {
  // Software decoded videos can't achieve detached mode. Until Bug 1731691 is fixed,
  // there's no benefit to specializing these videos. For now, only allow 420v or 420f,
  // which we get from hardware decode.
  OSType pixelFormat = IOSurfaceGetPixelFormat(surface);
  return (pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
          pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);
}

// Called when mMutex is already being held by the current thread.
Maybe<NativeLayerCA::SurfaceWithInvalidRegion> NativeLayerCA::GetUnusedSurfaceAndCleanUp(
    const MutexAutoLock& aProofOfLock) {
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

bool DownscaleTargetNLRS::DownscaleFrom(profiler_screenshots::RenderSource* aSource,
                                        const IntRect& aSourceRect, const IntRect& aDestRect) {
  mGL->BlitHelper()->BlitFramebufferToFramebuffer(static_cast<RenderSourceNLRS*>(aSource)->FB().mFB,
                                                  mRenderSource->FB().mFB, aSourceRect, aDestRect,
                                                  LOCAL_GL_LINEAR);

  return true;
}

void AsyncReadbackBufferNLRS::CopyFrom(profiler_screenshots::RenderSource* aSource) {
  IntSize size = aSource->Size();
  MOZ_RELEASE_ASSERT(Size() == size);

  gl::ScopedPackState scopedPackState(mGL);
  mGL->fBindBuffer(LOCAL_GL_PIXEL_PACK_BUFFER, mBufferHandle);
  mGL->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 1);
  const gl::ScopedBindFramebuffer bindFB(mGL, static_cast<RenderSourceNLRS*>(aSource)->FB().mFB);
  mGL->fReadPixels(0, 0, size.width, size.height, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, 0);
}

bool AsyncReadbackBufferNLRS::MapAndCopyInto(DataSourceSurface* aSurface,
                                             const IntSize& aReadSize) {
  MOZ_RELEASE_ASSERT(aReadSize <= aSurface->GetSize());

  if (!mGL || !mGL->MakeCurrent()) {
    return false;
  }

  gl::ScopedPackState scopedPackState(mGL);
  mGL->fBindBuffer(LOCAL_GL_PIXEL_PACK_BUFFER, mBufferHandle);
  mGL->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 1);

  const uint8_t* srcData = nullptr;
  if (mGL->IsSupported(gl::GLFeature::map_buffer_range)) {
    srcData = static_cast<uint8_t*>(mGL->fMapBufferRange(LOCAL_GL_PIXEL_PACK_BUFFER, 0,
                                                         aReadSize.height * aReadSize.width * 4,
                                                         LOCAL_GL_MAP_READ_BIT));
  } else {
    srcData =
        static_cast<uint8_t*>(mGL->fMapBuffer(LOCAL_GL_PIXEL_PACK_BUFFER, LOCAL_GL_READ_ONLY));
  }

  if (!srcData) {
    return false;
  }

  int32_t srcStride = mSize.width * 4;  // Bind() sets an alignment of 1
  DataSourceSurface::ScopedMap map(aSurface, DataSourceSurface::WRITE);
  uint8_t* destData = map.GetData();
  int32_t destStride = map.GetStride();
  SurfaceFormat destFormat = aSurface->GetFormat();
  for (int32_t destRow = 0; destRow < aReadSize.height; destRow++) {
    // Turn srcData upside down during the copy.
    int32_t srcRow = aReadSize.height - 1 - destRow;
    const uint8_t* src = &srcData[srcRow * srcStride];
    uint8_t* dest = &destData[destRow * destStride];
    SwizzleData(src, srcStride, SurfaceFormat::R8G8B8A8, dest, destStride, destFormat,
                IntSize(aReadSize.width, 1));
  }

  mGL->fUnmapBuffer(LOCAL_GL_PIXEL_PACK_BUFFER);

  return true;
}

AsyncReadbackBufferNLRS::~AsyncReadbackBufferNLRS() {
  if (mGL && mGL->MakeCurrent()) {
    mGL->fDeleteBuffers(1, &mBufferHandle);
  }
}

}  // namespace layers
}  // namespace mozilla
