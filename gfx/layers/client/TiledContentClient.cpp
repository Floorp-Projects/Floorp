/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TiledContentClient.h"
#include <math.h>                       // for ceil, ceilf, floor
#include <algorithm>
#include "ClientTiledPaintedLayer.h"     // for ClientTiledPaintedLayer
#include "GeckoProfiler.h"              // for PROFILER_LABEL
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
#include "mozilla/layers/CompositorChild.h" // for CompositorChild
#include "mozilla/layers/LayerMetricsWrapper.h"
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder
#include "TextureClientPool.h"
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for gfxContext::AddRef, etc
#include "nsSize.h"                     // for nsIntSize
#include "gfxReusableSharedImageSurfaceWrapper.h"
#include "nsExpirationTracker.h"        // for nsExpirationTracker
#include "nsMathUtils.h"               // for NS_roundf
#include "gfx2DGlue.h"
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
  c.SetDeviceColor(gfxRGBA(0.0, 0.0, 0.0, 1.0));
  c.Rectangle(gfxRect(0, 0, width, height));
  c.Stroke();

  // Build tile description
  std::stringstream ss;
  ss << x << ", " << y;

  // Draw text using cairo toy text API
  cairo_t* cr = c.GetCairo();
  cairo_set_font_size(cr, 25);
  cairo_text_extents_t extents;
  cairo_text_extents(cr, ss.str().c_str(), &extents);

  int textWidth = extents.width + 6;

  c.NewPath();
  c.SetDeviceColor(gfxRGBA(0.0, 0.0, 0.0, 1.0));
  c.Rectangle(gfxRect(gfxPoint(2,2),gfxSize(textWidth, 30)));
  c.Fill();

  c.NewPath();
  c.SetDeviceColor(gfxRGBA(1.0, 0.0, 0.0, 1.0));
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


TiledContentClient::TiledContentClient(ClientTiledPaintedLayer* aPaintedLayer,
                                       ClientLayerManager* aManager)
  : CompositableClient(aManager->AsShadowForwarder())
{
  MOZ_COUNT_CTOR(TiledContentClient);

  mTiledBuffer = ClientTiledLayerBuffer(aPaintedLayer, this, aManager,
                                        &mSharedFrameMetricsHelper);
  mLowPrecisionTiledBuffer = ClientTiledLayerBuffer(aPaintedLayer, this, aManager,
                                                    &mSharedFrameMetricsHelper);

  mLowPrecisionTiledBuffer.SetResolution(gfxPrefs::LowPrecisionResolution());
}

void
TiledContentClient::ClearCachedResources()
{
  mTiledBuffer.DiscardBuffers();
  mLowPrecisionTiledBuffer.DiscardBuffers();
}

