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
#include "mozilla/layers/TiledContentClient.h"

class gfxReusableSurfaceWrapper;

namespace mozilla {
using namespace gfx;
namespace layers {

class Layer;

TiledLayerBufferComposite::TiledLayerBufferComposite()
  : mFrameResolution(1.0)
  , mHasDoubleBufferedTiles(false)
  , mUninitialized(true)
{}

TiledLayerBufferComposite::TiledLayerBufferComposite(ISurfaceAllocator* aAllocator,
                                                     const SurfaceDescriptorTiles& aDescriptor,
                                                     const nsIntRegion& aOldPaintedRegion)
{
  mUninitialized = false;
  mHasDoubleBufferedTiles = false;
  mValidRegion = aDescriptor.validRegion();
  mPaintedRegion = aDescriptor.paintedRegion();
  mRetainedWidth = aDescriptor.retainedWidth();
  mRetainedHeight = aDescriptor.retainedHeight();
  mResolution = aDescriptor.resolution();
  mFrameResolution = CSSToScreenScale(aDescriptor.frameResolution());

  // Combine any valid content that wasn't already uploaded
  nsIntRegion oldPaintedRegion(aOldPaintedRegion);
  oldPaintedRegion.And(oldPaintedRegion, mValidRegion);
  mPaintedRegion.Or(mPaintedRegion, oldPaintedRegion);

  const InfallibleTArray<TileDescriptor>& tiles = aDescriptor.tiles();
  for(size_t i = 0; i < tiles.Length(); i++) {
    RefPtr<TextureHost> texture;
    const TileDescriptor& tileDesc = tiles[i];
    switch (tileDesc.type()) {
      case TileDescriptor::TTexturedTileDescriptor : {
        texture = TextureHost::AsTextureHost(tileDesc.get_TexturedTileDescriptor().textureParent());
        const TileLock& ipcLock = tileDesc.get_TexturedTileDescriptor().sharedLock();
        nsRefPtr<gfxSharedReadLock> sharedLock;
        if (ipcLock.type() == TileLock::TShmemSection) {
          sharedLock = gfxShmSharedReadLock::Open(aAllocator, ipcLock.get_ShmemSection());
        } else {
          sharedLock = reinterpret_cast<gfxMemorySharedReadLock*>(ipcLock.get_uintptr_t());
          if (sharedLock) {
            // The corresponding AddRef is in TiledClient::GetTileDescriptor
            sharedLock->Release();
          }
        }

        mRetainedTiles.AppendElement(TileHost(sharedLock, texture));
        break;
      }
      default:
        NS_WARNING("Unrecognised tile descriptor type");
        // Fall through
      case TileDescriptor::TPlaceholderTileDescriptor :
        mRetainedTiles.AppendElement(GetPlaceholderTile());
        break;
    }
    if (texture && !texture->HasInternalBuffer()) {
      mHasDoubleBufferedTiles = true;
    }
  }
}

void
TiledLayerBufferComposite::ReadUnlock()
{
  if (!IsValid()) {
    return;
  }
  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    mRetainedTiles[i].ReadUnlock();
  }
}

void
TiledLayerBufferComposite::ReleaseTextureHosts()
{
  if (!IsValid()) {
    return;
  }
  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    mRetainedTiles[i].mTextureHost = nullptr;
  }
}

void
TiledLayerBufferComposite::Upload()
{
  if(!IsValid()) {
    return;
  }
  // The TextureClients were created with the TEXTURE_IMMEDIATE_UPLOAD flag,
  // so calling Update on all the texture hosts will perform the texture upload.
  Update(mValidRegion, mPaintedRegion);
  ClearPaintedRegion();
}

