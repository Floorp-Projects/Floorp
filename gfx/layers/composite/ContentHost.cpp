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
  , mPaintWillResample(false)
  , mInitialised(false)
{}

ContentHostBase::~ContentHostBase()
{
}

struct AutoLockContentHost
{
  AutoLockContentHost(ContentHostBase* aHost)
    : mHost(aHost)
  {
    mSucceeded = mHost->Lock();
  }

  ~AutoLockContentHost()
  {
    if (mSucceeded) {
      mHost->Unlock();
    }
  }

  bool Failed() { return !mSucceeded; }

  ContentHostBase* mHost;
  bool mSucceeded;
};

void
ContentHostBase::Composite(EffectChain& aEffectChain,
                           float aOpacity,
                           const gfx::Matrix4x4& aTransform,
                           const Filter& aFilter,
                           const Rect& aClipRect,
                           const nsIntRegion* aVisibleRegion,
                           TiledLayerProperties* aLayerProperties)
{
  NS_ASSERTION(aVisibleRegion, "Requires a visible region");

  AutoLockContentHost lock(this);
  if (lock.Failed()) {
    return;
  }

  RefPtr<NewTextureSource> source = GetTextureSource();
  RefPtr<NewTextureSource> sourceOnWhite = GetTextureSourceOnWhite();

  if (!source) {
    return;
  }
  RefPtr<TexturedEffect> effect =
    CreateTexturedEffect(source, sourceOnWhite, aFilter);

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
  gfx::IntSize texSize = source->GetSize();
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

  TileIterator* tileIter = source->AsTileIterator();
  TileIterator* iterOnWhite = nullptr;
  if (tileIter) {
    tileIter->BeginTileIteration();
  }

  if (sourceOnWhite) {
    iterOnWhite = sourceOnWhite->AsTileIterator();
    MOZ_ASSERT(!tileIter || tileIter->GetTileCount() == iterOnWhite->GetTileCount(),
               "Tile count mismatch on component alpha texture");
    if (iterOnWhite) {
      iterOnWhite->BeginTileIteration();
    }
  }

  bool usingTiles = (tileIter && tileIter->GetTileCount() > 1);
  do {
    if (iterOnWhite) {
      MOZ_ASSERT(iterOnWhite->GetTileRect() == tileIter->GetTileRect(),
                 "component alpha textures should be the same size.");
    }

    nsIntRect texRect = tileIter ? tileIter->GetTileRect()
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
            DiagnosticTypes diagnostics = DIAGNOSTIC_CONTENT | DIAGNOSTIC_BIGIMAGE;
            diagnostics |= iterOnWhite ? DIAGNOSTIC_COMPONENT_ALPHA : 0;
            GetCompositor()->DrawDiagnostics(diagnostics, rect, aClipRect,
                                             aTransform, mFlashCounter);
          }
        }
      }
    }

    if (iterOnWhite) {
      iterOnWhite->NextTile();
    }
  } while (usingTiles && tileIter->NextTile());

  if (tileIter) {
    tileIter->EndTileIteration();
  }
  if (iterOnWhite) {
    iterOnWhite->EndTileIteration();
  }

  DiagnosticTypes diagnostics = DIAGNOSTIC_CONTENT;
  diagnostics |= iterOnWhite ? DIAGNOSTIC_COMPONENT_ALPHA : 0;
  GetCompositor()->DrawDiagnostics(diagnostics, *aVisibleRegion, aClipRect,
                                   aTransform, mFlashCounter);
}


void
ContentHostTexture::UseTextureHost(TextureHost* aTexture)
{
  ContentHostBase::UseTextureHost(aTexture);
  mTextureHost = aTexture;
  mTextureHostOnWhite = nullptr;
}

