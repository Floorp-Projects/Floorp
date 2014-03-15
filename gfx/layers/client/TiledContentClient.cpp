/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TiledContentClient.h"
#include <math.h>                       // for ceil, ceilf, floor
#include "ClientTiledThebesLayer.h"     // for ClientTiledThebesLayer
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "ClientLayerManager.h"         // for ClientLayerManager
#include "CompositorChild.h"            // for CompositorChild
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPrefs.h"                   // for gfxPrefs
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/MathAlgorithms.h"     // for Abs
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder
#include "TextureClientPool.h"
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for gfxContext::AddRef, etc
#include "nsSize.h"                     // for nsIntSize
#include "gfxReusableSharedImageSurfaceWrapper.h"
#include "nsMathUtils.h"               // for NS_roundf
#include "gfx2DGlue.h"

// This is the minimum area that we deem reasonable to copy from the front buffer to the
// back buffer on tile updates. If the valid region is smaller than this, we just
// redraw it and save on the copy (and requisite surface-locking involved).
#define MINIMUM_TILE_COPY_AREA ((TILEDLAYERBUFFER_TILE_SIZE * TILEDLAYERBUFFER_TILE_SIZE)/16)

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


TiledContentClient::TiledContentClient(ClientTiledThebesLayer* aThebesLayer,
                                       ClientLayerManager* aManager)
  : CompositableClient(aManager->AsShadowForwarder())
{
  MOZ_COUNT_CTOR(TiledContentClient);

  mTiledBuffer = ClientTiledLayerBuffer(aThebesLayer, this, aManager,
                                        &mSharedFrameMetricsHelper);
  mLowPrecisionTiledBuffer = ClientTiledLayerBuffer(aThebesLayer, this, aManager,
                                                    &mSharedFrameMetricsHelper);

  mLowPrecisionTiledBuffer.SetResolution(gfxPrefs::LowPrecisionResolution()/1000.f);
}

