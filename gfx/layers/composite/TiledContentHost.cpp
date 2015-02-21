/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TiledContentHost.h"
#include "PaintedLayerComposite.h"      // for PaintedLayerComposite
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/Effects.h"     // for TexturedEffect, Effect, etc
#include "mozilla/layers/LayerMetricsWrapper.h" // for LayerMetricsWrapper
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL
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
  , mIsValid(false)
{}

/* static */ void
TiledLayerBufferComposite::RecycleCallback(TextureHost* textureHost, void* aClosure)
{
  textureHost->CompositorRecycle();
}

TiledLayerBufferComposite::TiledLayerBufferComposite(ISurfaceAllocator* aAllocator,
                                                     const SurfaceDescriptorTiles& aDescriptor,
                                                     const nsIntRegion& aOldPaintedRegion,
                                                     Compositor* aCompositor)
{
  mIsValid = true;
  mHasDoubleBufferedTiles = false;
  mValidRegion = aDescriptor.validRegion();
  mPaintedRegion = aDescriptor.paintedRegion();
  mRetainedWidth = aDescriptor.retainedWidth();
  mRetainedHeight = aDescriptor.retainedHeight();
  mResolution = aDescriptor.resolution();
  mFrameResolution = CSSToParentLayerScale(aDescriptor.frameResolution());
  if (mResolution == 0 || IsNaN(mResolution)) {
    // There are divisions by mResolution so this protects the compositor process
    // against malicious content processes and fuzzing.
    mIsValid = false;
    return;
  }

  // Combine any valid content that wasn't already uploaded
  nsIntRegion oldPaintedRegion(aOldPaintedRegion);
  oldPaintedRegion.And(oldPaintedRegion, mValidRegion);
  mPaintedRegion.Or(mPaintedRegion, oldPaintedRegion);

  bool isSameProcess = aAllocator->IsSameProcess();

  const InfallibleTArray<TileDescriptor>& tiles = aDescriptor.tiles();
  for(size_t i = 0; i < tiles.Length(); i++) {
    CompositableTextureHostRef texture;
    CompositableTextureHostRef textureOnWhite;
    const TileDescriptor& tileDesc = tiles[i];
    switch (tileDesc.type()) {
      case TileDescriptor::TTexturedTileDescriptor : {
        texture = TextureHost::AsTextureHost(tileDesc.get_TexturedTileDescriptor().textureParent());
        MaybeTexture onWhite = tileDesc.get_TexturedTileDescriptor().textureOnWhite();
        if (onWhite.type() == MaybeTexture::TPTextureParent) {
          textureOnWhite = TextureHost::AsTextureHost(onWhite.get_PTextureParent());
        }
        const TileLock& ipcLock = tileDesc.get_TexturedTileDescriptor().sharedLock();
        nsRefPtr<gfxSharedReadLock> sharedLock;
        if (ipcLock.type() == TileLock::TShmemSection) {
          sharedLock = gfxShmSharedReadLock::Open(aAllocator, ipcLock.get_ShmemSection());
        } else {
          if (!isSameProcess) {
            // Trying to use a memory based lock instead of a shmem based one in
            // the cross-process case is a bad security violation.
            NS_ERROR("A client process may be trying to peek at the host's address space!");
            // This tells the TiledContentHost that deserialization failed so that
            // it can propagate the error.
            mIsValid = false;

            mRetainedTiles.Clear();
            return;
          }
          sharedLock = reinterpret_cast<gfxMemorySharedReadLock*>(ipcLock.get_uintptr_t());
          if (sharedLock) {
            // The corresponding AddRef is in TiledClient::GetTileDescriptor
            sharedLock.get()->Release();
          }
        }

        CompositableTextureSourceRef textureSource;
        CompositableTextureSourceRef textureSourceOnWhite;
        if (texture) {
          texture->SetCompositor(aCompositor);
          texture->PrepareTextureSource(textureSource);
        }
        if (textureOnWhite) {
          textureOnWhite->SetCompositor(aCompositor);
          textureOnWhite->PrepareTextureSource(textureSourceOnWhite);
        }
        mRetainedTiles.AppendElement(TileHost(sharedLock,
                                              texture.get(),
                                              textureOnWhite.get(),
                                              textureSource.get(),
                                              textureSourceOnWhite.get()));
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
    mRetainedTiles[i].mTextureHostOnWhite = nullptr;
    mRetainedTiles[i].mTextureSource = nullptr;
    mRetainedTiles[i].mTextureSourceOnWhite = nullptr;
  }
}

void
TiledLayerBufferComposite::Upload()
{
  if(!IsValid()) {
    return;
  }
  // The TextureClients were created with the TextureFlags::IMMEDIATE_UPLOAD flag,
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

  MOZ_ASSERT(aTile.mTextureHost->GetFlags() & TextureFlags::IMMEDIATE_UPLOAD);

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  MOZ_ASSERT(!aTile.mTextureHostOnWhite);
  // We possibly upload the entire texture contents here. This is a purposeful
  // decision, as sub-image upload can often be slow and/or unreliable, but
  // we may want to reevaluate this in the future.
  // For !HasInternalBuffer() textures, this is likely a no-op.
  aTile.mTextureHost->Updated(nullptr);
#else
  nsIntRegion tileUpdated = aDirtyRect.MovedBy(-aTileOrigin);
  aTile.mTextureHost->Updated(&tileUpdated);
  if (aTile.mTextureHostOnWhite) {
    aTile.mTextureHostOnWhite->Updated(&tileUpdated);
  }
#endif

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
    if (mRetainedTiles[i].mTextureHostOnWhite) {
      mRetainedTiles[i].mTextureHostOnWhite->SetCompositor(aCompositor);
    }
  }
}

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
void
TiledLayerBufferComposite::SetReleaseFence(const android::sp<android::Fence>& aReleaseFence)
{
  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    if (!mRetainedTiles[i].mTextureHost) {
      continue;
    }
    TextureHostOGL* texture = mRetainedTiles[i].mTextureHost->AsHostOGL();
    if (!texture) {
      continue;
    }
    texture->SetReleaseFence(new android::Fence(aReleaseFence->dup()));
  }
}
#endif

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
}

