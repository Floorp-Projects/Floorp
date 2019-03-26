/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TiledContentClient.h"
#include <math.h>  // for ceil, ceilf, floor
#include <algorithm>
#include "ClientTiledPaintedLayer.h"  // for ClientTiledPaintedLayer
#include "GeckoProfiler.h"            // for AUTO_PROFILER_LABEL
#include "ClientLayerManager.h"       // for ClientLayerManager
#include "gfxContext.h"               // for gfxContext, etc
#include "gfxPlatform.h"              // for gfxPlatform
#include "gfxPrefs.h"                 // for gfxPrefs
#include "gfxRect.h"                  // for gfxRect
#include "mozilla/MathAlgorithms.h"   // for Abs
#include "mozilla/gfx/Point.h"        // for IntSize
#include "mozilla/gfx/Rect.h"         // for Rect
#include "mozilla/gfx/Tools.h"        // for BytesPerPixel
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/CompositorBridgeChild.h"  // for CompositorBridgeChild
#include "mozilla/layers/LayerMetricsWrapper.h"
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder
#include "mozilla/layers/PaintThread.h"   // for PaintThread
#include "TextureClientPool.h"
#include "nsISupportsImpl.h"      // for gfxContext::AddRef, etc
#include "nsExpirationTracker.h"  // for nsExpirationTracker
#include "nsMathUtils.h"          // for NS_lroundf
#include "LayersLogging.h"
#include "UnitTransforms.h"  // for TransformTo
#include "mozilla/UniquePtr.h"

#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
#  include "cairo.h"
#  include <sstream>
using mozilla::layers::Layer;
static void DrawDebugOverlay(mozilla::gfx::DrawTarget* dt, int x, int y,
                             int width, int height) {
  gfxContext c(dt);

  // Draw border
  c.NewPath();
  c.SetDeviceColor(Color(0.f, 0.f, 0.f));
  c.Rectangle(gfxRect(0, 0, width, height));
  c.Stroke();

  // Build tile description
  std::stringstream ss;
  ss << x << ", " << y;

  // Draw text using cairo toy text API
  // XXX: this drawing will silently fail if |dt| doesn't have a Cairo backend
  cairo_t* cr = gfxFont::RefCairo(dt);
  cairo_set_font_size(cr, 25);
  cairo_text_extents_t extents;
  cairo_text_extents(cr, ss.str().c_str(), &extents);

  int textWidth = extents.width + 6;

  c.NewPath();
  c.SetDeviceColor(Color(0.f, 0.f, 0.f));
  c.Rectangle(gfxRect(gfxPoint(2, 2), gfxSize(textWidth, 30)));
  c.Fill();

  c.NewPath();
  c.SetDeviceColor(Color(1.0, 0.0, 0.0));
  c.Rectangle(gfxRect(gfxPoint(2, 2), gfxSize(textWidth, 30)));
  c.Stroke();

  c.NewPath();
  cairo_move_to(cr, 4, 28);
  cairo_show_text(cr, ss.str().c_str());
}

#endif

