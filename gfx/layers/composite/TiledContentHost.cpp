/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TiledContentHost.h"
#include "PaintedLayerComposite.h"      // for PaintedLayerComposite
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/Effects.h"     // for TexturedEffect, Effect, etc
#include "mozilla/layers/LayerMetricsWrapper.h" // for LayerMetricsWrapper
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_WARNING
#include "nsPoint.h"                    // for IntPoint
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsRect.h"                     // for IntRect
#include "mozilla/layers/TiledContentClient.h"

class gfxReusableSurfaceWrapper;

namespace mozilla {
using namespace gfx;
namespace layers {

class Layer;

TiledLayerBufferComposite::TiledLayerBufferComposite()
  : mFrameResolution()
{}

TiledLayerBufferComposite::~TiledLayerBufferComposite()
{
  Clear();
}

/* static */ void
TiledLayerBufferComposite::RecycleCallback(TextureHost* textureHost, void* aClosure)
{
  textureHost->CompositorRecycle();
}

void
TiledLayerBufferComposite::SetCompositor(Compositor* aCompositor)
{
  MOZ_ASSERT(aCompositor);
  for (TileHost& tile : mRetainedTiles) {
    if (tile.IsPlaceholderTile()) continue;
    tile.mTextureHost->SetCompositor(aCompositor);
    if (tile.mTextureHostOnWhite) {
      tile.mTextureHostOnWhite->SetCompositor(aCompositor);
    }
  }
}

TiledContentHost::TiledContentHost(const TextureInfo& aTextureInfo)
  : ContentHost(aTextureInfo)
  , mTiledBuffer(TiledLayerBufferComposite())
  , mLowPrecisionTiledBuffer(TiledLayerBufferComposite())
{
  MOZ_COUNT_CTOR(TiledContentHost);
}

TiledContentHost::~TiledContentHost()
{
  MOZ_COUNT_DTOR(TiledContentHost);
}

void
TiledContentHost::Attach(Layer* aLayer,
                         Compositor* aCompositor,
                         AttachFlags aFlags /* = NO_FLAGS */)
{
  CompositableHost::Attach(aLayer, aCompositor, aFlags);
}

void
TiledContentHost::Detach(Layer* aLayer,
                         AttachFlags aFlags /* = NO_FLAGS */)
{
  if (!mKeepAttached || aLayer == mLayer || aFlags & FORCE_DETACH) {
    // Clear the TiledLayerBuffers, which will take care of releasing the
    // copy-on-write locks.
    mTiledBuffer.Clear();
    mLowPrecisionTiledBuffer.Clear();
  }
  CompositableHost::Detach(aLayer,aFlags);
}

bool
TiledContentHost::UseTiledLayerBuffer(ISurfaceAllocator* aAllocator,
                                      const SurfaceDescriptorTiles& aTiledDescriptor)
{
  if (aTiledDescriptor.resolution() < 1) {
    if (!mLowPrecisionTiledBuffer.UseTiles(aTiledDescriptor, mCompositor, aAllocator)) {
      return false;
    }
  } else {
    if (!mTiledBuffer.UseTiles(aTiledDescriptor, mCompositor, aAllocator)) {
      return false;
    }
  }
  return true;
}

void
UseTileTexture(CompositableTextureHostRef& aTexture,
               CompositableTextureSourceRef& aTextureSource,
               const IntRect& aUpdateRect,
               TextureHost* aNewTexture,
               Compositor* aCompositor)
{
  MOZ_ASSERT(aNewTexture);
  if (!aNewTexture) {
    return;
  }

  if (aTexture) {
    aTexture->SetCompositor(aCompositor);
    aNewTexture->SetCompositor(aCompositor);

    if (aTexture->GetFormat() != aNewTexture->GetFormat()) {
      // Only reuse textures if their format match the new texture's.
      aTextureSource = nullptr;
      aTexture = nullptr;
    }
  }

  aTexture = aNewTexture;


  if (aCompositor) {
    aTexture->SetCompositor(aCompositor);
  }

  if (!aUpdateRect.IsEmpty()) {
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    aTexture->Updated(nullptr);
#else
    // We possibly upload the entire texture contents here. This is a purposeful
    // decision, as sub-image upload can often be slow and/or unreliable, but
    // we may want to reevaluate this in the future.
    // For !HasInternalBuffer() textures, this is likely a no-op.
    nsIntRegion region = aUpdateRect;
    aTexture->Updated(&region);
#endif
  }

  aTexture->PrepareTextureSource(aTextureSource);
}

bool
GetCopyOnWriteLock(const TileLock& ipcLock, TileHost& aTile, ISurfaceAllocator* aAllocator) {
  MOZ_ASSERT(aAllocator);

  nsRefPtr<gfxSharedReadLock> sharedLock;
  if (ipcLock.type() == TileLock::TShmemSection) {
    sharedLock = gfxShmSharedReadLock::Open(aAllocator, ipcLock.get_ShmemSection());
  } else {
    if (!aAllocator->IsSameProcess()) {
      // Trying to use a memory based lock instead of a shmem based one in
      // the cross-process case is a bad security violation.
      NS_ERROR("A client process may be trying to peek at the host's address space!");
      return false;
    }
    sharedLock = reinterpret_cast<gfxMemorySharedReadLock*>(ipcLock.get_uintptr_t());
    if (sharedLock) {
      // The corresponding AddRef is in TiledClient::GetTileDescriptor
      sharedLock.get()->Release();
    }
  }
  aTile.mSharedLock = sharedLock;
  return true;
}

bool
TiledLayerBufferComposite::UseTiles(const SurfaceDescriptorTiles& aTiles,
                                    Compositor* aCompositor,
                                    ISurfaceAllocator* aAllocator)
{
  if (mResolution != aTiles.resolution()) {
    Clear();
  }
  MOZ_ASSERT(aAllocator);
  MOZ_ASSERT(aCompositor);
  if (!aAllocator || !aCompositor) {
    return false;
  }

  if (aTiles.resolution() == 0 || IsNaN(aTiles.resolution())) {
    // There are divisions by mResolution so this protects the compositor process
    // against malicious content processes and fuzzing.
    return false;
  }

  TilesPlacement oldTiles = mTiles;
  TilesPlacement newTiles(aTiles.firstTileX(), aTiles.firstTileY(),
                          aTiles.retainedWidth(), aTiles.retainedHeight());

  const InfallibleTArray<TileDescriptor>& tileDescriptors = aTiles.tiles();

  nsTArray<TileHost> oldRetainedTiles;
  mRetainedTiles.SwapElements(oldRetainedTiles);
  mRetainedTiles.SetLength(tileDescriptors.Length());

  // Step 1, we need to unlock tiles that don't have an internal buffer after the
  // next frame where they are replaced.
  // Since we are about to replace the tiles' textures, we need to keep their locks
  // somewhere (in mPreviousSharedLock) until we composite the layer.
  for (size_t i = 0; i < oldRetainedTiles.Length(); ++i) {
    TileHost& tile = oldRetainedTiles[i];
    // It can happen that we still have a previous lock at this point,
    // if we changed a tile's front buffer (causing mSharedLock to
    // go into mPreviousSharedLock, and then did not composite that tile until
    // the next transaction, either because the tile is offscreen or because the
    // two transactions happened with no composition in between (over-production).
    tile.ReadUnlockPrevious();

    if (tile.mTextureHost && !tile.mTextureHost->HasInternalBuffer()) {
      MOZ_ASSERT(tile.mSharedLock);
      const TileIntPoint tilePosition = oldTiles.TilePosition(i);
      if (newTiles.HasTile(tilePosition)) {
        // This tile still exist in the new buffer
        tile.mPreviousSharedLock = tile.mSharedLock;
        tile.mSharedLock = nullptr;
      } else {
        // This tile does not exist anymore in the new buffer because the size
        // changed.
        tile.ReadUnlock();
      }
    }

    // By now we should not have anything in mSharedLock.
    MOZ_ASSERT(!tile.mSharedLock);
  }

  // Step 2, move the tiles in mRetainedTiles at places that correspond to where
  // they should be with the new retained with and height rather than the
  // old one.
  for (size_t i = 0; i < tileDescriptors.Length(); i++) {
    const TileIntPoint tilePosition = newTiles.TilePosition(i);
    // First, get the already existing tiles to the right place in the array,
    // and use placeholders where there was no tiles.
    if (!oldTiles.HasTile(tilePosition)) {
      mRetainedTiles[i] = GetPlaceholderTile();
    } else {
      mRetainedTiles[i] = oldRetainedTiles[oldTiles.TileIndex(tilePosition)];
      // If we hit this assertion it means we probably mixed something up in the
      // logic that tries to reuse tiles on the compositor side. It is most likely
      // benign, but we are missing some fast paths so let's try to make it not happen.
      MOZ_ASSERT(tilePosition.x == mRetainedTiles[i].x &&
                 tilePosition.y == mRetainedTiles[i].y);
    }
  }

  // It is important to remove the duplicated reference to tiles before calling
  // TextureHost::PrepareTextureSource, etc. because depending on the textures
  // ref counts we may or may not get some of the fast paths.
  oldRetainedTiles.Clear();

  // Step 3, handle the texture updates and release the copy-on-write locks.
  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    const TileDescriptor& tileDesc = tileDescriptors[i];

    TileHost& tile = mRetainedTiles[i];

    switch (tileDesc.type()) {
      case TileDescriptor::TTexturedTileDescriptor: {
        const TexturedTileDescriptor& texturedDesc = tileDesc.get_TexturedTileDescriptor();

        const TileLock& ipcLock = texturedDesc.sharedLock();
        if (!GetCopyOnWriteLock(ipcLock, tile, aAllocator)) {
          return false;
        }

        RefPtr<TextureHost> textureHost = TextureHost::AsTextureHost(
          texturedDesc.textureParent()
        );

        RefPtr<TextureHost> textureOnWhite = nullptr;
        if (texturedDesc.textureOnWhite().type() == MaybeTexture::TPTextureParent) {
          textureOnWhite = TextureHost::AsTextureHost(
            texturedDesc.textureOnWhite().get_PTextureParent()
          );
        }

        UseTileTexture(tile.mTextureHost,
                       tile.mTextureSource,
                       texturedDesc.updateRect(),
                       textureHost,
                       aCompositor);

        if (textureOnWhite) {
          UseTileTexture(tile.mTextureHostOnWhite,
                         tile.mTextureSourceOnWhite,
                         texturedDesc.updateRect(),
                         textureOnWhite,
                         aCompositor);
        } else {
          // We could still have component alpha textures from a previous frame.
          tile.mTextureSourceOnWhite = nullptr;
          tile.mTextureHostOnWhite = nullptr;
        }

        if (textureHost->HasInternalBuffer()) {
          // Now that we did the texture upload (in UseTileTexture), we can release
          // the lock.
          tile.ReadUnlock();
        }

        break;
      }
      default:
        NS_WARNING("Unrecognised tile descriptor type");
      case TileDescriptor::TPlaceholderTileDescriptor: {

        if (tile.mTextureHost) {
          tile.mTextureHost->UnbindTextureSource();
          tile.mTextureSource = nullptr;
        }
        if (tile.mTextureHostOnWhite) {
          tile.mTextureHostOnWhite->UnbindTextureSource();
          tile.mTextureSourceOnWhite = nullptr;
        }
        // we may have a previous lock, and are about to loose our reference to it.
        // It is okay to unlock it because we just destroyed the texture source.
        tile.ReadUnlockPrevious();
        tile = GetPlaceholderTile();

        break;
      }
    }
    TileIntPoint tilePosition = newTiles.TilePosition(i);
    tile.x = tilePosition.x;
    tile.y = tilePosition.y;
  }

