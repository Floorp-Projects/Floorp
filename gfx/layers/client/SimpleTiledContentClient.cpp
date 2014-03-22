/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/SimpleTiledContentClient.h"

#include <math.h>                       // for ceil, ceilf, floor
#include "ClientTiledThebesLayer.h"     // for ClientTiledThebesLayer
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "Units.h"                      // for ScreenIntRect, CSSPoint, etc
#include "UnitTransforms.h"             // for TransformTo
#include "ClientLayerManager.h"         // for ClientLayerManager
#include "CompositorChild.h"            // for CompositorChild
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/Attributes.h"         // for MOZ_THIS_IN_INITIALIZER_LIST
#include "mozilla/MathAlgorithms.h"     // for Abs
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder
#include "SimpleTextureClientPool.h"
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for gfxContext::AddRef, etc
#include "nsSize.h"                     // for nsIntSize
#include "gfxReusableSharedImageSurfaceWrapper.h"
#include "nsMathUtils.h"               // for NS_roundf
#include "gfx2DGlue.h"

#define ALOG(...)  __android_log_print(ANDROID_LOG_INFO, "SimpleTiles", __VA_ARGS__)

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

void
SimpleTiledLayerBuffer::PaintThebes(const nsIntRegion& aNewValidRegion,
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

  PROFILER_LABEL("SimpleTiledLayerBuffer", "PaintThebesUpdate");

  Update(aNewValidRegion, aPaintRegion);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 10) {
    const nsIntRect bounds = aPaintRegion.GetBounds();
    printf_stderr("Time to tile [%i, %i, %i, %i] -> %i\n", bounds.x, bounds.y, bounds.width, bounds.height, PR_IntervalNow() - start);
  }
#endif

  mLastPaintOpaque = mThebesLayer->CanUseOpaqueSurface();
  mCallback = nullptr;
  mCallbackData = nullptr;
}

SimpleTiledLayerTile
SimpleTiledLayerBuffer::ValidateTile(SimpleTiledLayerTile aTile,
                                     const nsIntPoint& aTileOrigin,
                                     const nsIntRegion& aDirtyRegion)
{
  PROFILER_LABEL("SimpleTiledLayerBuffer", "ValidateTile");
  static gfx::IntSize kTileSize(TILEDLAYERBUFFER_TILE_SIZE, TILEDLAYERBUFFER_TILE_SIZE);

  gfx::SurfaceFormat tileFormat = gfxPlatform::GetPlatform()->Optimal2DFormatForContent(GetContentType());

  // if this is true, we're using a separate buffer to do our drawing first
  bool doBufferedDrawing = true;
  bool fullPaint = false;

  RefPtr<TextureClient> textureClient = mManager->GetSimpleTileTexturePool(tileFormat)->GetTextureClientWithAutoRecycle();

  if (!textureClient) {
    NS_WARNING("TextureClient allocation failed");
    return SimpleTiledLayerTile();
  }

  if (!textureClient->Lock(OPEN_WRITE)) {
    NS_WARNING("TextureClient lock failed");
    return SimpleTiledLayerTile();
  }

  TextureClientSurface *textureClientSurf = textureClient->AsTextureClientSurface();
  if (!textureClientSurf) {
    doBufferedDrawing = false;
  }

  RefPtr<DrawTarget> drawTarget;

  nsRefPtr<gfxImageSurface> clientAsImageSurface;
  unsigned char *bufferData = nullptr;

  // these are set/updated differently based on doBufferedDrawing
  nsIntRect drawBounds;
  nsIntRegion drawRegion;
  nsIntRegion invalidateRegion;

  if (doBufferedDrawing) {
    // try to obtain the TextureClient as an ImageSurface, so that we can
    // access the pixels directly
    nsRefPtr<gfxASurface> asurf = textureClientSurf->GetAsSurface();
    clientAsImageSurface = asurf ? asurf->GetAsImageSurface() : nullptr;
    if (clientAsImageSurface) {
      int32_t bufferStride = clientAsImageSurface->Stride();

      if (!aTile.mCachedBuffer) {
        aTile.mCachedBuffer = SharedBuffer::Create(clientAsImageSurface->GetDataSize());
        fullPaint = true;
      }
      bufferData = (unsigned char*) aTile.mCachedBuffer->Data();

      drawTarget = gfxPlatform::GetPlatform()->CreateDrawTargetForData(bufferData,
                                                                       kTileSize,
                                                                       bufferStride,
                                                                       tileFormat);

      if (fullPaint) {
        drawBounds = nsIntRect(aTileOrigin.x, aTileOrigin.y, GetScaledTileLength(), GetScaledTileLength());
        drawRegion = nsIntRegion(drawBounds);
      } else {
        drawBounds = aDirtyRegion.GetBounds();
        drawRegion = nsIntRegion(drawBounds);
        if (GetContentType() == gfxContentType::COLOR_ALPHA)
          drawTarget->ClearRect(Rect(drawBounds.x - aTileOrigin.x, drawBounds.y - aTileOrigin.y,
                                     drawBounds.width, drawBounds.height));
      }
    } else {
      // failed to obtain the client as an ImageSurface
      doBufferedDrawing = false;
    }
  }

  // this might get set above if we couldn't extract out a buffer
  if (!doBufferedDrawing) {
    drawTarget = textureClient->AsTextureClientDrawTarget()->GetAsDrawTarget();

    fullPaint = true;
    drawBounds = nsIntRect(aTileOrigin.x, aTileOrigin.y, GetScaledTileLength(), GetScaledTileLength());
    drawRegion = nsIntRegion(drawBounds);

    if (GetContentType() == gfxContentType::COLOR_ALPHA)
      drawTarget->ClearRect(Rect(0, 0, drawBounds.width, drawBounds.height));
  }

  // do the drawing
  RefPtr<gfxContext> ctxt = new gfxContext(drawTarget);

  ctxt->Scale(mResolution, mResolution);
  ctxt->Translate(gfxPoint(-aTileOrigin.x, -aTileOrigin.y));

  mCallback(mThebesLayer, ctxt,
            drawRegion,
            fullPaint ? DrawRegionClip::CLIP_NONE : DrawRegionClip::DRAW_SNAPPED, // XXX DRAW or DRAW_SNAPPED?
            invalidateRegion,
            mCallbackData);

  ctxt = nullptr;
  drawTarget = nullptr;

  if (doBufferedDrawing) {
    memcpy(clientAsImageSurface->Data(), bufferData, clientAsImageSurface->GetDataSize());
    clientAsImageSurface = nullptr;
    bufferData = nullptr;
  }

  textureClient->Unlock();

  if (!mCompositableClient->AddTextureClient(textureClient)) {
    NS_WARNING("Failed to add tile TextureClient [simple]");
    return SimpleTiledLayerTile();
  }

  // aTile.mCachedBuffer was set earlier
  aTile.mTileBuffer = textureClient;
  aTile.mManager = mManager;
  aTile.mLastUpdate = TimeStamp::Now();

  return aTile;
}