void
TiledContentClient::UseTiledLayerBuffer(TiledBufferType aType)
{
  ClientTiledLayerBuffer* buffer = aType == LOW_PRECISION_TILED_BUFFER
    ? &mLowPrecisionTiledBuffer
    : &mTiledBuffer;

  // Take a ReadLock on behalf of the TiledContentHost. This
  // reference will be adopted when the descriptor is opened in
  // TiledLayerBufferComposite.
  buffer->ReadLock();

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

static ViewTransform
ComputeViewTransform(const FrameMetrics& aContentMetrics, const FrameMetrics& aCompositorMetrics)
{
  // This is basically the same code as AsyncPanZoomController::GetCurrentAsyncTransform
  // but with aContentMetrics used in place of mLastContentPaintMetrics, because they
  // should be equivalent, modulo race conditions while transactions are inflight.

  ParentLayerToScreenScale scale = aCompositorMetrics.GetZoom()
                     / aContentMetrics.mDevPixelsPerCSSPixel
                     / aCompositorMetrics.GetParentResolution();
  ScreenPoint translation = (aCompositorMetrics.GetScrollOffset() - aContentMetrics.GetScrollOffset())
                         * aCompositorMetrics.GetZoom();
  return ViewTransform(scale, -translation);
}

bool
SharedFrameMetricsHelper::UpdateFromCompositorFrameMetrics(
    const LayerMetricsWrapper& aLayer,
    bool aHasPendingNewThebesContent,
    bool aLowPrecision,
    ViewTransform& aViewTransform)
{
  MOZ_ASSERT(aLayer);

  CompositorChild* compositor = nullptr;
  if (aLayer.Manager() &&
      aLayer.Manager()->AsClientLayerManager()) {
    compositor = aLayer.Manager()->AsClientLayerManager()->GetCompositorChild();
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
  if (!FuzzyEquals(compositorMetrics.GetZoom().scale, contentMetrics.GetZoom().scale)) {
    TILING_LOG("TILING: Aborting because resolution changed from %f to %f\n",
        contentMetrics.GetZoom().scale, compositorMetrics.GetZoom().scale);
    return true;
  }

  // Never abort drawing if we can't be sure we've sent a more recent
  // display-port. If we abort updating when we shouldn't, we can end up
  // with blank regions on the screen and we open up the risk of entering
  // an endless updating cycle.
  if (fabsf(contentMetrics.GetScrollOffset().x - compositorMetrics.GetScrollOffset().x) <= 2 &&
      fabsf(contentMetrics.GetScrollOffset().y - compositorMetrics.GetScrollOffset().y) <= 2 &&
      fabsf(contentMetrics.mDisplayPort.x - compositorMetrics.mDisplayPort.x) <= 2 &&
      fabsf(contentMetrics.mDisplayPort.y - compositorMetrics.mDisplayPort.y) <= 2 &&
      fabsf(contentMetrics.mDisplayPort.width - compositorMetrics.mDisplayPort.width) <= 2 &&
      fabsf(contentMetrics.mDisplayPort.height - compositorMetrics.mDisplayPort.height)) {
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
  CSSRect painted = (aContentMetrics.mCriticalDisplayPort.IsEmpty()
                      ? aContentMetrics.mDisplayPort
                      : aContentMetrics.mCriticalDisplayPort)
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
  painted = painted.Intersect(aContentMetrics.mScrollableRect);
  showing = showing.Intersect(aContentMetrics.mScrollableRect);

  if (!painted.Contains(showing)) {
    TILING_LOG("TILING: About to checkerboard; content %s\n", Stringify(aContentMetrics).c_str());
    TILING_LOG("TILING: About to checkerboard; painted %s\n", Stringify(painted).c_str());
    TILING_LOG("TILING: About to checkerboard; compositor %s\n", Stringify(aCompositorMetrics).c_str());
    TILING_LOG("TILING: About to checkerboard; showing %s\n", Stringify(showing).c_str());
    return true;
  }
  return false;
}

ClientTiledLayerBuffer::ClientTiledLayerBuffer(ClientTiledPaintedLayer* aPaintedLayer,
                                               CompositableClient* aCompositableClient,
                                               ClientLayerManager* aManager,
                                               SharedFrameMetricsHelper* aHelper)
  : mPaintedLayer(aPaintedLayer)
  , mCompositableClient(aCompositableClient)
  , mManager(aManager)
  , mLastPaintContentType(gfxContentType::COLOR)
  , mLastPaintSurfaceMode(SurfaceMode::SURFACE_OPAQUE)
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
    mPaintedLayer->CanUseOpaqueSurface() ? gfxContentType::COLOR :
                                          gfxContentType::COLOR_ALPHA;
  SurfaceMode mode = mPaintedLayer->GetSurfaceMode();

  if (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
#if defined(MOZ_GFX_OPTIMIZE_MOBILE) || defined(MOZ_WIDGET_GONK)
     mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
  }
#else
      if (!mPaintedLayer->GetParent() ||
          !mPaintedLayer->GetParent()->SupportsComponentAlphaChildren() ||
          !gfxPrefs::TiledDrawTargetEnabled()) {
        mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
      } else {
        content = gfxContentType::COLOR;
      }
  } else if (mode == SurfaceMode::SURFACE_OPAQUE) {
    if (mPaintedLayer->MayResample()) {
      mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
      content = gfxContentType::COLOR_ALPHA;
    }
  }
#endif

  if (aMode) {
    *aMode = mode;
  }
  return content;
}

gfxMemorySharedReadLock::gfxMemorySharedReadLock()
  : mReadCount(1)
{
  MOZ_COUNT_CTOR(gfxMemorySharedReadLock);
}

gfxMemorySharedReadLock::~gfxMemorySharedReadLock()
{
  MOZ_ASSERT(mReadCount == 0);
  MOZ_COUNT_DTOR(gfxMemorySharedReadLock);
}

int32_t
gfxMemorySharedReadLock::ReadLock()
{
  NS_ASSERT_OWNINGTHREAD(gfxMemorySharedReadLock);

  return PR_ATOMIC_INCREMENT(&mReadCount);
}

int32_t
gfxMemorySharedReadLock::ReadUnlock()
{
  int32_t readCount = PR_ATOMIC_DECREMENT(&mReadCount);
  NS_ASSERTION(readCount >= 0, "ReadUnlock called without ReadLock.");

  return readCount;
}

int32_t
gfxMemorySharedReadLock::GetReadCount()
{
  NS_ASSERT_OWNINGTHREAD(gfxMemorySharedReadLock);
  return mReadCount;
}

gfxShmSharedReadLock::gfxShmSharedReadLock(ISurfaceAllocator* aAllocator)
  : mAllocator(aAllocator)
  , mAllocSuccess(false)
{
  MOZ_COUNT_CTOR(gfxShmSharedReadLock);
  MOZ_ASSERT(mAllocator);
  if (mAllocator) {
#define MOZ_ALIGN_WORD(x) (((x) + 3) & ~3)
    if (mAllocator->AllocShmemSection(MOZ_ALIGN_WORD(sizeof(ShmReadLockInfo)), &mShmemSection)) {
      ShmReadLockInfo* info = GetShmReadLockInfoPtr();
      info->readCount = 1;
      mAllocSuccess = true;
    }
  }
}

gfxShmSharedReadLock::~gfxShmSharedReadLock()
{
  MOZ_COUNT_DTOR(gfxShmSharedReadLock);
}

int32_t
gfxShmSharedReadLock::ReadLock() {
  NS_ASSERT_OWNINGTHREAD(gfxShmSharedReadLock);
  if (!mAllocSuccess) {
    return 0;
  }
  ShmReadLockInfo* info = GetShmReadLockInfoPtr();
  return PR_ATOMIC_INCREMENT(&info->readCount);
}

int32_t
gfxShmSharedReadLock::ReadUnlock() {
  if (!mAllocSuccess) {
    return 0;
  }
  ShmReadLockInfo* info = GetShmReadLockInfoPtr();
  int32_t readCount = PR_ATOMIC_DECREMENT(&info->readCount);
  NS_ASSERTION(readCount >= 0, "ReadUnlock called without a ReadLock.");
  if (readCount <= 0) {
    mAllocator->FreeShmemSection(mShmemSection);
  }
  return readCount;
}

int32_t
gfxShmSharedReadLock::GetReadCount() {
  NS_ASSERT_OWNINGTHREAD(gfxShmSharedReadLock);
  if (!mAllocSuccess) {
    return 0;
  }
  ShmReadLockInfo* info = GetShmReadLockInfoPtr();
  return info->readCount;
}

class TileExpiry MOZ_FINAL : public nsExpirationTracker<TileClient, 3>
{
  public:
    TileExpiry() : nsExpirationTracker<TileClient, 3>(1000) {}

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
    virtual void NotifyExpired(TileClient* aTile) MOZ_OVERRIDE
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
  : mCompositableClient(nullptr)
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
  mBackLock = o.mBackLock;
  mFrontLock = o.mFrontLock;
  mCompositableClient = o.mCompositableClient;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
  mLastUpdate = o.mLastUpdate;
#endif
  mManager = o.mManager;
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
  mBackLock = o.mBackLock;
  mFrontLock = o.mFrontLock;
  mCompositableClient = o.mCompositableClient;
#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
  mLastUpdate = o.mLastUpdate;
#endif
  mManager = o.mManager;
  mInvalidFront = o.mInvalidFront;
  mInvalidBack = o.mInvalidBack;
  return *this;
}


void
TileClient::Flip()
{
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  if (mFrontBuffer && mFrontBuffer->GetIPDLActor() &&
      mCompositableClient && mCompositableClient->GetIPDLActor()) {
    // remove old buffer from CompositableHost
    RefPtr<AsyncTransactionTracker> tracker = new RemoveTextureFromCompositableTracker();
    // Hold TextureClient until transaction complete.
    tracker->SetTextureClient(mFrontBuffer);
    mFrontBuffer->SetRemoveFromCompositableTracker(tracker);
    // RemoveTextureFromCompositableAsync() expects CompositorChild's presence.
    mManager->AsShadowForwarder()->RemoveTextureFromCompositableAsync(tracker,
                                                                      mCompositableClient,
                                                                      mFrontBuffer);
  }
#endif
  RefPtr<TextureClient> frontBuffer = mFrontBuffer;
  RefPtr<TextureClient> frontBufferOnWhite = mFrontBufferOnWhite;
  mFrontBuffer = mBackBuffer;
  mFrontBufferOnWhite = mBackBufferOnWhite;
  mBackBuffer.Set(this, frontBuffer);
  mBackBufferOnWhite = frontBufferOnWhite;
  RefPtr<gfxSharedReadLock> frontLock = mFrontLock;
  mFrontLock = mBackLock;
  mBackLock = frontLock;
  nsIntRegion invalidFront = mInvalidFront;
  mInvalidFront = mInvalidBack;
  mInvalidBack = invalidFront;
}

static bool
CopyFrontToBack(TextureClient* aFront,
                TextureClient* aBack,
                const gfx::IntRect& aRectToCopy)
{
  if (!aFront->Lock(OpenMode::OPEN_READ)) {
    NS_WARNING("Failed to lock the tile's front buffer");
    return false;
  }

  if (!aBack->Lock(OpenMode::OPEN_WRITE)) {
    NS_WARNING("Failed to lock the tile's back buffer");
    return false;
  }

  gfx::IntPoint rectToCopyTopLeft = aRectToCopy.TopLeft();
  aFront->CopyToTextureClient(aBack, &aRectToCopy, &rectToCopyTopLeft);
  return true;
}

void
TileClient::ValidateBackBufferFromFront(const nsIntRegion& aDirtyRegion,
                                        bool aCanRerasterizeValidRegion,
                                        nsIntRegion& aAddPaintedRegion)
{
  if (mBackBuffer && mFrontBuffer) {
    gfx::IntSize tileSize = mFrontBuffer->GetSize();
    const nsIntRect tileRect = nsIntRect(0, 0, tileSize.width, tileSize.height);

    if (aDirtyRegion.Contains(tileRect)) {
      // The dirty region means that we no longer need the front buffer, so
      // discard it.
      DiscardFrontBuffer();
    } else {
      // Region that needs copying.
      nsIntRegion regionToCopy = mInvalidBack;

      regionToCopy.Sub(regionToCopy, aDirtyRegion);

      aAddPaintedRegion = regionToCopy;

      if (regionToCopy.IsEmpty() ||
          (aCanRerasterizeValidRegion &&
           regionToCopy.Area() < tileSize.width * tileSize.height * MINIMUM_TILE_COPY_AREA)) {
        // Just redraw it all.
        return;
      }

      // Copy the bounding rect of regionToCopy. As tiles are quite small, it
      // is unlikely that we'd save much by copying each individual rect of the
      // region, but we can reevaluate this if it becomes an issue.
      const nsIntRect rectToCopy = regionToCopy.GetBounds();
      gfx::IntRect gfxRectToCopy(rectToCopy.x, rectToCopy.y, rectToCopy.width, rectToCopy.height);
      CopyFrontToBack(mFrontBuffer, mBackBuffer, gfxRectToCopy);

      if (mBackBufferOnWhite) {
        MOZ_ASSERT(mFrontBufferOnWhite);
        CopyFrontToBack(mFrontBufferOnWhite, mBackBufferOnWhite, gfxRectToCopy);
      }

      mInvalidBack.SetEmpty();
    }
  }
}

void
TileClient::DiscardFrontBuffer()
{
  if (mFrontBuffer) {
    MOZ_ASSERT(mFrontLock);
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
    MOZ_ASSERT(!mFrontBufferOnWhite);
    if (mFrontBuffer->GetIPDLActor() &&
        mCompositableClient && mCompositableClient->GetIPDLActor()) {
      // remove old buffer from CompositableHost
      RefPtr<AsyncTransactionTracker> tracker = new RemoveTextureFromCompositableTracker();
      // Hold TextureClient until transaction complete.
      tracker->SetTextureClient(mFrontBuffer);
      mFrontBuffer->SetRemoveFromCompositableTracker(tracker);
      // RemoveTextureFromCompositableAsync() expects CompositorChild's presence.
      mManager->AsShadowForwarder()->RemoveTextureFromCompositableAsync(tracker,
                                                                        mCompositableClient,
                                                                        mFrontBuffer);
    }
#endif
    mManager->ReturnTextureClientDeferred(*mFrontBuffer);
    if (mFrontBufferOnWhite) {
      mManager->ReturnTextureClientDeferred(*mFrontBufferOnWhite);
    }
    mFrontLock->ReadUnlock();
    if (mFrontBuffer->IsLocked()) {
      mFrontBuffer->Unlock();
    }
    if (mFrontBufferOnWhite && mFrontBufferOnWhite->IsLocked()) {
      mFrontBufferOnWhite->Unlock();
    }
    mFrontBuffer = nullptr;
    mFrontBufferOnWhite = nullptr;
    mFrontLock = nullptr;
  }
}

void
TileClient::DiscardBackBuffer()
{
  if (mBackBuffer) {
    MOZ_ASSERT(mBackLock);
    if (!mBackBuffer->ImplementsLocking() && mBackLock->GetReadCount() > 1) {
      // Our current back-buffer is still locked by the compositor. This can occur
      // when the client is producing faster than the compositor can consume. In
      // this case we just want to drop it and not return it to the pool.
     mManager->ReportClientLost(*mBackBuffer);
     if (mBackBufferOnWhite) {
       mManager->ReportClientLost(*mBackBufferOnWhite);
     }
    } else {
      mManager->ReturnTextureClient(*mBackBuffer);
      if (mBackBufferOnWhite) {
        mManager->ReturnTextureClient(*mBackBufferOnWhite);
      }
    }
    mBackLock->ReadUnlock();
    if (mBackBuffer->IsLocked()) {
      mBackBuffer->Unlock();
    }
    if (mBackBufferOnWhite && mBackBufferOnWhite->IsLocked()) {
      mBackBufferOnWhite->Unlock();
    }
    mBackBuffer.Set(this, nullptr);
    mBackBufferOnWhite = nullptr;
    mBackLock = nullptr;
  }
}

TextureClient*
TileClient::GetBackBuffer(const nsIntRegion& aDirtyRegion,
                          gfxContentType aContent,
                          SurfaceMode aMode,
                          bool *aCreatedTextureClient,
                          nsIntRegion& aAddPaintedRegion,
                          bool aCanRerasterizeValidRegion,
                          RefPtr<TextureClient>* aBackBufferOnWhite)
{
  // Try to re-use the front-buffer if possible
  if (mFrontBuffer &&
      mFrontBuffer->HasInternalBuffer() &&
      mFrontLock->GetReadCount() == 1) {
    // If we had a backbuffer we no longer care about it since we'll
    // re-use the front buffer.
    DiscardBackBuffer();
    Flip();
    *aBackBufferOnWhite = mBackBufferOnWhite;
    return mBackBuffer;
  }

  if (!mBackBuffer ||
      mBackLock->GetReadCount() > 1) {

    if (mBackLock) {
      // Before we Replacing the lock by another one we need to unlock it!
      mBackLock->ReadUnlock();
    }

    if (mBackBuffer) {
      // Our current back-buffer is still locked by the compositor. This can occur
      // when the client is producing faster than the compositor can consume. In
      // this case we just want to drop it and not return it to the pool.
      mManager->ReportClientLost(*mBackBuffer);
    }
    if (mBackBufferOnWhite) {
      mManager->ReportClientLost(*mBackBufferOnWhite);
      mBackBufferOnWhite = nullptr;
    }

    TextureClientPool *pool =
      mManager->GetTexturePool(gfxPlatform::GetPlatform()->Optimal2DFormatForContent(aContent));
    mBackBuffer.Set(this, pool->GetTextureClient());
    if (!mBackBuffer) {
      return nullptr;
    }

    if (aMode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
      mBackBufferOnWhite = pool->GetTextureClient();
      if (!mBackBufferOnWhite) {
        mBackBuffer.Set(this, nullptr);
        return nullptr;
      }
    }

    // Create a lock for our newly created back-buffer.
    if (mManager->AsShadowForwarder()->IsSameProcess()) {
      // If our compositor is in the same process, we can save some cycles by not
      // using shared memory.
      mBackLock = new gfxMemorySharedReadLock();
    } else {
      mBackLock = new gfxShmSharedReadLock(mManager->AsShadowForwarder());
    }

    MOZ_ASSERT(mBackLock->IsValid());

    *aCreatedTextureClient = true;
    mInvalidBack = nsIntRect(0, 0, mBackBuffer->GetSize().width, mBackBuffer->GetSize().height);
  }

  ValidateBackBufferFromFront(aDirtyRegion, aCanRerasterizeValidRegion, aAddPaintedRegion);

  *aBackBufferOnWhite = mBackBufferOnWhite;
  return mBackBuffer;
}

TileDescriptor
TileClient::GetTileDescriptor()
{
  if (IsPlaceholderTile()) {
    return PlaceholderTileDescriptor();
  }
  MOZ_ASSERT(mFrontLock);
  if (mFrontLock->GetType() == gfxSharedReadLock::TYPE_MEMORY) {
    // AddRef here and Release when receiving on the host side to make sure the
    // reference count doesn't go to zero before the host receives the message.
    // see TiledLayerBufferComposite::TiledLayerBufferComposite
    mFrontLock->AddRef();
  }

  if (mFrontLock->GetType() == gfxSharedReadLock::TYPE_MEMORY) {
    return TexturedTileDescriptor(nullptr, mFrontBuffer->GetIPDLActor(),
                                  mFrontBufferOnWhite ? MaybeTexture(mFrontBufferOnWhite->GetIPDLActor()) : MaybeTexture(null_t()),
                                  TileLock(uintptr_t(mFrontLock.get())));
  } else {
    gfxShmSharedReadLock *lock = static_cast<gfxShmSharedReadLock*>(mFrontLock.get());
    return TexturedTileDescriptor(nullptr, mFrontBuffer->GetIPDLActor(),
                                  mFrontBufferOnWhite ? MaybeTexture(mFrontBufferOnWhite->GetIPDLActor()) : MaybeTexture(null_t()),
                                  TileLock(lock->GetShmemSection()));
  }
}

void
ClientTiledLayerBuffer::ReadUnlock() {
  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    if (mRetainedTiles[i].IsPlaceholderTile()) continue;
    mRetainedTiles[i].ReadUnlock();
  }
}

