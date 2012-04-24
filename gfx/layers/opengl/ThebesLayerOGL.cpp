/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.org>
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/layers/PLayers.h"
#include "TiledLayerBuffer.h"

/* This must occur *after* layers/PLayers.h to avoid typedefs conflicts. */
#include "mozilla/Util.h"

#include "mozilla/layers/ShadowLayers.h"

#include "ThebesLayerBuffer.h"
#include "ThebesLayerOGL.h"
#include "gfxUtils.h"
#include "gfxTeeSurface.h"

#include "base/message_loop.h"

namespace mozilla {
namespace layers {

using gl::GLContext;
using gl::TextureImage;

static const int ALLOW_REPEAT = ThebesLayerBuffer::ALLOW_REPEAT;

// BindAndDrawQuadWithTextureRect can work with either GL_REPEAT (preferred)
// or GL_CLAMP_TO_EDGE textures. If ALLOW_REPEAT is set in aFlags, we
// select based on whether REPEAT is valid for non-power-of-two textures --
// if we have NPOT support we use it, otherwise we stick with CLAMP_TO_EDGE and
// decompose.
// If ALLOW_REPEAT is not set, we always use GL_CLAMP_TO_EDGE.
static already_AddRefed<TextureImage>
CreateClampOrRepeatTextureImage(GLContext *aGl,
                                const nsIntSize& aSize,
                                TextureImage::ContentType aContentType,
                                PRUint32 aFlags)
{
  GLenum wrapMode = LOCAL_GL_CLAMP_TO_EDGE;
  if ((aFlags & ALLOW_REPEAT) &&
      (aGl->IsExtensionSupported(GLContext::ARB_texture_non_power_of_two) ||
       aGl->IsExtensionSupported(GLContext::OES_texture_npot)))
  {
    wrapMode = LOCAL_GL_REPEAT;
  }

  return aGl->CreateTextureImage(aSize, aContentType, wrapMode);
}

static void
SetAntialiasingFlags(Layer* aLayer, gfxContext* aTarget)
{
  nsRefPtr<gfxASurface> surface = aTarget->CurrentSurface();
  if (surface->GetContentType() != gfxASurface::CONTENT_COLOR_ALPHA) {
    // Destination doesn't have alpha channel; no need to set any special flags
    return;
  }

  surface->SetSubpixelAntialiasingEnabled(
      !(aLayer->GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA));
}

class ThebesLayerBufferOGL
{
  NS_INLINE_DECL_REFCOUNTING(ThebesLayerBufferOGL)
public:
  typedef TextureImage::ContentType ContentType;
  typedef ThebesLayerBuffer::PaintState PaintState;

  ThebesLayerBufferOGL(ThebesLayer* aLayer, LayerOGL* aOGLLayer)
    : mLayer(aLayer)
    , mOGLLayer(aOGLLayer)
    , mInitialised(true)
  {}
  virtual ~ThebesLayerBufferOGL() {}

  enum { PAINT_WILL_RESAMPLE = ThebesLayerBuffer::PAINT_WILL_RESAMPLE };
  virtual PaintState BeginPaint(ContentType aContentType,
                                PRUint32 aFlags) = 0;

  void RenderTo(const nsIntPoint& aOffset, LayerManagerOGL* aManager,
                PRUint32 aFlags);

  nsIntSize GetSize() {
    if (mTexImage)
      return mTexImage->GetSize();
    return nsIntSize(0, 0);
  }

  bool Initialised() { return mInitialised; }

protected:
  virtual nsIntPoint GetOriginOffset() = 0;

  GLContext* gl() const { return mOGLLayer->gl(); }

  ThebesLayer* mLayer;
  LayerOGL* mOGLLayer;
  nsRefPtr<TextureImage> mTexImage;
  nsRefPtr<TextureImage> mTexImageOnWhite;
  bool mInitialised;
};

void
ThebesLayerBufferOGL::RenderTo(const nsIntPoint& aOffset,
                               LayerManagerOGL* aManager,
                               PRUint32 aFlags)
{
  NS_ASSERTION(Initialised(), "RenderTo with uninitialised buffer!");

  if (!mTexImage || !Initialised())
    return;

  if (mTexImage->InUpdate()) {
    mTexImage->EndUpdate();
  }

  if (mTexImageOnWhite && mTexImageOnWhite->InUpdate()) {
    mTexImageOnWhite->EndUpdate();
  }

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting) {
    nsRefPtr<gfxImageSurface> surf = 
      gl()->GetTexImage(mTexImage->GetTextureID(), false, mTexImage->GetShaderProgramType());
    
    WriteSnapshotToDumpFile(mLayer, surf);
  }
#endif

  PRInt32 passes = mTexImageOnWhite ? 2 : 1;
  for (PRInt32 pass = 1; pass <= passes; ++pass) {
    LayerProgram *program;

    if (passes == 2) {
      ComponentAlphaTextureLayerProgram *alphaProgram;
      if (pass == 1) {
        alphaProgram = aManager->GetComponentAlphaPass1LayerProgram();
        gl()->fBlendFuncSeparate(LOCAL_GL_ZERO, LOCAL_GL_ONE_MINUS_SRC_COLOR,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE);
      } else {
        alphaProgram = aManager->GetComponentAlphaPass2LayerProgram();
        gl()->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE);
      }

      alphaProgram->Activate();
      alphaProgram->SetBlackTextureUnit(0);
      alphaProgram->SetWhiteTextureUnit(1);
      program = alphaProgram;
    } else {
      // Note BGR: Cairo's image surfaces are always in what
      // OpenGL and our shaders consider BGR format.
      ColorTextureLayerProgram *basicProgram =
        aManager->GetColorTextureLayerProgram(mTexImage->GetShaderProgramType());

      basicProgram->Activate();
      basicProgram->SetTextureUnit(0);
      program = basicProgram;
    }

    program->SetLayerOpacity(mLayer->GetEffectiveOpacity());
    program->SetLayerTransform(mLayer->GetEffectiveTransform());
    program->SetRenderOffset(aOffset);

