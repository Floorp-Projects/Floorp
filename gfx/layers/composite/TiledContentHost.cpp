/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TiledContentHost.h"
#include "ThebesLayerComposite.h"       // for ThebesLayerComposite
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/Effects.h"     // for TexturedEffect, Effect, etc
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_WARNING
#include "nsPoint.h"                    // for nsIntPoint
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsRect.h"                     // for nsIntRect
#include "nsSize.h"                     // for nsIntSize

class gfxReusableSurfaceWrapper;

namespace mozilla {
using namespace gfx;
namespace layers {

class Layer;

void
TiledLayerBufferComposite::Upload(const BasicTiledLayerBuffer* aMainMemoryTiledBuffer,
                                  const nsIntRegion& aNewValidRegion,
                                  const nsIntRegion& aInvalidateRegion,
                                  const gfxSize& aResolution)
{
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  printf_stderr("Upload %i, %i, %i, %i\n", aInvalidateRegion.GetBounds().x, aInvalidateRegion.GetBounds().y, aInvalidateRegion.GetBounds().width, aInvalidateRegion.GetBounds().height);
  long start = PR_IntervalNow();
#endif

  mFrameResolution = aResolution;
  mMainMemoryTiledBuffer = aMainMemoryTiledBuffer;
  Update(aNewValidRegion, aInvalidateRegion);
  mMainMemoryTiledBuffer = nullptr;
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 10) {
    printf_stderr("Time to upload %i\n", PR_IntervalNow() - start);
  }
#endif
}

TiledTexture
TiledLayerBufferComposite::ValidateTile(TiledTexture aTile,
                                        const nsIntPoint& aTileOrigin,
                                        const nsIntRegion& aDirtyRect)
{
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  printf_stderr("Upload tile %i, %i\n", aTileOrigin.x, aTileOrigin.y);
  long start = PR_IntervalNow();
#endif

  aTile.Validate(mMainMemoryTiledBuffer->GetTile(aTileOrigin).GetSurface(), mCompositor, GetTileLength());

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 1) {
    printf_stderr("Tile Time to upload %i\n", PR_IntervalNow() - start);
  }
#endif
  return aTile;
}

void
TiledContentHost::Attach(Layer* aLayer, Compositor* aCompositor)
{
  CompositableHost::Attach(aLayer, aCompositor);
  static_cast<ThebesLayerComposite*>(aLayer)->EnsureTiled();
}

void
TiledContentHost::PaintedTiledLayerBuffer(ISurfaceAllocator* aAllocator,
                                          const SurfaceDescriptorTiles& aTiledDescriptor)
{
  if (aTiledDescriptor.resolution() < 1) {
    mLowPrecisionMainMemoryTiledBuffer = BasicTiledLayerBuffer::OpenDescriptor(aAllocator, aTiledDescriptor);
    mLowPrecisionRegionToUpload.Or(mLowPrecisionRegionToUpload,
                                   mLowPrecisionMainMemoryTiledBuffer.GetPaintedRegion());
    mLowPrecisionMainMemoryTiledBuffer.ClearPaintedRegion();
    mPendingLowPrecisionUpload = true;
  } else {
    mMainMemoryTiledBuffer = BasicTiledLayerBuffer::OpenDescriptor(aAllocator, aTiledDescriptor);
    mRegionToUpload.Or(mRegionToUpload, mMainMemoryTiledBuffer.GetPaintedRegion());
    mMainMemoryTiledBuffer.ClearPaintedRegion();
    mPendingUpload = true;
  }
}

void
TiledContentHost::ProcessLowPrecisionUploadQueue()
{
  if (!mPendingLowPrecisionUpload) {
    return;
  }

  mLowPrecisionRegionToUpload.And(mLowPrecisionRegionToUpload,
                                  mLowPrecisionMainMemoryTiledBuffer.GetValidRegion());
  mLowPrecisionVideoMemoryTiledBuffer.SetResolution(
    mLowPrecisionMainMemoryTiledBuffer.GetResolution());
  // It's assumed that the video memory tiled buffer has an up-to-date
  // frame resolution. As it's always updated first when zooming, this
  // should always be true.
  mLowPrecisionVideoMemoryTiledBuffer.Upload(&mLowPrecisionMainMemoryTiledBuffer,
                                 mLowPrecisionMainMemoryTiledBuffer.GetValidRegion(),
                                 mLowPrecisionRegionToUpload,
                                 mVideoMemoryTiledBuffer.GetFrameResolution());
  nsIntRegion validRegion = mLowPrecisionVideoMemoryTiledBuffer.GetValidRegion();

  mLowPrecisionMainMemoryTiledBuffer = BasicTiledLayerBuffer();
  mLowPrecisionRegionToUpload = nsIntRegion();
  mPendingLowPrecisionUpload = false;
}

