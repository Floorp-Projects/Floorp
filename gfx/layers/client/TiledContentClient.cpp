/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TiledContentClient.h"
#include <math.h>                       // for ceil, ceilf, floor
#include <algorithm>
#include "ClientTiledPaintedLayer.h"     // for ClientTiledPaintedLayer
#include "GeckoProfiler.h"              // for AUTO_PROFILER_LABEL
#include "ClientLayerManager.h"         // for ClientLayerManager
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPrefs.h"                   // for gfxPrefs
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/MathAlgorithms.h"     // for Abs
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Tools.h"          // for BytesPerPixel
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/CompositorBridgeChild.h" // for CompositorBridgeChild
#include "mozilla/layers/LayerMetricsWrapper.h"
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder
#include "mozilla/layers/PaintThread.h"  // for PaintThread
#include "TextureClientPool.h"
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for gfxContext::AddRef, etc
#include "nsExpirationTracker.h"        // for nsExpirationTracker
#include "nsMathUtils.h"               // for NS_lroundf
#include "LayersLogging.h"
#include "UnitTransforms.h"             // for TransformTo
#include "mozilla/UniquePtr.h"

// This is the minimum area that we deem reasonable to copy from the front buffer to the
// back buffer on tile updates. If the valid region is smaller than this, we just
// redraw it and save on the copy (and requisite surface-locking involved).
#define MINIMUM_TILE_COPY_AREA (1.f/16.f)

#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
#include "cairo.h"
#include <sstream>
using mozilla::layers::Layer;
static void DrawDebugOverlay(mozilla::gfx::DrawTarget* dt, int x, int y, int width, int height)
{
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
  c.Rectangle(gfxRect(gfxPoint(2,2),gfxSize(textWidth, 30)));
  c.Fill();

  c.NewPath();
  c.SetDeviceColor(Color(1.0, 0.0, 0.0));
  c.Rectangle(gfxRect(gfxPoint(2,2),gfxSize(textWidth, 30)));
  c.Stroke();

  c.NewPath();
  cairo_move_to(cr, 4, 28);
  cairo_show_text(cr, ss.str().c_str());

}

#endif

namespace mozilla {

using namespace gfx;

namespace layers {


MultiTiledContentClient::MultiTiledContentClient(ClientTiledPaintedLayer& aPaintedLayer,
                                                 ClientLayerManager* aManager)
  : TiledContentClient(aManager, "Multi")
  , mTiledBuffer(aPaintedLayer, *this, aManager, &mSharedFrameMetricsHelper)
  , mLowPrecisionTiledBuffer(aPaintedLayer, *this, aManager, &mSharedFrameMetricsHelper)
{
  MOZ_COUNT_CTOR(MultiTiledContentClient);
  mLowPrecisionTiledBuffer.SetResolution(gfxPrefs::LowPrecisionResolution());
  mHasLowPrecision = gfxPrefs::UseLowPrecisionBuffer();
}

void
MultiTiledContentClient::ClearCachedResources()
{
  CompositableClient::ClearCachedResources();
  mTiledBuffer.DiscardBuffers();
  mLowPrecisionTiledBuffer.DiscardBuffers();
}

void
MultiTiledContentClient::UpdatedBuffer(TiledBufferType aType)
{
  ClientMultiTiledLayerBuffer* buffer = aType == LOW_PRECISION_TILED_BUFFER
    ? &mLowPrecisionTiledBuffer
    : &mTiledBuffer;

  MOZ_ASSERT(aType != LOW_PRECISION_TILED_BUFFER || mHasLowPrecision);

  mForwarder->UseTiledLayerBuffer(this, buffer->GetSurfaceDescriptorTiles());
  buffer->ClearPaintedRegion();
}

SharedFrameMetricsHelper::SharedFrameMetricsHelper()
  : mLastProgressiveUpdateWasLowPrecision(false)
  , mProgressiveUpdateWasInDanger(false)
{
  MOZ_COUNT_CTOR(SharedFrameMetricsHelper);
}

SharedFrameMetricsHelper::~SharedFrameMetricsHelper()
{
  MOZ_COUNT_DTOR(SharedFrameMetricsHelper);
}

static inline bool
FuzzyEquals(float a, float b) {
  return (fabsf(a - b) < 1e-6);
}

static AsyncTransform
ComputeViewTransform(const FrameMetrics& aContentMetrics, const FrameMetrics& aCompositorMetrics)
{
  // This is basically the same code as AsyncPanZoomController::GetCurrentAsyncTransform
  // but with aContentMetrics used in place of mLastContentPaintMetrics, because they
  // should be equivalent, modulo race conditions while transactions are inflight.

  ParentLayerPoint translation = (aCompositorMetrics.GetScrollOffset() - aContentMetrics.GetScrollOffset())
                               * aCompositorMetrics.GetZoom();
  return AsyncTransform(aCompositorMetrics.GetAsyncZoom(), -translation);
}

bool
SharedFrameMetricsHelper::UpdateFromCompositorFrameMetrics(
    const LayerMetricsWrapper& aLayer,
    bool aHasPendingNewThebesContent,
    bool aLowPrecision,
    AsyncTransform& aViewTransform)
{
  MOZ_ASSERT(aLayer);

  CompositorBridgeChild* compositor = nullptr;
  if (aLayer.Manager() &&
      aLayer.Manager()->AsClientLayerManager()) {
    compositor = aLayer.Manager()->AsClientLayerManager()->GetCompositorBridgeChild();
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
      TILING_LOG("TILING: Aborting low-precision rendering because not at risk of checkerboarding\n");
      return true;
    }
    mProgressiveUpdateWasInDanger = false;
  }
  mLastProgressiveUpdateWasLowPrecision = aLowPrecision;

  // Always abort updates if the resolution has changed. There's no use
  // in drawing at the incorrect resolution.
  if (!FuzzyEquals(compositorMetrics.GetZoom().xScale, contentMetrics.GetZoom().xScale) ||
      !FuzzyEquals(compositorMetrics.GetZoom().yScale, contentMetrics.GetZoom().yScale)) {
    TILING_LOG("TILING: Aborting because resolution changed from %s to %s\n",
        ToString(contentMetrics.GetZoom()).c_str(), ToString(compositorMetrics.GetZoom()).c_str());
    return true;
  }

  // Never abort drawing if we can't be sure we've sent a more recent
  // display-port. If we abort updating when we shouldn't, we can end up
  // with blank regions on the screen and we open up the risk of entering
  // an endless updating cycle.
  if (fabsf(contentMetrics.GetScrollOffset().x - compositorMetrics.GetScrollOffset().x) <= 2 &&
      fabsf(contentMetrics.GetScrollOffset().y - compositorMetrics.GetScrollOffset().y) <= 2 &&
      fabsf(contentMetrics.GetDisplayPort().X() - compositorMetrics.GetDisplayPort().X()) <= 2 &&
      fabsf(contentMetrics.GetDisplayPort().Y() - compositorMetrics.GetDisplayPort().Y()) <= 2 &&
      fabsf(contentMetrics.GetDisplayPort().Width() - compositorMetrics.GetDisplayPort().Width()) <= 2 &&
      fabsf(contentMetrics.GetDisplayPort().Height() - compositorMetrics.GetDisplayPort().Height()) <= 2) {
    return false;
  }