    const nsIntRegion& visibleRegion = mLayer->GetEffectiveVisibleRegion();
    nsIntRegion tmpRegion;
    const nsIntRegion* renderRegion;
    if (aFlags & PAINT_WILL_RESAMPLE) {
      // If we're resampling, then the texture image will contain exactly the
      // entire visible region's bounds, and we should draw it all in one quad
      // to avoid unexpected aliasing.
      tmpRegion = visibleRegion.GetBounds();
      renderRegion = &tmpRegion;
    } else {
      renderRegion = &visibleRegion;
    }

    nsIntRegion region(*renderRegion);
    nsIntPoint origin = GetOriginOffset();
    region.MoveBy(-origin);           // translate into TexImage space, buffer origin might not be at texture (0,0)

    // Figure out the intersecting draw region
    nsIntSize texSize = mTexImage->GetSize();
    nsIntRect textureRect = nsIntRect(0, 0, texSize.width, texSize.height);
    textureRect.MoveBy(region.GetBounds().TopLeft());
    nsIntRegion subregion;
    subregion.And(region, textureRect);
    if (subregion.IsEmpty())  // Region is empty, nothing to draw
      return;

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

    mTexImage->BeginTileIteration();
    if (mTexImageOnWhite) {
      NS_ASSERTION(mTexImage->GetTileCount() == mTexImageOnWhite->GetTileCount(),
                   "Tile count mismatch on component alpha texture");
      mTexImageOnWhite->BeginTileIteration();
    }

    bool usingTiles = (mTexImage->GetTileCount() > 1);
    do {
      if (mTexImageOnWhite) {
        NS_ASSERTION(mTexImageOnWhite->GetTileRect() == mTexImage->GetTileRect(), "component alpha textures should be the same size.");
      }

      nsIntRect tileRect = mTexImage->GetTileRect();

      // Bind textures.
      TextureImage::ScopedBindTexture texBind(mTexImage, LOCAL_GL_TEXTURE0);
      TextureImage::ScopedBindTexture texOnWhiteBind(mTexImageOnWhite, LOCAL_GL_TEXTURE1);

      // Draw texture. If we're using tiles, we do repeating manually, as texture
      // repeat would cause each individual tile to repeat instead of the
      // compound texture as a whole. This involves drawing at most 4 sections,
      // 2 for each axis that has texture repeat.
      for (int y = 0; y < (usingTiles ? 2 : 1); y++) {
        for (int x = 0; x < (usingTiles ? 2 : 1); x++) {
          nsIntRect currentTileRect(tileRect);
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

#ifdef ANDROID
              // Bug 691354
              // Using the LINEAR filter we get unexplained artifacts.
              // Use NEAREST when no scaling is required.
              gfxMatrix matrix;
              bool is2D = mLayer->GetEffectiveTransform().Is2D(&matrix);
              if (is2D && !matrix.HasNonTranslationOrFlip()) {
                gl()->ApplyFilterToBoundTexture(gfxPattern::FILTER_NEAREST);
              } else {
                mTexImage->ApplyFilter();
              }
#endif
              program->SetLayerQuadRect(tileScreenRect);
              aManager->BindAndDrawQuadWithTextureRect(program, tileRegionRect,
                                                       tileRect.Size(),
                                                       mTexImage->GetWrapMode());
          }
        }
      }

      if (mTexImageOnWhite)
          mTexImageOnWhite->NextTile();
    } while (mTexImage->NextTile());
  }

  if (mTexImageOnWhite) {
    // Restore defaults
    gl()->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                             LOCAL_GL_ONE, LOCAL_GL_ONE);
  }
}


// This implementation is the fast-path for when our TextureImage is
// permanently backed with a server-side ASurface.  We can simply
// reuse the ThebesLayerBuffer logic in its entirety and profit.
class SurfaceBufferOGL : public ThebesLayerBufferOGL, private ThebesLayerBuffer
{
public:
  typedef ThebesLayerBufferOGL::ContentType ContentType;
  typedef ThebesLayerBufferOGL::PaintState PaintState;

  SurfaceBufferOGL(ThebesLayerOGL* aLayer)
    : ThebesLayerBufferOGL(aLayer, aLayer)
    , ThebesLayerBuffer(SizedToVisibleBounds)
  {
  }
  virtual ~SurfaceBufferOGL() {}

  // ThebesLayerBufferOGL interface
  virtual PaintState BeginPaint(ContentType aContentType, 
                                PRUint32 aFlags)
  {
    // Let ThebesLayerBuffer do all the hard work for us! :D
    return ThebesLayerBuffer::BeginPaint(mLayer, 
                                         aContentType, 
                                         aFlags);
  }

  // ThebesLayerBuffer interface
  virtual already_AddRefed<gfxASurface>
  CreateBuffer(ContentType aType, const nsIntSize& aSize, PRUint32 aFlags)
  {
    NS_ASSERTION(gfxASurface::CONTENT_ALPHA != aType,"ThebesBuffer has color");

    mTexImage = CreateClampOrRepeatTextureImage(gl(), aSize, aType, aFlags);
    return mTexImage ? mTexImage->GetBackingSurface() : nsnull;
  }

protected:
  virtual nsIntPoint GetOriginOffset() {
    return BufferRect().TopLeft() - BufferRotation();
  }
};


// This implementation is (currently) the slow-path for when we can't
// implement pixel retaining using thebes.  This implementation and
// the above could be unified by abstracting buffer-copy operations
// and implementing them here using GL hacketry.
class BasicBufferOGL : public ThebesLayerBufferOGL
{
public:
  BasicBufferOGL(ThebesLayerOGL* aLayer)
    : ThebesLayerBufferOGL(aLayer, aLayer)
    , mBufferRect(0,0,0,0)
    , mBufferRotation(0,0)
  {}
  virtual ~BasicBufferOGL() {}

  virtual PaintState BeginPaint(ContentType aContentType,
                                PRUint32 aFlags);

protected:
  enum XSide {
    LEFT, RIGHT
  };
  enum YSide {
    TOP, BOTTOM
  };
  nsIntRect GetQuadrantRectangle(XSide aXSide, YSide aYSide);

  virtual nsIntPoint GetOriginOffset() {
    return mBufferRect.TopLeft() - mBufferRotation;
  }

private:
  nsIntRect mBufferRect;
  nsIntPoint mBufferRotation;
};

static void
WrapRotationAxis(PRInt32* aRotationPoint, PRInt32 aSize)
{
  if (*aRotationPoint < 0) {
    *aRotationPoint += aSize;
  } else if (*aRotationPoint >= aSize) {
    *aRotationPoint -= aSize;
  }
}