void
TiledContentHost::ProcessUploadQueue(nsIntRegion* aNewValidRegion,
                                     TiledLayerProperties* aLayerProperties)
{
  if (!mPendingUpload)
    return;

  // If we coalesce uploads while the layers' valid region is changing we will
  // end up trying to upload area outside of the valid region. (bug 756555)
  mRegionToUpload.And(mRegionToUpload, mMainMemoryTiledBuffer.GetValidRegion());

  mVideoMemoryTiledBuffer.Upload(&mMainMemoryTiledBuffer,
                                 mMainMemoryTiledBuffer.GetValidRegion(),
                                 mRegionToUpload, aLayerProperties->mEffectiveResolution);

  *aNewValidRegion = mVideoMemoryTiledBuffer.GetValidRegion();

  // Release all the tiles by replacing the tile buffer with an empty
  // tiled buffer.
  mMainMemoryTiledBuffer = BasicTiledLayerBuffer();
  mRegionToUpload = nsIntRegion();
  mPendingUpload = false;
}

void
TiledContentHost::Composite(EffectChain& aEffectChain,
                            float aOpacity,
                            const gfx::Matrix4x4& aTransform,
                            const gfx::Point& aOffset,
                            const gfx::Filter& aFilter,
                            const gfx::Rect& aClipRect,
                            const nsIntRegion* aVisibleRegion /* = nullptr */,
                            TiledLayerProperties* aLayerProperties /* = nullptr */)
{
  MOZ_ASSERT(aLayerProperties, "aLayerProperties required for TiledContentHost");

  // note that ProcessUploadQueue updates the valid region which is then used by
  // the RenderLayerBuffer calls below and then sent back to the layer.
  ProcessUploadQueue(&aLayerProperties->mValidRegion, aLayerProperties);
  ProcessLowPrecisionUploadQueue();

  // Render valid tiles.
  nsIntRect visibleRect = aVisibleRegion->GetBounds();

  RenderLayerBuffer(mLowPrecisionVideoMemoryTiledBuffer,
                    mLowPrecisionVideoMemoryTiledBuffer.GetValidRegion(), aEffectChain, aOpacity,
                    aOffset, aFilter, aClipRect, aLayerProperties->mValidRegion, visibleRect, aTransform);
  RenderLayerBuffer(mVideoMemoryTiledBuffer, aLayerProperties->mValidRegion, aEffectChain, aOpacity, aOffset,
                    aFilter, aClipRect, nsIntRegion(), visibleRect, aTransform);
}


void
TiledContentHost::RenderTile(const TiledTexture& aTile,
                             EffectChain& aEffectChain,
                             float aOpacity,
                             const gfx::Matrix4x4& aTransform,
                             const gfx::Point& aOffset,
                             const gfx::Filter& aFilter,
                             const gfx::Rect& aClipRect,
                             const nsIntRegion& aScreenRegion,
                             const nsIntPoint& aTextureOffset,
                             const nsIntSize& aTextureBounds)
{
  MOZ_ASSERT(aTile.mDeprecatedTextureHost, "Trying to render a placeholder tile?");

  RefPtr<TexturedEffect> effect =
    CreateTexturedEffect(aTile.mDeprecatedTextureHost, aFilter);
  if (aTile.mDeprecatedTextureHost->Lock()) {
    aEffectChain.mPrimaryEffect = effect;
  } else {
    return;
  }

  nsIntRegionRectIterator it(aScreenRegion);
  for (const nsIntRect* rect = it.Next(); rect != nullptr; rect = it.Next()) {
    Rect graphicsRect(rect->x, rect->y, rect->width, rect->height);
    Rect textureRect(rect->x - aTextureOffset.x, rect->y - aTextureOffset.y,
                     rect->width, rect->height);

    effect->mTextureCoords = Rect(textureRect.x / aTextureBounds.width,
                                  textureRect.y / aTextureBounds.height,
                                  textureRect.width / aTextureBounds.width,
                                  textureRect.height / aTextureBounds.height);
    mCompositor->DrawQuad(graphicsRect, aClipRect, aEffectChain, aOpacity, aTransform, aOffset);
    mCompositor->DrawDiagnostics(DIAGNOSTIC_CONTENT|DIAGNOSTIC_TILE,
                                 graphicsRect, aClipRect, aTransform, aOffset);
  }

  aTile.mDeprecatedTextureHost->Unlock();
}

