/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_REUSABLETILESTOREOGL_H
#define GFX_REUSABLETILESTOREOGL_H

#include "TiledThebesLayerOGL.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"

namespace mozilla {

namespace gl {
class GLContext;
}

namespace layers {

// A storage class for the information required to render a single tile from
// a TiledLayerBufferOGL.
class ReusableTiledTextureOGL
{
public:
  ReusableTiledTextureOGL(TiledTexture aTexture,
                          const nsIntPoint& aTileOrigin,
                          const nsIntRegion& aTileRegion,
                          uint16_t aTileSize,
                          gfxSize aResolution)
    : mTexture(aTexture)
    , mTileOrigin(aTileOrigin)
    , mTileRegion(aTileRegion)
    , mTileSize(aTileSize)
    , mResolution(aResolution)
  {}

  ~ReusableTiledTextureOGL() {}

  TiledTexture mTexture;
  const nsIntPoint mTileOrigin;
  const nsIntRegion mTileRegion;
  uint16_t mTileSize;
  gfxSize mResolution;
};

// This class will operate on a TiledLayerBufferOGL to harvest tiles that have
// rendered content that is about to become invalid. We do this so that in the
// situation that we need to render an area of a TiledThebesLayerOGL that hasn't
// been updated quickly enough, we can still display something (and hopefully
// it'll be the same as the valid rendered content). While this may end up
// showing invalid data, it should only be momentarily.
class ReusableTileStoreOGL
{
public:
  ReusableTileStoreOGL(gl::GLContext* aContext, float aSizeLimit)
    : mContext(aContext)
    , mSizeLimit(aSizeLimit)
  {}

  ~ReusableTileStoreOGL();

  // Harvests tiles from a TiledLayerBufferOGL that are about to become
  // invalid. aOldValidRegion and aOldResolution should be the valid region
  // and resolution of the data currently in aVideoMemoryTiledBuffer, and
  // aNewValidRegion and aNewResolution should be the valid region and
  // resolution of the data that is about to update aVideoMemoryTiledBuffer.
  void HarvestTiles(TiledThebesLayerOGL* aLayer,
                    TiledLayerBufferOGL* aVideoMemoryTiledBuffer,
                    const nsIntRegion& aOldValidRegion,
                    const nsIntRegion& aNewValidRegion,
                    const gfxSize& aOldResolution,
                    const gfxSize& aNewResolution);

  // Draws all harvested tiles that don't intersect with the given valid region.
  // Differences in resolution will be reconciled via altering the given
  // transformation.
  void DrawTiles(TiledThebesLayerOGL* aLayer,
                 const nsIntRegion& aValidRegion,
                 const gfxSize& aResolution,
                 const gfx3DMatrix& aTransform,
                 const nsIntPoint& aRenderOffset,
                 Layer* aMaskLayer);

protected:
  // Invalidates tiles contained within the valid region, or intersecting with
  // the currently rendered region (discovered by looking for a display-port,
  // or failing that, looking at the widget size).
  void InvalidateTiles(TiledThebesLayerOGL* aLayer,
                       const nsIntRegion& aValidRegion,
                       const gfxSize& aResolution);

private:
  // This GLContext should correspond to the one used in any TiledLayerBufferOGL
  // that is passed into HarvestTiles and DrawTiles.
  nsRefPtr<gl::GLContext> mContext;

  // This determines the maximum number of tiles stored in this tile store,
  // as a fraction of the amount of tiles stored in the TiledLayerBufferOGL
  // given to HarvestTiles.
  float mSizeLimit;

  // This stores harvested tiles, in the order in which they were harvested.
  nsTArray< nsAutoPtr<ReusableTiledTextureOGL> > mTiles;
};

} // layers
} // mozilla

#endif // GFX_REUSABLETILESTOREOGL_H