  // When not a low precision pass and the page is in danger of checker boarding
  // abort update.
  if (!aLowPrecision && !mProgressiveUpdateWasInDanger) {
    bool scrollUpdatePending = contentMetrics.GetScrollOffsetUpdated() &&
        contentMetrics.GetScrollGeneration() != compositorMetrics.GetScrollGeneration();
    // If scrollUpdatePending is true, then that means the content-side
    // metrics has a new scroll offset that is going to be forced into the
    // compositor but it hasn't gotten there yet.
    // Even though right now comparing the metrics might indicate we're
    // about to checkerboard (and that's true), the checkerboarding will
    // disappear as soon as the new scroll offset update is processed
    // on the compositor side. To avoid leaving things in a low-precision
    // paint, we need to detect and handle this case (bug 1026756).
    if (!scrollUpdatePending && AboutToCheckerboard(contentMetrics, compositorMetrics)) {
      mProgressiveUpdateWasInDanger = true;
      return true;
    }
  }

  // Abort drawing stale low-precision content if there's a more recent
  // display-port in the pipeline.
  if (aLowPrecision && !aHasPendingNewThebesContent) {
    TILING_LOG("TILING: Aborting low-precision because of new pending content\n");
    return true;
  }

  return false;
}

bool
SharedFrameMetricsHelper::AboutToCheckerboard(const FrameMetrics& aContentMetrics,
                                              const FrameMetrics& aCompositorMetrics)
{
  // The size of the painted area is originally computed in layer pixels in layout, but then
  // converted to app units and then back to CSS pixels before being put in the FrameMetrics.
  // This process can introduce some rounding error, so we inflate the rect by one app unit
  // to account for that.
  CSSRect painted = (aContentMetrics.GetCriticalDisplayPort().IsEmpty()
                      ? aContentMetrics.GetDisplayPort()
                      : aContentMetrics.GetCriticalDisplayPort())
                    + aContentMetrics.GetScrollOffset();
  painted.Inflate(CSSMargin::FromAppUnits(nsMargin(1, 1, 1, 1)));

  // Inflate the rect by the danger zone. See the description of the danger zone prefs
  // in AsyncPanZoomController.cpp for an explanation of this.
  CSSRect showing = CSSRect(aCompositorMetrics.GetScrollOffset(),
                            aCompositorMetrics.CalculateBoundedCompositedSizeInCssPixels());
  showing.Inflate(LayerSize(gfxPrefs::APZDangerZoneX(), gfxPrefs::APZDangerZoneY())
                  / aCompositorMetrics.LayersPixelsPerCSSPixel());

  // Clamp both rects to the scrollable rect, because having either of those
  // exceed the scrollable rect doesn't make sense, and could lead to false
  // positives.
  painted = painted.Intersect(aContentMetrics.GetScrollableRect());
  showing = showing.Intersect(aContentMetrics.GetScrollableRect());

  if (!painted.Contains(showing)) {
    TILING_LOG("TILING: About to checkerboard; content %s\n", Stringify(aContentMetrics).c_str());
    TILING_LOG("TILING: About to checkerboard; painted %s\n", Stringify(painted).c_str());
    TILING_LOG("TILING: About to checkerboard; compositor %s\n", Stringify(aCompositorMetrics).c_str());
    TILING_LOG("TILING: About to checkerboard; showing %s\n", Stringify(showing).c_str());
    return true;
  }
  return false;
}

ClientMultiTiledLayerBuffer::ClientMultiTiledLayerBuffer(ClientTiledPaintedLayer& aPaintedLayer,
                                                         CompositableClient& aCompositableClient,
                                                         ClientLayerManager* aManager,
                                                         SharedFrameMetricsHelper* aHelper)
  : ClientTiledLayerBuffer(aPaintedLayer, aCompositableClient)
  , mManager(aManager)
  , mCallback(nullptr)
  , mCallbackData(nullptr)
  , mSharedFrameMetricsHelper(aHelper)
{
}

bool
ClientTiledLayerBuffer::HasFormatChanged() const
{
  SurfaceMode mode;
  gfxContentType content = GetContentType(&mode);
  return content != mLastPaintContentType ||
         mode != mLastPaintSurfaceMode;
}


gfxContentType
ClientTiledLayerBuffer::GetContentType(SurfaceMode* aMode) const
{
  gfxContentType content =
    mPaintedLayer.CanUseOpaqueSurface() ? gfxContentType::COLOR :
                                          gfxContentType::COLOR_ALPHA;
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

class TileExpiry final : public nsExpirationTracker<TileClient, 3>
{
  public:
    TileExpiry() : nsExpirationTracker<TileClient, 3>(1000, "TileExpiry") {}

    static void AddTile(TileClient* aTile)
    {
      if (!sTileExpiry) {
        sTileExpiry = MakeUnique<TileExpiry>();
      }

      sTileExpiry->AddObject(aTile);
    }

    static void RemoveTile(TileClient* aTile)
    {
      MOZ_ASSERT(sTileExpiry);
      sTileExpiry->RemoveObject(aTile);
    }

    static void Shutdown() {
      sTileExpiry = nullptr;
    }
  private:
    virtual void NotifyExpired(TileClient* aTile) override
    {
      aTile->DiscardBackBuffer();
    }

    static UniquePtr<TileExpiry> sTileExpiry;
};
UniquePtr<TileExpiry> TileExpiry::sTileExpiry;

void ShutdownTileCache()
{
  TileExpiry::Shutdown();
}

void
TileClient::PrivateProtector::Set(TileClient * const aContainer, RefPtr<TextureClient> aNewValue)
{
  if (mBuffer) {
    TileExpiry::RemoveTile(aContainer);
  }
  mBuffer = aNewValue;
  if (mBuffer) {
    TileExpiry::AddTile(aContainer);
  }
}

void
TileClient::PrivateProtector::Set(TileClient * const aContainer, TextureClient* aNewValue)
{
  Set(aContainer, RefPtr<TextureClient>(aNewValue));
}

// Placeholder
TileClient::TileClient()
  : mWasPlaceholder(false)
{
}

TileClient::~TileClient()
{
  if (mExpirationState.IsTracked()) {
    MOZ_ASSERT(mBackBuffer);
    TileExpiry::RemoveTile(this);
  }
}

TileClient::TileClient(const TileClient& o)
{
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

TileClient&
TileClient::operator=(const TileClient& o)
{
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

void
TileClient::Dump(std::stringstream& aStream)
{
  aStream << "TileClient(bb=" << (TextureClient*)mBackBuffer << " fb=" << mFrontBuffer.get();
  if (mBackBufferOnWhite) {
    aStream << " bbow=" << mBackBufferOnWhite.get();
  }
  if (mFrontBufferOnWhite) {
    aStream << " fbow=" << mFrontBufferOnWhite.get();
  }
  aStream << ")";
}

void
TileClient::Flip()
{
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

static bool
CopyFrontToBack(TextureClient* aFront,
                TextureClient* aBack,
                const gfx::IntRect& aRectToCopy,
                TilePaintFlags aFlags,
                std::vector<CapturedTiledPaintState::Copy>* aCopies,
                std::vector<RefPtr<TextureClient>>* aClients)
{
  bool asyncPaint = !!(aFlags & TilePaintFlags::Async);
  OpenMode asyncFlags = asyncPaint ? OpenMode::OPEN_ASYNC : OpenMode::OPEN_NONE;

  TextureClientAutoLock frontLock(aFront, OpenMode::OPEN_READ | asyncFlags);
  if (!frontLock.Succeeded()) {
    return false;
  }

  if (!aBack->Lock(OpenMode::OPEN_READ_WRITE | asyncFlags)) {
    gfxCriticalError() << "[Tiling:Client] Failed to lock the tile's back buffer";
    return false;
  }

  RefPtr<gfx::DrawTarget> backBuffer = aBack->BorrowDrawTarget();
  if (!backBuffer) {
    gfxWarning() << "[Tiling:Client] Failed to aquire the back buffer's draw target";
    return false;
  }

  RefPtr<gfx::DrawTarget> frontBuffer = aFront->BorrowDrawTarget();
  if (!frontBuffer) {
    gfxWarning() << "[Tiling:Client] Failed to aquire the front buffer's draw target";
    return false;
  }

  auto copy = CapturedTiledPaintState::Copy{
    frontBuffer, backBuffer, aRectToCopy, aRectToCopy.TopLeft()
  };

  if (asyncPaint && aCopies) {
    aClients->push_back(aFront);
    aCopies->push_back(copy);
  } else {
    copy.CopyBuffer();
  }
  return true;
}

void
TileClient::ValidateBackBufferFromFront(const nsIntRegion& aDirtyRegion,
                                        const nsIntRegion& aVisibleRegion,
                                        nsIntRegion& aAddPaintedRegion,
                                        TilePaintFlags aFlags,
                                        std::vector<CapturedTiledPaintState::Copy>* aCopies,
                                        std::vector<RefPtr<TextureClient>>* aClients)
{
  if (mBackBuffer && mFrontBuffer) {
    gfx::IntSize tileSize = mFrontBuffer->GetSize();
    const IntRect tileRect = IntRect(0, 0, tileSize.width, tileSize.height);

    if (aDirtyRegion.Contains(tileRect)) {
      // The dirty region means that we no longer need the front buffer, so
      // discard it.
      DiscardFrontBuffer();
    } else {
      // Region that needs copying.
      nsIntRegion regionToCopy = mInvalidBack;

      regionToCopy.Sub(regionToCopy, aDirtyRegion);
      regionToCopy.And(regionToCopy, aVisibleRegion);

      aAddPaintedRegion = regionToCopy;

      if (regionToCopy.IsEmpty()) {
        // Just redraw it all.
        return;
      }

      // Copy the bounding rect of regionToCopy. As tiles are quite small, it
      // is unlikely that we'd save much by copying each individual rect of the
      // region, but we can reevaluate this if it becomes an issue.
      const IntRect rectToCopy = regionToCopy.GetBounds();
      gfx::IntRect gfxRectToCopy(rectToCopy.X(), rectToCopy.Y(), rectToCopy.Width(), rectToCopy.Height());
      if (CopyFrontToBack(mFrontBuffer, mBackBuffer, gfxRectToCopy, aFlags, aCopies, aClients)) {
        if (mBackBufferOnWhite) {
          MOZ_ASSERT(mFrontBufferOnWhite);
          if (CopyFrontToBack(mFrontBufferOnWhite, mBackBufferOnWhite, gfxRectToCopy, aFlags, aCopies, aClients)) {
            mInvalidBack.Sub(mInvalidBack, aVisibleRegion);
          }
        } else {
          mInvalidBack.Sub(mInvalidBack, aVisibleRegion);
        }
      }
    }
  }
}

void
TileClient::DiscardFrontBuffer()
{
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

static void
DiscardTexture(TextureClient* aTexture, TextureClientAllocator* aAllocator)
{
  MOZ_ASSERT(aAllocator);
  if (aTexture && aAllocator) {
    MOZ_ASSERT(aTexture->GetReadLock());
    if (!aTexture->HasSynchronization() && aTexture->IsReadLocked()) {
      // Our current back-buffer is still locked by the compositor. This can occur
      // when the client is producing faster than the compositor can consume. In
      // this case we just want to drop it and not return it to the pool.
     aAllocator->ReportClientLost();
    } else {
      aAllocator->ReturnTextureClientDeferred(aTexture);
    }
    if (aTexture->IsLocked()) {
      aTexture->Unlock();
    }
  }
}

void
TileClient::DiscardBackBuffer()
{
  if (mBackBuffer) {
    DiscardTexture(mBackBuffer, mAllocator);
    mBackBuffer.Set(this, nullptr);
    DiscardTexture(mBackBufferOnWhite, mAllocator);
    mBackBufferOnWhite = nullptr;
  }
}

static already_AddRefed<TextureClient>
CreateBackBufferTexture(TextureClient* aCurrentTexture,
                        CompositableClient& aCompositable,
                        TextureClientAllocator* aAllocator)
{
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

TextureClient*
TileClient::GetBackBuffer(CompositableClient& aCompositable,
                          const nsIntRegion& aDirtyRegion,
                          const nsIntRegion& aVisibleRegion,
                          gfxContentType aContent,
                          SurfaceMode aMode,
                          nsIntRegion& aAddPaintedRegion,
                          TilePaintFlags aFlags,
                          RefPtr<TextureClient>* aBackBufferOnWhite,
                          std::vector<CapturedTiledPaintState::Copy>* aCopies,
                          std::vector<RefPtr<TextureClient>>* aClients)
{
  if (!mAllocator) {
    gfxCriticalError() << "[TileClient] Missing TextureClientAllocator.";
    return nullptr;
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
  if (mFrontBuffer &&
      mFrontBuffer->HasIntermediateBuffer() &&
      !mFrontBuffer->IsReadLocked() &&
      (aMode != SurfaceMode::SURFACE_COMPONENT_ALPHA || (
        mFrontBufferOnWhite && !mFrontBufferOnWhite->IsReadLocked()))) {
    // If we had a backbuffer we no longer care about it since we'll
    // re-use the front buffer.
    DiscardBackBuffer();
    Flip();
  } else {
    if (!mBackBuffer || mBackBuffer->IsReadLocked()) {
      mBackBuffer.Set(this,
        CreateBackBufferTexture(mBackBuffer, aCompositable, mAllocator)
      );
      if (!mBackBuffer) {
        DiscardBackBuffer();
        DiscardFrontBuffer();
        return nullptr;
      }
      mInvalidBack = IntRect(IntPoint(), mBackBuffer->GetSize());
    }

    if (aMode == SurfaceMode::SURFACE_COMPONENT_ALPHA
        && (!mBackBufferOnWhite || mBackBufferOnWhite->IsReadLocked())) {
      mBackBufferOnWhite = CreateBackBufferTexture(
        mBackBufferOnWhite, aCompositable, mAllocator
      );
      if (!mBackBufferOnWhite) {
        DiscardBackBuffer();
        DiscardFrontBuffer();
        return nullptr;
      }
      mInvalidBack = IntRect(IntPoint(), mBackBufferOnWhite->GetSize());
    }

    ValidateBackBufferFromFront(aDirtyRegion, aVisibleRegion, aAddPaintedRegion, aFlags, aCopies, aClients);
  }

  OpenMode lockMode = aFlags & TilePaintFlags::Async ? OpenMode::OPEN_READ_WRITE_ASYNC
                                                     : OpenMode::OPEN_READ_WRITE;

  if (!mBackBuffer->IsLocked()) {
    if (!mBackBuffer->Lock(lockMode)) {
      gfxCriticalError() << "[Tiling:Client] Failed to lock a tile (B)";
      DiscardBackBuffer();
      DiscardFrontBuffer();
      return nullptr;
    }
  }

  if (mBackBufferOnWhite && !mBackBufferOnWhite->IsLocked()) {
    if (!mBackBufferOnWhite->Lock(lockMode)) {
      gfxCriticalError() << "[Tiling:Client] Failed to lock a tile (W)";
      DiscardBackBuffer();
      DiscardFrontBuffer();
      return nullptr;
    }
  }

  *aBackBufferOnWhite = mBackBufferOnWhite;
  return mBackBuffer;
}

TileDescriptor
TileClient::GetTileDescriptor()
{
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

  return TexturedTileDescriptor(nullptr, mFrontBuffer->GetIPDLActor(),
                                mFrontBufferOnWhite ? MaybeTexture(mFrontBufferOnWhite->GetIPDLActor()) : MaybeTexture(null_t()),
                                mUpdateRect,
                                readLocked, readLockedOnWhite,
                                wasPlaceholder);
}

void
ClientMultiTiledLayerBuffer::DiscardBuffers()
{
  for (TileClient& tile : mRetainedTiles) {
    tile.DiscardBuffers();
  }
}

SurfaceDescriptorTiles
ClientMultiTiledLayerBuffer::GetSurfaceDescriptorTiles()
{
  InfallibleTArray<TileDescriptor> tiles;

  for (TileClient& tile : mRetainedTiles) {
    TileDescriptor tileDesc = tile.GetTileDescriptor();
    tiles.AppendElement(tileDesc);
    // Reset the update rect
    tile.mUpdateRect = IntRect();
  }
  return SurfaceDescriptorTiles(mValidRegion,
                                tiles,
                                mTileOrigin, mTileSize,
                                mTiles.mFirst.x, mTiles.mFirst.y,
                                mTiles.mSize.width, mTiles.mSize.height,
                                mResolution, mFrameResolution.xScale,
                                mFrameResolution.yScale,
                                mWasLastPaintProgressive);
}

void
ClientMultiTiledLayerBuffer::PaintThebes(const nsIntRegion& aNewValidRegion,
                                         const nsIntRegion& aPaintRegion,
                                         const nsIntRegion& aDirtyRegion,
                                         LayerManager::DrawPaintedLayerCallback aCallback,
                                         void* aCallbackData,
                                         TilePaintFlags aFlags)
{
  TILING_LOG("TILING %p: PaintThebes painting region %s\n", &mPaintedLayer, Stringify(aPaintRegion).c_str());
  TILING_LOG("TILING %p: PaintThebes new valid region %s\n", &mPaintedLayer, Stringify(aNewValidRegion).c_str());

  mCallback = aCallback;
  mCallbackData = aCallbackData;
  mWasLastPaintProgressive = !!(aFlags & TilePaintFlags::Progressive);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  long start = PR_IntervalNow();
#endif

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 30) {
    const IntRect bounds = aPaintRegion.GetBounds();
    printf_stderr("Time to draw %i: %i, %i, %i, %i\n", PR_IntervalNow() - start, bounds.x, bounds.y, bounds.width, bounds.height);
    if (aPaintRegion.IsComplex()) {
      printf_stderr("Complex region\n");
      for (auto iter = aPaintRegion.RectIter(); !iter.Done(); iter.Next()) {
        const IntRect& rect = iter.Get();
        printf_stderr(" rect %i, %i, %i, %i\n",
                      rect.x, rect.y, rect.width, rect.height);
      }
    }
  }
  start = PR_IntervalNow();
#endif

  AUTO_PROFILER_LABEL("ClientMultiTiledLayerBuffer::PaintThebes", GRAPHICS);

  mNewValidRegion = aNewValidRegion;
  Update(aNewValidRegion, aPaintRegion, aDirtyRegion, aFlags);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 10) {
    const IntRect bounds = aPaintRegion.GetBounds();
    printf_stderr("Time to tile %i: %i, %i, %i, %i\n", PR_IntervalNow() - start, bounds.x, bounds.y, bounds.width, bounds.height);
  }
#endif

  mLastPaintContentType = GetContentType(&mLastPaintSurfaceMode);
  mCallback = nullptr;
  mCallbackData = nullptr;
}

void PadDrawTargetOutFromRegion(RefPtr<DrawTarget> drawTarget, nsIntRegion &region)
{
  struct LockedBits {
    uint8_t *data;
    IntSize size;
    int32_t stride;
    SurfaceFormat format;
    static int clamp(int x, int min, int max)
    {
      if (x < min)
        x = min;
      if (x > max)
        x = max;
      return x;
    }

    static void ensure_memcpy(uint8_t *dst, uint8_t *src, size_t n, uint8_t *bitmap, int stride, int height)
    {
        if (src + n > bitmap + stride*height) {
            MOZ_CRASH("GFX: long src memcpy");
        }
        if (src < bitmap) {
            MOZ_CRASH("GFX: short src memcpy");
        }
        if (dst + n > bitmap + stride*height) {
            MOZ_CRASH("GFX: long dst mempcy");
        }
        if (dst < bitmap) {
            MOZ_CRASH("GFX: short dst mempcy");
        }
    }

    static void visitor(void *closure, VisitSide side, int x1, int y1, int x2, int y2) {
      LockedBits *lb = static_cast<LockedBits*>(closure);
      uint8_t *bitmap = lb->data;
      const int bpp = gfx::BytesPerPixel(lb->format);
      const int stride = lb->stride;
      const int width = lb->size.width;
      const int height = lb->size.height;

      if (side == VisitSide::TOP) {
        if (y1 > 0) {
          x1 = clamp(x1, 0, width - 1);
          x2 = clamp(x2, 0, width - 1);
          ensure_memcpy(&bitmap[x1*bpp + (y1-1) * stride], &bitmap[x1*bpp + y1 * stride], (x2 - x1) * bpp, bitmap, stride, height);
          memcpy(&bitmap[x1*bpp + (y1-1) * stride], &bitmap[x1*bpp + y1 * stride], (x2 - x1) * bpp);
        }
      } else if (side == VisitSide::BOTTOM) {
        if (y1 < height) {
          x1 = clamp(x1, 0, width - 1);
          x2 = clamp(x2, 0, width - 1);
          ensure_memcpy(&bitmap[x1*bpp + y1 * stride], &bitmap[x1*bpp + (y1-1) * stride], (x2 - x1) * bpp, bitmap, stride, height);
          memcpy(&bitmap[x1*bpp + y1 * stride], &bitmap[x1*bpp + (y1-1) * stride], (x2 - x1) * bpp);
        }
      } else if (side == VisitSide::LEFT) {
        if (x1 > 0) {
          while (y1 != y2) {
            memcpy(&bitmap[(x1-1)*bpp + y1 * stride], &bitmap[x1*bpp + y1*stride], bpp);
            y1++;
          }
        }
      } else if (side == VisitSide::RIGHT) {
        if (x1 < width) {
          while (y1 != y2) {
            memcpy(&bitmap[x1*bpp + y1 * stride], &bitmap[(x1-1)*bpp + y1*stride], bpp);
            y1++;
          }
        }
      }

    }
  } lb;

  if (drawTarget->LockBits(&lb.data, &lb.size, &lb.stride, &lb.format)) {
    // we can only pad software targets so if we can't lock the bits don't pad
    region.VisitEdges(lb.visitor, &lb);
    drawTarget->ReleaseBits(lb.data);
  }
}

void
ClientTiledLayerBuffer::UnlockTile(TileClient& aTile)
{
  // We locked the back buffer, and flipped so we now need to unlock the front
  if (aTile.mFrontBuffer && aTile.mFrontBuffer->IsLocked()) {
    aTile.mFrontBuffer->Unlock();
    aTile.mFrontBuffer->SyncWithObject(mCompositableClient.GetForwarder()->GetSyncObject());
  }
  if (aTile.mFrontBufferOnWhite && aTile.mFrontBufferOnWhite->IsLocked()) {
    aTile.mFrontBufferOnWhite->Unlock();
    aTile.mFrontBufferOnWhite->SyncWithObject(mCompositableClient.GetForwarder()->GetSyncObject());
  }
  if (aTile.mBackBuffer && aTile.mBackBuffer->IsLocked()) {
    aTile.mBackBuffer->Unlock();
  }
  if (aTile.mBackBufferOnWhite && aTile.mBackBufferOnWhite->IsLocked()) {
    aTile.mBackBufferOnWhite->Unlock();
  }
}

void ClientMultiTiledLayerBuffer::Update(const nsIntRegion& newValidRegion,
                                         const nsIntRegion& aPaintRegion,
                                         const nsIntRegion& aDirtyRegion,
                                         TilePaintFlags aFlags)
{
  const IntSize scaledTileSize = GetScaledTileSize();
  const gfx::IntRect newBounds = newValidRegion.GetBounds();

  const TilesPlacement oldTiles = mTiles;
  const TilesPlacement newTiles(floor_div(newBounds.X(), scaledTileSize.width),
                          floor_div(newBounds.Y(), scaledTileSize.height),
                                floor_div(GetTileStart(newBounds.X(), scaledTileSize.width)
                                    + newBounds.Width(), scaledTileSize.width) + 1,
                                floor_div(GetTileStart(newBounds.Y(), scaledTileSize.height)
                                    + newBounds.Height(), scaledTileSize.height) + 1);

  const size_t oldTileCount = mRetainedTiles.Length();
  const size_t newTileCount = newTiles.mSize.width * newTiles.mSize.height;

  nsTArray<TileClient> oldRetainedTiles;
  mRetainedTiles.SwapElements(oldRetainedTiles);
  mRetainedTiles.SetLength(newTileCount);

  for (size_t oldIndex = 0; oldIndex < oldTileCount; oldIndex++) {
    const TileIntPoint tilePosition = oldTiles.TilePosition(oldIndex);
    const size_t newIndex = newTiles.TileIndex(tilePosition);
    // First, get the already existing tiles to the right place in the new array.
    // Leave placeholders (default constructor) where there was no tile.
    if (newTiles.HasTile(tilePosition)) {
      mRetainedTiles[newIndex] = oldRetainedTiles[oldIndex];
    } else {
      // release tiles that we are not going to reuse before allocating new ones
      // to avoid allocating unnecessarily.
      oldRetainedTiles[oldIndex].DiscardBuffers();
    }
  }

  oldRetainedTiles.Clear();

  nsIntRegion paintRegion = aPaintRegion;
  nsIntRegion dirtyRegion = aDirtyRegion;
  if (!paintRegion.IsEmpty()) {
    MOZ_ASSERT(mPaintStates.size() == 0);
    for (size_t i = 0; i < newTileCount; ++i) {
      const TileIntPoint tilePosition = newTiles.TilePosition(i);

      IntPoint tileOffset = GetTileOffset(tilePosition);
      nsIntRegion tileDrawRegion = IntRect(tileOffset, scaledTileSize);
      tileDrawRegion.AndWith(paintRegion);

      if (tileDrawRegion.IsEmpty()) {
        continue;
      }

      TileClient& tile = mRetainedTiles[i];
      if (!ValidateTile(tile, GetTileOffset(tilePosition), tileDrawRegion, aFlags)) {
        gfxCriticalError() << "ValidateTile failed";
      }

      // Validating the tile may have required more to be painted.
      paintRegion.OrWith(tileDrawRegion);
      dirtyRegion.OrWith(tileDrawRegion);
    }

    if (!mPaintTiles.empty()) {
      // Create a tiled draw target
      gfx::TileSet tileset;
      for (size_t i = 0; i < mPaintTiles.size(); ++i) {
        mPaintTiles[i].mTileOrigin -= mTilingOrigin;
      }
      tileset.mTiles = &mPaintTiles[0];
      tileset.mTileCount = mPaintTiles.size();
      RefPtr<DrawTarget> drawTarget = gfx::Factory::CreateTiledDrawTarget(tileset);
      if (!drawTarget || !drawTarget->IsValid()) {
        gfxDevCrash(LogReason::InvalidContext) << "Invalid tiled draw target";
        return;
      }
      drawTarget->SetTransform(Matrix());

      // Draw into the tiled draw target
      RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(drawTarget);
      MOZ_ASSERT(ctx); // already checked the draw target above
      ctx->SetMatrix(
        ctx->CurrentMatrix().PreScale(mResolution, mResolution).PreTranslate(-mTilingOrigin));

      mCallback(&mPaintedLayer, ctx, paintRegion, dirtyRegion,
                DrawRegionClip::DRAW, nsIntRegion(), mCallbackData);
      ctx = nullptr;

      if (aFlags & TilePaintFlags::Async) {
        for (const auto& state : mPaintStates) {
          PaintThread::Get()->PaintTiledContents(state);
        }
        mManager->SetQueuedAsyncPaints();
        MOZ_ASSERT(mPaintStates.size() > 0);
        mPaintStates.clear();
      } else {
        MOZ_ASSERT(mPaintStates.size() == 0);
      }

      // Reset
      mPaintTiles.clear();
      mTilingOrigin = IntPoint(std::numeric_limits<int32_t>::max(),
                               std::numeric_limits<int32_t>::max());
    }

    bool edgePaddingEnabled = gfxPrefs::TileEdgePaddingEnabled();

    for (uint32_t i = 0; i < mRetainedTiles.Length(); ++i) {
      TileClient& tile = mRetainedTiles[i];

      // Only worry about padding when not doing low-res because it simplifies
      // the math and the artifacts won't be noticable
      // Edge padding prevents sampling artifacts when compositing.
      if (edgePaddingEnabled && mResolution == 1 &&
          tile.mFrontBuffer && tile.mFrontBuffer->IsLocked()) {

        const TileIntPoint tilePosition = newTiles.TilePosition(i);
        IntPoint tileOffset = GetTileOffset(tilePosition);
        // Strictly speakig we want the unscaled rect here, but it doesn't matter
        // because we only run this code when the resolution is equal to 1.
        IntRect tileRect = IntRect(tileOffset.x, tileOffset.y,
                                   GetTileSize().width, GetTileSize().height);

        nsIntRegion tileDrawRegion = IntRect(tileOffset, scaledTileSize);
        tileDrawRegion.AndWith(paintRegion);

        nsIntRegion tileValidRegion = mValidRegion;
        tileValidRegion.OrWith(tileDrawRegion);

        // We only need to pad out if the tile has area that's not valid
        if (!tileValidRegion.Contains(tileRect)) {
          tileValidRegion = tileValidRegion.Intersect(tileRect);
          // translate the region into tile space and pad
          tileValidRegion.MoveBy(-IntPoint(tileOffset.x, tileOffset.y));
          RefPtr<DrawTarget> drawTarget = tile.mFrontBuffer->BorrowDrawTarget();
          PadDrawTargetOutFromRegion(drawTarget, tileValidRegion);
        }
      }
      UnlockTile(tile);
    }
  }

  mTiles = newTiles;
  mValidRegion = newValidRegion;
  mPaintedRegion.OrWith(paintRegion);
}

bool
ClientMultiTiledLayerBuffer::ValidateTile(TileClient& aTile,
                                          const nsIntPoint& aTileOrigin,
                                          nsIntRegion& aDirtyRegion,
                                          TilePaintFlags aFlags)
{
  AUTO_PROFILER_LABEL("ClientMultiTiledLayerBuffer::ValidateTile", GRAPHICS);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (aDirtyRegion.IsComplex()) {
    printf_stderr("Complex region\n");
  }
#endif

  SurfaceMode mode;
  gfxContentType content = GetContentType(&mode);

  if (!aTile.mAllocator) {
    aTile.SetTextureAllocator(mManager->GetCompositorBridgeChild()->GetTexturePool(
      mManager->AsShadowForwarder(),
      gfxPlatform::GetPlatform()->Optimal2DFormatForContent(content),
      TextureFlags::DISALLOW_BIGIMAGE | TextureFlags::IMMEDIATE_UPLOAD | TextureFlags::NON_BLOCKING_READ_LOCK));
    MOZ_ASSERT(aTile.mAllocator);
  }

  nsIntRegion tileDirtyRegion = aDirtyRegion.MovedBy(-aTileOrigin);
  tileDirtyRegion.ScaleRoundOut(mResolution, mResolution);

  nsIntRegion tileVisibleRegion = mNewValidRegion.MovedBy(-aTileOrigin);
  tileVisibleRegion.ScaleRoundOut(mResolution, mResolution);

  std::vector<CapturedTiledPaintState::Copy> asyncPaintCopies;
  std::vector<RefPtr<TextureClient>> asyncPaintClients;

  nsIntRegion extraPainted;
  RefPtr<TextureClient> backBufferOnWhite;
  RefPtr<TextureClient> backBuffer =
    aTile.GetBackBuffer(mCompositableClient,
                        tileDirtyRegion,
                        tileVisibleRegion,
                        content, mode,
                        extraPainted,
                        aFlags,
                        &backBufferOnWhite,
                        &asyncPaintCopies,
                        &asyncPaintClients);

  // Mark the area we need to paint in the back buffer as invalid in the
  // front buffer as they will become out of sync.
  aTile.mInvalidFront.OrWith(tileDirtyRegion);

  // Add the backbuffer's invalid region intersected with the visible region to the
  // dirty region we will be painting. This will be empty if we are able to copy
  // from the front into the back.
  nsIntRegion tileInvalidRegion = aTile.mInvalidBack;
  tileInvalidRegion.AndWith(tileVisibleRegion);

  nsIntRegion invalidRegion = tileInvalidRegion;
  invalidRegion.MoveBy(aTileOrigin);
  invalidRegion.ScaleInverseRoundOut(mResolution, mResolution);

  tileDirtyRegion.OrWith(tileInvalidRegion);
  aDirtyRegion.OrWith(invalidRegion);

  // Mark the region we will be painting and the region we copied from the front buffer as
  // needing to be uploaded to the compositor
  aTile.mUpdateRect = tileDirtyRegion.GetBounds().Union(extraPainted.GetBounds());

  // Add the region we copied from the front buffer into the painted region
  extraPainted.MoveBy(aTileOrigin);
  extraPainted.And(extraPainted, mNewValidRegion);
  mPaintedRegion.Or(mPaintedRegion, extraPainted);

  if (!backBuffer) {
    return false;
  }

  RefPtr<DrawTarget> dt = backBuffer->BorrowDrawTarget();
  RefPtr<DrawTarget> dtOnWhite;
  if (backBufferOnWhite) {
    dtOnWhite = backBufferOnWhite->BorrowDrawTarget();
  }

  if (!dt || (backBufferOnWhite && !dtOnWhite)) {
    aTile.DiscardBuffers();
    return false;
  }

  RefPtr<DrawTarget> drawTarget;
  if (dtOnWhite) {
    drawTarget = Factory::CreateDualDrawTarget(dt, dtOnWhite);
  } else {
    drawTarget = dt;
  }

  auto clear = CapturedTiledPaintState::Clear{
    dt,
    dtOnWhite,
    tileDirtyRegion
  };

  gfx::Tile paintTile;
  paintTile.mTileOrigin = gfx::IntPoint(aTileOrigin.x, aTileOrigin.y);

  if (aFlags & TilePaintFlags::Async) {
    RefPtr<CapturedTiledPaintState> asyncPaint = new CapturedTiledPaintState();

    RefPtr<DrawTargetCapture> captureDT =
      Factory::CreateCaptureDrawTarget(drawTarget->GetBackendType(),
                                       drawTarget->GetSize(),
                                       drawTarget->GetFormat());
    paintTile.mDrawTarget = captureDT;
    asyncPaint->mTarget = drawTarget;
    asyncPaint->mCapture = captureDT;

    asyncPaint->mCopies = std::move(asyncPaintCopies);
    asyncPaint->mClears.push_back(clear);

    asyncPaint->mClients = std::move(asyncPaintClients);
    asyncPaint->mClients.push_back(backBuffer);
    if (backBufferOnWhite) {
      asyncPaint->mClients.push_back(backBufferOnWhite);
    }

    mPaintStates.push_back(asyncPaint);
  } else {
    paintTile.mDrawTarget = drawTarget;
    clear.ClearBuffer();
  }

  mPaintTiles.push_back(paintTile);

  mTilingOrigin.x = std::min(mTilingOrigin.x, paintTile.mTileOrigin.x);
  mTilingOrigin.y = std::min(mTilingOrigin.y, paintTile.mTileOrigin.y);

  // The new buffer is now validated, remove the dirty region from it.
  aTile.mInvalidBack.SubOut(tileDirtyRegion);

  aTile.Flip();

  return true;
}

/**
 * This function takes the transform stored in aTransformToCompBounds
 * (which was generated in GetTransformToAncestorsParentLayer), and
 * modifies it with the ViewTransform from the compositor side so that
 * it reflects what the compositor is actually rendering. This operation
 * basically adds in the layer's async transform.
 * This function then returns the scroll ancestor's composition bounds,
 * transformed into the painted layer's LayerPixel coordinates, accounting
 * for the compositor state.
 */
static Maybe<LayerRect>
GetCompositorSideCompositionBounds(const LayerMetricsWrapper& aScrollAncestor,
                                   const LayerToParentLayerMatrix4x4& aTransformToCompBounds,
                                   const AsyncTransform& aAPZTransform,
                                   const LayerRect& aClip)
{
  LayerToParentLayerMatrix4x4 transform = aTransformToCompBounds *
      AsyncTransformComponentMatrix(aAPZTransform);

  return UntransformBy(transform.Inverse(),
    aScrollAncestor.Metrics().GetCompositionBounds(), aClip);
}

bool
ClientMultiTiledLayerBuffer::ComputeProgressiveUpdateRegion(const nsIntRegion& aInvalidRegion,
                                                            const nsIntRegion& aOldValidRegion,
                                                            nsIntRegion& aRegionToPaint,
                                                            BasicTiledLayerPaintData* aPaintData,
                                                            bool aIsRepeated)
{
  aRegionToPaint = aInvalidRegion;

  // If the composition bounds rect is empty, we can't make any sensible
  // decision about how to update coherently. In this case, just update
  // everything in one transaction.
  if (aPaintData->mCompositionBounds.IsEmpty()) {
    aPaintData->mPaintFinished = true;
    return false;
  }

  // If this is a low precision buffer, we force progressive updates. The
  // assumption is that the contents is less important, so visual coherency
  // is lower priority than speed.
  bool drawingLowPrecision = IsLowPrecision();

  // Find out if we have any non-stale content to update.
  nsIntRegion staleRegion;
  staleRegion.And(aInvalidRegion, aOldValidRegion);

  TILING_LOG("TILING %p: Progressive update stale region %s\n", &mPaintedLayer, Stringify(staleRegion).c_str());

  LayerMetricsWrapper scrollAncestor;
  mPaintedLayer.GetAncestorLayers(&scrollAncestor, nullptr, nullptr);

  // Find out the current view transform to determine which tiles to draw
  // first, and see if we should just abort this paint. Aborting is usually
  // caused by there being an incoming, more relevant paint.
  AsyncTransform viewTransform;
  MOZ_ASSERT(mSharedFrameMetricsHelper);

  bool abortPaint =
    mSharedFrameMetricsHelper->UpdateFromCompositorFrameMetrics(
      scrollAncestor,
      !staleRegion.Contains(aInvalidRegion),
      drawingLowPrecision,
      viewTransform);

  TILING_LOG("TILING %p: Progressive update view transform %s zoom %f abort %d\n",
      &mPaintedLayer, ToString(viewTransform.mTranslation).c_str(), viewTransform.mScale.scale, abortPaint);

  if (abortPaint) {
    // We ignore if front-end wants to abort if this is the first,
    // non-low-precision paint, as in that situation, we're about to override
    // front-end's page/viewport metrics.
    if (!aPaintData->mFirstPaint || drawingLowPrecision) {
      AUTO_PROFILER_LABEL(
        "ClientMultiTiledLayerBuffer::ComputeProgressiveUpdateRegion",
        GRAPHICS);

      aRegionToPaint.SetEmpty();
      return aIsRepeated;
    }
  }

  Maybe<LayerRect> transformedCompositionBounds =
    GetCompositorSideCompositionBounds(scrollAncestor,
                                       aPaintData->mTransformToCompBounds,
                                       viewTransform,
                                       LayerRect(mPaintedLayer.GetVisibleRegion().GetBounds()));

  if (!transformedCompositionBounds) {
    aPaintData->mPaintFinished = true;
    return false;
  }

  TILING_LOG("TILING %p: Progressive update transformed compositor bounds %s\n", &mPaintedLayer, Stringify(*transformedCompositionBounds).c_str());

  // Compute a "coherent update rect" that we should paint all at once in a
  // single transaction. This is to avoid rendering glitches on animated
  // page content, and when layers change size/shape.
  // On Fennec uploads are more expensive because we're not using gralloc, so
  // we use a coherent update rect that is intersected with the screen at the
  // time of issuing the draw command. This will paint faster but also potentially
  // make the progressive paint more visible to the user while scrolling.
  IntRect coherentUpdateRect(RoundedOut(
#ifdef MOZ_WIDGET_ANDROID
    transformedCompositionBounds->Intersect(aPaintData->mCompositionBounds)
#else
    *transformedCompositionBounds
#endif
  ).ToUnknownRect());

  TILING_LOG("TILING %p: Progressive update final coherency rect %s\n", &mPaintedLayer, Stringify(coherentUpdateRect).c_str());

  aRegionToPaint.And(aInvalidRegion, coherentUpdateRect);
  aRegionToPaint.Or(aRegionToPaint, staleRegion);
  bool drawingStale = !aRegionToPaint.IsEmpty();
  if (!drawingStale) {
    aRegionToPaint = aInvalidRegion;
  }

  // Prioritise tiles that are currently visible on the screen.
  bool paintingVisible = false;
  if (aRegionToPaint.Intersects(coherentUpdateRect)) {
    aRegionToPaint.And(aRegionToPaint, coherentUpdateRect);
    paintingVisible = true;
  }

  TILING_LOG("TILING %p: Progressive update final paint region %s\n", &mPaintedLayer, Stringify(aRegionToPaint).c_str());

  // Paint area that's visible and overlaps previously valid content to avoid
  // visible glitches in animated elements, such as gifs.
  bool paintInSingleTransaction = paintingVisible && (drawingStale || aPaintData->mFirstPaint);

  TILING_LOG("TILING %p: paintingVisible %d drawingStale %d firstPaint %d singleTransaction %d\n",
    &mPaintedLayer, paintingVisible, drawingStale, aPaintData->mFirstPaint, paintInSingleTransaction);

  // The following code decides what order to draw tiles in, based on the
  // current scroll direction of the primary scrollable layer.
  NS_ASSERTION(!aRegionToPaint.IsEmpty(), "Unexpectedly empty paint region!");
  IntRect paintBounds = aRegionToPaint.GetBounds();

  int startX, incX, startY, incY;
  gfx::IntSize scaledTileSize = GetScaledTileSize();
  if (aPaintData->mScrollOffset.x >= aPaintData->mLastScrollOffset.x) {
    startX = RoundDownToTileEdge(paintBounds.X(), scaledTileSize.width);
    incX = scaledTileSize.width;
  } else {
    startX = RoundDownToTileEdge(paintBounds.XMost() - 1, scaledTileSize.width);
    incX = -scaledTileSize.width;
  }

  if (aPaintData->mScrollOffset.y >= aPaintData->mLastScrollOffset.y) {
    startY = RoundDownToTileEdge(paintBounds.Y(), scaledTileSize.height);
    incY = scaledTileSize.height;
  } else {
    startY = RoundDownToTileEdge(paintBounds.YMost() - 1, scaledTileSize.height);
    incY = -scaledTileSize.height;
  }

  // Find a tile to draw.
  IntRect tileBounds(startX, startY, scaledTileSize.width, scaledTileSize.height);
  int32_t scrollDiffX = aPaintData->mScrollOffset.x - aPaintData->mLastScrollOffset.x;
  int32_t scrollDiffY = aPaintData->mScrollOffset.y - aPaintData->mLastScrollOffset.y;
  // This loop will always terminate, as there is at least one tile area
  // along the first/last row/column intersecting with regionToPaint, or its
  // bounds would have been smaller.
  while (true) {
    aRegionToPaint.And(aInvalidRegion, tileBounds);
    if (!aRegionToPaint.IsEmpty()) {
      if (mResolution != 1) {
        // Paint the entire tile for low-res. This is aimed to fixing low-res resampling
        // and to avoid doing costly region accurate painting for a small area.
        aRegionToPaint = tileBounds;
      }
      break;
    }
    if (Abs(scrollDiffY) >= Abs(scrollDiffX)) {
      tileBounds.MoveByX(incX);
    } else {
      tileBounds.MoveByY(incY);
    }
  }

  if (!aRegionToPaint.Contains(aInvalidRegion)) {
    // The region needed to paint is larger then our progressive chunk size
    // therefore update what we want to paint and ask for a new paint transaction.

    // If we need to draw more than one tile to maintain coherency, make
    // sure it happens in the same transaction by requesting this work be
    // repeated immediately.
    // If this is unnecessary, the remaining work will be done tile-by-tile in
    // subsequent transactions. The caller code is responsible for scheduling
    // the subsequent transactions as long as we don't set the mPaintFinished
    // flag to true.
    return (!drawingLowPrecision && paintInSingleTransaction);
  }

  // We're not repeating painting and we've not requested a repeat transaction,
  // so the paint is finished. If there's still a separate low precision
  // paint to do, it will get marked as unfinished later.
  aPaintData->mPaintFinished = true;
  return false;
}

bool
ClientMultiTiledLayerBuffer::ProgressiveUpdate(const nsIntRegion& aValidRegion,
                                               const nsIntRegion& aInvalidRegion,
                                               const nsIntRegion& aOldValidRegion,
                                               nsIntRegion& aOutDrawnRegion,
                                               BasicTiledLayerPaintData* aPaintData,
                                               LayerManager::DrawPaintedLayerCallback aCallback,
                                               void* aCallbackData)
{
  TILING_LOG("TILING %p: Progressive update valid region %s\n", &mPaintedLayer, Stringify(aValidRegion).c_str());
  TILING_LOG("TILING %p: Progressive update invalid region %s\n", &mPaintedLayer, Stringify(aInvalidRegion).c_str());
  TILING_LOG("TILING %p: Progressive update old valid region %s\n", &mPaintedLayer, Stringify(aOldValidRegion).c_str());

  bool repeat = false;
  bool isBufferChanged = false;
  nsIntRegion remainingInvalidRegion = aInvalidRegion;
  nsIntRegion updatedValidRegion = aValidRegion;
  do {
    // Compute the region that should be updated. Repeat as many times as
    // is required.
    nsIntRegion regionToPaint;
    repeat = ComputeProgressiveUpdateRegion(remainingInvalidRegion,
                                            aOldValidRegion,
                                            regionToPaint,
                                            aPaintData,
                                            repeat);

    TILING_LOG("TILING %p: Progressive update computed paint region %s repeat %d\n", &mPaintedLayer, Stringify(regionToPaint).c_str(), repeat);

    // There's no further work to be done.
    if (regionToPaint.IsEmpty()) {
      break;
    }

    isBufferChanged = true;

    // Keep track of what we're about to refresh.
    aOutDrawnRegion.OrWith(regionToPaint);
    updatedValidRegion.OrWith(regionToPaint);

    // aValidRegion may have been altered by InvalidateRegion, but we still
    // want to display stale content until it gets progressively updated.
    // Create a region that includes stale content.
    nsIntRegion validOrStale;
    validOrStale.Or(updatedValidRegion, aOldValidRegion);

    // Paint the computed region and subtract it from the invalid region.
    PaintThebes(validOrStale, regionToPaint, remainingInvalidRegion,
                aCallback, aCallbackData, TilePaintFlags::Progressive);
    remainingInvalidRegion.SubOut(regionToPaint);
  } while (repeat);

  TILING_LOG("TILING %p: Progressive update final valid region %s buffer changed %d\n", &mPaintedLayer, Stringify(updatedValidRegion).c_str(), isBufferChanged);
  TILING_LOG("TILING %p: Progressive update final invalid region %s\n", &mPaintedLayer, Stringify(remainingInvalidRegion).c_str());

  // Return false if nothing has been drawn, or give what has been drawn
  // to the shadow layer to upload.
  return isBufferChanged;
}

void
TiledContentClient::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  aStream << aPrefix;
  aStream << nsPrintfCString("%sTiledContentClient (0x%p)", mName, this).get();
}

void
TiledContentClient::Dump(std::stringstream& aStream,
                         const char* aPrefix,
                         bool aDumpHtml,
                         TextureDumpMode aCompress)
{
  GetTiledBuffer()->Dump(aStream, aPrefix, aDumpHtml, aCompress);
}

void
BasicTiledLayerPaintData::ResetPaintData()
{
  mLowPrecisionPaintCount = 0;
  mPaintFinished = false;
  mHasTransformAnimation = false;
  mCompositionBounds.SetEmpty();
  mCriticalDisplayPort = Nothing();
}

} // namespace layers
} // namespace mozilla