void
TiledContentHost::RenderLayerBuffer(TiledLayerBufferComposite& aLayerBuffer,
                                    const nsIntRegion& aValidRegion,
                                    EffectChain& aEffectChain,
                                    float aOpacity,
                                    const gfx::Point& aOffset,
                                    const gfx::Filter& aFilter,
                                    const gfx::Rect& aClipRect,
                                    const nsIntRegion& aMaskRegion,
                                    nsIntRect aVisibleRect,
                                    gfx::Matrix4x4 aTransform)
{
  if (!mCompositor) {
    NS_WARNING("Can't render tiled content host - no compositor");
    return;
  }
  float resolution = aLayerBuffer.GetResolution();
  gfxSize layerScale(1, 1);
  // We assume that the current frame resolution is the one used in our primary
  // layer buffer. Compensate for a changing frame resolution.
  if (aLayerBuffer.GetFrameResolution() != mVideoMemoryTiledBuffer.GetFrameResolution()) {
    const gfxSize& layerResolution = aLayerBuffer.GetFrameResolution();
    const gfxSize& localResolution = mVideoMemoryTiledBuffer.GetFrameResolution();
    layerScale.width = layerResolution.width / localResolution.width;
    layerScale.height = layerResolution.height / localResolution.height;
    aVisibleRect.ScaleRoundOut(layerScale.width, layerScale.height);
  }
  aTransform.Scale(1/(resolution * layerScale.width),
                   1/(resolution * layerScale.height), 1);

  uint32_t rowCount = 0;
  uint32_t tileX = 0;
  for (int32_t x = aVisibleRect.x; x < aVisibleRect.x + aVisibleRect.width;) {
    rowCount++;
    int32_t tileStartX = aLayerBuffer.GetTileStart(x);
    int32_t w = aLayerBuffer.GetScaledTileLength() - tileStartX;
    if (x + w > aVisibleRect.x + aVisibleRect.width) {
      w = aVisibleRect.x + aVisibleRect.width - x;
    }
    int tileY = 0;
    for (int32_t y = aVisibleRect.y; y < aVisibleRect.y + aVisibleRect.height;) {
      int32_t tileStartY = aLayerBuffer.GetTileStart(y);
      int32_t h = aLayerBuffer.GetScaledTileLength() - tileStartY;
      if (y + h > aVisibleRect.y + aVisibleRect.height) {
        h = aVisibleRect.y + aVisibleRect.height - y;
      }

      TiledTexture tileTexture = aLayerBuffer.
        GetTile(nsIntPoint(aLayerBuffer.RoundDownToTileEdge(x),
                           aLayerBuffer.RoundDownToTileEdge(y)));
      if (tileTexture != aLayerBuffer.GetPlaceholderTile()) {
        nsIntRegion tileDrawRegion;
        tileDrawRegion.And(aValidRegion,
                           nsIntRect(x * layerScale.width,
                                     y * layerScale.height,
                                     w * layerScale.width,
                                     h * layerScale.height));
        tileDrawRegion.Sub(tileDrawRegion, aMaskRegion);

        if (!tileDrawRegion.IsEmpty()) {
          tileDrawRegion.ScaleRoundOut(resolution / layerScale.width,
                                       resolution / layerScale.height);

          nsIntPoint tileOffset((x - tileStartX) * resolution,
                                (y - tileStartY) * resolution);
          uint32_t tileSize = aLayerBuffer.GetTileLength();
          RenderTile(tileTexture, aEffectChain, aOpacity, aTransform, aOffset, aFilter, aClipRect, tileDrawRegion,
                     tileOffset, nsIntSize(tileSize, tileSize));
        }
      }
      tileY++;
      y += h;
    }
    tileX++;
    x += w;
  }
  gfx::Rect rect(aVisibleRect.x, aVisibleRect.y,
                 aVisibleRect.width, aVisibleRect.height);
  GetCompositor()->DrawDiagnostics(DIAGNOSTIC_CONTENT,
                                   rect, aClipRect, aTransform, aOffset);
}

void
TiledTexture::Validate(gfxReusableSurfaceWrapper* aReusableSurface, Compositor* aCompositor, uint16_t aSize)
{
  TextureFlags flags = 0;
  if (!mDeprecatedTextureHost) {
    // convert placeholder tile to a real tile
    mDeprecatedTextureHost = DeprecatedTextureHost::CreateDeprecatedTextureHost(SurfaceDescriptor::Tnull_t,
                                                  TEXTURE_HOST_TILED,
                                                  flags);
    mDeprecatedTextureHost->SetCompositor(aCompositor);
    flags |= TEXTURE_NEW_TILE;
  }

  mDeprecatedTextureHost->Update(aReusableSurface, flags, gfx::IntSize(aSize, aSize));
}

#ifdef MOZ_LAYERS_HAVE_LOG
void
TiledContentHost::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  aTo += aPrefix;
  aTo += nsPrintfCString("TiledContentHost (0x%p)", this);

}
#endif

#ifdef MOZ_DUMP_PAINTING
void
TiledContentHost::Dump(FILE* aFile,
                       const char* aPrefix,
                       bool aDumpHtml)
{
  if (!aFile) {
    aFile = stderr;
  }

  TiledLayerBufferComposite::Iterator it = mVideoMemoryTiledBuffer.TilesBegin();
  TiledLayerBufferComposite::Iterator stop = mVideoMemoryTiledBuffer.TilesEnd();
  if (aDumpHtml) {
    fprintf(aFile, "<ul>");
  }
  for (;it != stop; ++it) {
    fprintf(aFile, "%s", aPrefix);
    fprintf(aFile, aDumpHtml ? "<li> <a href=" : "Tile ");
    DumpDeprecatedTextureHost(aFile, it->mDeprecatedTextureHost);
    fprintf(aFile, aDumpHtml ? " >Tile</a></li>" : " ");
  }
    if (aDumpHtml) {
    fprintf(aFile, "</ul>");
  }
}
#endif

} // namespace
} // namespace