nsIntRect
BasicBufferOGL::GetQuadrantRectangle(XSide aXSide, YSide aYSide)
{
  // quadrantTranslation is the amount we translate the top-left
  // of the quadrant by to get coordinates relative to the layer
  nsIntPoint quadrantTranslation = -mBufferRotation;
  quadrantTranslation.x += aXSide == LEFT ? mBufferRect.width : 0;
  quadrantTranslation.y += aYSide == TOP ? mBufferRect.height : 0;
  return mBufferRect + quadrantTranslation;
}

static void
FillSurface(gfxASurface* aSurface, const nsIntRegion& aRegion,
            const nsIntPoint& aOffset, const gfxRGBA& aColor)
{
  nsRefPtr<gfxContext> ctx = new gfxContext(aSurface);
  ctx->Translate(-gfxPoint(aOffset.x, aOffset.y));
  gfxUtils::ClipToRegion(ctx, aRegion);
  ctx->SetColor(aColor);
  ctx->Paint();
}

BasicBufferOGL::PaintState
BasicBufferOGL::BeginPaint(ContentType aContentType,
                           PRUint32 aFlags)
{
  PaintState result;
  // We need to disable rotation if we're going to be resampled when
  // drawing, because we might sample across the rotation boundary.
  bool canHaveRotation =  !(aFlags & PAINT_WILL_RESAMPLE);

  nsIntRegion validRegion = mLayer->GetValidRegion();

  Layer::SurfaceMode mode;
  ContentType contentType;
  nsIntRegion neededRegion;
  bool canReuseBuffer;
  nsIntRect destBufferRect;

  while (true) {
    mode = mLayer->GetSurfaceMode();
    contentType = aContentType;
    neededRegion = mLayer->GetVisibleRegion();
    // If we're going to resample, we need a buffer that's in clamp mode.
    canReuseBuffer = neededRegion.GetBounds().Size() <= mBufferRect.Size() &&
      mTexImage &&
      (!(aFlags & PAINT_WILL_RESAMPLE) ||
       mTexImage->GetWrapMode() == LOCAL_GL_CLAMP_TO_EDGE);

    if (canReuseBuffer) {
      if (mBufferRect.Contains(neededRegion.GetBounds())) {
        // We don't need to adjust mBufferRect.
        destBufferRect = mBufferRect;
      } else {
        // The buffer's big enough but doesn't contain everything that's
        // going to be visible. We'll move it.
        destBufferRect = nsIntRect(neededRegion.GetBounds().TopLeft(), mBufferRect.Size());
      }
    } else {
      destBufferRect = neededRegion.GetBounds();
    }

    if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
      mode = Layer::SURFACE_SINGLE_CHANNEL_ALPHA;
#else
      if (!mLayer->GetParent() || !mLayer->GetParent()->SupportsComponentAlphaChildren()) {
        mode = Layer::SURFACE_SINGLE_CHANNEL_ALPHA;
      } else {
        contentType = gfxASurface::CONTENT_COLOR;
      }
 #endif
    }
 
    if ((aFlags & PAINT_WILL_RESAMPLE) &&
        (!neededRegion.GetBounds().IsEqualInterior(destBufferRect) ||
         neededRegion.GetNumRects() > 1)) {
      // The area we add to neededRegion might not be painted opaquely
      if (mode == Layer::SURFACE_OPAQUE) {
        contentType = gfxASurface::CONTENT_COLOR_ALPHA;
        mode = Layer::SURFACE_SINGLE_CHANNEL_ALPHA;
      }
      // For component alpha layers, we leave contentType as CONTENT_COLOR.

      // We need to validate the entire buffer, to make sure that only valid
      // pixels are sampled
      neededRegion = destBufferRect;
    }

    if (mTexImage &&
        (mTexImage->GetContentType() != contentType ||
         (mode == Layer::SURFACE_COMPONENT_ALPHA) != (mTexImageOnWhite != nsnull))) {
      // We're effectively clearing the valid region, so we need to draw
      // the entire needed region now.
      result.mRegionToInvalidate = mLayer->GetValidRegion();
      validRegion.SetEmpty();
      mTexImage = nsnull;
      mTexImageOnWhite = nsnull;
      mBufferRect.SetRect(0, 0, 0, 0);
      mBufferRotation.MoveTo(0, 0);
      // Restart decision process with the cleared buffer. We can only go
      // around the loop one more iteration, since mTexImage is null now.
      continue;
    }

    break;
  }

  result.mRegionToDraw.Sub(neededRegion, validRegion);
  if (result.mRegionToDraw.IsEmpty())
    return result;

  if (destBufferRect.width > gl()->GetMaxTextureImageSize() ||
      destBufferRect.height > gl()->GetMaxTextureImageSize()) {
    return result;
  }

  nsIntRect drawBounds = result.mRegionToDraw.GetBounds();
  nsRefPtr<TextureImage> destBuffer;
  nsRefPtr<TextureImage> destBufferOnWhite;

  PRUint32 bufferFlags = canHaveRotation ? ALLOW_REPEAT : 0;
  if (canReuseBuffer) {
    nsIntRect keepArea;
    if (keepArea.IntersectRect(destBufferRect, mBufferRect)) {
      // Set mBufferRotation so that the pixels currently in mBuffer
      // will still be rendered in the right place when mBufferRect
      // changes to destBufferRect.
      nsIntPoint newRotation = mBufferRotation +
        (destBufferRect.TopLeft() - mBufferRect.TopLeft());
      WrapRotationAxis(&newRotation.x, mBufferRect.width);
      WrapRotationAxis(&newRotation.y, mBufferRect.height);
      NS_ASSERTION(nsIntRect(nsIntPoint(0,0), mBufferRect.Size()).Contains(newRotation),
                   "newRotation out of bounds");
      PRInt32 xBoundary = destBufferRect.XMost() - newRotation.x;
      PRInt32 yBoundary = destBufferRect.YMost() - newRotation.y;
      if ((drawBounds.x < xBoundary && xBoundary < drawBounds.XMost()) ||
          (drawBounds.y < yBoundary && yBoundary < drawBounds.YMost()) ||
          (newRotation != nsIntPoint(0,0) && !canHaveRotation)) {
        // The stuff we need to redraw will wrap around an edge of the
        // buffer, so we will need to do a self-copy
        // If mBufferRotation == nsIntPoint(0,0) we could do a real
        // self-copy but we're not going to do that in GL yet.
        // We can't do a real self-copy because the buffer is rotated.
        // So allocate a new buffer for the destination.
        destBufferRect = neededRegion.GetBounds();
        destBuffer = CreateClampOrRepeatTextureImage(gl(), destBufferRect.Size(), contentType, bufferFlags);
        if (!destBuffer)
          return result;
        if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
          destBufferOnWhite =
            CreateClampOrRepeatTextureImage(gl(), destBufferRect.Size(), contentType, bufferFlags);
          if (!destBufferOnWhite)
            return result;
        }
      } else {
        mBufferRect = destBufferRect;
        mBufferRotation = newRotation;
      }
    } else {
      // No pixels are going to be kept. The whole visible region
      // will be redrawn, so we don't need to copy anything, so we don't
      // set destBuffer.
      mBufferRect = destBufferRect;
      mBufferRotation = nsIntPoint(0,0);
    }
  } else {
    // The buffer's not big enough, so allocate a new one
    destBuffer = CreateClampOrRepeatTextureImage(gl(), destBufferRect.Size(), contentType, bufferFlags);
    if (!destBuffer)
      return result;

    if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
      destBufferOnWhite = 
        CreateClampOrRepeatTextureImage(gl(), destBufferRect.Size(), contentType, bufferFlags);
      if (!destBufferOnWhite)
        return result;
    }
  }
  NS_ASSERTION(!(aFlags & PAINT_WILL_RESAMPLE) || destBufferRect == neededRegion.GetBounds(),
               "If we're resampling, we need to validate the entire buffer");

  if (!destBuffer && !mTexImage) {
    return result;
  }

  if (destBuffer) {
    if (mTexImage && (mode != Layer::SURFACE_COMPONENT_ALPHA || mTexImageOnWhite)) {
      // BlitTextureImage depends on the FBO texture target being
      // TEXTURE_2D.  This isn't the case on some older X1600-era Radeons.
      if (mOGLLayer->OGLManager()->FBOTextureTarget() == LOCAL_GL_TEXTURE_2D) {
        nsIntRect overlap;
        overlap.IntersectRect(mBufferRect, destBufferRect);

        nsIntRect srcRect(overlap), dstRect(overlap);
        srcRect.MoveBy(- mBufferRect.TopLeft() + mBufferRotation);
        dstRect.MoveBy(- destBufferRect.TopLeft());
        
        if (mBufferRotation != nsIntPoint(0, 0)) {
          // If mBuffer is rotated, then BlitTextureImage will only be copying the bottom-right section
          // of the buffer. We need to invalidate the remaining sections so that they get redrawn too.
          // Alternatively we could teach BlitTextureImage to rearrange the rotated segments onto
          // the new buffer.
          
          // When the rotated buffer is reorganised, the bottom-right section will be drawn in the top left.
          // Find the point where this content ends.
          nsIntPoint rotationPoint(mBufferRect.x + mBufferRect.width - mBufferRotation.x, 
                                   mBufferRect.y + mBufferRect.height - mBufferRotation.y);

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
          // |___|__|  from the top left corner - rotationPoint. Since only quadrant 4 will actually be copied, 
          // |2  |1 |  we need to invalidate the others.
          // |___|__|
          //
          // Quadrants 2 and 1
          nsIntRect bottom(mBufferRect.x, rotationPoint.y, mBufferRect.width, mBufferRotation.y);
          // Quadrant 3
          nsIntRect topright(rotationPoint.x, mBufferRect.y, mBufferRotation.x, rotationPoint.y - mBufferRect.y);

          if (!bottom.IsEmpty()) {
            nsIntRegion temp;
            temp.And(destBufferRect, bottom);
            result.mRegionToDraw.Or(result.mRegionToDraw, temp);
          }
          if (!topright.IsEmpty()) {
            nsIntRegion temp;
            temp.And(destBufferRect, topright);
            result.mRegionToDraw.Or(result.mRegionToDraw, temp);
          }
        }

        destBuffer->Resize(destBufferRect.Size());

        gl()->BlitTextureImage(mTexImage, srcRect,
                               destBuffer, dstRect);
        destBuffer->MarkValid();
        if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
          destBufferOnWhite->Resize(destBufferRect.Size());
          gl()->BlitTextureImage(mTexImageOnWhite, srcRect,
                                 destBufferOnWhite, dstRect);
          destBufferOnWhite->MarkValid();
        }
      } else {
        // can't blit, just draw everything
        destBuffer = CreateClampOrRepeatTextureImage(gl(), destBufferRect.Size(), contentType, bufferFlags);
        if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
          destBufferOnWhite = 
            CreateClampOrRepeatTextureImage(gl(), destBufferRect.Size(), contentType, bufferFlags);
        }
      }
    }

    mTexImage = destBuffer.forget();
    if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
      mTexImageOnWhite = destBufferOnWhite.forget();
    }
    mBufferRect = destBufferRect;
    mBufferRotation = nsIntPoint(0,0);
  }
  NS_ASSERTION(canHaveRotation || mBufferRotation == nsIntPoint(0,0),
               "Rotation disabled, but we have nonzero rotation?");

  nsIntRegion invalidate;
  invalidate.Sub(mLayer->GetValidRegion(), destBufferRect);
  result.mRegionToInvalidate.Or(result.mRegionToInvalidate, invalidate);

  // Figure out which quadrant to draw in
  PRInt32 xBoundary = mBufferRect.XMost() - mBufferRotation.x;
  PRInt32 yBoundary = mBufferRect.YMost() - mBufferRotation.y;
  XSide sideX = drawBounds.XMost() <= xBoundary ? RIGHT : LEFT;
  YSide sideY = drawBounds.YMost() <= yBoundary ? BOTTOM : TOP;
  nsIntRect quadrantRect = GetQuadrantRectangle(sideX, sideY);
  NS_ASSERTION(quadrantRect.Contains(drawBounds), "Messed up quadrants");

  nsIntPoint offset = -nsIntPoint(quadrantRect.x, quadrantRect.y);

  // Make the region to draw relative to the buffer, before
  // passing to BeginUpdate.
  result.mRegionToDraw.MoveBy(offset);
  // BeginUpdate is allowed to modify the given region,
  // if it wants more to be repainted than we request.
  if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
    nsIntRegion drawRegionCopy = result.mRegionToDraw;
    gfxASurface *onBlack = mTexImage->BeginUpdate(drawRegionCopy);
    gfxASurface *onWhite = mTexImageOnWhite->BeginUpdate(result.mRegionToDraw);
    NS_ASSERTION(result.mRegionToDraw == drawRegionCopy,
                 "BeginUpdate should always modify the draw region in the same way!");
    FillSurface(onBlack, result.mRegionToDraw, nsIntPoint(0,0), gfxRGBA(0.0, 0.0, 0.0, 1.0));
    FillSurface(onWhite, result.mRegionToDraw, nsIntPoint(0,0), gfxRGBA(1.0, 1.0, 1.0, 1.0));
    gfxASurface* surfaces[2] = { onBlack, onWhite };
    nsRefPtr<gfxTeeSurface> surf = new gfxTeeSurface(surfaces, ArrayLength(surfaces));

    // XXX If the device offset is set on the individual surfaces instead of on
    // the tee surface, we render in the wrong place. Why?
    gfxPoint deviceOffset = onBlack->GetDeviceOffset();
    onBlack->SetDeviceOffset(gfxPoint(0, 0));
    onWhite->SetDeviceOffset(gfxPoint(0, 0));
    surf->SetDeviceOffset(deviceOffset);

    // Using this surface as a source will likely go horribly wrong, since
    // only the onBlack surface will really be used, so alpha information will
    // be incorrect.
    surf->SetAllowUseAsSource(false);
    result.mContext = new gfxContext(surf);
  } else {
    result.mContext = new gfxContext(mTexImage->BeginUpdate(result.mRegionToDraw));
    if (mTexImage->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA) {
      gfxUtils::ClipToRegion(result.mContext, result.mRegionToDraw);
      result.mContext->SetOperator(gfxContext::OPERATOR_CLEAR);
      result.mContext->Paint();
      result.mContext->SetOperator(gfxContext::OPERATOR_OVER);
    }
  }
  if (!result.mContext) {
    NS_WARNING("unable to get context for update");
    return result;
  }
  result.mContext->Translate(-gfxPoint(quadrantRect.x, quadrantRect.y));
  // Move rgnToPaint back into position so that the thebes callback
  // gets the right coordintes.
  result.mRegionToDraw.MoveBy(-offset);

  // If we do partial updates, we have to clip drawing to the regionToDraw.
  // If we don't clip, background images will be fillrect'd to the region correctly,
  // while text or lines will paint outside of the regionToDraw. This becomes apparent
  // with concave regions. Right now the scrollbars invalidate a narrow strip of the awesomebar
  // although they never cover it. This leads to two draw rects, the narow strip and the actually
  // newly exposed area. It would be wise to fix this glitch in any way to have simpler
  // clip and draw regions.
  gfxUtils::ClipToRegion(result.mContext, result.mRegionToDraw);

  return result;
}

