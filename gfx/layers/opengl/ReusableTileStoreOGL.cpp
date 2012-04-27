/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ReusableTileStoreOGL.h"

namespace mozilla {
namespace layers {

ReusableTileStoreOGL::~ReusableTileStoreOGL()
{
  if (mTiles.Length() == 0)
    return;

  mContext->MakeCurrent();
  for (int i = 0; i < mTiles.Length(); i++)
    mContext->fDeleteTextures(1, &mTiles[i]->mTexture.mTextureHandle);
  mTiles.Clear();
}

void
ReusableTileStoreOGL::HarvestTiles(TiledLayerBufferOGL* aVideoMemoryTiledBuffer,
                                   const nsIntSize& aContentSize,
                                   const nsIntRegion& aOldValidRegion,
                                   const nsIntRegion& aNewValidRegion,
                                   const gfxSize& aOldResolution,
                                   const gfxSize& aNewResolution)
{
  gfxSize scaleFactor = gfxSize(aNewResolution.width / aOldResolution.width,
                                aNewResolution.height / aOldResolution.height);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  printf_stderr("Seeing if there are any tiles we can reuse\n");
#endif

  // Iterate over existing harvested tiles and release any that are contained
  // within the new valid region, or that fall outside of the layer.
  mContext->MakeCurrent();
  for (int i = 0; i < mTiles.Length();) {
    ReusableTiledTextureOGL* tile = mTiles[i];

    nsIntRect tileRect;
    bool release = false;
    if (tile->mResolution == aNewResolution) {
      if (aNewValidRegion.Contains(tile->mTileRegion)) {
        release = true;
      } else {
        tileRect = tile->mTileRegion.GetBounds();
      }
    } else {
      nsIntRegion transformedTileRegion(tile->mTileRegion);
      transformedTileRegion.ScaleRoundOut(tile->mResolution.width / aNewResolution.width,
                                          tile->mResolution.height / aNewResolution.height);
      if (aNewValidRegion.Contains(transformedTileRegion))
        release = true;
      else
        tileRect = transformedTileRegion.GetBounds();
    }

    if (!release) {
      if (tileRect.width > aContentSize.width ||
          tileRect.height > aContentSize.height)
        release = true;
    }

    if (release) {
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
      nsIntRect tileBounds = tile->mTileRegion.GetBounds();
      printf_stderr("Releasing obsolete reused tile at %d,%d, x%f\n",
                    tileBounds.x, tileBounds.y, tile->mResolution.width);
#endif
      mContext->fDeleteTextures(1, &tile->mTexture.mTextureHandle);
      mTiles.RemoveElementAt(i);
      continue;
    }

    i++;
  }

  // Iterate over the tiles and decide which ones we're going to harvest.
  // We harvest any tile that is entirely outside of the new valid region, or
  // any tile that is partially outside of the valid region and whose
  // resolution has changed.
  // XXX Tile iteration needs to be abstracted, or have some utility functions
  //     to make it simpler.
  uint16_t tileSize = aVideoMemoryTiledBuffer->GetTileLength();
  nsIntRect validBounds = aOldValidRegion.GetBounds();
  for (int x = validBounds.x; x < validBounds.XMost();) {
    int w = tileSize - x % tileSize;
    if (x + w > validBounds.x + validBounds.width)
      w = validBounds.x + validBounds.width - x;

    for (int y = validBounds.y; y < validBounds.YMost();) {
      int h = tileSize - y % tileSize;
      if (y + h > validBounds.y + validBounds.height)
        h = validBounds.y + validBounds.height - y;

      // If the new valid region doesn't contain this tile region,
      // harvest the tile.
      nsIntRegion tileRegion;
      tileRegion.And(aOldValidRegion, nsIntRect(x, y, w, h));

      nsIntRegion intersectingRegion;
      bool retainTile = false;
      if (aNewResolution != aOldResolution) {
        // Reconcile resolution changes.
        // If the resolution changes, we know the backing layer will have been
        // invalidated, so retain tiles that are partially encompassed by the
        // new valid area, instead of just tiles that don't intersect at all.
        nsIntRegion transformedTileRegion(tileRegion);
        transformedTileRegion.ScaleRoundOut(scaleFactor.width, scaleFactor.height);
        if (!aNewValidRegion.Contains(transformedTileRegion))
          retainTile = true;
      } else if (intersectingRegion.And(tileRegion, aNewValidRegion).IsEmpty()) {
        retainTile = true;
      }

      if (retainTile) {
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
        printf_stderr("Retaining tile at %d,%d, x%f for reuse\n", x, y, aOldResolution.width);
#endif
        TiledTexture removedTile;
        if (aVideoMemoryTiledBuffer->RemoveTile(nsIntPoint(x, y), removedTile)) {
          ReusableTiledTextureOGL* reusedTile =
            new ReusableTiledTextureOGL(removedTile, tileRegion, tileSize,
                                        aOldResolution);
          mTiles.AppendElement(reusedTile);
        }
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
        else
          printf_stderr("Failed to retain tile for reuse\n");
#endif
      }

      y += h;
    }

    x += w;
  }

  // Now prune our reused tile store of its oldest tiles if it gets too large.
  while (mTiles.Length() > aVideoMemoryTiledBuffer->GetTileCount() * mSizeLimit) {
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
    nsIntRect tileBounds = mTiles[0]->mTileRegion.GetBounds();
    printf_stderr("Releasing old reused tile at %d,%d, x%f\n",
                  tileBounds.x, tileBounds.y, mTiles[0]->mResolution.width);
#endif
    mContext->fDeleteTextures(1, &mTiles[0]->mTexture.mTextureHandle);
    mTiles.RemoveElementAt(0);
  }

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  printf_stderr("Retained %d tiles\n", mTiles.Length());
#endif
}

void
ReusableTileStoreOGL::DrawTiles(TiledThebesLayerOGL* aLayer,
                                const nsIntSize& aContentSize,
                                const nsIntRegion& aValidRegion,
                                const gfxSize& aResolution,
                                const gfx3DMatrix& aTransform,
                                const nsIntPoint& aRenderOffset)
{
  // Render old tiles to fill in gaps we haven't had the time to render yet.
  for (size_t i = 0; i < mTiles.Length(); i++) {
    ReusableTiledTextureOGL* tile = mTiles[i];

    // Work out the scaling factor in case of resolution differences.
    gfxSize scaleFactor = gfxSize(aResolution.width / tile->mResolution.width,
                                  aResolution.height / tile->mResolution.height);

    // Get the valid tile region, in the given coordinate space.
    nsIntRegion transformedTileRegion(tile->mTileRegion);
    if (aResolution != tile->mResolution)
      transformedTileRegion.ScaleRoundOut(scaleFactor.width, scaleFactor.height);

    // Skip drawing tiles that will be completely drawn over.
    if (aValidRegion.Contains(transformedTileRegion))
      continue;

    // Skip drawing tiles that have fallen outside of the layer area (these
    // will be discarded next time tiles are harvested).
    nsIntRect transformedTileRect = transformedTileRegion.GetBounds();
    if (transformedTileRect.XMost() > aContentSize.width ||
        transformedTileRect.YMost() > aContentSize.height)
      continue;

    // Reconcile the resolution difference by adjusting the transform.
    gfx3DMatrix transform = aTransform;
    if (aResolution != tile->mResolution)
      transform.Scale(scaleFactor.width, scaleFactor.height, 1);

    // XXX We should clip here to make sure we don't overlap with the valid
    //     region, otherwise we may end up with rendering artifacts on
    //     semi-transparent layers.
    //     Similarly, if we have multiple tiles covering the same area, we will
    //     end up with rendering artifacts if the aLayer isn't opaque.
    nsIntRect tileRect = tile->mTileRegion.GetBounds();
    uint16_t tileStartX = tileRect.x % tile->mTileSize;
    uint16_t tileStartY = tileRect.y % tile->mTileSize;
    nsIntRect textureRect(tileStartX, tileStartY, tileRect.width, tileRect.height);
    nsIntSize textureSize(tile->mTileSize, tile->mTileSize);
    aLayer->RenderTile(tile->mTexture, transform, aRenderOffset, tileRect, textureRect, textureSize);
  }
}

} // mozilla
} // layers