TileHost
TiledLayerBufferComposite::ValidateTile(TileHost aTile,
                                        const nsIntPoint& aTileOrigin,
                                        const nsIntRegion& aDirtyRect)
{
  if (aTile.IsPlaceholderTile()) {
    NS_WARNING("Placeholder tile encountered in painted region");
    return aTile;
  }

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  printf_stderr("Upload tile %i, %i\n", aTileOrigin.x, aTileOrigin.y);
  long start = PR_IntervalNow();
#endif

  MOZ_ASSERT(aTile.mTextureHost->GetFlags() & TEXTURE_IMMEDIATE_UPLOAD);
  // We possibly upload the entire texture contents here. This is a purposeful
  // decision, as sub-image upload can often be slow and/or unreliable, but
  // we may want to reevaluate this in the future.
  // For !HasInternalBuffer() textures, this is likely a no-op.
  aTile.mTextureHost->Updated(nullptr);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 1) {
    printf_stderr("Tile Time to upload %i\n", PR_IntervalNow() - start);
  }
#endif
  return aTile;
}

void
TiledLayerBufferComposite::SetCompositor(Compositor* aCompositor)
{
  if (!IsValid()) {
    return;
  }
  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    if (mRetainedTiles[i].IsPlaceholderTile()) continue;
    mRetainedTiles[i].mTextureHost->SetCompositor(aCompositor);
  }
}

TiledContentHost::TiledContentHost(const TextureInfo& aTextureInfo)
  : ContentHost(aTextureInfo)
  , mTiledBuffer(TiledLayerBufferComposite())
  , mLowPrecisionTiledBuffer(TiledLayerBufferComposite())
  , mOldTiledBuffer(TiledLayerBufferComposite())
  , mOldLowPrecisionTiledBuffer(TiledLayerBufferComposite())
  , mPendingUpload(false)
  , mPendingLowPrecisionUpload(false)
{
  MOZ_COUNT_CTOR(TiledContentHost);
}

TiledContentHost::~TiledContentHost()
{
  MOZ_COUNT_DTOR(TiledContentHost);

  // Unlock any buffers that may still be locked. If we have a pending upload,
  // we will need to unlock the buffer that was about to be uploaded.
  // If a buffer that was being composited had double-buffered tiles, we will
  // need to unlock that buffer too.
  if (mPendingUpload) {
    mTiledBuffer.ReadUnlock();
    if (mOldTiledBuffer.HasDoubleBufferedTiles()) {
      mOldTiledBuffer.ReadUnlock();
    }
  } else if (mTiledBuffer.HasDoubleBufferedTiles()) {
    mTiledBuffer.ReadUnlock();
  }

  if (mPendingLowPrecisionUpload) {
    mLowPrecisionTiledBuffer.ReadUnlock();
    if (mOldLowPrecisionTiledBuffer.HasDoubleBufferedTiles()) {
      mOldLowPrecisionTiledBuffer.ReadUnlock();
    }
  } else if (mLowPrecisionTiledBuffer.HasDoubleBufferedTiles()) {
    mLowPrecisionTiledBuffer.ReadUnlock();
  }
}

void
TiledContentHost::Attach(Layer* aLayer,
                         Compositor* aCompositor,
                         AttachFlags aFlags /* = NO_FLAGS */)
{
  CompositableHost::Attach(aLayer, aCompositor, aFlags);
  static_cast<ThebesLayerComposite*>(aLayer)->EnsureTiled();
}

void
TiledContentHost::UseTiledLayerBuffer(ISurfaceAllocator* aAllocator,
                                      const SurfaceDescriptorTiles& aTiledDescriptor)
{
  if (aTiledDescriptor.resolution() < 1) {
    if (mPendingLowPrecisionUpload) {
      mLowPrecisionTiledBuffer.ReadUnlock();
    } else {
      mPendingLowPrecisionUpload = true;
      // If the old buffer has double-buffered tiles, hang onto it so we can
      // unlock it after we've composited the new buffer.
      // We only need to hang onto the locks, but not the textures.
      // Releasing the textures here can help prevent a memory spike in the
      // situation that the client starts rendering new content before we get
      // to composite the new buffer.
      if (mLowPrecisionTiledBuffer.HasDoubleBufferedTiles()) {
        mOldLowPrecisionTiledBuffer = mLowPrecisionTiledBuffer;
        mOldLowPrecisionTiledBuffer.ReleaseTextureHosts();
      }
    }
    mLowPrecisionTiledBuffer =
      TiledLayerBufferComposite(aAllocator, aTiledDescriptor,
                                mLowPrecisionTiledBuffer.GetPaintedRegion());
  } else {
    if (mPendingUpload) {
      mTiledBuffer.ReadUnlock();
    } else {
      mPendingUpload = true;
      if (mTiledBuffer.HasDoubleBufferedTiles()) {
        mOldTiledBuffer = mTiledBuffer;
        mOldTiledBuffer.ReleaseTextureHosts();
      }
    }
    mTiledBuffer = TiledLayerBufferComposite(aAllocator, aTiledDescriptor,
                                             mTiledBuffer.GetPaintedRegion());
  }
}