SurfaceDescriptorTiles
SimpleTiledLayerBuffer::GetSurfaceDescriptorTiles()
{
  InfallibleTArray<TileDescriptor> tiles;

  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    tiles.AppendElement(mRetainedTiles[i].GetTileDescriptor());
  }

  return SurfaceDescriptorTiles(mValidRegion, mPaintedRegion,
                                tiles, mRetainedWidth, mRetainedHeight,
                                mResolution, mFrameResolution.scale);
}

bool
SimpleTiledLayerBuffer::HasFormatChanged() const
{
  return mThebesLayer->CanUseOpaqueSurface() != mLastPaintOpaque;
}

gfxContentType
SimpleTiledLayerBuffer::GetContentType() const
{
  if (mThebesLayer->CanUseOpaqueSurface())
    return gfxContentType::COLOR;

  return gfxContentType::COLOR_ALPHA;
}

SimpleTiledContentClient::SimpleTiledContentClient(SimpleClientTiledThebesLayer* aThebesLayer,
                                                   ClientLayerManager* aManager)
  : CompositableClient(aManager->AsShadowForwarder())
  , mTiledBuffer(aThebesLayer, MOZ_THIS_IN_INITIALIZER_LIST(), aManager)
{
  MOZ_COUNT_CTOR(SimpleTiledContentClient);
}

SimpleTiledContentClient::~SimpleTiledContentClient()
{
  MOZ_COUNT_DTOR(SimpleTiledContentClient);
  mTiledBuffer.Release();
}

void
SimpleTiledContentClient::UseTiledLayerBuffer()
{
  mForwarder->UseTiledLayerBuffer(this, mTiledBuffer.GetSurfaceDescriptorTiles());
  mTiledBuffer.ClearPaintedRegion();
}

SimpleClientTiledThebesLayer::SimpleClientTiledThebesLayer(ClientLayerManager* aManager)
  : ThebesLayer(aManager,
                static_cast<ClientLayer*>(MOZ_THIS_IN_INITIALIZER_LIST()))
  , mContentClient()
{
  MOZ_COUNT_CTOR(SimpleClientTiledThebesLayer);
}

SimpleClientTiledThebesLayer::~SimpleClientTiledThebesLayer()
{
  MOZ_COUNT_DTOR(SimpleClientTiledThebesLayer);
}

void
SimpleClientTiledThebesLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
{
  aAttrs = ThebesLayerAttributes(GetValidRegion());
}

void
SimpleClientTiledThebesLayer::RenderLayer()
{
  LayerManager::DrawThebesLayerCallback callback =
    ClientManager()->GetThebesLayerCallback();
  void *data = ClientManager()->GetThebesLayerCallbackData();
  if (!callback) {
    ClientManager()->SetTransactionIncomplete();
    return;
  }

  // First time? Create a content client.
  if (!mContentClient) {
    mContentClient = new SimpleTiledContentClient(this, ClientManager());

    mContentClient->Connect();
    ClientManager()->AsShadowForwarder()->Attach(mContentClient, this);
    MOZ_ASSERT(mContentClient->GetForwarder());
  }

  // If the format changed, nothing is valid
  if (mContentClient->mTiledBuffer.HasFormatChanged()) {
    mValidRegion = nsIntRegion();
  }

  nsIntRegion invalidRegion = mVisibleRegion;
  invalidRegion.Sub(invalidRegion, mValidRegion);
  if (invalidRegion.IsEmpty()) {
    return;
  }

  // Only paint the mask layer on the first transaction.
  if (GetMaskLayer() && !ClientManager()->IsRepeatTransaction()) {
    ToClientLayer(GetMaskLayer())->RenderLayer();
  }

  // SimpleTiledContentClient doesn't support progressive updates or the low
  // precision buffer yet.
  MOZ_ASSERT(!gfxPrefs::UseProgressiveTilePainting() &&
             !gfxPrefs::UseLowPrecisionBuffer());

  mValidRegion = mVisibleRegion;

  NS_ASSERTION(!ClientManager()->IsRepeatTransaction(), "Didn't paint our mask layer");

  mContentClient->mTiledBuffer.PaintThebes(mValidRegion, invalidRegion,
                                           callback, data);

  ClientManager()->Hold(this);

  mContentClient->UseTiledLayerBuffer();
}


}
}
