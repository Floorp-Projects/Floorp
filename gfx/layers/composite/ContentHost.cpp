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
namespace gfx {
class Matrix4x4;
}
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
ContentHostTexture::Composite(EffectChain& aEffectChain,
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
                                                       aFilter, true);
  if (!effect) {
    return;
  }

  aEffectChain.mPrimaryEffect = effect;

  nsIntRegion tmpRegion;
  const nsIntRegion* renderRegion;
  if (PaintWillResample()) {
    // If we're resampling, then the texture image will contain exactly the
    // entire visible region's bounds, and we should draw it all in one quad
    // to avoid unexpected aliasing.
    tmpRegion = aVisibleRegion->GetBounds();
    renderRegion = &tmpRegion;
  } else {
    renderRegion = aVisibleRegion;
  }

  nsIntRegion region(*renderRegion);
  nsIntPoint origin = GetOriginOffset();
  // translate into TexImage space, buffer origin might not be at texture (0,0)
  region.MoveBy(-origin);

  // Figure out the intersecting draw region
  gfx::IntSize texSize = mTextureSource->GetSize();
  nsIntRect textureRect = nsIntRect(0, 0, texSize.width, texSize.height);
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
  while (const nsIntRect* iterRect = iter.Next()) {
    nsIntRect regionRect = *iterRect;
    nsIntRect screenRect = regionRect;
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

    nsIntRect texRect = bigImgIter ? bigImgIter->GetTileRect()
                                 : nsIntRect(0, 0,
                                             texSize.width,
                                             texSize.height);

    // Draw texture. If we're using tiles, we do repeating manually, as texture
    // repeat would cause each individual tile to repeat instead of the
    // compound texture as a whole. This involves drawing at most 4 sections,
    // 2 for each axis that has texture repeat.
    for (int y = 0; y < (usingTiles ? 2 : 1); y++) {
      for (int x = 0; x < (usingTiles ? 2 : 1); x++) {
        nsIntRect currentTileRect(texRect);
        currentTileRect.MoveBy(x * texSize.width, y * texSize.height);

        nsIntRegionRectIterator screenIter(screenRects);
        nsIntRegionRectIterator regionIter(regionRects);

        const nsIntRect* screenRect;
        const nsIntRect* regionRect;
        while ((screenRect = screenIter.Next()) &&
               (regionRect = regionIter.Next())) {
          nsIntRect tileScreenRect(*screenRect);
          nsIntRect tileRegionRect(*regionRect);

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
ContentHostTexture::UseTextureHost(TextureHost* aTexture)
{
  if (mTextureHost && mTextureHost != aTexture) {
    mTextureHost->UnbindTextureSource();
  }
  ContentHostBase::UseTextureHost(aTexture);
  mTextureHost = aTexture;
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
  if (mTextureHost && mTextureHost != aTextureOnBlack) {
    mTextureHost->UnbindTextureSource();
  }
  if (mTextureHostOnWhite && mTextureHostOnWhite != aTextureOnWhite) {
    mTextureHostOnWhite->UnbindTextureSource();
  }
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

#ifdef MOZ_DUMP_PAINTING
void
ContentHostTexture::Dump(std::stringstream& aStream,
                         const char* aPrefix,
                         bool aDumpHtml)
{
  if (!aDumpHtml) {
    return;
  }
  aStream << "<ul>";
  if (mTextureHost) {
    aStream << aPrefix;
    aStream << "<li> <a href=";
    DumpTextureHost(aStream, mTextureHost);
    aStream << "> Front buffer </a></li> ";
  }
  if (mTextureHostOnWhite) {
    aStream <<  aPrefix;
    aStream << "<li> <a href=";
    DumpTextureHost(aStream, mTextureHostOnWhite);
    aStream << "> Front buffer on white </a> </li> ";
  }
  aStream << "</ul>";
}
#endif

static inline void
AddWrappedRegion(const nsIntRegion& aInput, nsIntRegion& aOutput,
                 const nsIntSize& aSize, const nsIntPoint& aShift)
{
  nsIntRegion tempRegion;
  tempRegion.And(nsIntRect(aShift, aSize), aInput);
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

  nsIntSize bufferSize = aData.rect().Size();

  // Select only the pixels that are still within the buffer.
  nsIntRegion finalRegion;
  finalRegion.And(nsIntRect(nsIntPoint(), bufferSize), destRegion);

  // For each of the overlap areas (right, bottom-right, bottom), select those
  // pixels and wrap them around to the opposite edge of the buffer rect.
  AddWrappedRegion(destRegion, finalRegion, bufferSize, nsIntPoint(aData.rect().width, 0));
  AddWrappedRegion(destRegion, finalRegion, bufferSize, nsIntPoint(aData.rect().width, aData.rect().height));
  AddWrappedRegion(destRegion, finalRegion, bufferSize, nsIntPoint(0, aData.rect().height));

  MOZ_ASSERT(nsIntRect(0, 0, aData.rect().width, aData.rect().height).Contains(finalRegion.GetBounds()));

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

ContentHostIncremental::ContentHostIncremental(const TextureInfo& aTextureInfo)
  : ContentHostBase(aTextureInfo)
  , mDeAllocator(nullptr)
  , mLocked(false)
{
}

ContentHostIncremental::~ContentHostIncremental()
{
}

bool
ContentHostIncremental::CreatedIncrementalTexture(ISurfaceAllocator* aAllocator,
                                                  const TextureInfo& aTextureInfo,
                                                  const nsIntRect& aBufferRect)
{
  mUpdateList.AppendElement(new TextureCreationRequest(aTextureInfo,
                                                       aBufferRect));
  mDeAllocator = aAllocator;
  FlushUpdateQueue();
  return true;
}

void
ContentHostIncremental::UpdateIncremental(TextureIdentifier aTextureId,
                                          SurfaceDescriptor& aSurface,
                                          const nsIntRegion& aUpdated,
                                          const nsIntRect& aBufferRect,
                                          const nsIntPoint& aBufferRotation)
{
  mUpdateList.AppendElement(new TextureUpdateRequest(mDeAllocator,
                                                     aTextureId,
                                                     aSurface,
                                                     aUpdated,
                                                     aBufferRect,
                                                     aBufferRotation));
  FlushUpdateQueue();
}

void
ContentHostIncremental::Composite(EffectChain& aEffectChain,
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

  if (!mSource) {
    return;
  }

  RefPtr<TexturedEffect> effect = CreateTexturedEffect(mSource.get(),
                                                       mSourceOnWhite.get(),
                                                       aFilter, true);
  if (!effect) {
    return;
  }

  aEffectChain.mPrimaryEffect = effect;

  nsIntRegion tmpRegion;
  const nsIntRegion* renderRegion;
  if (PaintWillResample()) {
    // If we're resampling, then the texture image will contain exactly the
    // entire visible region's bounds, and we should draw it all in one quad
    // to avoid unexpected aliasing.
    tmpRegion = aVisibleRegion->GetBounds();
    renderRegion = &tmpRegion;
  } else {
    renderRegion = aVisibleRegion;
  }

  nsIntRegion region(*renderRegion);
  nsIntPoint origin = GetOriginOffset();
  // translate into TexImage space, buffer origin might not be at texture (0,0)
  region.MoveBy(-origin);

  // Figure out the intersecting draw region
  gfx::IntSize texSize = mSource->GetSize();
  nsIntRect textureRect = nsIntRect(0, 0, texSize.width, texSize.height);
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
  while (const nsIntRect* iterRect = iter.Next()) {
    nsIntRect regionRect = *iterRect;
    nsIntRect screenRect = regionRect;
    screenRect.MoveBy(origin);

    screenRects.Or(screenRects, screenRect);
    regionRects.Or(regionRects, regionRect);
  }

  BigImageIterator* bigImgIter = mSource->AsBigImageIterator();
  BigImageIterator* iterOnWhite = nullptr;
  if (bigImgIter) {
    bigImgIter->BeginBigImageIteration();
  }

  if (mSourceOnWhite) {
    iterOnWhite = mSourceOnWhite->AsBigImageIterator();
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

    nsIntRect texRect = bigImgIter ? bigImgIter->GetTileRect()
                                   : nsIntRect(0, 0,
                                               texSize.width,
                                               texSize.height);

    // Draw texture. If we're using tiles, we do repeating manually, as texture
    // repeat would cause each individual tile to repeat instead of the
    // compound texture as a whole. This involves drawing at most 4 sections,
    // 2 for each axis that has texture repeat.
    for (int y = 0; y < (usingTiles ? 2 : 1); y++) {
      for (int x = 0; x < (usingTiles ? 2 : 1); x++) {
        nsIntRect currentTileRect(texRect);
        currentTileRect.MoveBy(x * texSize.width, y * texSize.height);

        nsIntRegionRectIterator screenIter(screenRects);
        nsIntRegionRectIterator regionIter(regionRects);

        const nsIntRect* screenRect;
        const nsIntRect* regionRect;
        while ((screenRect = screenIter.Next()) &&
               (regionRect = regionIter.Next())) {
          nsIntRect tileScreenRect(*screenRect);
          nsIntRect tileRegionRect(*regionRect);

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
ContentHostIncremental::FlushUpdateQueue()
{
  // If we're not compositing for some reason (the window being minimized
  // is one example), then we never process these updates and it can consume
  // huge amounts of memory. Instead we forcibly process the updates (during the
  // transaction) if the list gets too long.
  static const uint32_t kMaxUpdateCount = 6;
  if (mUpdateList.Length() >= kMaxUpdateCount) {
    ProcessTextureUpdates();
  }
}

void
ContentHostIncremental::ProcessTextureUpdates()
{
  for (uint32_t i = 0; i < mUpdateList.Length(); i++) {
    mUpdateList[i]->Execute(this);
  }
  mUpdateList.Clear();
}

void
ContentHostIncremental::TextureCreationRequest::Execute(ContentHostIncremental* aHost)
{
  Compositor* compositor = aHost->GetCompositor();
  MOZ_ASSERT(compositor);

  RefPtr<DataTextureSource> temp =
    compositor->CreateDataTextureSource(mTextureInfo.mTextureFlags);
  MOZ_ASSERT(temp->AsSourceOGL() &&
             temp->AsSourceOGL()->AsTextureImageTextureSource());
  RefPtr<TextureImageTextureSourceOGL> newSource =
    temp->AsSourceOGL()->AsTextureImageTextureSource();

  RefPtr<TextureImageTextureSourceOGL> newSourceOnWhite;
  if (mTextureInfo.mTextureFlags & TextureFlags::COMPONENT_ALPHA) {
    temp =
      compositor->CreateDataTextureSource(mTextureInfo.mTextureFlags);
    MOZ_ASSERT(temp->AsSourceOGL() &&
               temp->AsSourceOGL()->AsTextureImageTextureSource());
    newSourceOnWhite = temp->AsSourceOGL()->AsTextureImageTextureSource();
  }

  if (mTextureInfo.mDeprecatedTextureHostFlags & DeprecatedTextureHostFlags::COPY_PREVIOUS) {
    MOZ_ASSERT(aHost->mSource);
    MOZ_ASSERT(aHost->mSource->IsValid());
    nsIntRect bufferRect = aHost->mBufferRect;
    nsIntPoint bufferRotation = aHost->mBufferRotation;
    nsIntRect overlap;

    // The buffer looks like:
    //  ______
    // |1  |2 |  Where the center point is offset by mBufferRotation from the top-left corner.
    // |___|__|
    // |3  |4 |
    // |___|__|
    //
    // This is drawn to the screen as:
    //  ______
    // |4  |3 |  Where the center point is { width - mBufferRotation.x, height - mBufferRotation.y } from
    // |___|__|  from the top left corner - rotationPoint.
    // |2  |1 |
    // |___|__|
    //

    // The basic idea below is to take all quadrant rectangles from the src and transform them into rectangles
    // in the destination. Unfortunately, it seems it is overly complex and could perhaps be simplified.

    nsIntRect srcBufferSpaceBottomRight(bufferRotation.x, bufferRotation.y, bufferRect.width - bufferRotation.x, bufferRect.height - bufferRotation.y);
    nsIntRect srcBufferSpaceTopRight(bufferRotation.x, 0, bufferRect.width - bufferRotation.x, bufferRotation.y);
    nsIntRect srcBufferSpaceTopLeft(0, 0, bufferRotation.x, bufferRotation.y);
    nsIntRect srcBufferSpaceBottomLeft(0, bufferRotation.y, bufferRotation.x, bufferRect.height - bufferRotation.y);

    overlap.IntersectRect(bufferRect, mBufferRect);

    nsIntRect srcRect(overlap), dstRect(overlap);
    srcRect.MoveBy(- bufferRect.TopLeft() + bufferRotation);

    nsIntRect srcRectDrawTopRight(srcRect);
    nsIntRect srcRectDrawTopLeft(srcRect);
    nsIntRect srcRectDrawBottomLeft(srcRect);
    // transform into the different quadrants
    srcRectDrawTopRight  .MoveBy(-nsIntPoint(0, bufferRect.height));
    srcRectDrawTopLeft   .MoveBy(-nsIntPoint(bufferRect.width, bufferRect.height));
    srcRectDrawBottomLeft.MoveBy(-nsIntPoint(bufferRect.width, 0));

    // Intersect with the quadrant
    srcRect               = srcRect              .Intersect(srcBufferSpaceBottomRight);
    srcRectDrawTopRight   = srcRectDrawTopRight  .Intersect(srcBufferSpaceTopRight);
    srcRectDrawTopLeft    = srcRectDrawTopLeft   .Intersect(srcBufferSpaceTopLeft);
    srcRectDrawBottomLeft = srcRectDrawBottomLeft.Intersect(srcBufferSpaceBottomLeft);

    dstRect = srcRect;
    nsIntRect dstRectDrawTopRight(srcRectDrawTopRight);
    nsIntRect dstRectDrawTopLeft(srcRectDrawTopLeft);
    nsIntRect dstRectDrawBottomLeft(srcRectDrawBottomLeft);

    // transform back to src buffer space
    dstRect              .MoveBy(-bufferRotation);
    dstRectDrawTopRight  .MoveBy(-bufferRotation + nsIntPoint(0, bufferRect.height));
    dstRectDrawTopLeft   .MoveBy(-bufferRotation + nsIntPoint(bufferRect.width, bufferRect.height));
    dstRectDrawBottomLeft.MoveBy(-bufferRotation + nsIntPoint(bufferRect.width, 0));

    // transform back to draw coordinates
    dstRect              .MoveBy(bufferRect.TopLeft());
    dstRectDrawTopRight  .MoveBy(bufferRect.TopLeft());
    dstRectDrawTopLeft   .MoveBy(bufferRect.TopLeft());
    dstRectDrawBottomLeft.MoveBy(bufferRect.TopLeft());

    // transform to destBuffer space
    dstRect              .MoveBy(-mBufferRect.TopLeft());
    dstRectDrawTopRight  .MoveBy(-mBufferRect.TopLeft());
    dstRectDrawTopLeft   .MoveBy(-mBufferRect.TopLeft());
    dstRectDrawBottomLeft.MoveBy(-mBufferRect.TopLeft());

    newSource->EnsureBuffer(mBufferRect.Size(),
                           ContentForFormat(aHost->mSource->GetFormat()));

    aHost->mSource->CopyTo(srcRect, newSource, dstRect);
    if (bufferRotation != nsIntPoint(0, 0)) {
      // Draw the remaining quadrants. We call BlitTextureImage 3 extra
      // times instead of doing a single draw call because supporting that
      // with a tiled source is quite tricky.

      if (!srcRectDrawTopRight.IsEmpty())
        aHost->mSource->CopyTo(srcRectDrawTopRight,
                               newSource, dstRectDrawTopRight);
      if (!srcRectDrawTopLeft.IsEmpty())
        aHost->mSource->CopyTo(srcRectDrawTopLeft,
                               newSource, dstRectDrawTopLeft);
      if (!srcRectDrawBottomLeft.IsEmpty())
        aHost->mSource->CopyTo(srcRectDrawBottomLeft,
                               newSource, dstRectDrawBottomLeft);
    }

    if (newSourceOnWhite) {
      newSourceOnWhite->EnsureBuffer(mBufferRect.Size(),
                                    ContentForFormat(aHost->mSourceOnWhite->GetFormat()));
      aHost->mSourceOnWhite->CopyTo(srcRect, newSourceOnWhite, dstRect);
      if (bufferRotation != nsIntPoint(0, 0)) {
        // draw the remaining quadrants
        if (!srcRectDrawTopRight.IsEmpty())
          aHost->mSourceOnWhite->CopyTo(srcRectDrawTopRight,
                                        newSourceOnWhite, dstRectDrawTopRight);
        if (!srcRectDrawTopLeft.IsEmpty())
          aHost->mSourceOnWhite->CopyTo(srcRectDrawTopLeft,
                                        newSourceOnWhite, dstRectDrawTopLeft);
        if (!srcRectDrawBottomLeft.IsEmpty())
          aHost->mSourceOnWhite->CopyTo(srcRectDrawBottomLeft,
                                        newSourceOnWhite, dstRectDrawBottomLeft);
      }
    }
  }

  aHost->mSource = newSource;
  aHost->mSourceOnWhite = newSourceOnWhite;

  aHost->mBufferRect = mBufferRect;
  aHost->mBufferRotation = nsIntPoint();
}

nsIntRect
ContentHostIncremental::TextureUpdateRequest::GetQuadrantRectangle(XSide aXSide,
                                                                   YSide aYSide) const
{
  // quadrantTranslation is the amount we translate the top-left
  // of the quadrant by to get coordinates relative to the layer
  nsIntPoint quadrantTranslation = -mBufferRotation;
  quadrantTranslation.x += aXSide == LEFT ? mBufferRect.width : 0;
  quadrantTranslation.y += aYSide == TOP ? mBufferRect.height : 0;
  return mBufferRect + quadrantTranslation;
}

void
ContentHostIncremental::TextureUpdateRequest::Execute(ContentHostIncremental* aHost)
{
  nsIntRect drawBounds = mUpdated.GetBounds();

  aHost->mBufferRect = mBufferRect;
  aHost->mBufferRotation = mBufferRotation;

  // Figure out which quadrant to draw in
  int32_t xBoundary = mBufferRect.XMost() - mBufferRotation.x;
  int32_t yBoundary = mBufferRect.YMost() - mBufferRotation.y;
  XSide sideX = drawBounds.XMost() <= xBoundary ? RIGHT : LEFT;
  YSide sideY = drawBounds.YMost() <= yBoundary ? BOTTOM : TOP;
  nsIntRect quadrantRect = GetQuadrantRectangle(sideX, sideY);
  NS_ASSERTION(quadrantRect.Contains(drawBounds), "Messed up quadrants");

  mUpdated.MoveBy(-nsIntPoint(quadrantRect.x, quadrantRect.y));

  IntPoint offset = ToIntPoint(-mUpdated.GetBounds().TopLeft());

  RefPtr<DataSourceSurface> surf = GetSurfaceForDescriptor(mDescriptor);

  if (mTextureId == TextureIdentifier::Front) {
    aHost->mSource->Update(surf, &mUpdated, &offset);
  } else {
    aHost->mSourceOnWhite->Update(surf, &mUpdated, &offset);
  }
}

void
ContentHostIncremental::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  aStream << aPrefix;
  aStream << nsPrintfCString("ContentHostIncremental (0x%p)", this).get();

  if (PaintWillResample()) {
    aStream << " [paint-will-resample]";
  }
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

TemporaryRef<TexturedEffect>
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
                              aFilter, true);
}

TemporaryRef<TexturedEffect>
ContentHostIncremental::GenEffect(const gfx::Filter& aFilter)
{
  if (!mSource) {
    return nullptr;
  }
  return CreateTexturedEffect(mSource, mSourceOnWhite, aFilter, true);
}

#ifdef MOZ_DUMP_PAINTING
TemporaryRef<gfx::DataSourceSurface>
ContentHostTexture::GetAsSurface()
{
  if (!mTextureHost) {
    return nullptr;
  }

  return mTextureHost->GetAsSurface();
}

#endif


} // namespace
} // namespace