ThebesLayerOGL::ThebesLayerOGL(LayerManagerOGL *aManager)
  : ThebesLayer(aManager, nsnull)
  , LayerOGL(aManager)
  , mBuffer(nsnull)
{
  mImplData = static_cast<LayerOGL*>(this);
}

ThebesLayerOGL::~ThebesLayerOGL()
{
  Destroy();
}

void
ThebesLayerOGL::Destroy()
{
  if (!mDestroyed) {
    mBuffer = nsnull;
    mDestroyed = true;
  }
}

bool
ThebesLayerOGL::CreateSurface()
{
  NS_ASSERTION(!mBuffer, "buffer already created?");

  if (mVisibleRegion.IsEmpty()) {
    return false;
  }

  if (gl()->TextureImageSupportsGetBackingSurface()) {
    // use the ThebesLayerBuffer fast-path
    mBuffer = new SurfaceBufferOGL(this);
  } else {
    mBuffer = new BasicBufferOGL(this);
  }
  return true;
}

void
ThebesLayerOGL::SetVisibleRegion(const nsIntRegion &aRegion)
{
  if (aRegion.IsEqual(mVisibleRegion))
    return;
  ThebesLayer::SetVisibleRegion(aRegion);
}

void
ThebesLayerOGL::InvalidateRegion(const nsIntRegion &aRegion)
{
  mValidRegion.Sub(mValidRegion, aRegion);
}