void
ContentHostTexture::UseComponentAlphaTextures(TextureHost* aTextureOnBlack,
                                              TextureHost* aTextureOnWhite)
{
  ContentHostBase::UseComponentAlphaTextures(aTextureOnBlack, aTextureOnWhite);
  mTextureHost = aTextureOnBlack;
  mTextureHostOnWhite = aTextureOnWhite;
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
ContentHostTexture::Dump(FILE* aFile,
                         const char* aPrefix,
                         bool aDumpHtml)
{
  if (!aDumpHtml) {
    return;
  }
  if (!aFile) {
    aFile = stderr;
  }
  fprintf(aFile, "<ul>");
  if (mTextureHost) {
    fprintf(aFile, "%s", aPrefix);
    fprintf(aFile, "<li> <a href=");
    DumpTextureHost(aFile, mTextureHost);
    fprintf(aFile, "> Front buffer </a></li> ");
  }
  if (mTextureHostOnWhite) {
    fprintf(aFile, "%s", aPrefix);
    fprintf(aFile, "<li> <a href=");
    DumpTextureHost(aFile, mTextureHostOnWhite);
    fprintf(aFile, "> Front buffer on white </a> </li> ");
  }
  fprintf(aFile, "</ul>");
}
#endif

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

  // Correct for rotation
  destRegion.MoveBy(aData.rotation());

  IntSize size = aData.rect().Size().ToIntSize();
  nsIntRect destBounds = destRegion.GetBounds();
  destRegion.MoveBy((destBounds.x >= size.width) ? -size.width : 0,
                    (destBounds.y >= size.height) ? -size.height : 0);

  // We can get arbitrary bad regions from an untrusted client,
  // which we need to be resilient to. See bug 967330.
  if((destBounds.x % size.width) + destBounds.width > size.width ||
     (destBounds.y % size.height) + destBounds.height > size.height)
  {
    NS_ERROR("updated region lies across rotation boundaries!");
    return false;
  }

  mTextureHost->Updated(&destRegion);
  if (mTextureHostOnWhite) {
    mTextureHostOnWhite->Updated(&destRegion);
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

void
ContentHostIncremental::DestroyTextures()
{
  mSource = nullptr;
  mSourceOnWhite = nullptr;
  mUpdateList.Clear();
}

void
ContentHostIncremental::CreatedIncrementalTexture(ISurfaceAllocator* aAllocator,
                                                  const TextureInfo& aTextureInfo,
                                                  const nsIntRect& aBufferRect)
{
  mUpdateList.AppendElement(new TextureCreationRequest(aTextureInfo,
                                                       aBufferRect));
  mDeAllocator = aAllocator;
  FlushUpdateQueue();
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

NewTextureSource*
ContentHostIncremental::GetTextureSource()
{
  MOZ_ASSERT(mLocked);
  return mSource;
}

NewTextureSource*
ContentHostIncremental::GetTextureSourceOnWhite()
{
  MOZ_ASSERT(mLocked);
  return mSourceOnWhite;
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
  if (mTextureInfo.mTextureFlags & TEXTURE_COMPONENT_ALPHA) {
    temp =
      compositor->CreateDataTextureSource(mTextureInfo.mTextureFlags);
    MOZ_ASSERT(temp->AsSourceOGL() &&
               temp->AsSourceOGL()->AsTextureImageTextureSource());
    newSourceOnWhite = temp->AsSourceOGL()->AsTextureImageTextureSource();
  }

  if (mTextureInfo.mDeprecatedTextureHostFlags & TEXTURE_HOST_COPY_PREVIOUS) {
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

  if (mTextureId == TextureFront) {
    aHost->mSource->Update(surf, &mUpdated, &offset);
  } else {
    aHost->mSourceOnWhite->Update(surf, &mUpdated, &offset);
  }
}

void
ContentHostTexture::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  aTo += aPrefix;
  aTo += nsPrintfCString("ContentHost (0x%p)", this);

  AppendToString(aTo, mBufferRect, " [buffer-rect=", "]");
  AppendToString(aTo, mBufferRotation, " [buffer-rotation=", "]");
  if (PaintWillResample()) {
    aTo += " [paint-will-resample]";
  }

  nsAutoCString pfx(aPrefix);
  pfx += "  ";

  if (mTextureHost) {
    aTo += "\n";
    mTextureHost->PrintInfo(aTo, pfx.get());
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
    result.mFlags |= LAYER_RENDER_STATE_BUFFER_ROTATION;
  }
  result.SetOffset(GetOriginOffset());
  return result;
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
