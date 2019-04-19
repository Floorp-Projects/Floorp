/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaintedLayerMLGPU.h"
#include "LayerManagerMLGPU.h"
#include "mozilla/layers/LayersHelpers.h"
#include "mozilla/layers/TiledContentHost.h"
#include "UnitTransforms.h"

namespace mozilla {

using namespace gfx;

namespace layers {

PaintedLayerMLGPU::PaintedLayerMLGPU(LayerManagerMLGPU* aManager)
    : PaintedLayer(aManager, static_cast<HostLayer*>(this)),
      LayerMLGPU(aManager) {
  MOZ_COUNT_CTOR(PaintedLayerMLGPU);
}

PaintedLayerMLGPU::~PaintedLayerMLGPU() {
  MOZ_COUNT_DTOR(PaintedLayerMLGPU);

  CleanupResources();
}

bool PaintedLayerMLGPU::OnPrepareToRender(FrameBuilder* aBuilder) {
  // Reset our cached texture pointers. The next call to AssignToView will
  // populate them again.
  mTexture = nullptr;
  mTextureOnWhite = nullptr;
  return !!mHost;
}

void PaintedLayerMLGPU::SetRenderRegion(LayerIntRegion&& aRegion) {
  mRenderRegion = std::move(aRegion);

  LayerIntRect bounds(mRenderRegion.GetBounds().TopLeft(),
                      ViewAs<LayerPixel>(mTexture->GetSize()));
  mRenderRegion.AndWith(bounds);
}

const LayerIntRegion& PaintedLayerMLGPU::GetDrawRects() {
#ifndef MOZ_IGNORE_PAINT_WILL_RESAMPLE
  // Note: we don't set PaintWillResample on our ContentTextureHost. The old
  // compositor must do this since ContentHost is responsible for issuing
  // draw calls, but in AL we can handle it directly here.
  //
  // Note that when AL performs CPU-based occlusion culling (the default
  // behavior), we might break up the visible region again. If that turns
  // out to be a problem, we can factor this into ForEachDrawRect instead.
  if (MayResample()) {
    mDrawRects = mRenderRegion.GetBounds();
    return mDrawRects;
  }
#endif
  return mRenderRegion;
}

bool PaintedLayerMLGPU::SetCompositableHost(CompositableHost* aHost) {
  switch (aHost->GetType()) {
    case CompositableType::CONTENT_TILED:
    case CompositableType::CONTENT_SINGLE:
    case CompositableType::CONTENT_DOUBLE: {
      if (mHost && mHost != aHost->AsContentHost()) {
        mHost->Detach(this);
      }
      mHost = aHost->AsContentHost();
      if (!mHost) {
        gfxWarning() << "ContentHostBase is not a ContentHostTexture";
      }
      return true;
    }
    default:
      return false;
  }
}

CompositableHost* PaintedLayerMLGPU::GetCompositableHost() { return mHost; }

gfx::Point PaintedLayerMLGPU::GetDestOrigin() const { return mDestOrigin; }

void PaintedLayerMLGPU::AssignToView(FrameBuilder* aBuilder,
                                     RenderViewMLGPU* aView,
                                     Maybe<Polygon>&& aGeometry) {
  if (TiledContentHost* tiles = mHost->AsTiledContentHost()) {
    // Note: we do not support the low-res buffer yet.
    MOZ_ASSERT(tiles->GetLowResBuffer().GetTileCount() == 0);
    AssignHighResTilesToView(aBuilder, aView, tiles, aGeometry);
    return;
  }

  // If we don't have a texture yet, acquire one from the ContentHost now.
  if (!mTexture) {
    ContentHostTexture* single = mHost->AsContentHostTexture();
    if (!single) {
      return;
    }

    mTexture = single->AcquireTextureSource();
    if (!mTexture) {
      return;
    }
    mTextureOnWhite = single->AcquireTextureSourceOnWhite();
    mDestOrigin = single->GetOriginOffset();
  }

  // Fall through to the single texture case.
  LayerMLGPU::AssignToView(aBuilder, aView, std::move(aGeometry));
}

void PaintedLayerMLGPU::AssignHighResTilesToView(
    FrameBuilder* aBuilder, RenderViewMLGPU* aView, TiledContentHost* aTileHost,
    const Maybe<Polygon>& aGeometry) {
  TiledLayerBufferComposite& tiles = aTileHost->GetHighResBuffer();

  LayerIntRegion compositeRegion = ViewAs<LayerPixel>(tiles.GetValidRegion());
  compositeRegion.AndWith(GetShadowVisibleRegion());
  if (compositeRegion.IsEmpty()) {
    return;
  }

  AssignTileBufferToView(aBuilder, aView, tiles, compositeRegion, aGeometry);
}

void PaintedLayerMLGPU::AssignTileBufferToView(
    FrameBuilder* aBuilder, RenderViewMLGPU* aView,
    TiledLayerBufferComposite& aTiles, const LayerIntRegion& aCompositeRegion,
    const Maybe<Polygon>& aGeometry) {
  float resolution = aTiles.GetResolution();

  // Save these so they can be restored at the end.
  float baseOpacity = mComputedOpacity;
  LayerIntRegion visible = GetShadowVisibleRegion();

  for (size_t i = 0; i < aTiles.GetTileCount(); i++) {
    TileHost& tile = aTiles.GetTile(i);
    if (tile.IsPlaceholderTile()) {
      continue;
    }

    TileCoordIntPoint coord = aTiles.GetPlacement().TileCoord(i);
    // A sanity check that catches a lot of mistakes.
    MOZ_ASSERT(coord.x == tile.mTileCoord.x && coord.y == tile.mTileCoord.y);

    IntPoint offset = aTiles.GetTileOffset(coord);

    // Use LayerIntRect here so we don't have to keep re-allocating the region
    // to change the unit type.
    LayerIntRect tileRect(ViewAs<LayerPixel>(offset),
                          ViewAs<LayerPixel>(aTiles.GetScaledTileSize()));
    LayerIntRegion tileDrawRegion = tileRect;
    tileDrawRegion.AndWith(aCompositeRegion);
    if (tileDrawRegion.IsEmpty()) {
      continue;
    }
    tileDrawRegion.ScaleRoundOut(resolution, resolution);

    // Update layer state for this tile - that includes the texture, visible
    // region, and opacity.
    mTexture = tile.AcquireTextureSource();
    if (!mTexture) {
      continue;
    }

    mTextureOnWhite = tile.AcquireTextureSourceOnWhite();

    SetShadowVisibleRegion(tileDrawRegion);
    mComputedOpacity = tile.GetFadeInOpacity(baseOpacity);
    mDestOrigin = offset;

    // Yes, it's a bit weird that we're assigning the same layer to the same
    // view multiple times. Note that each time, the texture, computed
    // opacity, origin, and visible region are updated to match the current
    // tile, and we restore these properties after we've finished processing
    // all tiles.
    Maybe<Polygon> geometry = aGeometry;
    LayerMLGPU::AssignToView(aBuilder, aView, std::move(geometry));
  }

  // Restore the computed opacity and visible region.
  mComputedOpacity = baseOpacity;
  SetShadowVisibleRegion(std::move(visible));
}

void PaintedLayerMLGPU::CleanupResources() {
  if (mHost) {
    mHost->Detach(this);
  }
  mTexture = nullptr;
  mTextureOnWhite = nullptr;
  mHost = nullptr;
}

void PaintedLayerMLGPU::PrintInfo(std::stringstream& aStream,
                                  const char* aPrefix) {
  PaintedLayer::PrintInfo(aStream, aPrefix);
  if (mHost && mHost->IsAttached()) {
    aStream << "\n";
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mHost->PrintInfo(aStream, pfx.get());
  }
}

void PaintedLayerMLGPU::Disconnect() { CleanupResources(); }

bool PaintedLayerMLGPU::IsContentOpaque() {
  return !!(GetContentFlags() & CONTENT_OPAQUE);
}

void PaintedLayerMLGPU::CleanupCachedResources() { CleanupResources(); }

}  // namespace layers
}  // namespace mozilla