void
ThebesLayerOGL::RenderLayer(int aPreviousFrameBuffer,
                            const nsIntPoint& aOffset)
{
  if (!mBuffer && !CreateSurface()) {
    return;
  }
  NS_ABORT_IF_FALSE(mBuffer, "should have a buffer here");

  mOGLManager->MakeCurrent();
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

  TextureImage::ContentType contentType =
    CanUseOpaqueSurface() ? gfxASurface::CONTENT_COLOR :
                            gfxASurface::CONTENT_COLOR_ALPHA;

  PRUint32 flags = 0;
#ifndef MOZ_GFX_OPTIMIZE_MOBILE
  gfxMatrix transform2d;
  if (GetEffectiveTransform().Is2D(&transform2d)) {
    if (transform2d.HasNonIntegerTranslation()) {
      flags |= ThebesLayerBufferOGL::PAINT_WILL_RESAMPLE;
    }
  } else {
    flags |= ThebesLayerBufferOGL::PAINT_WILL_RESAMPLE;
  }
#endif

  Buffer::PaintState state = mBuffer->BeginPaint(contentType, flags);
  mValidRegion.Sub(mValidRegion, state.mRegionToInvalidate);

  if (state.mContext) {
    state.mRegionToInvalidate.And(state.mRegionToInvalidate, mVisibleRegion);

    LayerManager::DrawThebesLayerCallback callback =
      mOGLManager->GetThebesLayerCallback();
    if (!callback) {
      NS_ERROR("GL should never need to update ThebesLayers in an empty transaction");
    } else {
      void* callbackData = mOGLManager->GetThebesLayerCallbackData();
      SetAntialiasingFlags(this, state.mContext);
      callback(this, state.mContext, state.mRegionToDraw,
               state.mRegionToInvalidate, callbackData);
      // Everything that's visible has been validated. Do this instead of just
      // OR-ing with aRegionToDraw, since that can lead to a very complex region
      // here (OR doesn't automatically simplify to the simplest possible
      // representation of a region.)
      nsIntRegion tmp;
      tmp.Or(mVisibleRegion, state.mRegionToDraw);
      mValidRegion.Or(mValidRegion, tmp);
    }
  }

  // Drawing thebes layers can change the current context, reset it.
  gl()->MakeCurrent();

  gl()->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, aPreviousFrameBuffer);
  mBuffer->RenderTo(aOffset, mOGLManager, flags);
}

