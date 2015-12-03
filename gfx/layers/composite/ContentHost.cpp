/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ContentHost.h"
#include "LayersLogging.h"              // for AppendToString
#include "gfx2DGlue.h"                  // for ContentForFormat
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/Effects.h"     // for TexturedEffect, Effect, etc
#include "mozilla/layers/LayersMessages.h"  // for ThebesBufferData
#include "nsAString.h"
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsString.h"                   // for nsAutoCString
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL

namespace mozilla {
using namespace gfx;

namespace layers {

ContentHostBase::ContentHostBase(const TextureInfo& aTextureInfo)
  : ContentHost(aTextureInfo)
  , mInitialised(false)
{}

ContentHostBase::~ContentHostBase()
{
}

void
ContentHostTexture::Composite(LayerComposite* aLayer,
                              EffectChain& aEffectChain,
                              float aOpacity,
                              const gfx::Matrix4x4& aTransform,
                              const Filter& aFilter,
                              const Rect& aClipRect,
                              const nsIntRegion* aVisibleRegion)
{
  NS_ASSERTION(aVisibleRegion, "Requires a visible region");

  AutoLockCompositableHost lock(this);
  if (lock.Failed()) {
    return;
  }

  if (!mTextureHost->BindTextureSource(mTextureSource)) {
    return;
  }
  MOZ_ASSERT(mTextureSource.get());

  if (!mTextureHostOnWhite) {
    mTextureSourceOnWhite = nullptr;
  }
  if (mTextureHostOnWhite && !mTextureHostOnWhite->BindTextureSource(mTextureSourceOnWhite)) {
    return;
  }

  RefPtr<TexturedEffect> effect = CreateTexturedEffect(mTextureSource.get(),
                                                       mTextureSourceOnWhite.get(),
                                                       aFilter, true,
                                                       GetRenderState());
  if (!effect) {
    return;
  }

  aEffectChain.mPrimaryEffect = effect;

  nsIntRegion tmpRegion;
  const nsIntRegion* renderRegion;
#ifndef MOZ_IGNORE_PAINT_WILL_RESAMPLE
  if (PaintWillResample()) {
    // If we're resampling, then the texture image will contain exactly the
    // entire visible region's bounds, and we should draw it all in one quad
    // to avoid unexpected aliasing.
    tmpRegion = aVisibleRegion->GetBounds();
    renderRegion = &tmpRegion;
  } else {
    renderRegion = aVisibleRegion;
  }
#else
  renderRegion = aVisibleRegion;
#endif

  nsIntRegion region(*renderRegion);
  nsIntPoint origin = GetOriginOffset();
  // translate into TexImage space, buffer origin might not be at texture (0,0)
  region.MoveBy(-origin);

  // Figure out the intersecting draw region
  gfx::IntSize texSize = mTextureSource->GetSize();
  IntRect textureRect = IntRect(0, 0, texSize.width, texSize.height);
  textureRect.MoveBy(region.GetBounds().TopLeft());
  nsIntRegion subregion;
  subregion.And(region, textureRect);
  if (subregion.IsEmpty()) {
    // Region is empty, nothing to draw
    return;
  }

  nsIntRegion screenRects;
  nsIntRegion regionRects;

  // Collect texture/screen coordinates for drawing
  nsIntRegionRectIterator iter(subregion);
  while (const IntRect* iterRect = iter.Next()) {
    IntRect regionRect = *iterRect;
    IntRect screenRect = regionRect;
    screenRect.MoveBy(origin);

    screenRects.Or(screenRects, screenRect);
    regionRects.Or(regionRects, regionRect);
  }

  BigImageIterator* bigImgIter = mTextureSource->AsBigImageIterator();
  BigImageIterator* iterOnWhite = nullptr;
  if (bigImgIter) {
    bigImgIter->BeginBigImageIteration();
  }

  if (mTextureSourceOnWhite) {
    iterOnWhite = mTextureSourceOnWhite->AsBigImageIterator();
    MOZ_ASSERT(!bigImgIter || bigImgIter->GetTileCount() == iterOnWhite->GetTileCount(),
               "Tile count mismatch on component alpha texture");
    if (iterOnWhite) {
      iterOnWhite->BeginBigImageIteration();
    }
  }

  bool usingTiles = (bigImgIter && bigImgIter->GetTileCount() > 1);
  do {
    if (iterOnWhite) {
      MOZ_ASSERT(iterOnWhite->GetTileRect() == bigImgIter->GetTileRect(),
                 "component alpha textures should be the same size.");
    }

    IntRect texRect = bigImgIter ? bigImgIter->GetTileRect()
                                 : IntRect(0, 0,
                                             texSize.width,
                                             texSize.height);

    // Draw texture. If we're using tiles, we do repeating manually, as texture
    // repeat would cause each individual tile to repeat instead of the
    // compound texture as a whole. This involves drawing at most 4 sections,
    // 2 for each axis that has texture repeat.
    for (int y = 0; y < (usingTiles ? 2 : 1); y++) {
      for (int x = 0; x < (usingTiles ? 2 : 1); x++) {
        IntRect currentTileRect(texRect);
        currentTileRect.MoveBy(x * texSize.width, y * texSize.height);

        nsIntRegionRectIterator screenIter(screenRects);
        nsIntRegionRectIterator regionIter(regionRects);

        const IntRect* screenRect;
        const IntRect* regionRect;
        while ((screenRect = screenIter.Next()) &&
               (regionRect = regionIter.Next())) {
          IntRect tileScreenRect(*screenRect);
          IntRect tileRegionRect(*regionRect);

          // When we're using tiles, find the intersection between the tile
          // rect and this region rect. Tiling is then handled by the
          // outer for-loops and modifying the tile rect.
          if (usingTiles) {
            tileScreenRect.MoveBy(-origin);
            tileScreenRect = tileScreenRect.Intersect(currentTileRect);
            tileScreenRect.MoveBy(origin);

            if (tileScreenRect.IsEmpty())
              continue;

            tileRegionRect = regionRect->Intersect(currentTileRect);
            tileRegionRect.MoveBy(-currentTileRect.TopLeft());
          }
          gfx::Rect rect(tileScreenRect.x, tileScreenRect.y,
                         tileScreenRect.width, tileScreenRect.height);

          effect->mTextureCoords = Rect(Float(tileRegionRect.x) / texRect.width,
                                        Float(tileRegionRect.y) / texRect.height,
                                        Float(tileRegionRect.width) / texRect.width,
                                        Float(tileRegionRect.height) / texRect.height);
          GetCompositor()->DrawQuad(rect, aClipRect, aEffectChain, aOpacity, aTransform);
          if (usingTiles) {
            DiagnosticFlags diagnostics = DiagnosticFlags::CONTENT | DiagnosticFlags::BIGIMAGE;
            if (iterOnWhite) {
              diagnostics |= DiagnosticFlags::COMPONENT_ALPHA;
            }
            GetCompositor()->DrawDiagnostics(diagnostics, rect, aClipRect,
                                             aTransform, mFlashCounter);
          }
        }
      }
    }

    if (iterOnWhite) {
      iterOnWhite->NextTile();
    }
  } while (usingTiles && bigImgIter->NextTile());

  if (bigImgIter) {
    bigImgIter->EndBigImageIteration();
  }
  if (iterOnWhite) {
    iterOnWhite->EndBigImageIteration();
  }

  DiagnosticFlags diagnostics = DiagnosticFlags::CONTENT;
  if (iterOnWhite) {
    diagnostics |= DiagnosticFlags::COMPONENT_ALPHA;
  }
  GetCompositor()->DrawDiagnostics(diagnostics, nsIntRegion(mBufferRect), aClipRect,
                                   aTransform, mFlashCounter);
}

void
ContentHostTexture::UseTextureHost(const nsTArray<TimedTexture>& aTextures)
{
  ContentHostBase::UseTextureHost(aTextures);
  MOZ_ASSERT(aTextures.Length() == 1);
  const TimedTexture& t = aTextures[0];
  MOZ_ASSERT(t.mPictureRect.IsEqualInterior(
      nsIntRect(nsIntPoint(0, 0), nsIntSize(t.mTexture->GetSize()))),
      "Only default picture rect supported");

  if (t.mTexture != mTextureHost) {
    mReceivedNewHost = true;
  }

  mTextureHost = t.mTexture;
  mTextureHostOnWhite = nullptr;
  mTextureSourceOnWhite = nullptr;
  if (mTextureHost) {
    mTextureHost->PrepareTextureSource(mTextureSource);
  }
}

void
ContentHostTexture::UseComponentAlphaTextures(TextureHost* aTextureOnBlack,
                                              TextureHost* aTextureOnWhite)
{
  ContentHostBase::UseComponentAlphaTextures(aTextureOnBlack, aTextureOnWhite);
  mTextureHost = aTextureOnBlack;
  mTextureHostOnWhite = aTextureOnWhite;
  if (mTextureHost) {
    mTextureHost->PrepareTextureSource(mTextureSource);
  }
  if (mTextureHostOnWhite) {
    mTextureHostOnWhite->PrepareTextureSource(mTextureSourceOnWhite);
  }
}

void
ContentHostTexture::SetCompositor(Compositor* aCompositor)
{
  ContentHostBase::SetCompositor(aCompositor);
  if (mTextureHost) {
    mTextureHost->SetCompositor(aCompositor);
  }
  if (mTextureHostOnWhite) {
    mTextureHostOnWhite->SetCompositor(aCompositor);
  }
}

void
ContentHostTexture::Dump(std::stringstream& aStream,
                         const char* aPrefix,
                         bool aDumpHtml)
{
#ifdef MOZ_DUMP_PAINTING
  if (aDumpHtml) {
    aStream << "<ul>";
  }
  if (mTextureHost) {
    aStream << aPrefix;
    if (aDumpHtml) {
      aStream << "<li> <a href=";
    } else {
      aStream << "Front buffer: ";
    }
    DumpTextureHost(aStream, mTextureHost);
    if (aDumpHtml) {
      aStream << "> Front buffer </a></li> ";
    } else {
      aStream << "\n";
    }
  }
  if (mTextureHostOnWhite) {
    aStream << aPrefix;
    if (aDumpHtml) {
      aStream << "<li> <a href=";
    } else {
      aStream << "Front buffer on white: ";
    }
    DumpTextureHost(aStream, mTextureHostOnWhite);
    if (aDumpHtml) {
      aStream << "> Front buffer on white </a> </li> ";
    } else {
      aStream << "\n";
    }
  }
  if (aDumpHtml) {
    aStream << "</ul>";
  }
#endif
}

static inline void
AddWrappedRegion(const nsIntRegion& aInput, nsIntRegion& aOutput,
                 const IntSize& aSize, const nsIntPoint& aShift)
{
  nsIntRegion tempRegion;
  tempRegion.And(IntRect(aShift, aSize), aInput);
  tempRegion.MoveBy(-aShift);
  aOutput.Or(aOutput, tempRegion);
}

bool
ContentHostSingleBuffered::UpdateThebes(const ThebesBufferData& aData,
                                        const nsIntRegion& aUpdated,
                                        const nsIntRegion& aOldValidRegionBack,
                                        nsIntRegion* aUpdatedRegionBack)
{
  aUpdatedRegionBack->SetEmpty();

  if (!mTextureHost) {
    mInitialised = false;
    return true; // FIXME should we return false? Returning true for now
  }              // to preserve existing behavior of NOT causing IPC errors.

  // updated is in screen coordinates. Convert it to buffer coordinates.
  nsIntRegion destRegion(aUpdated);

  if (mReceivedNewHost) {
    destRegion.Or(destRegion, aOldValidRegionBack);
    mReceivedNewHost = false;
  }
  destRegion.MoveBy(-aData.rect().TopLeft());

  if (!aData.rect().Contains(aUpdated.GetBounds()) ||
      aData.rotation().x > aData.rect().width ||
      aData.rotation().y > aData.rect().height) {
    NS_ERROR("Invalid update data");
    return false;
  }

  // destRegion is now in logical coordinates relative to the buffer, but we
  // need to account for rotation. We do that by moving the region to the
  // rotation offset and then wrapping any pixels that extend off the
  // bottom/right edges.

  // Shift to the rotation point
  destRegion.MoveBy(aData.rotation());

  IntSize bufferSize = aData.rect().Size();

  // Select only the pixels that are still within the buffer.
  nsIntRegion finalRegion;
  finalRegion.And(IntRect(IntPoint(), bufferSize), destRegion);

  // For each of the overlap areas (right, bottom-right, bottom), select those
  // pixels and wrap them around to the opposite edge of the buffer rect.
  AddWrappedRegion(destRegion, finalRegion, bufferSize, nsIntPoint(aData.rect().width, 0));
  AddWrappedRegion(destRegion, finalRegion, bufferSize, nsIntPoint(aData.rect().width, aData.rect().height));
  AddWrappedRegion(destRegion, finalRegion, bufferSize, nsIntPoint(0, aData.rect().height));

  MOZ_ASSERT(IntRect(0, 0, aData.rect().width, aData.rect().height).Contains(finalRegion.GetBounds()));

  mTextureHost->Updated(&finalRegion);
  if (mTextureHostOnWhite) {
    mTextureHostOnWhite->Updated(&finalRegion);
  }
  mInitialised = true;

  mBufferRect = aData.rect();
  mBufferRotation = aData.rotation();

  return true;
}

bool
ContentHostDoubleBuffered::UpdateThebes(const ThebesBufferData& aData,
                                        const nsIntRegion& aUpdated,
                                        const nsIntRegion& aOldValidRegionBack,
                                        nsIntRegion* aUpdatedRegionBack)
{
  if (!mTextureHost) {
    mInitialised = false;

    *aUpdatedRegionBack = aUpdated;
    return true;
  }

  // We don't need to calculate an update region because we assume that if we
  // are using double buffering then we have render-to-texture and thus no
  // upload to do.
  mTextureHost->Updated();
  if (mTextureHostOnWhite) {
    mTextureHostOnWhite->Updated();
  }
  mInitialised = true;

  mBufferRect = aData.rect();
  mBufferRotation = aData.rotation();

  *aUpdatedRegionBack = aUpdated;

  // Save the current valid region of our front buffer, because if
  // we're double buffering, it's going to be the valid region for the
  // next back buffer sent back to the renderer.
  //
  // NB: we rely here on the fact that mValidRegion is initialized to
  // empty, and that the first time Swap() is called we don't have a
  // valid front buffer that we're going to return to content.
  mValidRegionForNextBackBuffer = aOldValidRegionBack;

  return true;
}

void
ContentHostTexture::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  aStream << aPrefix;
  aStream << nsPrintfCString("ContentHost (0x%p)", this).get();