void
ClientTiledLayerBuffer::ReadLock() {
  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    if (mRetainedTiles[i].IsPlaceholderTile()) continue;
    mRetainedTiles[i].ReadLock();
  }
}

void
ClientTiledLayerBuffer::Release()
{
  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    if (mRetainedTiles[i].IsPlaceholderTile()) continue;
    mRetainedTiles[i].Release();
  }
}

void
ClientTiledLayerBuffer::DiscardBuffers()
{
  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    if (mRetainedTiles[i].IsPlaceholderTile()) continue;
    mRetainedTiles[i].DiscardFrontBuffer();
    mRetainedTiles[i].DiscardBackBuffer();
  }
}

SurfaceDescriptorTiles
ClientTiledLayerBuffer::GetSurfaceDescriptorTiles()
{
  InfallibleTArray<TileDescriptor> tiles;

  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    TileDescriptor tileDesc;
    if (mRetainedTiles.SafeElementAt(i, GetPlaceholderTile()) == GetPlaceholderTile()) {
      tileDesc = PlaceholderTileDescriptor();
    } else {
      tileDesc = mRetainedTiles[i].GetTileDescriptor();
    }
    tiles.AppendElement(tileDesc);
  }
  return SurfaceDescriptorTiles(mValidRegion, mPaintedRegion,
                                tiles, mRetainedWidth, mRetainedHeight,
                                mResolution, mFrameResolution.scale);
}

