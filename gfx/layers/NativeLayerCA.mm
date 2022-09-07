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
#include "mozilla/Telemetry.h"
#include "mozilla/webrender/RenderMacIOSurfaceTextureHost.h"
#include "nsCocoaFeatures.h"
#include "ScopedGLHelpers.h"
#include "SDKDeclarations.h"

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

static Maybe<Telemetry::LABELS_GFX_MACOS_VIDEO_LOW_POWER> VideoLowPowerTypeToTelemetryType(
    VideoLowPowerType aVideoLowPower) {
  switch (aVideoLowPower) {
    case VideoLowPowerType::LowPower:
      return Some(Telemetry::LABELS_GFX_MACOS_VIDEO_LOW_POWER::LowPower);

    case VideoLowPowerType::FailMultipleVideo:
      return Some(Telemetry::LABELS_GFX_MACOS_VIDEO_LOW_POWER::FailMultipleVideo);

    case VideoLowPowerType::FailWindowed:
      return Some(Telemetry::LABELS_GFX_MACOS_VIDEO_LOW_POWER::FailWindowed);

    case VideoLowPowerType::FailOverlaid:
      return Some(Telemetry::LABELS_GFX_MACOS_VIDEO_LOW_POWER::FailOverlaid);

    case VideoLowPowerType::FailBacking:
      return Some(Telemetry::LABELS_GFX_MACOS_VIDEO_LOW_POWER::FailBacking);

    case VideoLowPowerType::FailMacOSVersion:
      return Some(Telemetry::LABELS_GFX_MACOS_VIDEO_LOW_POWER::FailMacOSVersion);

    case VideoLowPowerType::FailPref:
      return Some(Telemetry::LABELS_GFX_MACOS_VIDEO_LOW_POWER::FailPref);

    case VideoLowPowerType::FailSurface:
      return Some(Telemetry::LABELS_GFX_MACOS_VIDEO_LOW_POWER::FailSurface);

    case VideoLowPowerType::FailEnqueue:
      return Some(Telemetry::LABELS_GFX_MACOS_VIDEO_LOW_POWER::FailEnqueue);

    default:
      return Nothing();
  }
}

static void EmitTelemetryForVideoLowPower(VideoLowPowerType aVideoLowPower) {
  auto telemetryValue = VideoLowPowerTypeToTelemetryType(aVideoLowPower);
  if (telemetryValue.isSome()) {
    Telemetry::AccumulateCategorical(telemetryValue.value());
  }
}

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