  AppendToString(aStream, mBufferRect, " [buffer-rect=", "]");
  AppendToString(aStream, mBufferRotation, " [buffer-rotation=", "]");
  if (PaintWillResample()) {
    aStream << " [paint-will-resample]";
  }

  if (mTextureHost) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";

    aStream << "\n";
    mTextureHost->PrintInfo(aStream, pfx.get());
  }
}


LayerRenderState
ContentHostTexture::GetRenderState()
{
  if (!mTextureHost) {
    return LayerRenderState();
  }

  LayerRenderState result = mTextureHost->GetRenderState();

  if (mBufferRotation != nsIntPoint()) {
    result.mFlags |= LayerRenderStateFlags::BUFFER_ROTATION;
  }
  result.SetOffset(GetOriginOffset());
  return result;
}

already_AddRefed<TexturedEffect>
ContentHostTexture::GenEffect(const gfx::Filter& aFilter)
{
  if (!mTextureHost) {
    return nullptr;
  }
  if (!mTextureHost->BindTextureSource(mTextureSource)) {
    return nullptr;
  }
  if (!mTextureHostOnWhite) {
    mTextureSourceOnWhite = nullptr;
  }
  if (mTextureHostOnWhite && !mTextureHostOnWhite->BindTextureSource(mTextureSourceOnWhite)) {
    return nullptr;
  }
  return CreateTexturedEffect(mTextureSource.get(),
                              mTextureSourceOnWhite.get(),
                              aFilter, true,
                              GetRenderState());
}

already_AddRefed<gfx::DataSourceSurface>
ContentHostTexture::GetAsSurface()
{
  if (!mTextureHost) {
    return nullptr;
  }

  return mTextureHost->GetAsSurface();
}


} // namespace layers
} // namespace mozilla