void
TiledContentHost::Composite(EffectChain& aEffectChain,
                            float aOpacity,
                            const gfx::Matrix4x4& aTransform,
                            const gfx::Filter& aFilter,
                            const gfx::Rect& aClipRect,
                            const nsIntRegion* aVisibleRegion /* = nullptr */,
                            TiledLayerProperties* aLayerProperties /* = nullptr */)
{
  MOZ_ASSERT(aLayerProperties, "aLayerProperties required for TiledContentHost");

  // Render valid tiles.
  nsIntRect visibleRect = aVisibleRegion->GetBounds();

  if (mPendingUpload) {
    mTiledBuffer.SetCompositor(mCompositor);
    mTiledBuffer.Upload();

    // For a single-buffered tiled buffer, Upload will upload the shared memory
    // surface to texture memory and we no longer need to read from them.
    if (!mTiledBuffer.HasDoubleBufferedTiles()) {
      mTiledBuffer.ReadUnlock();
    }
  }
  if (mPendingLowPrecisionUpload) {
    mLowPrecisionTiledBuffer.SetCompositor(mCompositor);
    mLowPrecisionTiledBuffer.Upload();

    if (!mLowPrecisionTiledBuffer.HasDoubleBufferedTiles()) {
      mLowPrecisionTiledBuffer.ReadUnlock();
    }
  }

  RenderLayerBuffer(mLowPrecisionTiledBuffer,
                    mLowPrecisionTiledBuffer.GetValidRegion(), aEffectChain, aOpacity,
                    aFilter, aClipRect, aLayerProperties->mValidRegion, visibleRect, aTransform);
  RenderLayerBuffer(mTiledBuffer, aLayerProperties->mValidRegion, aEffectChain, aOpacity,
                    aFilter, aClipRect, nsIntRegion(), visibleRect, aTransform);

  // Now release the old buffer if it had double-buffered tiles, as we can
  // guarantee that they're no longer on the screen (and so any locks that may
  // have been held have been released).
  if (mPendingUpload && mOldTiledBuffer.HasDoubleBufferedTiles()) {
    mOldTiledBuffer.ReadUnlock();
    mOldTiledBuffer = TiledLayerBufferComposite();
  }
  if (mPendingLowPrecisionUpload && mOldLowPrecisionTiledBuffer.HasDoubleBufferedTiles()) {
    mOldLowPrecisionTiledBuffer.ReadUnlock();
    mOldLowPrecisionTiledBuffer = TiledLayerBufferComposite();
  }
  mPendingUpload = mPendingLowPrecisionUpload = false;
}