void
ClientTiledLayerBuffer::PaintThebes(const nsIntRegion& aNewValidRegion,
                                   const nsIntRegion& aPaintRegion,
                                   LayerManager::DrawPaintedLayerCallback aCallback,
                                   void* aCallbackData)
{
  TILING_LOG("TILING %p: PaintThebes painting region %s\n", mPaintedLayer, Stringify(aPaintRegion).c_str());
  TILING_LOG("TILING %p: PaintThebes new valid region %s\n", mPaintedLayer, Stringify(aNewValidRegion).c_str());

  mCallback = aCallback;
  mCallbackData = aCallbackData;

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  long start = PR_IntervalNow();
#endif

  // If this region is empty XMost() - 1 will give us a negative value.
  NS_ASSERTION(!aPaintRegion.GetBounds().IsEmpty(), "Empty paint region\n");

  bool useSinglePaintBuffer = UseSinglePaintBuffer();
  // XXX The single-tile case doesn't work at the moment, see bug 850396
  /*
  if (useSinglePaintBuffer) {
    // Check if the paint only spans a single tile. If that's
    // the case there's no point in using a single paint buffer.
    nsIntRect paintBounds = aPaintRegion.GetBounds();
    useSinglePaintBuffer = GetTileStart(paintBounds.x) !=
                           GetTileStart(paintBounds.XMost() - 1) ||
                           GetTileStart(paintBounds.y) !=
                           GetTileStart(paintBounds.YMost() - 1);
  }
  */
  if (useSinglePaintBuffer && !gfxPrefs::TiledDrawTargetEnabled()) {
    nsRefPtr<gfxContext> ctxt;

    const nsIntRect bounds = aPaintRegion.GetBounds();
    {
      PROFILER_LABEL("ClientTiledLayerBuffer", "PaintThebesSingleBufferAlloc",
        js::ProfileEntry::Category::GRAPHICS);

      gfxImageFormat format =
        gfxPlatform::GetPlatform()->OptimalFormatForContent(
          GetContentType());

      mSinglePaintDrawTarget =
        gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
          gfx::IntSize(ceilf(bounds.width * mResolution),
                       ceilf(bounds.height * mResolution)),
          gfx::ImageFormatToSurfaceFormat(format));

      if (!mSinglePaintDrawTarget) {
        return;
      }

      ctxt = new gfxContext(mSinglePaintDrawTarget);

      mSinglePaintBufferOffset = nsIntPoint(bounds.x, bounds.y);
    }
    ctxt->NewPath();
    ctxt->SetMatrix(
      ctxt->CurrentMatrix().Scale(mResolution, mResolution).
                            Translate(-bounds.x, -bounds.y));
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
    if (PR_IntervalNow() - start > 3) {
      printf_stderr("Slow alloc %i\n", PR_IntervalNow() - start);
    }
    start = PR_IntervalNow();
#endif
    PROFILER_LABEL("ClientTiledLayerBuffer", "PaintThebesSingleBufferDraw",
      js::ProfileEntry::Category::GRAPHICS);

    mCallback(mPaintedLayer, ctxt, aPaintRegion, DrawRegionClip::NONE, nsIntRegion(), mCallbackData);
  }

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 30) {
    const nsIntRect bounds = aPaintRegion.GetBounds();
    printf_stderr("Time to draw %i: %i, %i, %i, %i\n", PR_IntervalNow() - start, bounds.x, bounds.y, bounds.width, bounds.height);
    if (aPaintRegion.IsComplex()) {
      printf_stderr("Complex region\n");
      nsIntRegionRectIterator it(aPaintRegion);
      for (const nsIntRect* rect = it.Next(); rect != nullptr; rect = it.Next()) {
        printf_stderr(" rect %i, %i, %i, %i\n", rect->x, rect->y, rect->width, rect->height);
      }
    }
  }
  start = PR_IntervalNow();