Layer*
ThebesLayerOGL::GetLayer()
{
  return this;
}

bool
ThebesLayerOGL::IsEmpty()
{
  return !mBuffer;
}

void
ThebesLayerOGL::CleanupResources()
{
  mBuffer = nsnull;
}

class ShadowBufferOGL : public ThebesLayerBufferOGL
{
public:
  ShadowBufferOGL(ShadowThebesLayerOGL* aLayer)
    : ThebesLayerBufferOGL(aLayer, aLayer)
  {
    mInitialised = false;
  }

  virtual PaintState BeginPaint(ContentType aContentType, PRUint32) {
    NS_RUNTIMEABORT("can't BeginPaint for a shadow layer");
    return PaintState();
  }

  void EnsureTexture(gfxIntSize aSize, ContentType aContentType);

  void DirectUpdate(gfxASurface* aUpdate, nsIntRegion& aRegion);

  void Upload(gfxASurface* aUpdate, const nsIntRegion& aUpdated,
              const nsIntRect& aRect, const nsIntPoint& aRotation,
              bool aDelayUpload, nsIntRegion& aPendingUploadRegion);

  nsRefPtr<TextureImage> GetTextureImage() { return mTexImage; }

protected:
  virtual nsIntPoint GetOriginOffset() {
    return mBufferRect.TopLeft() - mBufferRotation;
  }

private:
  nsIntRect mBufferRect;
  nsIntPoint mBufferRotation;
};

void
ShadowBufferOGL::EnsureTexture(gfxIntSize aSize, ContentType aContentType)
{
  if (!mTexImage ||
      GetSize() != nsIntSize(aSize.width, aSize.height) ||
      mTexImage->GetContentType() != aContentType) {
    // XXX we should do something here to decide whether to use REPEAT or not,
    // but I'm not sure what
    mTexImage = CreateClampOrRepeatTextureImage(gl(),
      nsIntSize(aSize.width, aSize.height), aContentType, ALLOW_REPEAT);
    mInitialised = false;
  }
}

void
ShadowBufferOGL::DirectUpdate(gfxASurface* aUpdate, nsIntRegion& aRegion)
{
  EnsureTexture(aUpdate->GetSize(), aUpdate->GetContentType());
  mInitialised = true;
  mTexImage->DirectUpdate(aUpdate, aRegion);
}

void
ShadowBufferOGL::Upload(gfxASurface* aUpdate, const nsIntRegion& aUpdated,
                        const nsIntRect& aRect, const nsIntPoint& aRotation,
                        bool aDelayUpload, nsIntRegion& aPendingUploadRegion)
{
  // aUpdated is in screen coordinates.  Move it so that the layer's
  // top-left is 0,0
  nsIntRegion destRegion(aUpdated);
  nsIntPoint visTopLeft = mLayer->GetVisibleRegion().GetBounds().TopLeft();
  destRegion.MoveBy(-visTopLeft);

  // Correct for rotation
  destRegion.MoveBy(aRotation);
  gfxIntSize size = aUpdate->GetSize();
  nsIntRect destBounds = destRegion.GetBounds();
  destRegion.MoveBy((destBounds.x >= size.width) ? -size.width : 0,
                    (destBounds.y >= size.height) ? -size.height : 0);

  // There's code to make sure that updated regions don't cross rotation
  // boundaries, so assert here that this is the case
  NS_ASSERTION(((destBounds.x % size.width) + destBounds.width <= size.width) &&
               ((destBounds.y % size.height) + destBounds.height <= size.height),
               "Updated region lies across rotation boundaries!");

  if (aDelayUpload) {
    // Record the region that needs to be updated, and clip it to the size of
    // the texture.
    aPendingUploadRegion.Or(aPendingUploadRegion, destRegion).
      And(aPendingUploadRegion, nsIntRect(0, 0, size.width, size.height));
  } else {
    // NB: this gfxContext must not escape EndUpdate() below
    DirectUpdate(aUpdate, destRegion);
    aPendingUploadRegion.Sub(aPendingUploadRegion, destRegion);
  }

  mBufferRect = aRect;
  mBufferRotation = aRotation;
}

ShadowThebesLayerOGL::ShadowThebesLayerOGL(LayerManagerOGL *aManager)
  : ShadowThebesLayer(aManager, nsnull)
  , LayerOGL(aManager)
  , mUploadTask(nsnull)
{
#ifdef FORCE_BASICTILEDTHEBESLAYER
  NS_ABORT();
#endif
  mImplData = static_cast<LayerOGL*>(this);
}

ShadowThebesLayerOGL::~ShadowThebesLayerOGL()
{}

bool
ShadowThebesLayerOGL::ShouldDoubleBuffer()
{
#ifdef ANDROID
  /* Enable double-buffering on Android so that we don't block for as long
   * when uploading textures. This is a work-around for the lack of an
   * asynchronous texture upload facility.
   *
   * As the progressive upload relies on tile size, doing this when large
   * tiles are in use is harder to justify.
   *
   * XXX When bug 730718 is fixed, we will likely want this only to apply for
   *     Adreno-equipped devices (which have broken sub-image texture updates,
   *     and no threaded texture upload capability).
   */
  return gl()->WantsSmallTiles();
#else
  return false;
#endif
}

void
ShadowThebesLayerOGL::EnsureTextureUpdated()
{
  if (mRegionPendingUpload.IsEmpty() || !IsSurfaceDescriptorValid(mFrontBufferDescriptor))
    return;

  mBuffer->DirectUpdate(mFrontBuffer.Buffer(), mRegionPendingUpload);
  mRegionPendingUpload.SetEmpty();
}