void
TiledContentHost::RenderTile(const TileHost& aTile,
                             EffectChain& aEffectChain,
                             float aOpacity,
                             const gfx::Matrix4x4& aTransform,
                             const gfx::Filter& aFilter,
                             const gfx::Rect& aClipRect,
                             const nsIntRegion& aScreenRegion,
                             const nsIntPoint& aTextureOffset,
                             const nsIntSize& aTextureBounds)
{
  if (aTile.IsPlaceholderTile()) {
    // This shouldn't ever happen, but let's fail semi-gracefully. No need
    // to warn, the texture update would have already caught this.
    return;
  }

  nsIntRect screenBounds = aScreenRegion.GetBounds();
  Rect quad(screenBounds.x, screenBounds.y, screenBounds.width, screenBounds.height);
  quad = aTransform.TransformBounds(quad);

  if (!quad.Intersects(mCompositor->ClipRectInLayersCoordinates(aClipRect))) {
    return;
  }

  AutoLockTextureHost autoLock(aTile.mTextureHost);
  if (autoLock.Failed()) {
    NS_WARNING("Failed to lock tile");
    return;
  }
  RefPtr<NewTextureSource> source = aTile.mTextureHost->GetTextureSources();
  if (!source) {
    return;
  }

  RefPtr<TexturedEffect> effect =
    CreateTexturedEffect(aTile.mTextureHost->GetFormat(), source, aFilter);
  if (!effect) {
    return;
  }

  aEffectChain.mPrimaryEffect = effect;

  nsIntRegionRectIterator it(aScreenRegion);
  for (const nsIntRect* rect = it.Next(); rect != nullptr; rect = it.Next()) {
    Rect graphicsRect(rect->x, rect->y, rect->width, rect->height);
    Rect textureRect(rect->x - aTextureOffset.x, rect->y - aTextureOffset.y,
                     rect->width, rect->height);

    effect->mTextureCoords = Rect(textureRect.x / aTextureBounds.width,
                                  textureRect.y / aTextureBounds.height,
                                  textureRect.width / aTextureBounds.width,
                                  textureRect.height / aTextureBounds.height);
    mCompositor->DrawQuad(graphicsRect, aClipRect, aEffectChain, aOpacity, aTransform);
  }
  mCompositor->DrawDiagnostics(DIAGNOSTIC_CONTENT|DIAGNOSTIC_TILE,
                               aScreenRegion, aClipRect, aTransform);
}

void
TiledContentHost::RenderLayerBuffer(TiledLayerBufferComposite& aLayerBuffer,
                                    const nsIntRegion& aValidRegion,
                                    EffectChain& aEffectChain,
                                    float aOpacity,
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
  gfx::Size layerScale(1, 1);
  // We assume that the current frame resolution is the one used in our primary
  // layer buffer. Compensate for a changing frame resolution.
  if (aLayerBuffer.GetFrameResolution() != mTiledBuffer.GetFrameResolution()) {
    const CSSToScreenScale& layerResolution = aLayerBuffer.GetFrameResolution();
    const CSSToScreenScale& localResolution = mTiledBuffer.GetFrameResolution();
    layerScale.width = layerScale.height = layerResolution.scale / localResolution.scale;
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

      TileHost tileTexture = aLayerBuffer.
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
          RenderTile(tileTexture, aEffectChain, aOpacity, aTransform, aFilter, aClipRect, tileDrawRegion,
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
                                   rect, aClipRect, aTransform);
}

void
TiledContentHost::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  aTo += aPrefix;
  aTo += nsPrintfCString("TiledContentHost (0x%p)", this);

}

#ifdef MOZ_DUMP_PAINTING
void
TiledContentHost::Dump(FILE* aFile,
                       const char* aPrefix,
                       bool aDumpHtml)
{
  if (!aFile) {
    aFile = stderr;
  }

  TiledLayerBufferComposite::Iterator it = mTiledBuffer.TilesBegin();
  TiledLayerBufferComposite::Iterator stop = mTiledBuffer.TilesEnd();
  if (aDumpHtml) {
    fprintf_stderr(aFile, "<ul>");
  }
  for (;it != stop; ++it) {
    fprintf_stderr(aFile, "%s", aPrefix);
    fprintf_stderr(aFile, aDumpHtml ? "<li> <a href=" : "Tile ");
    if (it->IsPlaceholderTile()) {
      fprintf_stderr(aFile, "empty tile");
    } else {
      DumpTextureHost(aFile, it->mTextureHost);
    }
    fprintf_stderr(aFile, aDumpHtml ? " >Tile</a></li>" : " ");
  }
    if (aDumpHtml) {
    fprintf_stderr(aFile, "</ul>");
  }
}
#endif

} // namespace
} // namespace