void
TiledContentClient::ClearCachedResources()
{
  mTiledBuffer.DiscardBackBuffers();
  mLowPrecisionTiledBuffer.DiscardBackBuffers();
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

bool
SharedFrameMetricsHelper::UpdateFromCompositorFrameMetrics(
    ContainerLayer* aLayer,
    bool aHasPendingNewThebesContent,
    bool aLowPrecision,
    ParentLayerRect& aCompositionBounds,
    CSSToParentLayerScale& aZoom)
{
  MOZ_ASSERT(aLayer);

  CompositorChild* compositor = CompositorChild::Get();

  if (!compositor) {
    FindFallbackContentFrameMetrics(aLayer, aCompositionBounds, aZoom);
    return false;
  }

  const FrameMetrics& contentMetrics = aLayer->GetFrameMetrics();
  FrameMetrics compositorMetrics;

  if (!compositor->LookupCompositorFrameMetrics(contentMetrics.mScrollId,
                                                compositorMetrics)) {
    FindFallbackContentFrameMetrics(aLayer, aCompositionBounds, aZoom);
    return false;
  }

  aCompositionBounds = ParentLayerRect(compositorMetrics.mCompositionBounds);
  aZoom = compositorMetrics.GetZoomToParent();

  // Reset the checkerboard risk flag when switching to low precision
  // rendering.
  if (aLowPrecision && !mLastProgressiveUpdateWasLowPrecision) {
    // Skip low precision rendering until we're at risk of checkerboarding.
    if (!mProgressiveUpdateWasInDanger) {
      return true;
    }
    mProgressiveUpdateWasInDanger = false;
  }
  mLastProgressiveUpdateWasLowPrecision = aLowPrecision;

  // Always abort updates if the resolution has changed. There's no use
  // in drawing at the incorrect resolution.
  if (!FuzzyEquals(compositorMetrics.GetZoom().scale, contentMetrics.GetZoom().scale)) {
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
    if (AboutToCheckerboard(contentMetrics, compositorMetrics)) {
      mProgressiveUpdateWasInDanger = true;
      return true;
    }
  }

  // Abort drawing stale low-precision content if there's a more recent
  // display-port in the pipeline.
  if (aLowPrecision && !aHasPendingNewThebesContent) {
    return true;
  }

  return false;
}

void
SharedFrameMetricsHelper::FindFallbackContentFrameMetrics(ContainerLayer* aLayer,
                                                          ParentLayerRect& aCompositionBounds,
                                                          CSSToParentLayerScale& aZoom) {
  if (!aLayer) {
    return;
  }

  ContainerLayer* layer = aLayer;
  const FrameMetrics* contentMetrics = &(layer->GetFrameMetrics());

  // Walk up the layer tree until a valid composition bounds is found
  while (layer && contentMetrics->mCompositionBounds.IsEmpty()) {
    layer = layer->GetParent();
    contentMetrics = layer ? &(layer->GetFrameMetrics()) : contentMetrics;
  }

  MOZ_ASSERT(!contentMetrics->mCompositionBounds.IsEmpty());

  aCompositionBounds = ParentLayerRect(contentMetrics->mCompositionBounds);
  aZoom = contentMetrics->GetZoomToParent();  // TODO(botond): double-check this
  return;
}

bool
SharedFrameMetricsHelper::AboutToCheckerboard(const FrameMetrics& aContentMetrics,
                                              const FrameMetrics& aCompositorMetrics)
{
  return !aContentMetrics.mDisplayPort.Contains(CSSRect(aCompositorMetrics.CalculateCompositedRectInCssPixels()) - aCompositorMetrics.GetScrollOffset());
}

ClientTiledLayerBuffer::ClientTiledLayerBuffer(ClientTiledThebesLayer* aThebesLayer,
                                             CompositableClient* aCompositableClient,
                                             ClientLayerManager* aManager,
                                             SharedFrameMetricsHelper* aHelper)
  : mThebesLayer(aThebesLayer)
  , mCompositableClient(aCompositableClient)
  , mManager(aManager)
  , mLastPaintOpaque(false)
  , mSharedFrameMetricsHelper(aHelper)
{
}

bool
ClientTiledLayerBuffer::HasFormatChanged() const
{
  return mThebesLayer->CanUseOpaqueSurface() != mLastPaintOpaque;
}


gfxContentType
ClientTiledLayerBuffer::GetContentType() const
{
  if (mThebesLayer->CanUseOpaqueSurface()) {
    return gfxContentType::COLOR;
  } else {
    return gfxContentType::COLOR_ALPHA;
  }
}

gfxMemorySharedReadLock::gfxMemorySharedReadLock()
  : mReadCount(1)
{
  MOZ_COUNT_CTOR(gfxMemorySharedReadLock);
}

gfxMemorySharedReadLock::~gfxMemorySharedReadLock()
{
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

// Placeholder
TileClient::TileClient()
  : mBackBuffer(nullptr)
  , mFrontBuffer(nullptr)
  , mBackLock(nullptr)
  , mFrontLock(nullptr)
{
}

TileClient::TileClient(const TileClient& o)
{
  mBackBuffer = o.mBackBuffer;
  mFrontBuffer = o.mFrontBuffer;
  mBackLock = o.mBackLock;
  mFrontLock = o.mFrontLock;
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
  mBackBuffer = o.mBackBuffer;
  mFrontBuffer = o.mFrontBuffer;
  mBackLock = o.mBackLock;
  mFrontLock = o.mFrontLock;
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
  RefPtr<TextureClient> frontBuffer = mFrontBuffer;
  mFrontBuffer = mBackBuffer;
  mBackBuffer = frontBuffer;
  RefPtr<gfxSharedReadLock> frontLock = mFrontLock;
  mFrontLock = mBackLock;
  mBackLock = frontLock;
  nsIntRegion invalidFront = mInvalidFront;
  mInvalidFront = mInvalidBack;
  mInvalidBack = invalidFront;
}

void
TileClient::ValidateBackBufferFromFront(const nsIntRegion& aDirtyRegion,
                                        bool aCanRerasterizeValidRegion)
{
  if (mBackBuffer && mFrontBuffer) {
    const nsIntRect tileRect = nsIntRect(0, 0, TILEDLAYERBUFFER_TILE_SIZE, TILEDLAYERBUFFER_TILE_SIZE);

    if (aDirtyRegion.Contains(tileRect)) {
      // The dirty region means that we no longer need the front buffer, so
      // discard it.
      DiscardFrontBuffer();
    } else {
      // Region that needs copying.
      nsIntRegion regionToCopy = mInvalidBack;

      regionToCopy.Sub(regionToCopy, aDirtyRegion);

      if (regionToCopy.IsEmpty() ||
          (aCanRerasterizeValidRegion &&
           regionToCopy.Area() < MINIMUM_TILE_COPY_AREA)) {
        // Just redraw it all.
        return;
      }

      if (!mFrontBuffer->Lock(OPEN_READ)) {
        NS_WARNING("Failed to lock the tile's front buffer");
        return;
      }
      TextureClientAutoUnlock autoFront(mFrontBuffer);

      if (!mBackBuffer->Lock(OPEN_WRITE)) {
        NS_WARNING("Failed to lock the tile's back buffer");
        return;
      }
      TextureClientAutoUnlock autoBack(mBackBuffer);

      // Copy the bounding rect of regionToCopy. As tiles are quite small, it
      // is unlikely that we'd save much by copying each individual rect of the
      // region, but we can reevaluate this if it becomes an issue.
      const nsIntRect rectToCopy = regionToCopy.GetBounds();
      gfx::IntRect gfxRectToCopy(rectToCopy.x, rectToCopy.y, rectToCopy.width, rectToCopy.height);
      gfx::IntPoint gfxRectToCopyTopLeft = gfxRectToCopy.TopLeft();
      mFrontBuffer->CopyToTextureClient(mBackBuffer, &gfxRectToCopy, &gfxRectToCopyTopLeft);

      mInvalidBack.SetEmpty();
    }
  }
}

void
TileClient::DiscardFrontBuffer()
{
  if (mFrontBuffer) {
    MOZ_ASSERT(mFrontLock);
    mManager->GetTexturePool(mFrontBuffer->AsTextureClientDrawTarget()->GetFormat())->ReturnTextureClientDeferred(mFrontBuffer);
    mFrontLock->ReadUnlock();
    mFrontBuffer = nullptr;
    mFrontLock = nullptr;
  }
}

void
TileClient::DiscardBackBuffer()
{
  if (mBackBuffer) {
    MOZ_ASSERT(mBackLock);
    mManager->GetTexturePool(mBackBuffer->AsTextureClientDrawTarget()->GetFormat())->ReturnTextureClient(mBackBuffer);
    mBackLock->ReadUnlock();
    mBackBuffer = nullptr;
    mBackLock = nullptr;
  }
}

TextureClient*
TileClient::GetBackBuffer(const nsIntRegion& aDirtyRegion, TextureClientPool *aPool, bool *aCreatedTextureClient, bool aCanRerasterizeValidRegion)
{
  // Try to re-use the front-buffer if possible
  if (mFrontBuffer &&
      mFrontBuffer->HasInternalBuffer() &&
      mFrontLock->GetReadCount() == 1) {
    // If we had a backbuffer we no longer care about it since we'll
    // re-use the front buffer.
    DiscardBackBuffer();
    Flip();
    return mBackBuffer;
  }

  if (!mBackBuffer ||
      mBackLock->GetReadCount() > 1) {
    if (mBackBuffer) {
      // Our current back-buffer is still locked by the compositor. This can occur
      // when the client is producing faster than the compositor can consume. In
      // this case we just want to drop it and not return it to the pool.
      aPool->ReportClientLost();
    }
    mBackBuffer = aPool->GetTextureClient();
    // Create a lock for our newly created back-buffer.
    if (gfxPlatform::GetPlatform()->PreferMemoryOverShmem()) {
      // If our compositor is in the same process, we can save some cycles by not
      // using shared memory.
      mBackLock = new gfxMemorySharedReadLock();
    } else {
      mBackLock = new gfxShmSharedReadLock(mManager->AsShadowForwarder());
    }

    MOZ_ASSERT(mBackLock->IsValid());

    *aCreatedTextureClient = true;
    mInvalidBack = nsIntRect(0, 0, TILEDLAYERBUFFER_TILE_SIZE, TILEDLAYERBUFFER_TILE_SIZE);
  }

  ValidateBackBufferFromFront(aDirtyRegion, aCanRerasterizeValidRegion);

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
                                  TileLock(uintptr_t(mFrontLock.get())));
  } else {
    gfxShmSharedReadLock *lock = static_cast<gfxShmSharedReadLock*>(mFrontLock.get());
    return TexturedTileDescriptor(nullptr, mFrontBuffer->GetIPDLActor(),
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
ClientTiledLayerBuffer::DiscardBackBuffers()
{
  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    if (mRetainedTiles[i].IsPlaceholderTile()) continue;
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
                                   LayerManager::DrawThebesLayerCallback aCallback,
                                   void* aCallbackData)
{
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

  if (useSinglePaintBuffer) {
    nsRefPtr<gfxContext> ctxt;

    const nsIntRect bounds = aPaintRegion.GetBounds();
    {
      PROFILER_LABEL("ClientTiledLayerBuffer", "PaintThebesSingleBufferAlloc");
      gfxImageFormat format =
        gfxPlatform::GetPlatform()->OptimalFormatForContent(
          GetContentType());

      mSinglePaintDrawTarget =
        gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
          gfx::IntSize(ceilf(bounds.width * mResolution),
                       ceilf(bounds.height * mResolution)),
          gfx::ImageFormatToSurfaceFormat(format));

      ctxt = new gfxContext(mSinglePaintDrawTarget);

      mSinglePaintBufferOffset = nsIntPoint(bounds.x, bounds.y);
    }
    ctxt->NewPath();
    ctxt->Scale(mResolution, mResolution);
    ctxt->Translate(gfxPoint(-bounds.x, -bounds.y));
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
    if (PR_IntervalNow() - start > 3) {
      printf_stderr("Slow alloc %i\n", PR_IntervalNow() - start);
    }
    start = PR_IntervalNow();
#endif
    PROFILER_LABEL("ClientTiledLayerBuffer", "PaintThebesSingleBufferDraw");

    mCallback(mThebesLayer, ctxt, aPaintRegion, DrawRegionClip::CLIP_NONE, nsIntRegion(), mCallbackData);
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

  PROFILER_LABEL("ClientTiledLayerBuffer", "PaintThebesUpdate");
  Update(aNewValidRegion, aPaintRegion);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 10) {
    const nsIntRect bounds = aPaintRegion.GetBounds();
    printf_stderr("Time to tile %i: %i, %i, %i, %i\n", PR_IntervalNow() - start, bounds.x, bounds.y, bounds.width, bounds.height);
  }
#endif

  mLastPaintOpaque = mThebesLayer->CanUseOpaqueSurface();
  mCallback = nullptr;
  mCallbackData = nullptr;
  mSinglePaintDrawTarget = nullptr;
}

TileClient
ClientTiledLayerBuffer::ValidateTile(TileClient aTile,
                                    const nsIntPoint& aTileOrigin,
                                    const nsIntRegion& aDirtyRegion)
{
  PROFILER_LABEL("ClientTiledLayerBuffer", "ValidateTile");

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (aDirtyRegion.IsComplex()) {
    printf_stderr("Complex region\n");
  }
#endif

  if (aTile.IsPlaceholderTile()) {
    aTile.SetLayerManager(mManager);
  }

  // Discard our front and backbuffers if our contents changed. In this case
  // the calling code will already have taken care of invalidating the entire
  // layer.
  if (HasFormatChanged()) {
    aTile.DiscardBackBuffer();
    aTile.DiscardFrontBuffer();
  }

  bool createdTextureClient = false;
  nsIntRegion offsetDirtyRegion = aDirtyRegion.MovedBy(-aTileOrigin);
  bool usingSinglePaintBuffer = !!mSinglePaintDrawTarget;
  RefPtr<TextureClient> backBuffer =
    aTile.GetBackBuffer(offsetDirtyRegion,
                        mManager->GetTexturePool(gfxPlatform::GetPlatform()->Optimal2DFormatForContent(GetContentType())),
                        &createdTextureClient, !usingSinglePaintBuffer);

  if (!backBuffer->Lock(OPEN_READ_WRITE)) {
    NS_WARNING("Failed to lock tile TextureClient for updating.");
    aTile.DiscardFrontBuffer();
    return aTile;
  }

  // We must not keep a reference to the DrawTarget after it has been unlocked,
  // make sure these are null'd before unlocking as destruction of the context
  // may cause the target to be flushed.
  RefPtr<DrawTarget> drawTarget = backBuffer->AsTextureClientDrawTarget()->GetAsDrawTarget();
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

    // The new buffer is now validated, remove the dirty region from it.
    aTile.mInvalidBack.Sub(nsIntRect(0, 0, TILEDLAYERBUFFER_TILE_SIZE, TILEDLAYERBUFFER_TILE_SIZE),
                           offsetDirtyRegion);
  } else {
    // Area of the full tile...
    nsIntRegion tileRegion = nsIntRect(aTileOrigin.x, aTileOrigin.y, TILEDLAYERBUFFER_TILE_SIZE, TILEDLAYERBUFFER_TILE_SIZE);

    // Intersect this area with the portion that's dirty.
    tileRegion = tileRegion.Intersect(aDirtyRegion);

    // Move invalid areas into layer space.
    aTile.mInvalidFront.MoveBy(aTileOrigin);
    aTile.mInvalidBack.MoveBy(aTileOrigin);

    // Add the area that's going to be redrawn to the invalid area of the
    // front region.
    aTile.mInvalidFront.Or(aTile.mInvalidFront, tileRegion);

    // Add invalid areas of the backbuffer to the area to redraw.
    tileRegion.Or(tileRegion, aTile.mInvalidBack);

    // Move invalid areas back into tile space.
    aTile.mInvalidFront.MoveBy(-aTileOrigin);

    // This will be validated now.
    aTile.mInvalidBack.SetEmpty();

    nsIntRect bounds = tileRegion.GetBounds();
    bounds.ScaleRoundOut(mResolution, mResolution);
    bounds.MoveBy(-aTileOrigin);

    if (GetContentType() != gfxContentType::COLOR) {
      drawTarget->ClearRect(Rect(bounds.x, bounds.y, bounds.width, bounds.height));
    }

    ctxt->NewPath();
    ctxt->Clip(gfxRect(bounds.x, bounds.y, bounds.width, bounds.height));
    ctxt->Scale(mResolution, mResolution);
    ctxt->Translate(gfxPoint(-aTileOrigin.x, -aTileOrigin.y));
    mCallback(mThebesLayer, ctxt,
              tileRegion.GetBounds(),
              DrawRegionClip::CLIP_NONE,
              nsIntRegion(), mCallbackData);

  }

#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
  DrawDebugOverlay(drawTarget, aTileOrigin.x * mResolution,
                   aTileOrigin.y * mResolution, GetTileLength(), GetTileLength());
#endif

  ctxt = nullptr;
  drawTarget = nullptr;

  backBuffer->Unlock();

  aTile.Flip();

  if (createdTextureClient) {
    if (!mCompositableClient->AddTextureClient(backBuffer)) {
      NS_WARNING("Failed to add tile TextureClient.");
      aTile.DiscardFrontBuffer();
      aTile.DiscardBackBuffer();
      return aTile;
    }
  }

  // Note, we don't call UpdatedTexture. The Updated function is called manually
  // by the TiledContentHost before composition.

  if (backBuffer->HasInternalBuffer()) {
    // If our new buffer has an internal buffer, we don't want to keep another
    // TextureClient around unnecessarily, so discard the back-buffer.
    aTile.DiscardBackBuffer();
  }

  return aTile;
}

static LayoutDeviceRect
TransformCompositionBounds(const ParentLayerRect& aCompositionBounds,
                           const CSSToParentLayerScale& aZoom,
                           const ScreenPoint& aScrollOffset,
                           const CSSToScreenScale& aResolution,
                           const gfx3DMatrix& aTransformScreenToLayout)
{
  // Transform the current composition bounds into transformed layout device
  // space by compensating for the difference in resolution and subtracting the
  // old composition bounds origin.
  ScreenRect offsetViewportRect = (aCompositionBounds / aZoom) * aResolution;
  offsetViewportRect.MoveBy(-aScrollOffset);

  gfxRect transformedViewport =
    aTransformScreenToLayout.TransformBounds(
      gfxRect(offsetViewportRect.x, offsetViewportRect.y,
              offsetViewportRect.width, offsetViewportRect.height));

  return LayoutDeviceRect(transformedViewport.x,
                          transformedViewport.y,
                          transformedViewport.width,
                          transformedViewport.height);
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

  // Find out the current view transform to determine which tiles to draw
  // first, and see if we should just abort this paint. Aborting is usually
  // caused by there being an incoming, more relevant paint.
  ParentLayerRect compositionBounds;
  CSSToParentLayerScale zoom;
#if defined(MOZ_WIDGET_ANDROID)
  bool abortPaint = mManager->ProgressiveUpdateCallback(!staleRegion.Contains(aInvalidRegion),
                                                        compositionBounds, zoom,
                                                        !drawingLowPrecision);
#else
  MOZ_ASSERT(mSharedFrameMetricsHelper);

  ContainerLayer* parent = mThebesLayer->AsLayer()->GetParent();

  bool abortPaint =
    mSharedFrameMetricsHelper->UpdateFromCompositorFrameMetrics(
      parent,
      !staleRegion.Contains(aInvalidRegion),
      drawingLowPrecision,
      compositionBounds,
      zoom);
#endif

  if (abortPaint) {
    // We ignore if front-end wants to abort if this is the first,
    // non-low-precision paint, as in that situation, we're about to override
    // front-end's page/viewport metrics.
    if (!aPaintData->mFirstPaint || drawingLowPrecision) {
      PROFILER_LABEL("ContentClient", "Abort painting");
      aRegionToPaint.SetEmpty();
      return aIsRepeated;
    }
  }

  // Transform the screen coordinates into transformed layout device coordinates.
  LayoutDeviceRect transformedCompositionBounds =
    TransformCompositionBounds(compositionBounds, zoom, aPaintData->mScrollOffset,
                               aPaintData->mResolution, aPaintData->mTransformParentLayerToLayout);

  // Paint tiles that have stale content or that intersected with the screen
  // at the time of issuing the draw command in a single transaction first.
  // This is to avoid rendering glitches on animated page content, and when
  // layers change size/shape.
  LayoutDeviceRect coherentUpdateRect =
    transformedCompositionBounds.Intersect(aPaintData->mCompositionBounds);

  nsIntRect roundedCoherentUpdateRect =
    LayoutDeviceIntRect::ToUntyped(RoundedOut(coherentUpdateRect));

  aRegionToPaint.And(aInvalidRegion, roundedCoherentUpdateRect);
  aRegionToPaint.Or(aRegionToPaint, staleRegion);
  bool drawingStale = !aRegionToPaint.IsEmpty();
  if (!drawingStale) {
    aRegionToPaint = aInvalidRegion;
  }

  // Prioritise tiles that are currently visible on the screen.
  bool paintVisible = false;
  if (aRegionToPaint.Intersects(roundedCoherentUpdateRect)) {
    aRegionToPaint.And(aRegionToPaint, roundedCoherentUpdateRect);
    paintVisible = true;
  }

  // Paint area that's visible and overlaps previously valid content to avoid
  // visible glitches in animated elements, such as gifs.
  bool paintInSingleTransaction = paintVisible && (drawingStale || aPaintData->mFirstPaint);

  // The following code decides what order to draw tiles in, based on the
  // current scroll direction of the primary scrollable layer.
  NS_ASSERTION(!aRegionToPaint.IsEmpty(), "Unexpectedly empty paint region!");
  nsIntRect paintBounds = aRegionToPaint.GetBounds();

  int startX, incX, startY, incY;
  int tileLength = GetScaledTileLength();
  if (aPaintData->mScrollOffset.x >= aPaintData->mLastScrollOffset.x) {
    startX = RoundDownToTileEdge(paintBounds.x);
    incX = tileLength;
  } else {
    startX = RoundDownToTileEdge(paintBounds.XMost() - 1);
    incX = -tileLength;
  }

  if (aPaintData->mScrollOffset.y >= aPaintData->mLastScrollOffset.y) {
    startY = RoundDownToTileEdge(paintBounds.y);
    incY = tileLength;
  } else {
    startY = RoundDownToTileEdge(paintBounds.YMost() - 1);
    incY = -tileLength;
  }

  // Find a tile to draw.
  nsIntRect tileBounds(startX, startY, tileLength, tileLength);
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
    // subsequent transactions.
    if (!drawingLowPrecision && paintInSingleTransaction) {
      return true;
    }

    mManager->SetRepeatTransaction();
    return false;
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
                                         LayerManager::DrawThebesLayerCallback aCallback,
                                         void* aCallbackData)
{
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

  // Return false if nothing has been drawn, or give what has been drawn
  // to the shadow layer to upload.
  return isBufferChanged;
}

}
}