void
TiledContentHost::Detach(Layer* aLayer,
                         AttachFlags aFlags /* = NO_FLAGS */)
{
  if (!mKeepAttached || aLayer == mLayer || aFlags & FORCE_DETACH) {

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

    mTiledBuffer = TiledLayerBufferComposite();
    mLowPrecisionTiledBuffer = TiledLayerBufferComposite();
    mOldTiledBuffer = TiledLayerBufferComposite();
    mOldLowPrecisionTiledBuffer = TiledLayerBufferComposite();
  }
  CompositableHost::Detach(aLayer,aFlags);
}

bool
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
      TiledLayerBufferComposite(aAllocator,
                                aTiledDescriptor,
                                mLowPrecisionTiledBuffer.GetPaintedRegion(),
                                mCompositor);
    if (!mLowPrecisionTiledBuffer.IsValid()) {
      // Something bad happened. Stop here, return false (kills the child process),
      // and do as little work as possible on the received data as it appears
      // to be corrupted.
      mPendingLowPrecisionUpload = false;
      mPendingUpload = false;
      return false;
    }
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
    mTiledBuffer = TiledLayerBufferComposite(aAllocator,
                                             aTiledDescriptor,
                                             mTiledBuffer.GetPaintedRegion(),
                                             mCompositor);
    if (!mTiledBuffer.IsValid()) {
      // Something bad happened. Stop here, return false (kills the child process),
      // and do as little work as possible on the received data as it appears
      // to be corrupted.
      mPendingLowPrecisionUpload = false;
      mPendingUpload = false;
      return false;
    }
  }
  return true;
}

