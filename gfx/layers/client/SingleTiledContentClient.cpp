/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/SingleTiledContentClient.h"

#include "ClientTiledPaintedLayer.h"

namespace mozilla {
namespace layers {


SingleTiledContentClient::SingleTiledContentClient(ClientTiledPaintedLayer* aPaintedLayer,
                                                   ClientLayerManager* aManager)
  : TiledContentClient(aManager, "Single")
{
  MOZ_COUNT_CTOR(SingleTiledContentClient);

  mTiledBuffer = new ClientSingleTiledLayerBuffer(aPaintedLayer, this, aManager);
}

void
SingleTiledContentClient::ClearCachedResources()
{
  CompositableClient::ClearCachedResources();
  mTiledBuffer->DiscardBuffers();
}

void
SingleTiledContentClient::UpdatedBuffer(TiledBufferType aType)
{
  mForwarder->UseTiledLayerBuffer(this, mTiledBuffer->GetSurfaceDescriptorTiles());
  mTiledBuffer->ClearPaintedRegion();
}

/* static */ bool
SingleTiledContentClient::ClientSupportsLayerSize(const gfx::IntSize& aSize, ClientLayerManager* aManager)
{
  int32_t maxTextureSize = aManager->GetMaxTextureSize();
  return aSize.width <= maxTextureSize && aSize.height <= maxTextureSize;
}

ClientSingleTiledLayerBuffer::ClientSingleTiledLayerBuffer(ClientTiledPaintedLayer* aPaintedLayer,
                                                           CompositableClient* aCompositableClient,
                                                           ClientLayerManager* aManager)
  : ClientTiledLayerBuffer(aPaintedLayer, aCompositableClient)
  , mManager(aManager)
  , mWasLastPaintProgressive(false)
  , mFormat(gfx::SurfaceFormat::UNKNOWN)
{
}

void
ClientSingleTiledLayerBuffer::ReleaseTiles()
{
  if (!mTile.IsPlaceholderTile()) {
    mTile.DiscardBuffers();
  }
  mTile.SetTextureAllocator(nullptr);
}

void
ClientSingleTiledLayerBuffer::DiscardBuffers()
{
  if (!mTile.IsPlaceholderTile()) {
    mTile.DiscardFrontBuffer();
    mTile.DiscardBackBuffer();
  }
}

SurfaceDescriptorTiles
ClientSingleTiledLayerBuffer::GetSurfaceDescriptorTiles()
{
  InfallibleTArray<TileDescriptor> tiles;

  TileDescriptor tileDesc = mTile.GetTileDescriptor();
  tiles.AppendElement(tileDesc);
  mTile.mUpdateRect = gfx::IntRect();

  return SurfaceDescriptorTiles(mValidRegion,
                                tiles,
                                mTilingOrigin,
                                mSize,
                                0, 0, 1, 1,
                                1.0,
                                mFrameResolution.xScale,
                                mFrameResolution.yScale,
                                mWasLastPaintProgressive);
}

already_AddRefed<TextureClient>
ClientSingleTiledLayerBuffer::GetTextureClient()
{
  MOZ_ASSERT(mFormat != gfx::SurfaceFormat::UNKNOWN);
  return mCompositableClient->CreateTextureClientForDrawing(
    gfx::ImageFormatToSurfaceFormat(mFormat), mSize, BackendSelector::Content,
    TextureFlags::DISALLOW_BIGIMAGE | TextureFlags::IMMEDIATE_UPLOAD);
}

void
ClientSingleTiledLayerBuffer::PaintThebes(const nsIntRegion& aNewValidRegion,
                                          const nsIntRegion& aPaintRegion,
                                          const nsIntRegion& aDirtyRegion,
                                          LayerManager::DrawPaintedLayerCallback aCallback,
                                          void* aCallbackData,
                                          bool aIsProgressive)
{
  mWasLastPaintProgressive = aIsProgressive;

  // Compare layer valid region size to current backbuffer size, discard if not matching.
  gfx::IntSize size = aNewValidRegion.GetBounds().Size();
  gfx::IntPoint origin = aNewValidRegion.GetBounds().TopLeft();
  nsIntRegion paintRegion = aPaintRegion;
  if (mSize != size ||
      mTilingOrigin != origin) {
    ResetPaintedAndValidState();
    mSize = size;
    mTilingOrigin = origin;
    paintRegion = aNewValidRegion;
  }

  SurfaceMode mode;
  gfxContentType content = GetContentType(&mode);
  mFormat = gfxPlatform::GetPlatform()->OptimalFormatForContent(content);

  if (mTile.IsPlaceholderTile()) {
    mTile.SetLayerManager(mManager);
    mTile.SetTextureAllocator(this);
  }
  mTile.SetCompositableClient(mCompositableClient);

  // The dirty region relative to the top-left of the tile.
  nsIntRegion tileDirtyRegion = paintRegion.MovedBy(-mTilingOrigin);

  nsIntRegion extraPainted;
  RefPtr<TextureClient> backBufferOnWhite;
  RefPtr<TextureClient> backBuffer =
    mTile.GetBackBuffer(tileDirtyRegion,
                        content, mode,
                        extraPainted,
                        &backBufferOnWhite);

  mTile.mUpdateRect = tileDirtyRegion.GetBounds().Union(extraPainted.GetBounds());

  extraPainted.MoveBy(mTilingOrigin);
  extraPainted.And(extraPainted, aNewValidRegion);
  mPaintedRegion.OrWith(paintRegion);
  mPaintedRegion.OrWith(extraPainted);

  if (!backBuffer) {
    return;
  }

  RefPtr<gfx::DrawTarget> dt = backBuffer->BorrowDrawTarget();
  RefPtr<gfx::DrawTarget> dtOnWhite;
  if (backBufferOnWhite) {
    dtOnWhite = backBufferOnWhite->BorrowDrawTarget();
  }

  if (mode != SurfaceMode::SURFACE_OPAQUE) {
    for (auto iter = tileDirtyRegion.RectIter(); !iter.Done(); iter.Next()) {
      const gfx::IntRect& rect = iter.Get();
      if (dtOnWhite) {
        dt->FillRect(gfx::Rect(rect.x, rect.y, rect.width, rect.height),
                     gfx::ColorPattern(gfx::Color(0.0, 0.0, 0.0, 1.0)));
        dtOnWhite->FillRect(gfx::Rect(rect.x, rect.y, rect.width, rect.height),
                            gfx::ColorPattern(gfx::Color(1.0, 1.0, 1.0, 1.0)));
      } else {
        dt->ClearRect(gfx::Rect(rect.x, rect.y, rect.width, rect.height));
      }
    }
  }

  if (dtOnWhite) {
    dt = gfx::Factory::CreateDualDrawTarget(dt, dtOnWhite);
    dtOnWhite = nullptr;
  }

  {
    RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(dt);
    if (!ctx) {
      gfxDevCrash(gfx::LogReason::InvalidContext) << "SingleTiledContextClient context problem " << gfx::hexa(dt);
      return;
    }
    ctx->SetMatrix(ctx->CurrentMatrix().Translate(-mTilingOrigin.x, -mTilingOrigin.y));

    aCallback(mPaintedLayer, ctx, paintRegion, paintRegion, DrawRegionClip::DRAW, nsIntRegion(), aCallbackData);
  }

  // Mark the area we just drew into the back buffer as invalid in the front buffer as they're
  // now out of sync.
  mTile.mInvalidFront.OrWith(tileDirtyRegion);

  // The new buffer is now validated, remove the dirty region from it.
  mTile.mInvalidBack.SubOut(tileDirtyRegion);

  dt = nullptr;

  mTile.Flip();
  UnlockTile(mTile);

  if (backBuffer->HasIntermediateBuffer()) {
    // If our new buffer has an internal buffer, we don't want to keep another
    // TextureClient around unnecessarily, so discard the back-buffer.
    mTile.DiscardBackBuffer();
  }

  mValidRegion = aNewValidRegion;
  mLastPaintSurfaceMode = mode;
  mLastPaintContentType = content;
}

} // namespace layers
} // namespace mozilla