namespace mozilla {

using namespace gfx;

namespace layers {

SharedFrameMetricsHelper::SharedFrameMetricsHelper()
    : mLastProgressiveUpdateWasLowPrecision(false),
      mProgressiveUpdateWasInDanger(false) {
  MOZ_COUNT_CTOR(SharedFrameMetricsHelper);
}

SharedFrameMetricsHelper::~SharedFrameMetricsHelper() {
  MOZ_COUNT_DTOR(SharedFrameMetricsHelper);
}

static inline bool FuzzyEquals(float a, float b) {
  return (fabsf(a - b) < 1e-6);
}

static AsyncTransform ComputeViewTransform(
    const FrameMetrics& aContentMetrics,
    const FrameMetrics& aCompositorMetrics) {
  // This is basically the same code as
  // AsyncPanZoomController::GetCurrentAsyncTransform but with aContentMetrics
  // used in place of mLastContentPaintMetrics, because they should be
  // equivalent, modulo race conditions while transactions are inflight.

  ParentLayerPoint translation = (aCompositorMetrics.GetScrollOffset() -
                                  aContentMetrics.GetScrollOffset()) *
                                 aCompositorMetrics.GetZoom();
  return AsyncTransform(aCompositorMetrics.GetAsyncZoom(), -translation);
}

bool SharedFrameMetricsHelper::UpdateFromCompositorFrameMetrics(
    const LayerMetricsWrapper& aLayer, bool aHasPendingNewThebesContent,
    bool aLowPrecision, AsyncTransform& aViewTransform) {
  MOZ_ASSERT(aLayer);

  CompositorBridgeChild* compositor = nullptr;
  if (aLayer.Manager() && aLayer.Manager()->AsClientLayerManager()) {
    compositor =
        aLayer.Manager()->AsClientLayerManager()->GetCompositorBridgeChild();
  }

  if (!compositor) {
    return false;
  }

  const FrameMetrics& contentMetrics = aLayer.Metrics();
  FrameMetrics compositorMetrics;

  if (!compositor->LookupCompositorFrameMetrics(contentMetrics.GetScrollId(),
                                                compositorMetrics)) {
    return false;
  }

  aViewTransform = ComputeViewTransform(contentMetrics, compositorMetrics);

  // Reset the checkerboard risk flag when switching to low precision
  // rendering.
  if (aLowPrecision && !mLastProgressiveUpdateWasLowPrecision) {
    // Skip low precision rendering until we're at risk of checkerboarding.
    if (!mProgressiveUpdateWasInDanger) {
      TILING_LOG(
          "TILING: Aborting low-precision rendering because not at risk of "
          "checkerboarding\n");
      return true;
    }
    mProgressiveUpdateWasInDanger = false;
  }
  mLastProgressiveUpdateWasLowPrecision = aLowPrecision;

  // Always abort updates if the resolution has changed. There's no use
  // in drawing at the incorrect resolution.
  if (!FuzzyEquals(compositorMetrics.GetZoom().xScale,
                   contentMetrics.GetZoom().xScale) ||
      !FuzzyEquals(compositorMetrics.GetZoom().yScale,
                   contentMetrics.GetZoom().yScale)) {
    TILING_LOG("TILING: Aborting because resolution changed from %s to %s\n",
               ToString(contentMetrics.GetZoom()).c_str(),
               ToString(compositorMetrics.GetZoom()).c_str());
    return true;
  }

  // Never abort drawing if we can't be sure we've sent a more recent
  // display-port. If we abort updating when we shouldn't, we can end up
  // with blank regions on the screen and we open up the risk of entering
  // an endless updating cycle.
  if (fabsf(contentMetrics.GetScrollOffset().x -
            compositorMetrics.GetScrollOffset().x) <= 2 &&
      fabsf(contentMetrics.GetScrollOffset().y -
            compositorMetrics.GetScrollOffset().y) <= 2 &&
      fabsf(contentMetrics.GetDisplayPort().X() -
            compositorMetrics.GetDisplayPort().X()) <= 2 &&
      fabsf(contentMetrics.GetDisplayPort().Y() -
            compositorMetrics.GetDisplayPort().Y()) <= 2 &&
      fabsf(contentMetrics.GetDisplayPort().Width() -
            compositorMetrics.GetDisplayPort().Width()) <= 2 &&
      fabsf(contentMetrics.GetDisplayPort().Height() -
            compositorMetrics.GetDisplayPort().Height()) <= 2) {
    return false;
  }

  // When not a low precision pass and the page is in danger of checker boarding
  // abort update.
  if (!aLowPrecision && !mProgressiveUpdateWasInDanger) {
    bool scrollUpdatePending = contentMetrics.GetScrollOffsetUpdated() &&
                               contentMetrics.GetScrollGeneration() !=
                                   compositorMetrics.GetScrollGeneration();
    // If scrollUpdatePending is true, then that means the content-side
    // metrics has a new scroll offset that is going to be forced into the
    // compositor but it hasn't gotten there yet.
    // Even though right now comparing the metrics might indicate we're
    // about to checkerboard (and that's true), the checkerboarding will
    // disappear as soon as the new scroll offset update is processed
    // on the compositor side. To avoid leaving things in a low-precision
    // paint, we need to detect and handle this case (bug 1026756).
    if (!scrollUpdatePending &&
        AboutToCheckerboard(contentMetrics, compositorMetrics)) {
      mProgressiveUpdateWasInDanger = true;
      return true;
    }
  }

  // Abort drawing stale low-precision content if there's a more recent
  // display-port in the pipeline.
  if (aLowPrecision && !aHasPendingNewThebesContent) {
    TILING_LOG(
        "TILING: Aborting low-precision because of new pending content\n");
    return true;
  }

  return false;
}

bool SharedFrameMetricsHelper::AboutToCheckerboard(
    const FrameMetrics& aContentMetrics,
    const FrameMetrics& aCompositorMetrics) {
  // The size of the painted area is originally computed in layer pixels in
  // layout, but then converted to app units and then back to CSS pixels before
  // being put in the FrameMetrics. This process can introduce some rounding
  // error, so we inflate the rect by one app unit to account for that.
  CSSRect painted = (aContentMetrics.GetCriticalDisplayPort().IsEmpty()
                         ? aContentMetrics.GetDisplayPort()
                         : aContentMetrics.GetCriticalDisplayPort()) +
                    aContentMetrics.GetScrollOffset();
  painted.Inflate(CSSMargin::FromAppUnits(nsMargin(1, 1, 1, 1)));

  // Inflate the rect by the danger zone. See the description of the danger zone
  // prefs in AsyncPanZoomController.cpp for an explanation of this.
  CSSRect showing =
      CSSRect(aCompositorMetrics.GetScrollOffset(),
              aCompositorMetrics.CalculateBoundedCompositedSizeInCssPixels());
  showing.Inflate(
      LayerSize(gfxPrefs::APZDangerZoneX(), gfxPrefs::APZDangerZoneY()) /
      aCompositorMetrics.LayersPixelsPerCSSPixel());

  // Clamp both rects to the scrollable rect, because having either of those
  // exceed the scrollable rect doesn't make sense, and could lead to false
  // positives.
  painted = painted.Intersect(aContentMetrics.GetScrollableRect());
  showing = showing.Intersect(aContentMetrics.GetScrollableRect());

  if (!painted.Contains(showing)) {
    TILING_LOG("TILING: About to checkerboard; content %s\n",
               Stringify(aContentMetrics).c_str());
    TILING_LOG("TILING: About to checkerboard; painted %s\n",
               Stringify(painted).c_str());
    TILING_LOG("TILING: About to checkerboard; compositor %s\n",
               Stringify(aCompositorMetrics).c_str());
    TILING_LOG("TILING: About to checkerboard; showing %s\n",
               Stringify(showing).c_str());
    return true;
  }
  return false;
}

bool ClientTiledLayerBuffer::HasFormatChanged() const {
  SurfaceMode mode;
  gfxContentType content = GetContentType(&mode);
  return content != mLastPaintContentType || mode != mLastPaintSurfaceMode;
}

gfxContentType ClientTiledLayerBuffer::GetContentType(
    SurfaceMode* aMode) const {
  gfxContentType content = mPaintedLayer.CanUseOpaqueSurface()
                               ? gfxContentType::COLOR
                               : gfxContentType::COLOR_ALPHA;
  SurfaceMode mode = mPaintedLayer.GetSurfaceMode();

  if (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
#if defined(MOZ_GFX_OPTIMIZE_MOBILE)
    mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
#else
    if (!mPaintedLayer.GetParent() ||
        !mPaintedLayer.GetParent()->SupportsComponentAlphaChildren()) {
      mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
    } else {
      content = gfxContentType::COLOR;
    }
#endif
  } else if (mode == SurfaceMode::SURFACE_OPAQUE) {
#if defined(MOZ_GFX_OPTIMIZE_MOBILE)
    if (IsLowPrecision()) {
      // If we're in low-res mode, drawing can sample from outside the visible
      // region. Make sure that we only sample transparency if that happens.
      mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
      content = gfxContentType::COLOR_ALPHA;
    }
#else
    if (mPaintedLayer.MayResample()) {
      mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
      content = gfxContentType::COLOR_ALPHA;
    }
#endif
  }

  if (aMode) {
    *aMode = mode;
  }
  return content;
}

class TileExpiry final : public nsExpirationTracker<TileClient, 3> {
 public:
  TileExpiry() : nsExpirationTracker<TileClient, 3>(1000, "TileExpiry") {}