static bool
EnsureTextureUpdatedCallback(gl::TextureImage* aImage, int aTileNumber,
                             void *aData)
{
  // If this tile intersects with the region we asked to update, it will be
  // entirely updated - so add it to the update region so that our pending-
  // upload region will be correctly updated after the iteration finishes.
  nsIntRegion* updateRegion = (nsIntRegion*)aData;
  nsIntRect tileRect = aImage->GetTileRect();
  if (updateRegion->Intersects(tileRect))
    updateRegion->Or(*updateRegion, tileRect);
  return true;
}

void
ShadowThebesLayerOGL::EnsureTextureUpdated(nsIntRegion& aRegion)
{
  if (mRegionPendingUpload.IsEmpty() || !IsSurfaceDescriptorValid(mFrontBufferDescriptor))
    return;

  // Do this in possibly 4 chunks, to account for rotation boundaries
  nsIntRegion updateRegion;
  nsIntRect bufferRect = mFrontBuffer.Rect();
  for (int i = 0; i < 4; i++) {
    switch(i) {
      case 0:
        // The given region is in unrotated texture space, so alter the
        // update region to account for buffer rotation on first iteration.
        aRegion.MoveBy(mFrontBuffer.Rotation());
        break;
      case 1:
      case 3:
        // On the 2nd and 4th iteration, move the region left, to make sure
        // texture is updated on both sides of the x-axis rotation boundary.
        aRegion.MoveBy(-bufferRect.width, 0);
        break;
      case 2:
        // On the 3rd iteration, move the region up to make sure the texture
        // is updated no both sides of the y-axis rotation boundary.
        aRegion.MoveBy(bufferRect.width, -bufferRect.height);
    }

    // Check if any part of the texture that's pending upload intersects with
    // this region.
    updateRegion.And(aRegion, mRegionPendingUpload);

    if (updateRegion.IsEmpty())
      continue;

    nsRefPtr<TextureImage> texImage;
    if (!gl()->CanUploadSubTextures()) {
      // When sub-textures are unsupported, TiledTextureImage expands the
      // boundaries of DirectUpdate to tile boundaries. So that we don't
      // re-upload texture data, use the tile iteration to monitor how much
      // of the texture was actually uploaded.
      gfxASurface* surface = mFrontBuffer.Buffer();
      gfxIntSize size = surface->GetSize();
      mBuffer->EnsureTexture(size, surface->GetContentType());
      texImage = mBuffer->GetTextureImage().get();
      if (texImage->GetTileCount() > 1)
        texImage->SetIterationCallback(EnsureTextureUpdatedCallback, (void *)&updateRegion);
      else
        updateRegion = nsIntRect(0, 0, size.width, size.height);
    }

    // Upload this quadrant of the region.
    mBuffer->DirectUpdate(mFrontBuffer.Buffer(), updateRegion);

    if (!gl()->CanUploadSubTextures())
      texImage->SetIterationCallback(nsnull, nsnull);

    // Remove the updated region from the pending-upload region.
    mRegionPendingUpload.Sub(mRegionPendingUpload, updateRegion);
  }
}

static bool
ProgressiveUploadCallback(gl::TextureImage* aImage, int aTileNumber,
                          void *aData)
{
  nsIntRegion* regionPendingUpload = (nsIntRegion*)aData;

  // Continue iteration if nothing was uploaded
  nsIntRect tileRect = aImage->GetTileRect();
  if (!regionPendingUpload->Intersects(tileRect))
    return true;

  regionPendingUpload->Sub(*regionPendingUpload, tileRect);

  // XXX If there was a function on MessageLoop to see if there were pending
  // tasks, we could return true here depending on that. As it is, always return
  // false and schedule another upload immediately after this one.
  return false;
}

void
ShadowThebesLayerOGL::ProgressiveUpload()
{
  // Mark the task as completed
  mUploadTask = nsnull;

  if (mRegionPendingUpload.IsEmpty() || mBuffer == nsnull)
    return;

  // Set a tile iteration callback so we can cancel the upload after a tile
  // has been uploaded and subtract it from mRegionPendingUpload
  mBuffer->EnsureTexture(mFrontBuffer.Buffer()->GetSize(),
                         mFrontBuffer.Buffer()->GetContentType());
  nsRefPtr<gl::TextureImage> tiledImage = mBuffer->GetTextureImage().get();
  if (tiledImage->GetTileCount() > 1)
    tiledImage->SetIterationCallback(ProgressiveUploadCallback, (void *)&mRegionPendingUpload);
  else
    mRegionPendingUpload.SetEmpty();

  // Upload a tile
  mBuffer->DirectUpdate(mFrontBuffer.Buffer(), mRegionPendingUpload);

  // Remove the iteration callback
  tiledImage->SetIterationCallback(nsnull, nsnull);

  if (!mRegionPendingUpload.IsEmpty()) {
    // Schedule another upload task
    mUploadTask = NewRunnableMethod(this, &ShadowThebesLayerOGL::ProgressiveUpload);
    // Post a delayed task to complete more of the upload - give a reasonable delay to allow
    // for events to be processed.
    MessageLoop::current()->PostDelayedTask(FROM_HERE, mUploadTask, 5);
  }
}