already_AddRefed<NativeLayer> NativeLayerRootCA::CreateLayerForColor(gfx::DeviceColor aColor) {
  RefPtr<NativeLayer> layer = new NativeLayerCA(aColor);
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

    mOnscreenRepresentation.Commit(WhichRepresentation::ONSCREEN, mSublayers, mWindowIsFullscreen);

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

  // Decide if we are going to emit telemetry about video low power on this commit.
  mTelemetryCommitCount = (mTelemetryCommitCount + 1) % TELEMETRY_COMMIT_PERIOD;
  if (mTelemetryCommitCount == 0) {
    // Figure out if we are hitting video low power mode.
    VideoLowPowerType videoLowPower = CheckVideoLowPower();
    EmitTelemetryForVideoLowPower(videoLowPower);
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
  mOffscreenRepresentation.Commit(WhichRepresentation::OFFSCREEN, mSublayers, mWindowIsFullscreen);
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
                                               bool aWindowIsFullscreen) {
  bool mustRebuild = mMutatedLayerStructure;
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

  // We're going to do a full update now, which requires a transaction. Update all of the
  // sublayers. Afterwards, only continue processing the sublayers which have an extent.
  AutoCATransaction transaction;
  nsTArray<NativeLayerCA*> sublayersWithExtent;
  for (auto layer : aSublayers) {
    mustRebuild |= layer->WillUpdateAffectLayers(aRepresentation);
    layer->ApplyChanges(aRepresentation, NativeLayerCA::UpdateType::All);
    CALayer* caLayer = layer->UnderlyingCALayer(aRepresentation);
    if (!caLayer.masksToBounds || !NSIsEmptyRect(caLayer.bounds)) {
      // This layer has an extent. If it didn't before, we need to rebuild.
      mustRebuild |= !layer->HasExtent();
      layer->SetHasExtent(true);
      sublayersWithExtent.AppendElement(layer);
    } else {
      // This layer has no extent. If it did before, we need to rebuild.
      mustRebuild |= layer->HasExtent();
      layer->SetHasExtent(false);
    }

    // One other reason we may need to rebuild is if the caLayer is not part of the
    // root layer's sublayers. This might happen if the caLayer was rebuilt.
    // We construct this check in a way that maximizes the boolean short-circuit,
    // because we don't want to call containsObject unless absolutely necessary.
    mustRebuild = mustRebuild || ![mRootCALayer.sublayers containsObject:caLayer];
  }

  if (mustRebuild) {
    uint32_t sublayersCount = sublayersWithExtent.Length();
    NSMutableArray<CALayer*>* sublayers = [NSMutableArray arrayWithCapacity:sublayersCount];
    for (auto layer : sublayersWithExtent) {
      [sublayers addObject:layer->UnderlyingCALayer(aRepresentation)];
    }
    mRootCALayer.sublayers = sublayers;
  }

  mMutatedLayerStructure = false;
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
}

/* static */ bool IsCGColorOpaqueBlack(CGColorRef aColor) {
  if (CGColorEqualToColor(aColor, CGColorGetConstantColor(kCGColorBlack))) {
    return true;
  }
  size_t componentCount = CGColorGetNumberOfComponents(aColor);
  if (componentCount == 0) {
    // This will happen if aColor is kCGColorClear. It's not opaque black.
    return false;
  }

  const CGFloat* components = CGColorGetComponents(aColor);
  for (size_t c = 0; c < componentCount - 1; ++c) {
    if (components[c] > 0.0f) {
      return false;
    }
  }
  return components[componentCount - 1] >= 1.0f;
}

VideoLowPowerType NativeLayerRootCA::CheckVideoLowPower() {
  // This deteremines whether the current layer contents qualify for the
  // macOS Core Animation video low power mode. Those requirements are
  // summarized at
  // https://developer.apple.com/documentation/webkit/delivering_video_content_for_safari
  // and we verify them by checking:
  // 1) There must be exactly one video showing.
  // 2) The topmost CALayer must be a AVSampleBufferDisplayLayer.
  // 3) The video layer must be showing a buffer encoded in one of the
  //    kCVPixelFormatType_420YpCbCr pixel formats.
  // 4) The layer below that must cover the entire screen and have a black
  //    background color.
  // 5) The window must be fullscreen.
  // This function checks these requirements empirically. If one of the checks
  // fail, we either return immediately or do additional processing to
  // determine more detail.

  uint32_t videoLayerCount = 0;
  NativeLayerCA* topLayer = nullptr;
  CALayer* topCALayer = nil;
  CALayer* secondCALayer = nil;
  bool topLayerIsVideo = false;

  for (auto layer : mSublayers) {
    // Only layers with extent are contributing to our sublayers.
    if (layer->HasExtent()) {
      topLayer = layer;

      secondCALayer = topCALayer;
      topCALayer = topLayer->UnderlyingCALayer(WhichRepresentation::ONSCREEN);
      topLayerIsVideo = topLayer->IsVideo();
      if (topLayerIsVideo) {
        ++videoLayerCount;
      }
    }
  }

  if (videoLayerCount == 0) {
    return VideoLowPowerType::NotVideo;
  }

  // Most importantly, check if the window is fullscreen. If the user is watching
  // video in a window, then all of the other enums are irrelevant to achieving
  // the low power mode.
  if (!mWindowIsFullscreen) {
    return VideoLowPowerType::FailWindowed;
  }

  if (videoLayerCount > 1) {
    return VideoLowPowerType::FailMultipleVideo;
  }

  if (!topLayerIsVideo) {
    return VideoLowPowerType::FailOverlaid;
  }

  if (!secondCALayer || !IsCGColorOpaqueBlack(secondCALayer.backgroundColor) ||
      !CGRectContainsRect(secondCALayer.frame, secondCALayer.superlayer.bounds)) {
    return VideoLowPowerType::FailBacking;
  }

  CALayer* topContentCALayer = topCALayer.sublayers[0];
  if (![topContentCALayer isKindOfClass:[AVSampleBufferDisplayLayer class]]) {
    // We didn't create a AVSampleBufferDisplayLayer for the top video layer.
    // Try to figure out why by following some of the logic in
    // NativeLayerCA::ShouldSpecializeVideo.
    if (!nsCocoaFeatures::OnHighSierraOrLater()) {
      return VideoLowPowerType::FailMacOSVersion;
    }

    if (!StaticPrefs::gfx_core_animation_specialize_video()) {
      return VideoLowPowerType::FailPref;
    }

    // The only remaining reason is that the surface wasn't eligible. We
    // assert this instead of if-ing it, to ensure that we always have a
    // return value from this clause.
#ifdef DEBUG
    MOZ_ASSERT(topLayer->mTextureHost);
    MacIOSurface* macIOSurface = topLayer->mTextureHost->GetSurface();
    CFTypeRefPtr<IOSurfaceRef> surface = macIOSurface->GetIOSurfaceRef();
    OSType pixelFormat = IOSurfaceGetPixelFormat(surface.get());
    MOZ_ASSERT(!(pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
                 pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange ||
                 pixelFormat == kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange ||
                 pixelFormat == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange));
#endif
    return VideoLowPowerType::FailSurface;
  }

  AVSampleBufferDisplayLayer* topVideoLayer = (AVSampleBufferDisplayLayer*)topContentCALayer;
  if (topVideoLayer.status != AVQueuedSampleBufferRenderingStatusRendering) {
    return VideoLowPowerType::FailEnqueue;
  }

  // As best we can tell, we're eligible for video low power mode. Hurrah!
  return VideoLowPowerType::LowPower;
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

CGColorRef CGColorCreateForDeviceColor(gfx::DeviceColor aColor) {
  if (StaticPrefs::gfx_color_management_native_srgb()) {
    // Use CGColorCreateSRGB if it's available, otherwise use older macOS API methods,
    // which unfortunately allocate additional memory for the colorSpace object.
    if (@available(macOS 10.15, iOS 13.0, *)) {
      // Even if it is available, we have to address the function dynamically, to keep
      // compiler happy when building with earlier versions of the SDK.
      static auto CGColorCreateSRGBPtr = (CGColorRef(*)(CGFloat, CGFloat, CGFloat, CGFloat))dlsym(
          RTLD_DEFAULT, "CGColorCreateSRGB");
      if (CGColorCreateSRGBPtr) {
        return CGColorCreateSRGBPtr(aColor.r, aColor.g, aColor.b, aColor.a);
      }
    }

    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGFloat components[] = {aColor.r, aColor.g, aColor.b, aColor.a};
    CGColorRef color = CGColorCreate(colorSpace, components);
    CFRelease(colorSpace);
    return color;
  }

  return CGColorCreateGenericRGB(aColor.r, aColor.g, aColor.b, aColor.a);
}

NativeLayerCA::NativeLayerCA(gfx::DeviceColor aColor)
    : mMutex("NativeLayerCA"), mSurfacePoolHandle(nullptr), mIsOpaque(aColor.a >= 1.0f) {
  MOZ_ASSERT(aColor.a > 0.0f, "Can't handle a fully transparent backdrop.");
  mColor.AssignUnderCreateRule(CGColorCreateForDeviceColor(aColor));
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
}

void NativeLayerCA::AttachExternalImage(wr::RenderTextureHost* aExternalImage) {
  MutexAutoLock lock(mMutex);

  wr::RenderMacIOSurfaceTextureHost* texture = aExternalImage->AsRenderMacIOSurfaceTextureHost();
  MOZ_ASSERT(texture);
  mTextureHost = texture;
  if (!mTextureHost) {
    gfxCriticalNoteOnce << "ExternalImage is not RenderMacIOSurfaceTextureHost";
    return;
  }

  gfx::IntSize oldSize = mSize;
  mSize = texture->GetSize(0);
  bool changedSizeAndDisplayRect = (mSize != oldSize);

  mDisplayRect = IntRect(IntPoint{}, mSize);

  bool oldSpecializeVideo = mSpecializeVideo;
  mSpecializeVideo = ShouldSpecializeVideo(lock);
  bool changedSpecializeVideo = (mSpecializeVideo != oldSpecializeVideo);

  bool oldIsDRM = mIsDRM;
  mIsDRM = aExternalImage->IsFromDRMSource();
  bool changedIsDRM = (mIsDRM != oldIsDRM);

  ForAllRepresentations([&](Representation& r) {
    r.mMutatedFrontSurface = true;
    r.mMutatedDisplayRect |= changedSizeAndDisplayRect;
    r.mMutatedSize |= changedSizeAndDisplayRect;
    r.mMutatedSpecializeVideo |= changedSpecializeVideo;
    r.mMutatedIsDRM |= changedIsDRM;
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
  if (!IsVideoAndLocked(aProofOfLock)) {
    // Only videos are eligible.
    return false;
  }

  if (!nsCocoaFeatures::OnHighSierraOrLater()) {
    // We must be on a modern-enough macOS.
    return false;
  }

  MOZ_ASSERT(mTextureHost);

  // DRM video must use a specialized video layer.
  if (mTextureHost->IsFromDRMSource()) {
    return true;
  }

  // Beyond this point, we need to know about the format of the video.
  MacIOSurface* macIOSurface = mTextureHost->GetSurface();
  if (macIOSurface->GetYUVColorSpace() == gfx::YUVColorSpace::BT2020) {
    // BT2020 is a signifier of HDR color space, whether or not the bit depth
    // is expanded to cover that color space. This video needs a specialized
    // video layer.
    return true;
  }

  CFTypeRefPtr<IOSurfaceRef> surface = macIOSurface->GetIOSurfaceRef();
  OSType pixelFormat = IOSurfaceGetPixelFormat(surface.get());
  if (pixelFormat == kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange ||
      pixelFormat == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange) {
    // HDR videos require specialized video layers.
    return true;
  }

  // Beyond this point, we return true if-and-only-if we think we can achieve
  // the power-saving "detached mode" of the macOS compositor.

  if (!StaticPrefs::gfx_core_animation_specialize_video()) {
    // Pref must be set.
    return false;
  }

  if (pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange &&
      pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
    // The video is not in one of the formats that qualifies for detachment.
    return false;
  }

  // It will only detach if we're fullscreen.
  return mRootWindowIsFullscreen;
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

  if (mColor) {
    const CGFloat* components = CGColorGetComponents(mColor.get());
    aOutputStream << "background: rgb(" << components[0] * 255.0f << " " << components[1] * 255.0f
                  << " " << components[2] * 255.0f << "); opacity: " << components[3] << "; ";

    // That's all we need for color layers. We don't need to specify an image.
    aOutputStream << "\"/></div>\n";
    return;
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
    // Attempt to render the surface as a PNG. Skia can do this for RGB surfaces.
    RefPtr<MacIOSurface> surf = new MacIOSurface(surface);
    surf->Lock(true);
    SurfaceFormat format = surf->GetFormat();
    if (format == SurfaceFormat::B8G8R8A8 || format == SurfaceFormat::B8G8R8X8) {
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
      mMutatedSpecializeVideo(true),
      mMutatedIsDRM(true) {}

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
                    mBackingScale, mSurfaceIsFlipped, mSamplingFilter, mSpecializeVideo, surface,
                    mColor, mIsDRM);
}

CALayer* NativeLayerCA::UnderlyingCALayer(WhichRepresentation aRepresentation) {
  MutexAutoLock lock(mMutex);
  return GetRepresentation(aRepresentation).UnderlyingCALayer();
}

static NSString* NSStringForOSType(OSType type) {
  unichar c[4];
  c[0] = (type >> 24) & 0xFF;
  c[1] = (type >> 16) & 0xFF;
  c[2] = (type >> 8) & 0xFF;
  c[3] = (type >> 0) & 0xFF;
  NSString* string = [[NSString stringWithCharacters:c length:4] autorelease];
  return string;
}

/* static */ void LogSurface(IOSurfaceRef aSurfaceRef, CVPixelBufferRef aBuffer,
                             CMVideoFormatDescriptionRef aFormat) {
  NSLog(@"VIDEO_LOG: LogSurface...\n");

  CFDictionaryRef surfaceValues = IOSurfaceCopyAllValues(aSurfaceRef);
  NSLog(@"Surface values are %@.\n", surfaceValues);
  CFRelease(surfaceValues);

  CGColorSpaceRef colorSpace = CVImageBufferGetColorSpace(aBuffer);
  NSLog(@"ColorSpace is %@.\n", colorSpace);

  CFDictionaryRef bufferAttachments =
      CVBufferGetAttachments(aBuffer, kCVAttachmentMode_ShouldPropagate);
  NSLog(@"Buffer attachments are %@.\n", bufferAttachments);

  OSType codec = CMFormatDescriptionGetMediaSubType(aFormat);
  NSLog(@"Codec is %@.\n", NSStringForOSType(codec));

  CFDictionaryRef extensions = CMFormatDescriptionGetExtensions(aFormat);
  NSLog(@"Format extensions are %@.\n", extensions);
}

bool NativeLayerCA::Representation::EnqueueSurface(IOSurfaceRef aSurfaceRef) {
  MOZ_ASSERT([mContentCALayer isKindOfClass:[AVSampleBufferDisplayLayer class]]);
  AVSampleBufferDisplayLayer* videoLayer = (AVSampleBufferDisplayLayer*)mContentCALayer;

  if (@available(macOS 11.0, iOS 14.0, *)) {
    if (videoLayer.requiresFlushToResumeDecoding) {
      [videoLayer flush];
    }
  }

  // If the layer can't handle a new sample, early exit.
  if (!videoLayer.readyForMoreMediaData) {
#ifdef NIGHTLY_BUILD
    if (StaticPrefs::gfx_core_animation_specialize_video_log()) {
      NSLog(@"VIDEO_LOG: EnqueueSurface failed on readyForMoreMediaData.");
    }
#endif
    return false;
  }

  // Convert the IOSurfaceRef into a CMSampleBuffer, so we can enqueue it in mContentCALayer
  CVPixelBufferRef pixelBuffer = nullptr;
  CVReturn cvValue =
      CVPixelBufferCreateWithIOSurface(kCFAllocatorDefault, aSurfaceRef, nullptr, &pixelBuffer);
  if (cvValue != kCVReturnSuccess) {
    MOZ_ASSERT(pixelBuffer == nullptr, "Failed call shouldn't allocate memory.");
#ifdef NIGHTLY_BUILD
    if (StaticPrefs::gfx_core_animation_specialize_video_log()) {
      NSLog(@"VIDEO_LOG: EnqueueSurface failed on allocating pixel buffer.");
    }
#endif
    return false;
  }

#ifdef NIGHTLY_BUILD
  if (StaticPrefs::gfx_core_animation_specialize_video_check_color_space()) {
    // Ensure the resulting pixel buffer has a color space. If it doesn't, then modify
    // the surface and create the buffer again.
    CFTypeRefPtr<CGColorSpaceRef> colorSpace =
        CFTypeRefPtr<CGColorSpaceRef>::WrapUnderGetRule(CVImageBufferGetColorSpace(pixelBuffer));
    if (!colorSpace) {
      // Use our main display color space.
      colorSpace = CFTypeRefPtr<CGColorSpaceRef>::WrapUnderCreateRule(
          CGDisplayCopyColorSpace(CGMainDisplayID()));
      auto colorData =
          CFTypeRefPtr<CFDataRef>::WrapUnderCreateRule(CGColorSpaceCopyICCData(colorSpace.get()));
      IOSurfaceSetValue(aSurfaceRef, CFSTR("IOSurfaceColorSpace"), colorData.get());

      // Get rid of our old pixel buffer and create a new one.
      CFRelease(pixelBuffer);
      cvValue =
          CVPixelBufferCreateWithIOSurface(kCFAllocatorDefault, aSurfaceRef, nullptr, &pixelBuffer);
      if (cvValue != kCVReturnSuccess) {
        MOZ_ASSERT(pixelBuffer == nullptr, "Failed call shouldn't allocate memory.");
        return false;
      }
    }
    MOZ_ASSERT(CVImageBufferGetColorSpace(pixelBuffer), "Pixel buffer should have a color space.");
  }
#endif

  CFTypeRefPtr<CVPixelBufferRef> pixelBufferDeallocator =
      CFTypeRefPtr<CVPixelBufferRef>::WrapUnderCreateRule(pixelBuffer);

  CMVideoFormatDescriptionRef formatDescription = nullptr;
  OSStatus osValue = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer,
                                                                  &formatDescription);
  if (osValue != noErr) {
    MOZ_ASSERT(formatDescription == nullptr, "Failed call shouldn't allocate memory.");
#ifdef NIGHTLY_BUILD
    if (StaticPrefs::gfx_core_animation_specialize_video_log()) {
      NSLog(@"VIDEO_LOG: EnqueueSurface failed on allocating format description.");
    }
#endif
    return false;
  }
  CFTypeRefPtr<CMVideoFormatDescriptionRef> formatDescriptionDeallocator =
      CFTypeRefPtr<CMVideoFormatDescriptionRef>::WrapUnderCreateRule(formatDescription);

#ifdef NIGHTLY_BUILD
  if (StaticPrefs::gfx_core_animation_specialize_video_log()) {
    LogSurface(aSurfaceRef, pixelBuffer, formatDescription);
  }
#endif

  CMSampleTimingInfo timingInfo = kCMTimingInfoInvalid;

  bool spoofTiming = false;
#ifdef NIGHTLY_BUILD
  spoofTiming = StaticPrefs::gfx_core_animation_specialize_video_spoof_timing();
#endif
  if (spoofTiming) {
    // Since we don't have timing information for the sample, set the sample to play at the
    // current timestamp.
    CMTimebaseRef timebase = [(AVSampleBufferDisplayLayer*)mContentCALayer controlTimebase];
    CMTime nowTime = CMTimebaseGetTime(timebase);
    timingInfo = {.presentationTimeStamp = nowTime};
  }

  CMSampleBufferRef sampleBuffer = nullptr;
  osValue = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, pixelBuffer,
                                                     formatDescription, &timingInfo, &sampleBuffer);
  if (osValue != noErr) {
    MOZ_ASSERT(sampleBuffer == nullptr, "Failed call shouldn't allocate memory.");
#ifdef NIGHTLY_BUILD
    if (StaticPrefs::gfx_core_animation_specialize_video_log()) {
      NSLog(@"VIDEO_LOG: EnqueueSurface failed on allocating sample buffer.");
    }
#endif
    return false;
  }
  CFTypeRefPtr<CMSampleBufferRef> sampleBufferDeallocator =
      CFTypeRefPtr<CMSampleBufferRef>::WrapUnderCreateRule(sampleBuffer);

  if (!spoofTiming) {
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
  }

  [videoLayer enqueueSampleBuffer:sampleBuffer];

  return true;
}

bool NativeLayerCA::Representation::ApplyChanges(
    NativeLayerCA::UpdateType aUpdate, const IntSize& aSize, bool aIsOpaque,
    const IntPoint& aPosition, const Matrix4x4& aTransform, const IntRect& aDisplayRect,
    const Maybe<IntRect>& aClipRect, float aBackingScale, bool aSurfaceIsFlipped,
    gfx::SamplingFilter aSamplingFilter, bool aSpecializeVideo,
    CFTypeRefPtr<IOSurfaceRef> aFrontSurface, CFTypeRefPtr<CGColorRef> aColor, bool aIsDRM) {
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
      updateSucceeded = EnqueueSurface(surface);

      if (updateSucceeded) {
        mMutatedFrontSurface = false;
      } else {
        // Set mMutatedSpecializeVideo, which will ensure that the next update
        // will rebuild the video layer.
        mMutatedSpecializeVideo = true;
#ifdef NIGHTLY_BUILD
        if (StaticPrefs::gfx_core_animation_specialize_video_log()) {
          NSLog(@"VIDEO_LOG: EnqueueSurface failed in OnlyVideo update.");
        }
#endif
      }
    }

    return updateSucceeded;
  }

  MOZ_ASSERT(aUpdate == UpdateType::All);

  if (mWrappingCALayer && mMutatedSpecializeVideo) {
    // Since specialize video changes the way we construct our wrapping and content layers,
    // we have to scrap them if this value has changed.
    [mContentCALayer release];
    mContentCALayer = nil;
    [mOpaquenessTintLayer release];
    mOpaquenessTintLayer = nil;
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

    if (aColor) {
      // Color layers set a color on the wrapping layer and don't get a content layer.
      mWrappingCALayer.backgroundColor = aColor.get();
    } else {
      if (aSpecializeVideo) {
        mContentCALayer = [[AVSampleBufferDisplayLayer layer] retain];
        CMTimebaseRef timebase;
        CMTimebaseCreateWithMasterClock(kCFAllocatorDefault, CMClockGetHostTimeClock(), &timebase);
        CMTimebaseSetRate(timebase, 1.0f);
        [(AVSampleBufferDisplayLayer*)mContentCALayer setControlTimebase:timebase];
        CFRelease(timebase);
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
  }

  if (@available(macOS 10.15, iOS 13.0, *)) {
    if (aSpecializeVideo && mMutatedIsDRM) {
      ((AVSampleBufferDisplayLayer*)mContentCALayer).preventsCapture = aIsDRM;
    }
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

  if (mContentCALayer && (mMutatedBackingScale || mMutatedSize || layerNeedsInitialization)) {
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

    if (mContentCALayer) {
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
  }

  if (mContentCALayer && (mMutatedSamplingFilter || layerNeedsInitialization)) {
    if (aSamplingFilter == gfx::SamplingFilter::POINT) {
      mContentCALayer.minificationFilter = kCAFilterNearest;
      mContentCALayer.magnificationFilter = kCAFilterNearest;
    } else {
      mContentCALayer.minificationFilter = kCAFilterLinear;
      mContentCALayer.magnificationFilter = kCAFilterLinear;
    }
  }

  if (mMutatedFrontSurface) {
    // This is handled last because a video update could fail, causing us to
    // early exit, leaving the mutation bits untouched. We do this so that the
    // *next* update will clear the video layer and setup a regular layer.

    IOSurfaceRef surface = aFrontSurface.get();
    if (aSpecializeVideo) {
      // Attempt to enqueue this as a video frame. If we fail, we'll rebuild
      // our video layer in the next update.
      bool isEnqueued = EnqueueSurface(surface);
      if (!isEnqueued) {
        // Set mMutatedSpecializeVideo, which will ensure that the next update
        // will rebuild the video layer.
        mMutatedSpecializeVideo = true;
#ifdef NIGHTLY_BUILD
        if (StaticPrefs::gfx_core_animation_specialize_video_log()) {
          NSLog(@"VIDEO_LOG: EnqueueSurface failed in All update.");
        }
#endif
        return false;
      }
    } else {
      mContentCALayer.contents = (id)surface;
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
  mMutatedIsDRM = false;

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
      mMutatedSpecializeVideo || mMutatedIsDRM) {
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
  auto& r = GetRepresentation(aRepresentation);
  return r.mMutatedSpecializeVideo || !r.UnderlyingCALayer();
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
