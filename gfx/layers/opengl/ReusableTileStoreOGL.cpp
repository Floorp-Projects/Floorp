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
  for (PRUint32 i = 0; i < mTiles.Length(); i++)
    mContext->fDeleteTextures(1, &mTiles[i]->mTexture.mTextureHandle);
  mTiles.Clear();
}

void
ReusableTileStoreOGL::InvalidateTiles(TiledThebesLayerOGL* aLayer,
                                      const nsIntRegion& aValidRegion,
                                      const gfxSize& aResolution)
{
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  printf_stderr("Invalidating reused tiles\n");
#endif

  // Find out the area of the nearest display-port to invalidate retained
  // tiles.
  gfxRect renderBounds;
  for (ContainerLayer* parent = aLayer->GetParent(); parent; parent = parent->GetParent()) {
      const FrameMetrics& metrics = parent->GetFrameMetrics();
      if (!metrics.mDisplayPort.IsEmpty()) {
          // We use the bounds to cut down on complication/computation time.
          // This will be incorrect when the transform involves rotation, but
          // it'd be quite hard to retain invalid tiles correctly in this
          // situation anyway.
          renderBounds = parent->GetEffectiveTransform().TransformBounds(gfxRect(metrics.mDisplayPort));
          break;
      }
  }

  // If no display port was found, use the widget size from the layer manager.
  if (renderBounds.IsEmpty()) {
      LayerManagerOGL* manager = static_cast<LayerManagerOGL*>(aLayer->Manager());
      const nsIntSize& widgetSize = manager->GetWidgetSize();
      renderBounds.width = widgetSize.width;
      renderBounds.height = widgetSize.height;
  }

  // Iterate over existing harvested tiles and release any that are contained
  // within the new valid region, the display-port or the widget area. The
  // assumption is that anything within this area should be valid, so there's
  // no need to keep invalid tiles there.
  mContext->MakeCurrent();
  for (PRUint32 i = 0; i < mTiles.Length();) {
    ReusableTiledTextureOGL* tile = mTiles[i];

    // Check if the tile region is contained within the new valid region.
    nsIntRect tileRect;
    bool release = false;
    if (tile->mResolution == aResolution) {
      if (aValidRegion.Contains(tile->mTileRegion)) {
        release = true;
      } else {
        tileRect = tile->mTileRegion.GetBounds();
      }
    } else {
      nsIntRegion transformedTileRegion(tile->mTileRegion);
      transformedTileRegion.ScaleRoundOut(tile->mResolution.width / aResolution.width,
                                          tile->mResolution.height / aResolution.height);
      if (aValidRegion.Contains(transformedTileRegion))
        release = true;
      else
        tileRect = transformedTileRegion.GetBounds();
    }

    // If the tile region wasn't contained within the valid region, check if
    // it intersects with the currently rendered region.
    if (!release) {
      // Transform the tile region to see if it falls inside the rendered bounds
      gfxRect tileBounds = aLayer->GetEffectiveTransform().TransformBounds(gfxRect(tileRect));
      if (renderBounds.Contains(tileBounds))
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
}

void
ReusableTileStoreOGL::HarvestTiles(TiledThebesLayerOGL* aLayer,
                                   TiledLayerBufferOGL* aVideoMemoryTiledBuffer,
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

  // Iterate over the tiles and decide which ones we're going to harvest.
  // We harvest any tile that is entirely outside of the new valid region, or
  // any tile that is partially outside of the valid region and whose
  // resolution has changed.
  // XXX Tile iteration needs to be abstracted, or have some utility functions
  //     to make it simpler.
  uint16_t tileSize = aVideoMemoryTiledBuffer->GetTileLength();
  nsIntRect validBounds = aOldValidRegion.GetBounds();
  for (int x = validBounds.x; x < validBounds.XMost();) {
    int w = tileSize - aVideoMemoryTiledBuffer->GetTileStart(x);
    if (x + w > validBounds.x + validBounds.width)
      w = validBounds.x + validBounds.width - x;

    for (int y = validBounds.y; y < validBounds.YMost();) {
      int h = tileSize - aVideoMemoryTiledBuffer->GetTileStart(y);
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
            new ReusableTiledTextureOGL(removedTile, nsIntPoint(x, y), tileRegion,
                                        tileSize, aOldResolution);
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

  // Make sure we don't hold onto tiles that may cause visible rendering glitches
  InvalidateTiles(aLayer, aNewValidRegion, aNewResolution);

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
                                const nsIntRegion& aValidRegion,
                                const gfxSize& aResolution,
                                const gfx3DMatrix& aTransform,
                                const nsIntPoint& aRenderOffset,
                                Layer* aMaskLayer)
{
  // Walk up the tree, looking for a display-port - if we find one, we know
  // that this layer represents a content node and we can use its first
  // scrollable child, in conjunction with its content area and viewport offset
  // to establish the screen coordinates to which the content area will be
  // rendered.
  gfxRect contentBounds, displayPort;
  ContainerLayer* scrollableLayer = nsnull;
  for (ContainerLayer* parent = aLayer->GetParent(); parent; parent = parent->GetParent()) {
      const FrameMetrics& parentMetrics = parent->GetFrameMetrics();
      if (parentMetrics.IsScrollable())
        scrollableLayer = parent;
      if (!parentMetrics.mDisplayPort.IsEmpty() && scrollableLayer) {
          displayPort = parent->GetEffectiveTransform().
            TransformBounds(gfxRect(parentMetrics.mDisplayPort));
          const FrameMetrics& metrics = scrollableLayer->GetFrameMetrics();
          const nsIntSize& contentSize = metrics.mContentRect.Size();
          const nsIntPoint& contentOrigin = metrics.mContentRect.TopLeft() - metrics.mViewportScrollOffset;
          gfxRect contentRect = gfxRect(contentOrigin.x, contentOrigin.y,
                                        contentSize.width, contentSize.height);
          contentBounds = scrollableLayer->GetEffectiveTransform().TransformBounds(contentRect);
          break;
      }
  }

  // Render old tiles to fill in gaps we haven't had the time to render yet.
  for (PRUint32 i = 0; i < mTiles.Length(); i++) {
    ReusableTiledTextureOGL* tile = mTiles[i];

    // Work out the scaling factor in case of resolution differences.
    gfxSize scaleFactor = gfxSize(aResolution.width / tile->mResolution.width,
                                  aResolution.height / tile->mResolution.height);

    // Reconcile the resolution difference by adjusting the transform.
    gfx3DMatrix transform = aTransform;
    if (aResolution != tile->mResolution)
      transform.Scale(scaleFactor.width, scaleFactor.height, 1);

    // Subtract the layer's valid region from the tile region.
    nsIntRegion transformedValidRegion(aValidRegion);
    if (aResolution != tile->mResolution)
      transformedValidRegion.ScaleRoundOut(1.0f/scaleFactor.width,
                                           1.0f/scaleFactor.height);
    nsIntRegion tileRegion;
    tileRegion.Sub(tile->mTileRegion, transformedValidRegion);

    // Subtract the display-port from the tile region.
    if (!displayPort.IsEmpty()) {
      gfxRect transformedRenderBounds = transform.Inverse().TransformBounds(displayPort);
      tileRegion.Sub(tileRegion, nsIntRect(transformedRenderBounds.x,
                                           transformedRenderBounds.y,
                                           transformedRenderBounds.width,
                                           transformedRenderBounds.height));
    }

    // Intersect the tile region with the content area.
    if (!contentBounds.IsEmpty()) {
      gfxRect transformedRenderBounds = transform.Inverse().TransformBounds(contentBounds);
      tileRegion.And(tileRegion, nsIntRect(transformedRenderBounds.x,
                                           transformedRenderBounds.y,
                                           transformedRenderBounds.width,
                                           transformedRenderBounds.height));
    }

    // If the tile region is empty, skip drawing.
    if (tileRegion.IsEmpty())
      continue;

    // XXX If we have multiple tiles covering the same area, we will
    //     end up with rendering artifacts if the aLayer isn't opaque.
    int32_t tileStartX;
    int32_t tileStartY;
    if (tile->mTileOrigin.x >= 0) {
      tileStartX = tile->mTileOrigin.x % tile->mTileSize;
    } else {
      tileStartX = (tile->mTileSize - (-tile->mTileOrigin.x % tile->mTileSize)) % tile->mTileSize;
    }
    if (tile->mTileOrigin.y >= 0) {
      tileStartY = tile->mTileOrigin.y % tile->mTileSize;
    } else {
      tileStartY = (tile->mTileSize - (-tile->mTileOrigin.y % tile->mTileSize)) % tile->mTileSize;
    }
    nsIntPoint tileOffset(tile->mTileOrigin.x - tileStartX, tile->mTileOrigin.y - tileStartY);
    nsIntSize textureSize(tile->mTileSize, tile->mTileSize);
    aLayer->RenderTile(tile->mTexture, transform, aRenderOffset, tileRegion, tileOffset, textureSize, aMaskLayer);
  }
}

} // mozilla
} // layers