  mTiles = newTiles;
  mValidRegion = aTiles.validRegion();
  mResolution = aTiles.resolution();
  mFrameResolution = CSSToParentLayerScale2D(aTiles.frameXResolution(),
                                             aTiles.frameYResolution());

  return true;
}

void
TiledLayerBufferComposite::Clear()
{
  for (TileHost& tile : mRetainedTiles) {
    tile.ReadUnlock();
    tile.ReadUnlockPrevious();
  }
  mRetainedTiles.Clear();
  mTiles.mFirst = TileIntPoint();
  mTiles.mSize = TileIntSize();
  mValidRegion = nsIntRegion();
  mPaintedRegion = nsIntRegion();
  mResolution = 1.0;
}

void
TiledContentHost::Composite(LayerComposite* aLayer,
                            EffectChain& aEffectChain,
                            float aOpacity,
                            const gfx::Matrix4x4& aTransform,
                            const gfx::Filter& aFilter,
                            const gfx::Rect& aClipRect,
                            const nsIntRegion* aVisibleRegion /* = nullptr */)
{
  MOZ_ASSERT(mCompositor);
  // Reduce the opacity of the low-precision buffer to make it a
  // little more subtle and less jarring. In particular, text
  // rendered at low-resolution and scaled tends to look pretty
  // heavy and this helps mitigate that. When we reduce the opacity
  // we also make sure to draw the background color behind the
  // reduced-opacity tile so that content underneath doesn't show
  // through.
  // However, in cases where the background is transparent, or the layer
  // already has some opacity, we want to skip this behaviour. Otherwise
  // we end up changing the expected overall transparency of the content,
  // and it just looks wrong.
  gfxRGBA backgroundColor(0);
  if (aOpacity == 1.0f && gfxPrefs::LowPrecisionOpacity() < 1.0f) {
    // Background colors are only stored on scrollable layers. Grab
    // the one from the nearest scrollable ancestor layer.
    for (LayerMetricsWrapper ancestor(GetLayer(), LayerMetricsWrapper::StartAt::BOTTOM); ancestor; ancestor = ancestor.GetParent()) {
      if (ancestor.Metrics().IsScrollable()) {
        backgroundColor = ancestor.Metrics().GetBackgroundColor();
        break;
      }
    }
  }
  float lowPrecisionOpacityReduction =
        (aOpacity == 1.0f && backgroundColor.a == 1.0f)
        ? gfxPrefs::LowPrecisionOpacity() : 1.0f;

  nsIntRegion tmpRegion;
  const nsIntRegion* renderRegion = aVisibleRegion;
#ifndef MOZ_IGNORE_PAINT_WILL_RESAMPLE
  if (PaintWillResample()) {
    // If we're resampling, then the texture image will contain exactly the
    // entire visible region's bounds, and we should draw it all in one quad
    // to avoid unexpected aliasing.
    tmpRegion = aVisibleRegion->GetBounds();
    renderRegion = &tmpRegion;
  }
#endif

  // Render the low and high precision buffers.
  RenderLayerBuffer(mLowPrecisionTiledBuffer,
                    lowPrecisionOpacityReduction < 1.0f ? &backgroundColor : nullptr,
                    aEffectChain, lowPrecisionOpacityReduction * aOpacity,
                    aFilter, aClipRect, *renderRegion, aTransform);
  RenderLayerBuffer(mTiledBuffer, nullptr, aEffectChain, aOpacity, aFilter,
                    aClipRect, *renderRegion, aTransform);
}


void
TiledContentHost::RenderTile(TileHost& aTile,
                             EffectChain& aEffectChain,
                             float aOpacity,
                             const gfx::Matrix4x4& aTransform,
                             const gfx::Filter& aFilter,
                             const gfx::Rect& aClipRect,
                             const nsIntRegion& aScreenRegion,
                             const IntPoint& aTextureOffset,
                             const IntSize& aTextureBounds,
                             const gfx::Rect& aVisibleRect)
{
  MOZ_ASSERT(!aTile.IsPlaceholderTile());

  AutoLockTextureHost autoLock(aTile.mTextureHost);
  AutoLockTextureHost autoLockOnWhite(aTile.mTextureHostOnWhite);
  if (autoLock.Failed() ||
      autoLockOnWhite.Failed()) {
    NS_WARNING("Failed to lock tile");
    return;
  }

  if (!aTile.mTextureHost->BindTextureSource(aTile.mTextureSource)) {
    return;
  }

  if (aTile.mTextureHostOnWhite && !aTile.mTextureHostOnWhite->BindTextureSource(aTile.mTextureSourceOnWhite)) {
    return;
  }

  RefPtr<TexturedEffect> effect =
    CreateTexturedEffect(aTile.mTextureSource,
                         aTile.mTextureSourceOnWhite,
                         aFilter,
                         true,
                         aTile.mTextureHost->GetRenderState());
  if (!effect) {
    return;
  }

  aEffectChain.mPrimaryEffect = effect;

  nsIntRegionRectIterator it(aScreenRegion);
  for (const IntRect* rect = it.Next(); rect != nullptr; rect = it.Next()) {
    Rect graphicsRect(rect->x, rect->y, rect->width, rect->height);
    Rect textureRect(rect->x - aTextureOffset.x, rect->y - aTextureOffset.y,
                     rect->width, rect->height);

    effect->mTextureCoords = Rect(textureRect.x / aTextureBounds.width,
                                  textureRect.y / aTextureBounds.height,
                                  textureRect.width / aTextureBounds.width,
                                  textureRect.height / aTextureBounds.height);
    mCompositor->DrawQuad(graphicsRect, aClipRect, aEffectChain, aOpacity, aTransform, aVisibleRect);
  }
  DiagnosticFlags flags = DiagnosticFlags::CONTENT | DiagnosticFlags::TILE;
  if (aTile.mTextureHostOnWhite) {
    flags |= DiagnosticFlags::COMPONENT_ALPHA;
  }
  mCompositor->DrawDiagnostics(flags,
                               aScreenRegion, aClipRect, aTransform, mFlashCounter);
  aTile.ReadUnlockPrevious();
}

void
TiledContentHost::RenderLayerBuffer(TiledLayerBufferComposite& aLayerBuffer,
                                    const gfxRGBA* aBackgroundColor,
                                    EffectChain& aEffectChain,
                                    float aOpacity,
                                    const gfx::Filter& aFilter,
                                    const gfx::Rect& aClipRect,
                                    nsIntRegion aVisibleRegion,
                                    gfx::Matrix4x4 aTransform)
{
  if (!mCompositor) {
    NS_WARNING("Can't render tiled content host - no compositor");
    return;
  }
  float resolution = aLayerBuffer.GetResolution();
  gfx::Size layerScale(1, 1);

  // We assume that the current frame resolution is the one used in our high
  // precision layer buffer. Compensate for a changing frame resolution when
  // rendering the low precision buffer.
  if (aLayerBuffer.GetFrameResolution() != mTiledBuffer.GetFrameResolution()) {
    const CSSToParentLayerScale2D& layerResolution = aLayerBuffer.GetFrameResolution();
    const CSSToParentLayerScale2D& localResolution = mTiledBuffer.GetFrameResolution();
    layerScale.width = layerResolution.xScale / localResolution.xScale;
    layerScale.height = layerResolution.yScale / localResolution.yScale;
    aVisibleRegion.ScaleRoundOut(layerScale.width, layerScale.height);
  }

  // Make sure we don't render at low resolution where we have valid high
  // resolution content, to avoid overdraw and artifacts with semi-transparent
  // layers.
  nsIntRegion maskRegion;
  if (resolution != mTiledBuffer.GetResolution()) {
    maskRegion = mTiledBuffer.GetValidRegion();
    // XXX This should be ScaleRoundIn, but there is no such function on
    //     nsIntRegion.
    maskRegion.ScaleRoundOut(layerScale.width, layerScale.height);
  }

  // Make sure the resolution and difference in frame resolution are accounted
  // for in the layer transform.
  aTransform.PreScale(1/(resolution * layerScale.width),
                      1/(resolution * layerScale.height), 1);

  DiagnosticFlags componentAlphaDiagnostic = DiagnosticFlags::NO_DIAGNOSTIC;

  nsIntRegion compositeRegion = aLayerBuffer.GetValidRegion();
  compositeRegion.AndWith(aVisibleRegion);
  compositeRegion.SubOut(maskRegion);

  IntRect visibleRect = aVisibleRegion.GetBounds();

  if (compositeRegion.IsEmpty()) {
    return;
  }

  if (aBackgroundColor) {
    nsIntRegion backgroundRegion = compositeRegion;
    backgroundRegion.ScaleRoundOut(resolution, resolution);
    EffectChain effect;
    effect.mPrimaryEffect = new EffectSolidColor(ToColor(*aBackgroundColor));
    nsIntRegionRectIterator it(backgroundRegion);
    for (const IntRect* rect = it.Next(); rect != nullptr; rect = it.Next()) {
      Rect graphicsRect(rect->x, rect->y, rect->width, rect->height);
      mCompositor->DrawQuad(graphicsRect, aClipRect, effect, 1.0, aTransform);
    }
  }

  for (size_t i = 0; i < aLayerBuffer.GetTileCount(); ++i) {
    TileHost& tile = aLayerBuffer.GetTile(i);
    if (tile.IsPlaceholderTile()) {
      continue;
    }

    TileIntPoint tilePosition = aLayerBuffer.GetPlacement().TilePosition(i);
    // A sanity check that catches a lot of mistakes.
    MOZ_ASSERT(tilePosition.x == tile.x && tilePosition.y == tile.y);

    IntPoint tileOffset = aLayerBuffer.GetTileOffset(tilePosition);
    nsIntRegion tileDrawRegion = IntRect(tileOffset, aLayerBuffer.GetScaledTileSize());
    tileDrawRegion.AndWith(compositeRegion);

    if (tileDrawRegion.IsEmpty()) {
      continue;
    }

    tileDrawRegion.ScaleRoundOut(resolution, resolution);
    RenderTile(tile, aEffectChain, aOpacity,
               aTransform, aFilter, aClipRect, tileDrawRegion,
               tileOffset * resolution, aLayerBuffer.GetTileSize(),
               gfx::Rect(visibleRect.x, visibleRect.y,
                         visibleRect.width, visibleRect.height));
    if (tile.mTextureHostOnWhite) {
      componentAlphaDiagnostic = DiagnosticFlags::COMPONENT_ALPHA;
    }
  }

  gfx::Rect rect(visibleRect.x, visibleRect.y,
                 visibleRect.width, visibleRect.height);
  GetCompositor()->DrawDiagnostics(DiagnosticFlags::CONTENT | componentAlphaDiagnostic,
                                   rect, aClipRect, aTransform, mFlashCounter);
}

void
TiledContentHost::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  aStream << aPrefix;
  aStream << nsPrintfCString("TiledContentHost (0x%p)", this).get();

  if (gfxPrefs::LayersDumpTexture() || profiler_feature_active("layersdump")) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";

    Dump(aStream, pfx.get(), false);
  }
}

void
TiledContentHost::Dump(std::stringstream& aStream,
                       const char* aPrefix,
                       bool aDumpHtml)
{
  mTiledBuffer.Dump(aStream, aPrefix, aDumpHtml);
}

} // namespace
} // namespace