#endif

  PROFILER_LABEL("ClientTiledLayerBuffer", "PaintThebesUpdate",
    js::ProfileEntry::Category::GRAPHICS);

  mNewValidRegion = aNewValidRegion;
  Update(aNewValidRegion, aPaintRegion);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 10) {
    const nsIntRect bounds = aPaintRegion.GetBounds();
    printf_stderr("Time to tile %i: %i, %i, %i, %i\n", PR_IntervalNow() - start, bounds.x, bounds.y, bounds.width, bounds.height);
  }
#endif

  mLastPaintContentType = GetContentType(&mLastPaintSurfaceMode);
  mCallback = nullptr;
  mCallbackData = nullptr;
  mSinglePaintDrawTarget = nullptr;
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
          memcpy(&bitmap[x1*bpp + (y1-1) * stride], &bitmap[x1*bpp + y1 * stride], (x2 - x1) * bpp);
        }
      } else if (side == VisitSide::BOTTOM) {
        if (y1 < height) {
          x1 = clamp(x1, 0, width - 1);
          x2 = clamp(x2, 0, width - 1);
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
ClientTiledLayerBuffer::PostValidate(const nsIntRegion& aPaintRegion)
{
  if (gfxPrefs::TiledDrawTargetEnabled() && mMoz2DTiles.size() > 0) {
    gfx::TileSet tileset;
    tileset.mTiles = &mMoz2DTiles[0];
    tileset.mTileCount = mMoz2DTiles.size();
    RefPtr<DrawTarget> drawTarget = gfx::Factory::CreateTiledDrawTarget(tileset);
    drawTarget->SetTransform(Matrix());

    RefPtr<gfxContext> ctx = new gfxContext(drawTarget);
    ctx->SetMatrix(
      ctx->CurrentMatrix().Scale(mResolution, mResolution));

    mCallback(mPaintedLayer, ctx, aPaintRegion, DrawRegionClip::DRAW, nsIntRegion(), mCallbackData);
    mMoz2DTiles.clear();
  }
}

void
ClientTiledLayerBuffer::UnlockTile(TileClient aTile)
{
  // We locked the back buffer, and flipped so we now need to unlock the front
  if (aTile.mFrontBuffer && aTile.mFrontBuffer->IsLocked()) {
    aTile.mFrontBuffer->Unlock();
  }
  if (aTile.mFrontBufferOnWhite && aTile.mFrontBufferOnWhite->IsLocked()) {
    aTile.mFrontBufferOnWhite->Unlock();
  }
  if (aTile.mBackBuffer && aTile.mBackBuffer->IsLocked()) {
    aTile.mBackBuffer->Unlock();
  }
  if (aTile.mBackBufferOnWhite && aTile.mBackBufferOnWhite->IsLocked()) {
    aTile.mBackBufferOnWhite->Unlock();
  }
}

TileClient
ClientTiledLayerBuffer::ValidateTile(TileClient aTile,
                                    const nsIntPoint& aTileOrigin,
                                    const nsIntRegion& aDirtyRegion)
{
  PROFILER_LABEL("ClientTiledLayerBuffer", "ValidateTile",
    js::ProfileEntry::Category::GRAPHICS);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (aDirtyRegion.IsComplex()) {
    printf_stderr("Complex region\n");
  }
#endif

  if (aTile.IsPlaceholderTile()) {
    aTile.SetLayerManager(mManager);
  }
  aTile.SetCompositableClient(mCompositableClient);

  bool createdTextureClient = false;
  nsIntRegion offsetScaledDirtyRegion = aDirtyRegion.MovedBy(-aTileOrigin);
  offsetScaledDirtyRegion.ScaleRoundOut(mResolution, mResolution);

  bool usingSinglePaintBuffer = !!mSinglePaintDrawTarget;
  SurfaceMode mode;
  gfxContentType content = GetContentType(&mode);
  nsIntRegion extraPainted;
  RefPtr<TextureClient> backBufferOnWhite;
  RefPtr<TextureClient> backBuffer =
    aTile.GetBackBuffer(offsetScaledDirtyRegion,
                        content, mode,
                        &createdTextureClient, extraPainted,
                        !usingSinglePaintBuffer && !gfxPrefs::TiledDrawTargetEnabled(),
                        &backBufferOnWhite);

  extraPainted.MoveBy(aTileOrigin);
  extraPainted.And(extraPainted, mNewValidRegion);
  mPaintedRegion.Or(mPaintedRegion, extraPainted);

  if (!backBuffer) {
    NS_WARNING("Failed to allocate a tile TextureClient");
    aTile.DiscardBackBuffer();
    aTile.DiscardFrontBuffer();
    return TileClient();
  }

  // the back buffer may have been already locked in ValidateBackBufferFromFront
  if (!backBuffer->IsLocked()) {
    if (!backBuffer->Lock(OpenMode::OPEN_READ_WRITE)) {
      NS_WARNING("Failed to lock a tile TextureClient");
      aTile.DiscardBackBuffer();
      aTile.DiscardFrontBuffer();
      return TileClient();
    }
  }

  if (backBufferOnWhite && !backBufferOnWhite->IsLocked()) {
    if (!backBufferOnWhite->Lock(OpenMode::OPEN_READ_WRITE)) {
      NS_WARNING("Failed to lock tile TextureClient for updating.");
      aTile.DiscardBackBuffer();
      aTile.DiscardFrontBuffer();
      return TileClient();
    }
  }

  if (gfxPrefs::TiledDrawTargetEnabled()) {
    aTile.Flip();

    if (createdTextureClient) {
      if (!mCompositableClient->AddTextureClient(backBuffer)) {
        NS_WARNING("Failed to add tile TextureClient.");
        aTile.DiscardFrontBuffer();
        aTile.DiscardBackBuffer();
        return aTile;
      }
      if (backBufferOnWhite && !mCompositableClient->AddTextureClient(backBufferOnWhite)) {
        NS_WARNING("Failed to add tile TextureClient.");
        aTile.DiscardFrontBuffer();
        aTile.DiscardBackBuffer();
        return aTile;
      }
    }

    // prepare an array of Moz2D tiles that will be painted into in PostValidate
    gfx::Tile moz2DTile;
    RefPtr<DrawTarget> dt = backBuffer->BorrowDrawTarget();
    RefPtr<DrawTarget> dtOnWhite;
    if (backBufferOnWhite) {
      dtOnWhite = backBufferOnWhite->BorrowDrawTarget();
      moz2DTile.mDrawTarget = Factory::CreateDualDrawTarget(dt, dtOnWhite);
    } else {
      moz2DTile.mDrawTarget = dt;
    }
    moz2DTile.mTileOrigin = gfx::IntPoint(aTileOrigin.x * mResolution, aTileOrigin.y * mResolution);
    if (!dt || (backBufferOnWhite && !dtOnWhite)) {
      aTile.DiscardFrontBuffer();
      return aTile;
    }

    mMoz2DTiles.push_back(moz2DTile);

    nsIntRegionRectIterator it(aDirtyRegion);
    for (const nsIntRect* dirtyRect = it.Next(); dirtyRect != nullptr; dirtyRect = it.Next()) {
      gfx::Rect drawRect(dirtyRect->x - aTileOrigin.x,
                         dirtyRect->y - aTileOrigin.y,
                         dirtyRect->width,
                         dirtyRect->height);
      drawRect.Scale(mResolution);

      gfx::IntRect copyRect(NS_roundf((dirtyRect->x - mSinglePaintBufferOffset.x) * mResolution),
                            NS_roundf((dirtyRect->y - mSinglePaintBufferOffset.y) * mResolution),
                            drawRect.width,
                            drawRect.height);
      gfx::IntPoint copyTarget(NS_roundf(drawRect.x), NS_roundf(drawRect.y));
      // Mark the newly updated area as invalid in the back buffer
      aTile.mInvalidBack.Or(aTile.mInvalidBack, nsIntRect(copyTarget.x, copyTarget.y, copyRect.width, copyRect.height));

      if (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
        dt->FillRect(drawRect, ColorPattern(Color(0.0, 0.0, 0.0, 1.0)));
        dtOnWhite->FillRect(drawRect, ColorPattern(Color(1.0, 1.0, 1.0, 1.0)));
      } else if (content == gfxContentType::COLOR_ALPHA) {
        dt->ClearRect(drawRect);
      }
    }

    return aTile;
  } else {
    MOZ_ASSERT(!backBufferOnWhite, "Component alpha only supported with TiledDrawTarget");
  }

  // We must not keep a reference to the DrawTarget after it has been unlocked,
  // make sure these are null'd before unlocking as destruction of the context
  // may cause the target to be flushed.
  RefPtr<DrawTarget> drawTarget = backBuffer->BorrowDrawTarget();
  drawTarget->SetTransform(Matrix());

  RefPtr<gfxContext> ctxt = new gfxContext(drawTarget);

  if (usingSinglePaintBuffer) {
    // XXX Perhaps we should just copy the bounding rectangle here?
    RefPtr<gfx::SourceSurface> source = mSinglePaintDrawTarget->Snapshot();
    nsIntRegionRectIterator it(aDirtyRegion);
    for (const nsIntRect* dirtyRect = it.Next(); dirtyRect != nullptr; dirtyRect = it.Next()) {
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
      printf_stderr(" break into subdirtyRect %i, %i, %i, %i\n",
                    dirtyRect->x, dirtyRect->y, dirtyRect->width, dirtyRect->height);
#endif
      gfx::Rect drawRect(dirtyRect->x - aTileOrigin.x,
                         dirtyRect->y - aTileOrigin.y,
                         dirtyRect->width,
                         dirtyRect->height);
      drawRect.Scale(mResolution);

      gfx::IntRect copyRect(NS_roundf((dirtyRect->x - mSinglePaintBufferOffset.x) * mResolution),
                            NS_roundf((dirtyRect->y - mSinglePaintBufferOffset.y) * mResolution),
                            drawRect.width,
                            drawRect.height);
      gfx::IntPoint copyTarget(NS_roundf(drawRect.x), NS_roundf(drawRect.y));
      drawTarget->CopySurface(source, copyRect, copyTarget);

      // Mark the newly updated area as invalid in the front buffer
      aTile.mInvalidFront.Or(aTile.mInvalidFront, nsIntRect(copyTarget.x, copyTarget.y, copyRect.width, copyRect.height));
    }

    // only worry about padding when not doing low-res
    // because it simplifies the math and the artifacts
    // won't be noticable
    if (mResolution == 1) {
      nsIntRect unscaledTile = nsIntRect(aTileOrigin.x,
                                         aTileOrigin.y,
                                         GetTileSize().width,
                                         GetTileSize().height);

      nsIntRegion tileValidRegion = GetValidRegion();
      tileValidRegion.Or(tileValidRegion, aDirtyRegion);
      // We only need to pad out if the tile has area that's not valid
      if (!tileValidRegion.Contains(unscaledTile)) {
        tileValidRegion = tileValidRegion.Intersect(unscaledTile);
        // translate the region into tile space and pad
        tileValidRegion.MoveBy(-nsIntPoint(unscaledTile.x, unscaledTile.y));
        PadDrawTargetOutFromRegion(drawTarget, tileValidRegion);
      }
    }

    // The new buffer is now validated, remove the dirty region from it.
    aTile.mInvalidBack.Sub(nsIntRect(0, 0, GetTileSize().width, GetTileSize().height),
                           offsetScaledDirtyRegion);
  } else {
    // Area of the full tile...
    nsIntRegion tileRegion =
      nsIntRect(aTileOrigin.x, aTileOrigin.y,
                GetScaledTileSize().width, GetScaledTileSize().height);

    // Intersect this area with the portion that's dirty.
    tileRegion = tileRegion.Intersect(aDirtyRegion);

    // Add the resolution scale to store the dirty region.
    nsIntPoint unscaledTileOrigin = nsIntPoint(aTileOrigin.x * mResolution,
                                               aTileOrigin.y * mResolution);
    nsIntRegion unscaledTileRegion(tileRegion);
    unscaledTileRegion.ScaleRoundOut(mResolution, mResolution);

    // Move invalid areas into scaled layer space.
    aTile.mInvalidFront.MoveBy(unscaledTileOrigin);
    aTile.mInvalidBack.MoveBy(unscaledTileOrigin);

    // Add the area that's going to be redrawn to the invalid area of the
    // front region.
    aTile.mInvalidFront.Or(aTile.mInvalidFront, unscaledTileRegion);

    // Add invalid areas of the backbuffer to the area to redraw.
    tileRegion.Or(tileRegion, aTile.mInvalidBack);

    // Move invalid areas back into tile space.
    aTile.mInvalidFront.MoveBy(-unscaledTileOrigin);

    // This will be validated now.
    aTile.mInvalidBack.SetEmpty();

    nsIntRect bounds = tileRegion.GetBounds();
    bounds.MoveBy(-aTileOrigin);

    if (GetContentType() != gfxContentType::COLOR) {
      drawTarget->ClearRect(Rect(bounds.x, bounds.y, bounds.width, bounds.height));
    }

    ctxt->NewPath();
    ctxt->Clip(gfxRect(bounds.x, bounds.y, bounds.width, bounds.height));
    ctxt->SetMatrix(
      ctxt->CurrentMatrix().Translate(-unscaledTileOrigin.x,
                                      -unscaledTileOrigin.y).
                            Scale(mResolution, mResolution));
    mCallback(mPaintedLayer, ctxt,
              tileRegion.GetBounds(),
              DrawRegionClip::NONE,
              nsIntRegion(), mCallbackData);

  }

#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
  DrawDebugOverlay(drawTarget, aTileOrigin.x * mResolution,
                   aTileOrigin.y * mResolution, GetTileLength(), GetTileLength());
#endif

  ctxt = nullptr;
  drawTarget = nullptr;

  nsIntRegion tileRegion =
    nsIntRect(aTileOrigin.x, aTileOrigin.y,
              GetScaledTileSize().width, GetScaledTileSize().height);
  // Intersect this area with the portion that's invalid.
  tileRegion = tileRegion.Sub(tileRegion, GetValidRegion());
  tileRegion = tileRegion.Sub(tileRegion, aDirtyRegion); // Has now been validated

  backBuffer->SetWaste(tileRegion.Area() * mResolution * mResolution);
  backBuffer->Unlock();

  if (createdTextureClient) {
    if (!mCompositableClient->AddTextureClient(backBuffer)) {
      NS_WARNING("Failed to add tile TextureClient.");
      aTile.DiscardFrontBuffer();
      aTile.DiscardBackBuffer();
      return aTile;
    }
  }

  aTile.Flip();

  // Note, we don't call UpdatedTexture. The Updated function is called manually
  // by the TiledContentHost before composition.

  if (backBuffer->HasInternalBuffer()) {
    // If our new buffer has an internal buffer, we don't want to keep another
    // TextureClient around unnecessarily, so discard the back-buffer.
    aTile.DiscardBackBuffer();
  }

  return aTile;
}

/**
 * This function takes the transform stored in aTransformToCompBounds
 * (which was generated in GetTransformToAncestorsParentLayer), and
 * modifies it with the ViewTransform from the compositor side so that
 * it reflects what the compositor is actually rendering. This operation
 * basically replaces the nontransient async transform that was injected
 * in GetTransformToAncestorsParentLayer with the complete async transform.
 * This function then returns the scroll ancestor's composition bounds,
 * transformed into the painted layer's LayerPixel coordinates, accounting
 * for the compositor state.
 */
static LayerRect
GetCompositorSideCompositionBounds(const LayerMetricsWrapper& aScrollAncestor,
                                   const Matrix4x4& aTransformToCompBounds,
                                   const ViewTransform& aAPZTransform)
{
  Matrix4x4 nonTransientAPZUntransform = Matrix4x4::Scaling(
    aScrollAncestor.Metrics().mResolution.scale,
    aScrollAncestor.Metrics().mResolution.scale,
    1.f);
  nonTransientAPZUntransform.Invert();

  // Take off the last "term" of aTransformToCompBounds, which
  // is the APZ's nontransient async transform. Replace it with
  // the APZ's async transform (this includes the nontransient
  // component as well).
  Matrix4x4 transform = aTransformToCompBounds
                      * nonTransientAPZUntransform
                      * Matrix4x4(aAPZTransform);
  transform.Invert();

  return TransformTo<LayerPixel>(transform,
            aScrollAncestor.Metrics().mCompositionBounds);
}

bool
ClientTiledLayerBuffer::ComputeProgressiveUpdateRegion(const nsIntRegion& aInvalidRegion,
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

  TILING_LOG("TILING %p: Progressive update stale region %s\n", mPaintedLayer, Stringify(staleRegion).c_str());

  LayerMetricsWrapper scrollAncestor;
  mPaintedLayer->GetAncestorLayers(&scrollAncestor, nullptr);

  // Find out the current view transform to determine which tiles to draw
  // first, and see if we should just abort this paint. Aborting is usually
  // caused by there being an incoming, more relevant paint.
  ViewTransform viewTransform;
#if defined(MOZ_WIDGET_ANDROID) && !defined(MOZ_ANDROID_APZ)
  FrameMetrics contentMetrics = scrollAncestor.Metrics();
  bool abortPaint = false;
  // On Android, only the primary scrollable layer is async-scrolled, and the only one
  // that the Java-side code can provide details about. If we're tiling some other layer
  // then we already have all the information we need about it.
  if (contentMetrics.GetScrollId() == mManager->GetRootScrollableLayerId()) {
    FrameMetrics compositorMetrics = contentMetrics;
    // The ProgressiveUpdateCallback updates the compositorMetrics
    abortPaint = mManager->ProgressiveUpdateCallback(!staleRegion.Contains(aInvalidRegion),
                                                     compositorMetrics,
                                                     !drawingLowPrecision);
    viewTransform = ComputeViewTransform(contentMetrics, compositorMetrics);
  }
#else
  MOZ_ASSERT(mSharedFrameMetricsHelper);

  bool abortPaint =
    mSharedFrameMetricsHelper->UpdateFromCompositorFrameMetrics(
      scrollAncestor,
      !staleRegion.Contains(aInvalidRegion),
      drawingLowPrecision,
      viewTransform);
#endif

  TILING_LOG("TILING %p: Progressive update view transform %s zoom %f abort %d\n",
      mPaintedLayer, ToString(viewTransform.mTranslation).c_str(), viewTransform.mScale.scale, abortPaint);

  if (abortPaint) {
    // We ignore if front-end wants to abort if this is the first,
    // non-low-precision paint, as in that situation, we're about to override
    // front-end's page/viewport metrics.
    if (!aPaintData->mFirstPaint || drawingLowPrecision) {
      PROFILER_LABEL("ClientTiledLayerBuffer", "ComputeProgressiveUpdateRegion",
        js::ProfileEntry::Category::GRAPHICS);

      aRegionToPaint.SetEmpty();
      return aIsRepeated;
    }
  }

  LayerRect transformedCompositionBounds =
    GetCompositorSideCompositionBounds(scrollAncestor,
                                       aPaintData->mTransformToCompBounds,
                                       viewTransform);

  TILING_LOG("TILING %p: Progressive update transformed compositor bounds %s\n", mPaintedLayer, Stringify(transformedCompositionBounds).c_str());

  // Compute a "coherent update rect" that we should paint all at once in a
  // single transaction. This is to avoid rendering glitches on animated
  // page content, and when layers change size/shape.
  // On Fennec uploads are more expensive because we're not using gralloc, so
  // we use a coherent update rect that is intersected with the screen at the
  // time of issuing the draw command. This will paint faster but also potentially
  // make the progressive paint more visible to the user while scrolling.
  // On B2G uploads are cheaper and we value coherency more, especially outside
  // the browser, so we always use the entire user-visible area.
  nsIntRect coherentUpdateRect(LayerIntRect::ToUntyped(RoundedOut(
#ifdef MOZ_WIDGET_ANDROID
    transformedCompositionBounds.Intersect(aPaintData->mCompositionBounds)
#else
    transformedCompositionBounds
#endif
  )));

  TILING_LOG("TILING %p: Progressive update final coherency rect %s\n", mPaintedLayer, Stringify(coherentUpdateRect).c_str());

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

  TILING_LOG("TILING %p: Progressive update final paint region %s\n", mPaintedLayer, Stringify(aRegionToPaint).c_str());

  // Paint area that's visible and overlaps previously valid content to avoid
  // visible glitches in animated elements, such as gifs.
  bool paintInSingleTransaction = paintingVisible && (drawingStale || aPaintData->mFirstPaint);

  TILING_LOG("TILING %p: paintingVisible %d drawingStale %d firstPaint %d singleTransaction %d\n",
    mPaintedLayer, paintingVisible, drawingStale, aPaintData->mFirstPaint, paintInSingleTransaction);

  // The following code decides what order to draw tiles in, based on the
  // current scroll direction of the primary scrollable layer.
  NS_ASSERTION(!aRegionToPaint.IsEmpty(), "Unexpectedly empty paint region!");
  nsIntRect paintBounds = aRegionToPaint.GetBounds();

  int startX, incX, startY, incY;
  gfx::IntSize scaledTileSize = GetScaledTileSize();
  if (aPaintData->mScrollOffset.x >= aPaintData->mLastScrollOffset.x) {
    startX = RoundDownToTileEdge(paintBounds.x, scaledTileSize.width);
    incX = scaledTileSize.width;
  } else {
    startX = RoundDownToTileEdge(paintBounds.XMost() - 1, scaledTileSize.width);
    incX = -scaledTileSize.width;
  }

  if (aPaintData->mScrollOffset.y >= aPaintData->mLastScrollOffset.y) {
    startY = RoundDownToTileEdge(paintBounds.y, scaledTileSize.height);
    incY = scaledTileSize.height;
  } else {
    startY = RoundDownToTileEdge(paintBounds.YMost() - 1, scaledTileSize.height);
    incY = -scaledTileSize.height;
  }

  // Find a tile to draw.
  nsIntRect tileBounds(startX, startY, scaledTileSize.width, scaledTileSize.height);
  int32_t scrollDiffX = aPaintData->mScrollOffset.x - aPaintData->mLastScrollOffset.x;
  int32_t scrollDiffY = aPaintData->mScrollOffset.y - aPaintData->mLastScrollOffset.y;
  // This loop will always terminate, as there is at least one tile area
  // along the first/last row/column intersecting with regionToPaint, or its
  // bounds would have been smaller.
  while (true) {
    aRegionToPaint.And(aInvalidRegion, tileBounds);
    if (!aRegionToPaint.IsEmpty()) {
      break;
    }
    if (Abs(scrollDiffY) >= Abs(scrollDiffX)) {
      tileBounds.x += incX;
    } else {
      tileBounds.y += incY;
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
ClientTiledLayerBuffer::ProgressiveUpdate(nsIntRegion& aValidRegion,
                                         nsIntRegion& aInvalidRegion,
                                         const nsIntRegion& aOldValidRegion,
                                         BasicTiledLayerPaintData* aPaintData,
                                         LayerManager::DrawPaintedLayerCallback aCallback,
                                         void* aCallbackData)
{
  TILING_LOG("TILING %p: Progressive update valid region %s\n", mPaintedLayer, Stringify(aValidRegion).c_str());
  TILING_LOG("TILING %p: Progressive update invalid region %s\n", mPaintedLayer, Stringify(aInvalidRegion).c_str());
  TILING_LOG("TILING %p: Progressive update old valid region %s\n", mPaintedLayer, Stringify(aOldValidRegion).c_str());

  bool repeat = false;
  bool isBufferChanged = false;
  do {
    // Compute the region that should be updated. Repeat as many times as
    // is required.
    nsIntRegion regionToPaint;
    repeat = ComputeProgressiveUpdateRegion(aInvalidRegion,
                                            aOldValidRegion,
                                            regionToPaint,
                                            aPaintData,
                                            repeat);

    TILING_LOG("TILING %p: Progressive update computed paint region %s repeat %d\n", mPaintedLayer, Stringify(regionToPaint).c_str(), repeat);

    // There's no further work to be done.
    if (regionToPaint.IsEmpty()) {
      break;
    }

    isBufferChanged = true;

    // Keep track of what we're about to refresh.
    aValidRegion.Or(aValidRegion, regionToPaint);

    // aValidRegion may have been altered by InvalidateRegion, but we still
    // want to display stale content until it gets progressively updated.
    // Create a region that includes stale content.
    nsIntRegion validOrStale;
    validOrStale.Or(aValidRegion, aOldValidRegion);

    // Paint the computed region and subtract it from the invalid region.
    PaintThebes(validOrStale, regionToPaint, aCallback, aCallbackData);
    aInvalidRegion.Sub(aInvalidRegion, regionToPaint);
  } while (repeat);

  TILING_LOG("TILING %p: Progressive update final valid region %s buffer changed %d\n", mPaintedLayer, Stringify(aValidRegion).c_str(), isBufferChanged);
  TILING_LOG("TILING %p: Progressive update final invalid region %s\n", mPaintedLayer, Stringify(aInvalidRegion).c_str());

  // Return false if nothing has been drawn, or give what has been drawn
  // to the shadow layer to upload.
  return isBufferChanged;
}

}
}