void
ShadowThebesLayerOGL::Swap(const ThebesBuffer& aNewFront,
                           const nsIntRegion& aUpdatedRegion,
                           OptionalThebesBuffer* aNewBack,
                           nsIntRegion* aNewBackValidRegion,
                           OptionalThebesBuffer* aReadOnlyFront,
                           nsIntRegion* aFrontUpdatedRegion)
{
  // The double-buffer path is copied and adapted from BasicLayers.cpp
  if (ShouldDoubleBuffer()) {
    nsRefPtr<gfxASurface> newFrontBuffer =
      ShadowLayerForwarder::OpenDescriptor(aNewFront.buffer());

    if (IsSurfaceDescriptorValid(mFrontBufferDescriptor)) {
      nsRefPtr<gfxASurface> currentFront =
        ShadowLayerForwarder::OpenDescriptor(mFrontBufferDescriptor);
      if (currentFront->GetSize() != newFrontBuffer->GetSize()) {
        // Current front buffer is obsolete
        DestroyFrontBuffer();
      }
    }

    // This code relies on Swap() arriving *after* attribute mutations.
    if (IsSurfaceDescriptorValid(mFrontBufferDescriptor)) {
      *aNewBack = ThebesBuffer();
      aNewBack->get_ThebesBuffer().buffer() = mFrontBufferDescriptor;
    } else {
      *aNewBack = null_t();
    }

    // We have to invalidate the pixels painted into the new buffer.
    // They might overlap with our old pixels.
    aNewBackValidRegion->Sub(mOldValidRegion, aUpdatedRegion);

    nsRefPtr<gfxASurface> unused;
    nsIntRect backRect;
    nsIntPoint backRotation;
    mFrontBuffer.Swap(
      newFrontBuffer, aNewFront.rect(), aNewFront.rotation(),
      getter_AddRefs(unused), &backRect, &backRotation);

    if (aNewBack->type() != OptionalThebesBuffer::Tnull_t) {
      aNewBack->get_ThebesBuffer().rect() = backRect;
      aNewBack->get_ThebesBuffer().rotation() = backRotation;
    }

    mFrontBufferDescriptor = aNewFront.buffer();

    // Upload new front-buffer to texture
    if (!mDestroyed) {
      if (!mBuffer) {
        mBuffer = new ShadowBufferOGL(this);
      }
      nsRefPtr<gfxASurface> surf = ShadowLayerForwarder::OpenDescriptor(mFrontBufferDescriptor);
      mBuffer->Upload(surf, aUpdatedRegion, aNewFront.rect(), aNewFront.rotation(), true, mRegionPendingUpload);

      // Schedule a task to progressively upload the texture
      if (!mUploadTask) {
        mUploadTask = NewRunnableMethod(this, &ShadowThebesLayerOGL::ProgressiveUpload);
        MessageLoop::current()->PostDelayedTask(FROM_HERE, mUploadTask, 5);
      }
    }

    *aReadOnlyFront = aNewFront;
    *aFrontUpdatedRegion = aUpdatedRegion;

    return;
  }

  // Single-buffer path
  if (!mDestroyed) {
    if (!mBuffer) {
      mBuffer = new ShadowBufferOGL(this);
    }
    nsRefPtr<gfxASurface> surf = ShadowLayerForwarder::OpenDescriptor(aNewFront.buffer());
    mBuffer->Upload(surf, aUpdatedRegion, aNewFront.rect(), aNewFront.rotation(), false, mRegionPendingUpload);
  }

  *aNewBack = aNewFront;
  *aNewBackValidRegion = mValidRegion;
  *aReadOnlyFront = null_t();
  aFrontUpdatedRegion->SetEmpty();
}

void
ShadowThebesLayerOGL::DestroyFrontBuffer()
{
  if (ShouldDoubleBuffer()) {
    // Cancel the progressive upload task, if it's running
    if (mUploadTask) {
      mUploadTask->Cancel();
      mUploadTask = nsnull;
    }

    mFrontBuffer.Clear();
    mOldValidRegion.SetEmpty();

    if (IsSurfaceDescriptorValid(mFrontBufferDescriptor)) {
      mAllocator->DestroySharedSurface(&mFrontBufferDescriptor);
    }
  }

  mBuffer = nsnull;
}

void
ShadowThebesLayerOGL::Disconnect()
{
  Destroy();
}

void
ShadowThebesLayerOGL::Destroy()
{
  if (!mDestroyed) {
    mDestroyed = true;
    DestroyFrontBuffer();
  }
}

Layer*
ShadowThebesLayerOGL::GetLayer()
{
  return this;
}

bool
ShadowThebesLayerOGL::IsEmpty()
{
  return !mBuffer;
}

void
ShadowThebesLayerOGL::RenderLayer(int aPreviousFrameBuffer,
                                  const nsIntPoint& aOffset)
{
  if (!mBuffer) {
    return;
  }
  NS_ABORT_IF_FALSE(mBuffer, "should have a buffer here");

  mOGLManager->MakeCurrent();

  if (ShouldDoubleBuffer()) {
    // Find out what part of the screen this layer intersects with
    gfxMatrix transform2d;
    const gfx3DMatrix& transform = GetLayer()->GetEffectiveTransform();

    if (transform.Is2D(&transform2d) && transform2d.PreservesAxisAlignedRectangles()) {
      // Find out the layer rect in screen coordinates and store this in layerRect.
      // Derived from ThebesLayerBufferOGL::RenderTo.
      nsIntRect bufferRect = mFrontBuffer.Rect();
      gfxRect layerRect = transform2d.Transform(gfxRect(bufferRect));
      layerRect.MoveBy(gfxPoint(aOffset));

      // Find how much of this rect will be visible taking into account the size
      // of the widget being rendered to.
      nsIntSize widgetSize = mOGLManager->GetWidgetSize();
      gfxRect clippedLayerRect = layerRect.Intersect(gfxRect(0, 0, widgetSize.width, widgetSize.height));

      // Now derive the area of the texture that will be visible to make sure
      // it's updated.
      gfxPoint scaleFactor = gfxPoint(bufferRect.width / (float)layerRect.width,
                                      bufferRect.height / (float)layerRect.height);
      float x1 = (clippedLayerRect.x - layerRect.x) * scaleFactor.x;
      float y1 = (clippedLayerRect.y - layerRect.y) * scaleFactor.y;
      float x2 = (clippedLayerRect.XMost() - layerRect.x) * scaleFactor.x;
      float y2 = (clippedLayerRect.YMost() - layerRect.y) * scaleFactor.y;

      // No need to clamp x2, y2, EnsureTextureUpdated will do that for us
      nsIntRect updateRect = nsIntRect(NS_MAX(0.0f, x1), NS_MAX(0.0f, y1),
                                       NS_MAX(0.0f, x2 - x1), NS_MAX(0.0f, y2 - y1));

      nsIntRegion updateRegion = updateRect;
      EnsureTextureUpdated(updateRegion);
    } else {
      // 3D or rotation transform, just give up and upload the whole thing.
      // XXX It's not hard to find the intersecting rect given a 3D transform, but
      //     it's more expensive and, I expect, less likely to give benefit.
      EnsureTextureUpdated();
    }
  }

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

  gl()->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, aPreviousFrameBuffer);
  mBuffer->RenderTo(aOffset, mOGLManager, 0);
}

void
ShadowThebesLayerOGL::CleanupResources()
{
  DestroyFrontBuffer();
}

} /* layers */
} /* mozilla */