void
TiledContentHost::Composite(EffectChain& aEffectChain,
                            float aOpacity,
                            const gfx::Matrix4x4& aTransform,
                            const gfx::Filter& aFilter,
                            const gfx::Rect& aClipRect,
                            const nsIntRegion* aVisibleRegion /* = nullptr */)
{
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
                             const gfxRGBA* aBackgroundColor,
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

  if (aBackgroundColor) {
    aEffectChain.mPrimaryEffect = new EffectSolidColor(ToColor(*aBackgroundColor));
    nsIntRegionRectIterator it(aScreenRegion);
    for (const nsIntRect* rect = it.Next(); rect != nullptr; rect = it.Next()) {
      Rect graphicsRect(rect->x, rect->y, rect->width, rect->height);
      mCompositor->DrawQuad(graphicsRect, aClipRect, aEffectChain, 1.0, aTransform);
    }
  }

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

  RefPtr<TexturedEffect> effect = CreateTexturedEffect(aTile.mTextureSource, aTile.mTextureSourceOnWhite, aFilter, true);
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
  DiagnosticFlags flags = DiagnosticFlags::CONTENT | DiagnosticFlags::TILE;
  if (aTile.mTextureHostOnWhite) {
    flags |= DiagnosticFlags::COMPONENT_ALPHA;
  }
  mCompositor->DrawDiagnostics(flags,
                               aScreenRegion, aClipRect, aTransform, mFlashCounter);
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
    const CSSToParentLayerScale& layerResolution = aLayerBuffer.GetFrameResolution();
    const CSSToParentLayerScale& localResolution = mTiledBuffer.GetFrameResolution();
    layerScale.width = layerScale.height = layerResolution.scale / localResolution.scale;
    aVisibleRegion.ScaleRoundOut(layerScale.width, layerScale.height);
  }

  // If we're drawing the low precision buffer, make sure the high precision
  // buffer is masked out to avoid overdraw and rendering artifacts with
  // non-opaque layers.
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

  uint32_t rowCount = 0;
  uint32_t tileX = 0;
  nsIntRect visibleRect = aVisibleRegion.GetBounds();
  gfx::IntSize scaledTileSize = aLayerBuffer.GetScaledTileSize();
  for (int32_t x = visibleRect.x; x < visibleRect.x + visibleRect.width;) {
    rowCount++;
    int32_t tileStartX = aLayerBuffer.GetTileStart(x, scaledTileSize.width);
    int32_t w = scaledTileSize.width - tileStartX;
    if (x + w > visibleRect.x + visibleRect.width) {
      w = visibleRect.x + visibleRect.width - x;
    }
    int tileY = 0;
    for (int32_t y = visibleRect.y; y < visibleRect.y + visibleRect.height;) {
      int32_t tileStartY = aLayerBuffer.GetTileStart(y, scaledTileSize.height);
      int32_t h = scaledTileSize.height - tileStartY;
      if (y + h > visibleRect.y + visibleRect.height) {
        h = visibleRect.y + visibleRect.height - y;
      }

      TileHost tileTexture = aLayerBuffer.
        GetTile(nsIntPoint(aLayerBuffer.RoundDownToTileEdge(x, scaledTileSize.width),
                           aLayerBuffer.RoundDownToTileEdge(y, scaledTileSize.height)));
      if (tileTexture != aLayerBuffer.GetPlaceholderTile()) {
        nsIntRegion tileDrawRegion;
        tileDrawRegion.And(nsIntRect(x, y, w, h), aLayerBuffer.GetValidRegion());
        tileDrawRegion.And(tileDrawRegion, aVisibleRegion);
        tileDrawRegion.Sub(tileDrawRegion, maskRegion);

        if (!tileDrawRegion.IsEmpty()) {
          tileDrawRegion.ScaleRoundOut(resolution, resolution);
          nsIntPoint tileOffset((x - tileStartX) * resolution,
                                (y - tileStartY) * resolution);
          gfx::IntSize tileSize = aLayerBuffer.GetTileSize();
          RenderTile(tileTexture, aBackgroundColor, aEffectChain, aOpacity, aTransform,
                     aFilter, aClipRect, tileDrawRegion, tileOffset,
                     nsIntSize(tileSize.width, tileSize.height));
          if (tileTexture.mTextureHostOnWhite) {
            componentAlphaDiagnostic = DiagnosticFlags::COMPONENT_ALPHA;
          }
        }
      }
      tileY++;
      y += h;
    }
    tileX++;
    x += w;
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
