/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/SingleTiledContentClient.h"

namespace mozilla {

using namespace gfx;

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
  // Take a ReadLock on behalf of the TiledContentHost. This
  // reference will be adopted when the descriptor is opened in
  // TiledLayerBufferComposite.
  mTiledBuffer->ReadLock();

  mForwarder->UseTiledLayerBuffer(this, mTiledBuffer->GetSurfaceDescriptorTiles());
  mTiledBuffer->ClearPaintedRegion();
}

ClientSingleTiledLayerBuffer::ClientSingleTiledLayerBuffer(ClientTiledPaintedLayer* aPaintedLayer,
                                                           CompositableClient* aCompositableClient,
                                                           ClientLayerManager* aManager)
  : ClientTiledLayerBuffer(aPaintedLayer, aCompositableClient)
  , mManager(aManager)
{
}

void
ClientSingleTiledLayerBuffer::ReadLock() {
  if (!mTile.IsPlaceholderTile()) {
    mTile.ReadLock();
  }
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
  mTile.mUpdateRect = IntRect();

  return SurfaceDescriptorTiles(mValidRegion,
                                tiles,
                                mTilingOrigin,
                                mSize,
                                0, 0, 1, 1,
                                1.0,
                                mFrameResolution.xScale,
                                mFrameResolution.yScale);
}

already_AddRefed<TextureClient>
ClientSingleTiledLayerBuffer::GetTextureClient()
{
  return mCompositableClient->CreateTextureClientForDrawing(
    gfx::ImageFormatToSurfaceFormat(mFormat), mSize, BackendSelector::Content,
    TextureFlags::IMMEDIATE_UPLOAD);
}

void
ClientSingleTiledLayerBuffer::PaintThebes(const nsIntRegion& aNewValidRegion,
                                          const nsIntRegion& aPaintRegion,
                                          const nsIntRegion& aDirtyRegion,
                                          LayerManager::DrawPaintedLayerCallback aCallback,
                                          void* aCallbackData)
{
  // Compare layer visible region size to current backbuffer size, discard if not matching.
  IntSize size = mPaintedLayer->GetVisibleRegion().GetBounds().Size();
  IntPoint origin = mPaintedLayer->GetVisibleRegion().GetBounds().TopLeft();
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

  RefPtr<DrawTarget> dt = backBuffer->BorrowDrawTarget();
  RefPtr<DrawTarget> dtOnWhite;
  if (backBufferOnWhite) {
    dtOnWhite = backBufferOnWhite->BorrowDrawTarget();
  }

  if (mode != SurfaceMode::SURFACE_OPAQUE) {
    nsIntRegionRectIterator iter(tileDirtyRegion);
    const IntRect *iterRect;
    while ((iterRect = iter.Next())) {
      if (dtOnWhite) {
        dt->FillRect(Rect(iterRect->x, iterRect->y, iterRect->width, iterRect->height),
                     ColorPattern(Color(0.0, 0.0, 0.0, 1.0)));
        dtOnWhite->FillRect(Rect(iterRect->x, iterRect->y, iterRect->width, iterRect->height),
                            ColorPattern(Color(1.0, 1.0, 1.0, 1.0)));
      } else {
        dt->ClearRect(Rect(iterRect->x, iterRect->y, iterRect->width, iterRect->height));
      }
    }
  }

  if (dtOnWhite) {
    dt = Factory::CreateDualDrawTarget(dt, dtOnWhite);
    dtOnWhite = nullptr;
  }

  {
    nsRefPtr<gfxContext> ctx = new gfxContext(dt);
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

  if (backBuffer->HasInternalBuffer()) {
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