  static void AddTile(TileClient* aTile) {
    if (!sTileExpiry) {
      sTileExpiry = MakeUnique<TileExpiry>();
    }

    sTileExpiry->AddObject(aTile);
  }

  static void RemoveTile(TileClient* aTile) {
    MOZ_ASSERT(sTileExpiry);
    sTileExpiry->RemoveObject(aTile);
  }

  static void Shutdown() { sTileExpiry = nullptr; }

 private:
  virtual void NotifyExpired(TileClient* aTile) override {
    aTile->DiscardBackBuffer();
  }

  static UniquePtr<TileExpiry> sTileExpiry;
};
UniquePtr<TileExpiry> TileExpiry::sTileExpiry;

void ShutdownTileCache() { TileExpiry::Shutdown(); }

void TileClient::PrivateProtector::Set(TileClient* const aContainer,
                                       RefPtr<TextureClient> aNewValue) {
  if (mBuffer) {
    TileExpiry::RemoveTile(aContainer);
  }
  mBuffer = aNewValue;
  if (mBuffer) {
    TileExpiry::AddTile(aContainer);
  }
}

void TileClient::PrivateProtector::Set(TileClient* const aContainer,
                                       TextureClient* aNewValue) {
  Set(aContainer, RefPtr<TextureClient>(aNewValue));
}

// Placeholder
TileClient::TileClient() : mWasPlaceholder(false) {}

TileClient::~TileClient() {
  if (mExpirationState.IsTracked()) {
    MOZ_ASSERT(mBackBuffer);
    TileExpiry::RemoveTile(this);
  }
}

TileClient::TileClient(const TileClient& o) {
  mBackBuffer.Set(this, o.mBackBuffer);
  mBackBufferOnWhite = o.mBackBufferOnWhite;
  mFrontBuffer = o.mFrontBuffer;
  mFrontBufferOnWhite = o.mFrontBufferOnWhite;
  mWasPlaceholder = o.mWasPlaceholder;
  mUpdateRect = o.mUpdateRect;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
  mLastUpdate = o.mLastUpdate;
#endif
  mAllocator = o.mAllocator;
  mInvalidFront = o.mInvalidFront;
  mInvalidBack = o.mInvalidBack;
}

TileClient& TileClient::operator=(const TileClient& o) {
  if (this == &o) return *this;
  mBackBuffer.Set(this, o.mBackBuffer);
  mBackBufferOnWhite = o.mBackBufferOnWhite;
  mFrontBuffer = o.mFrontBuffer;
  mFrontBufferOnWhite = o.mFrontBufferOnWhite;
  mWasPlaceholder = o.mWasPlaceholder;
  mUpdateRect = o.mUpdateRect;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
  mLastUpdate = o.mLastUpdate;
#endif
  mAllocator = o.mAllocator;
  mInvalidFront = o.mInvalidFront;
  mInvalidBack = o.mInvalidBack;
  return *this;
}

void TileClient::Dump(std::stringstream& aStream) {
  aStream << "TileClient(bb=" << (TextureClient*)mBackBuffer
          << " fb=" << mFrontBuffer.get();
  if (mBackBufferOnWhite) {
    aStream << " bbow=" << mBackBufferOnWhite.get();
  }
  if (mFrontBufferOnWhite) {
    aStream << " fbow=" << mFrontBufferOnWhite.get();
  }
  aStream << ")";
}

void TileClient::Flip() {
  RefPtr<TextureClient> frontBuffer = mFrontBuffer;
  RefPtr<TextureClient> frontBufferOnWhite = mFrontBufferOnWhite;
  mFrontBuffer = mBackBuffer;
  mFrontBufferOnWhite = mBackBufferOnWhite;
  mBackBuffer.Set(this, frontBuffer);
  mBackBufferOnWhite = frontBufferOnWhite;
  nsIntRegion invalidFront = mInvalidFront;
  mInvalidFront = mInvalidBack;
  mInvalidBack = invalidFront;
}

void TileClient::ValidateFromFront(
    const nsIntRegion& aDirtyRegion, const nsIntRegion& aVisibleRegion,
    gfx::DrawTarget* aBackBuffer, TilePaintFlags aFlags, IntRect* aCopiedRect,
    AutoTArray<RefPtr<TextureClient>, 4>* aClients) {
  if (!mBackBuffer || !mFrontBuffer) {
    return;
  }

  gfx::IntSize tileSize = mFrontBuffer->GetSize();
  const IntRect tileRect = IntRect(0, 0, tileSize.width, tileSize.height);

  if (aDirtyRegion.Contains(tileRect)) {
    // The dirty region means that we no longer need the front buffer, so
    // discard it.
    DiscardFrontBuffer();
    return;
  }

  // Region that needs copying.
  nsIntRegion regionToCopy = mInvalidBack;

  regionToCopy.Sub(regionToCopy, aDirtyRegion);
  regionToCopy.And(regionToCopy, aVisibleRegion);

  *aCopiedRect = regionToCopy.GetBounds();

  if (regionToCopy.IsEmpty()) {
    // Just redraw it all.
    return;
  }

  // Copy the bounding rect of regionToCopy. As tiles are quite small, it
  // is unlikely that we'd save much by copying each individual rect of the
  // region, but we can reevaluate this if it becomes an issue.
  const IntRect rectToCopy = regionToCopy.GetBounds();
  OpenMode readMode = !!(aFlags & TilePaintFlags::Async)
                          ? OpenMode::OPEN_READ_ASYNC
                          : OpenMode::OPEN_READ;

  DualTextureClientAutoLock frontBuffer(mFrontBuffer, mFrontBufferOnWhite,
                                        readMode);
  if (!frontBuffer.Succeeded()) {
    return;
  }

  RefPtr<gfx::SourceSurface> frontSurface = frontBuffer->Snapshot();
  aBackBuffer->CopySurface(frontSurface, rectToCopy, rectToCopy.TopLeft());

  if (aFlags & TilePaintFlags::Async) {
    aClients->AppendElement(mFrontBuffer);
    if (mFrontBufferOnWhite) {
      aClients->AppendElement(mFrontBufferOnWhite);
    }
  }

  mInvalidBack.Sub(mInvalidBack, aVisibleRegion);
}

void TileClient::DiscardFrontBuffer() {
  if (mFrontBuffer) {
    MOZ_ASSERT(mFrontBuffer->GetReadLock());

    MOZ_ASSERT(mAllocator);
    if (mAllocator) {
      mAllocator->ReturnTextureClientDeferred(mFrontBuffer);
      if (mFrontBufferOnWhite) {
        mAllocator->ReturnTextureClientDeferred(mFrontBufferOnWhite);
      }
    }

    if (mFrontBuffer->IsLocked()) {
      mFrontBuffer->Unlock();
    }
    if (mFrontBufferOnWhite && mFrontBufferOnWhite->IsLocked()) {
      mFrontBufferOnWhite->Unlock();
    }
    mFrontBuffer = nullptr;
    mFrontBufferOnWhite = nullptr;
  }
}

static void DiscardTexture(TextureClient* aTexture,
                           TextureClientAllocator* aAllocator) {
  MOZ_ASSERT(aAllocator);
  if (aTexture && aAllocator) {
    MOZ_ASSERT(aTexture->GetReadLock());
    if (!aTexture->HasSynchronization() && aTexture->IsReadLocked()) {
      // Our current back-buffer is still locked by the compositor. This can
      // occur when the client is producing faster than the compositor can
      // consume. In this case we just want to drop it and not return it to the
      // pool.
      aAllocator->ReportClientLost();
    } else {
      aAllocator->ReturnTextureClientDeferred(aTexture);
    }
    if (aTexture->IsLocked()) {
      aTexture->Unlock();
    }
  }
}

void TileClient::DiscardBackBuffer() {
  if (mBackBuffer) {
    DiscardTexture(mBackBuffer, mAllocator);
    mBackBuffer.Set(this, nullptr);
    DiscardTexture(mBackBufferOnWhite, mAllocator);
    mBackBufferOnWhite = nullptr;
  }
}

static already_AddRefed<TextureClient> CreateBackBufferTexture(
    TextureClient* aCurrentTexture, CompositableClient& aCompositable,
    TextureClientAllocator* aAllocator) {
  if (aCurrentTexture) {
    // Our current back-buffer is still locked by the compositor. This can occur
    // when the client is producing faster than the compositor can consume. In
    // this case we just want to drop it and not return it to the pool.
    aAllocator->ReportClientLost();
  }

  RefPtr<TextureClient> texture = aAllocator->GetTextureClient();

  if (!texture) {
    gfxCriticalError() << "[Tiling:Client] Failed to allocate a TextureClient";
    return nullptr;
  }

  if (!aCompositable.AddTextureClient(texture)) {
    gfxCriticalError() << "[Tiling:Client] Failed to connect a TextureClient";
    return nullptr;
  }

  return texture.forget();
}

void TileClient::GetSyncTextureSerials(SurfaceMode aMode,
                                       nsTArray<uint64_t>& aSerials) {
  if (mFrontBuffer && mFrontBuffer->HasIntermediateBuffer() &&
      !mFrontBuffer->IsReadLocked() &&
      (aMode != SurfaceMode::SURFACE_COMPONENT_ALPHA ||
       (mFrontBufferOnWhite && !mFrontBufferOnWhite->IsReadLocked()))) {
    return;
  }

  if (mBackBuffer && !mBackBuffer->HasIntermediateBuffer() &&
      mBackBuffer->IsReadLocked()) {
    aSerials.AppendElement(mBackBuffer->GetSerial());
  }

  if (aMode == SurfaceMode::SURFACE_COMPONENT_ALPHA && mBackBufferOnWhite &&
      !mBackBufferOnWhite->HasIntermediateBuffer() &&
      mBackBufferOnWhite->IsReadLocked()) {
    aSerials.AppendElement(mBackBufferOnWhite->GetSerial());
  }
}

Maybe<AcquiredBackBuffer> TileClient::AcquireBackBuffer(
    CompositableClient& aCompositable, const nsIntRegion& aDirtyRegion,
    const nsIntRegion& aVisibleRegion, gfxContentType aContent,
    SurfaceMode aMode, TilePaintFlags aFlags) {
  AUTO_PROFILER_LABEL("TileClient::AcquireBackBuffer", GRAPHICS_TileAllocation);
  if (!mAllocator) {
    gfxCriticalError() << "[TileClient] Missing TextureClientAllocator.";
    return Nothing();
  }
  if (aMode != SurfaceMode::SURFACE_COMPONENT_ALPHA) {
    // It can happen that a component-alpha layer stops being on component alpha
    // on the next frame, just drop the buffers on white if that happens.
    if (mBackBufferOnWhite) {
      mAllocator->ReportClientLost();
      mBackBufferOnWhite = nullptr;
    }
    if (mFrontBufferOnWhite) {
      mAllocator->ReportClientLost();
      mFrontBufferOnWhite = nullptr;
    }
  }

  // Try to re-use the front-buffer if possible
  if (mFrontBuffer && mFrontBuffer->HasIntermediateBuffer() &&
      !mFrontBuffer->IsReadLocked() &&
      (aMode != SurfaceMode::SURFACE_COMPONENT_ALPHA ||
       (mFrontBufferOnWhite && !mFrontBufferOnWhite->IsReadLocked()))) {
    // If we had a backbuffer we no longer need it since we can re-use the
    // front buffer here. It can be worth it to hold on to the back buffer
    // so we don't need to pay the cost of initializing a new back buffer
    // later (copying pixels and texture upload). But this could increase
    // our memory usage and lead to OOM more frequently from spikes in usage,
    // so we have this behavior behind a pref.
    if (!gfxPrefs::LayersTileRetainBackBuffer()) {
      DiscardBackBuffer();
    }
    Flip();
  } else {
    if (!mBackBuffer || mBackBuffer->IsReadLocked()) {
      mBackBuffer.Set(this, CreateBackBufferTexture(mBackBuffer, aCompositable,
                                                    mAllocator));
      if (!mBackBuffer) {
        DiscardBackBuffer();
        DiscardFrontBuffer();
        return Nothing();
      }
      mInvalidBack = IntRect(IntPoint(), mBackBuffer->GetSize());
    }

    if (aMode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
      if (!mBackBufferOnWhite || mBackBufferOnWhite->IsReadLocked()) {
        mBackBufferOnWhite = CreateBackBufferTexture(mBackBufferOnWhite,
                                                     aCompositable, mAllocator);
        if (!mBackBufferOnWhite) {
          DiscardBackBuffer();
          DiscardFrontBuffer();
          return Nothing();
        }
        mInvalidBack = IntRect(IntPoint(), mBackBufferOnWhite->GetSize());
      }
    }
  }

  // Lock the texture clients
  OpenMode lockMode = aFlags & TilePaintFlags::Async
                          ? OpenMode::OPEN_READ_WRITE_ASYNC
                          : OpenMode::OPEN_READ_WRITE;

  if (!mBackBuffer->Lock(lockMode)) {
    gfxCriticalError() << "[Tiling:Client] Failed to lock a tile (B)";
    DiscardBackBuffer();
    DiscardFrontBuffer();
    return Nothing();
  }

  if (mBackBufferOnWhite && !mBackBufferOnWhite->Lock(lockMode)) {
    gfxCriticalError() << "[Tiling:Client] Failed to lock a tile (W)";
    DiscardBackBuffer();
    DiscardFrontBuffer();
    return Nothing();
  }

  // Borrow their draw targets
  RefPtr<gfx::DrawTarget> backBuffer = mBackBuffer->BorrowDrawTarget();
  RefPtr<gfx::DrawTarget> backBufferOnWhite = nullptr;
  if (mBackBufferOnWhite) {
    backBufferOnWhite = mBackBufferOnWhite->BorrowDrawTarget();
  }

  if (!backBuffer || (mBackBufferOnWhite && !backBufferOnWhite)) {
    gfxCriticalError()
        << "[Tiling:Client] Failed to acquire draw targets for tile";
    DiscardBackBuffer();
    DiscardFrontBuffer();
    return Nothing();
  }

  // Construct a dual target if necessary
  RefPtr<DrawTarget> target;
  if (backBufferOnWhite) {
    backBuffer = Factory::CreateDualDrawTarget(backBuffer, backBufferOnWhite);
    backBufferOnWhite = nullptr;
    target = backBuffer;
  } else {
    target = backBuffer;
  }

  // Construct a capture draw target if necessary
  RefPtr<DrawTargetCapture> capture;
  if (aFlags & TilePaintFlags::Async) {
    capture = Factory::CreateCaptureDrawTargetForTarget(
        target, gfxPrefs::LayersOMTPCaptureLimit());
    target = capture;
  }

  // Gather texture clients
  AutoTArray<RefPtr<TextureClient>, 4> clients;
  clients.AppendElement(RefPtr<TextureClient>(mBackBuffer));
  if (mBackBufferOnWhite) {
    clients.AppendElement(mBackBufferOnWhite);
  }

  // Copy from the front buerr to the back if necessary
  IntRect updatedRect;
  ValidateFromFront(aDirtyRegion, aVisibleRegion, target, aFlags, &updatedRect,
                    &clients);

  return Some(AcquiredBackBuffer{
      target,
      capture,
      backBuffer,
      std::move(updatedRect),
      std::move(clients),
  });
}

TileDescriptor TileClient::GetTileDescriptor() {
  if (IsPlaceholderTile()) {
    mWasPlaceholder = true;
    return PlaceholderTileDescriptor();
  }
  bool wasPlaceholder = mWasPlaceholder;
  mWasPlaceholder = false;

  bool readLocked = mFrontBuffer->OnForwardedToHost();
  bool readLockedOnWhite = false;

  if (mFrontBufferOnWhite) {
    readLockedOnWhite = mFrontBufferOnWhite->OnForwardedToHost();
  }

  return TexturedTileDescriptor(
      nullptr, mFrontBuffer->GetIPDLActor(), Nothing(),
      mFrontBufferOnWhite ? Some(mFrontBufferOnWhite->GetIPDLActor())
                          : Nothing(),
      mUpdateRect, readLocked, readLockedOnWhite, wasPlaceholder);
}

void ClientTiledLayerBuffer::UnlockTile(TileClient& aTile) {
  // We locked the back buffer, and flipped so we now need to unlock the front
  if (aTile.mFrontBuffer && aTile.mFrontBuffer->IsLocked()) {
    aTile.mFrontBuffer->Unlock();
    aTile.mFrontBuffer->SyncWithObject(
        mCompositableClient.GetForwarder()->GetSyncObject());
  }
  if (aTile.mFrontBufferOnWhite && aTile.mFrontBufferOnWhite->IsLocked()) {
    aTile.mFrontBufferOnWhite->Unlock();
    aTile.mFrontBufferOnWhite->SyncWithObject(
        mCompositableClient.GetForwarder()->GetSyncObject());
  }
  if (aTile.mBackBuffer && aTile.mBackBuffer->IsLocked()) {
    aTile.mBackBuffer->Unlock();
  }
  if (aTile.mBackBufferOnWhite && aTile.mBackBufferOnWhite->IsLocked()) {
    aTile.mBackBufferOnWhite->Unlock();
  }
}

void TiledContentClient::PrintInfo(std::stringstream& aStream,
                                   const char* aPrefix) {
  aStream << aPrefix;
  aStream << nsPrintfCString("%sTiledContentClient (0x%p)", mName, this).get();
}

void TiledContentClient::Dump(std::stringstream& aStream, const char* aPrefix,
                              bool aDumpHtml, TextureDumpMode aCompress) {
  GetTiledBuffer()->Dump(aStream, aPrefix, aDumpHtml, aCompress);
}

void BasicTiledLayerPaintData::ResetPaintData() {
  mLowPrecisionPaintCount = 0;
  mPaintFinished = false;
  mHasTransformAnimation = false;
  mCompositionBounds.SetEmpty();
  mCriticalDisplayPort = Nothing();
}

}  // namespace layers
}  // namespace mozilla
