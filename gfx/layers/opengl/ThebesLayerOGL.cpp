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

#ifdef MOZ_IPC
# include "mozilla/layers/PLayers.h"
# include "mozilla/layers/ShadowLayers.h"
#endif

#include "ThebesLayerBuffer.h"
#include "ThebesLayerOGL.h"
#include "gfxUtils.h"
#include "gfxTeeSurface.h"

namespace mozilla {
namespace layers {

using gl::GLContext;
using gl::TextureImage;

// BindAndDrawQuadWithTextureRect can work with either GL_REPEAT (preferred)
// or GL_CLAMP_TO_EDGE textures.  We select based on whether REPEAT is
// valid for non-power-of-two textures -- if we have NPOT support we use it,
// otherwise we stick with CLAMP_TO_EDGE and decompose.
static already_AddRefed<TextureImage>
CreateClampOrRepeatTextureImage(GLContext *aGl,
                                const nsIntSize& aSize,
                                TextureImage::ContentType aContentType)
{
  GLenum wrapMode = LOCAL_GL_CLAMP_TO_EDGE;
  if (aGl->IsExtensionSupported(GLContext::ARB_texture_non_power_of_two) ||
      aGl->IsExtensionSupported(GLContext::OES_texture_npot))
  {
    wrapMode = LOCAL_GL_REPEAT;
  }

  return aGl->CreateTextureImage(aSize, aContentType, wrapMode);
}

// |aTexCoordRect| is the rectangle from the texture that we want to
// draw using the given program.  The program already has a necessary
// offset and scale, so the geometry that needs to be drawn is a unit
// square from 0,0 to 1,1.
//
// |aTexSize| is the actual size of the texture, as it can be larger
// than the rectangle given by |aTexCoordRect|.
static void
BindAndDrawQuadWithTextureRect(GLContext* aGl,
                               LayerProgram *aProg,
                               const nsIntRect& aTexCoordRect,
                               const nsIntSize& aTexSize,
                               GLenum aWrapMode)
{
  GLuint vertAttribIndex =
    aProg->AttribLocation(LayerProgram::VertexAttrib);
  GLuint texCoordAttribIndex =
    aProg->AttribLocation(LayerProgram::TexCoordAttrib);
  NS_ASSERTION(texCoordAttribIndex != GLuint(-1), "no texture coords?");

  // clear any bound VBO so that glVertexAttribPointer() goes back to
  // "pointer mode"
  aGl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  // Given what we know about these textures and coordinates, we can
  // compute fmod(t, 1.0f) to get the same texture coordinate out.  If
  // the texCoordRect dimension is < 0 or > width/height, then we have
  // wraparound that we need to deal with by drawing multiple quads,
  // because we can't rely on full non-power-of-two texture support
  // (which is required for the REPEAT wrap mode).

  GLContext::RectTriangles rects;

  if (aWrapMode == LOCAL_GL_REPEAT) {
    rects.addRect(/* dest rectangle */
                  0.0f, 0.0f, 1.0f, 1.0f,
                  /* tex coords */
                  aTexCoordRect.x / GLfloat(aTexSize.width),
                  aTexCoordRect.y / GLfloat(aTexSize.height),
                  aTexCoordRect.XMost() / GLfloat(aTexSize.width),
                  aTexCoordRect.YMost() / GLfloat(aTexSize.height));
  } else {
    GLContext::DecomposeIntoNoRepeatTriangles(aTexCoordRect, aTexSize, rects);
  }

  aGl->fVertexAttribPointer(vertAttribIndex, 2,
                            LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                            rects.vertexCoords);

  aGl->fVertexAttribPointer(texCoordAttribIndex, 2,
                            LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                            rects.texCoords);

  DEBUG_GL_ERROR_CHECK(aGl);

  {
    aGl->fEnableVertexAttribArray(texCoordAttribIndex);
    {
      aGl->fEnableVertexAttribArray(vertAttribIndex);

      aGl->fDrawArrays(LOCAL_GL_TRIANGLES, 0, rects.numRects * 6);
      DEBUG_GL_ERROR_CHECK(aGl);

      aGl->fDisableVertexAttribArray(vertAttribIndex);
    }
    aGl->fDisableVertexAttribArray(texCoordAttribIndex);
  }

  DEBUG_GL_ERROR_CHECK(aGl);
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
  {}
  virtual ~ThebesLayerBufferOGL() {}

  virtual PaintState BeginPaint(ContentType aContentType) = 0;

  void RenderTo(const nsIntPoint& aOffset, LayerManagerOGL* aManager);

  nsIntSize GetSize() {
    if (mTexImage)
      return mTexImage->GetSize();
    return nsIntSize(0, 0);
  }

protected:
  virtual nsIntPoint GetOriginOffset() = 0;

  GLContext* gl() const { return mOGLLayer->gl(); }

  ThebesLayer* mLayer;
  LayerOGL* mOGLLayer;
  nsRefPtr<TextureImage> mTexImage;
  nsRefPtr<TextureImage> mTexImageOnWhite;
};

void
ThebesLayerBufferOGL::RenderTo(const nsIntPoint& aOffset,
                               LayerManagerOGL* aManager)
{
  if (!mTexImage)
    return;

  if (mTexImage->InUpdate()) {
    mTexImage->EndUpdate();
  }

  if (mTexImageOnWhite && mTexImageOnWhite->InUpdate()) {
    mTexImageOnWhite->EndUpdate();
  }

  // Bind textures.
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexImage->Texture());

  if (mTexImageOnWhite) {
    gl()->fActiveTexture(LOCAL_GL_TEXTURE1);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexImageOnWhite->Texture());
  }

  float xres = mLayer->GetXResolution();
  float yres = mLayer->GetYResolution();

  PRInt32 passes = mTexImageOnWhite ? 2 : 1;
  for (PRInt32 pass = 1; pass <= passes; ++pass) {
    LayerProgram *program;

    if (passes == 2) {
      ComponentAlphaTextureLayerProgram *alphaProgram;
      NS_ASSERTION(!mTexImage->IsRGB() && !mTexImageOnWhite->IsRGB(),
                   "Only BGR image surported with component alpha (currently!)");
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
      DEBUG_GL_ERROR_CHECK(gl());
      alphaProgram->SetBlackTextureUnit(0);
      alphaProgram->SetWhiteTextureUnit(1);
      program = alphaProgram;
    } else {
      // Note BGR: Cairo's image surfaces are always in what
      // OpenGL and our shaders consider BGR format.
      ColorTextureLayerProgram *basicProgram =
        aManager->GetBasicLayerProgram(mLayer->CanUseOpaqueSurface(),
                                       mTexImage->IsRGB());

      basicProgram->Activate();
      DEBUG_GL_ERROR_CHECK(gl());
      basicProgram->SetTextureUnit(0);
      program = basicProgram;
    }

    program->SetLayerOpacity(mLayer->GetEffectiveOpacity());
    program->SetLayerTransform(mLayer->GetEffectiveTransform());
    program->SetRenderOffset(aOffset);

    nsIntRegionRectIterator iter(mLayer->GetEffectiveVisibleRegion());
    while (const nsIntRect *iterRect = iter.Next()) {
      nsIntRect quadRect = *iterRect;
      program->SetLayerQuadRect(quadRect);
      DEBUG_GL_ERROR_CHECK(gl());

      quadRect.MoveBy(-GetOriginOffset());

      // The buffer rect and rotation are resolution-neutral; with a
      // non-1.0 resolution, only the texture size is scaled by the
      // resolution.  So map the quadrent rect into the space scaled to
      // the texture size and let GL do the rest.
      gfxRect sqr(quadRect.x, quadRect.y, quadRect.width, quadRect.height);
      sqr.Scale(xres, yres);
      sqr.Round();
      nsIntRect scaledQuadRect(sqr.pos.x, sqr.pos.y, sqr.size.width, sqr.size.height);

      BindAndDrawQuadWithTextureRect(gl(), program, scaledQuadRect,
                                     mTexImage->GetSize(),
                                     mTexImage->GetWrapMode());
      DEBUG_GL_ERROR_CHECK(gl());
    }
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
  virtual PaintState BeginPaint(ContentType aContentType)
  {
    // Let ThebesLayerBuffer do all the hard work for us! :D
    return ThebesLayerBuffer::BeginPaint(mLayer, aContentType, 1.0, 1.0);
  }

  // ThebesLayerBuffer interface
  virtual already_AddRefed<gfxASurface>
  CreateBuffer(ContentType aType, const nsIntSize& aSize)
  {
    NS_ASSERTION(gfxASurface::CONTENT_ALPHA != aType,"ThebesBuffer has color");

    mTexImage = CreateClampOrRepeatTextureImage(gl(), aSize, aType);
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

  virtual PaintState BeginPaint(ContentType aContentType);

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
BasicBufferOGL::BeginPaint(ContentType aContentType)
{
  PaintState result;

  result.mRegionToDraw.Sub(mLayer->GetVisibleRegion(), mLayer->GetValidRegion());

  Layer::SurfaceMode mode = mLayer->GetSurfaceMode();

  if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    mode = Layer::SURFACE_SINGLE_CHANNEL_ALPHA;
#else
    if (!mLayer->GetParent() || !mLayer->GetParent()->SupportsComponentAlphaChildren()) {
      mode = Layer::SURFACE_SINGLE_CHANNEL_ALPHA;
    } else {
      aContentType = gfxASurface::CONTENT_COLOR;
    }
#endif
  }

  if (!mTexImage || mTexImage->GetContentType() != aContentType ||
      (mode == Layer::SURFACE_COMPONENT_ALPHA) != (mTexImageOnWhite != nsnull)) {
    // We're effectively clearing the valid region, so we need to draw
    // the entire visible region now.
    //
    // XXX/cjones: a possibly worthwhile optimization to keep in mind
    // is to re-use buffers when the resolution and visible region
    // have changed in such a way that the buffer size stays the same.
    // It might make even more sense to allocate buffers from a
    // recyclable pool, so that we could keep this logic simple and
    // still get back the same buffer.
    result.mRegionToDraw = mLayer->GetVisibleRegion();
    result.mRegionToInvalidate = mLayer->GetValidRegion();
    mTexImage = nsnull;
    mTexImageOnWhite = nsnull;
    mBufferRect.SetRect(0, 0, 0, 0);
    mBufferRotation.MoveTo(0, 0);
  }

  if (result.mRegionToDraw.IsEmpty())
    return result;

  nsIntRect visibleBounds = mLayer->GetVisibleRegion().GetBounds();
  if (visibleBounds.width > gl()->GetMaxTextureSize() ||
      visibleBounds.height > gl()->GetMaxTextureSize()) {
    return result;
  }

  nsIntRect drawBounds = result.mRegionToDraw.GetBounds();
  nsRefPtr<TextureImage> destBuffer;
  nsRefPtr<TextureImage> destBufferOnWhite;
  nsIntRect destBufferRect;

  if (visibleBounds.Size() <= mBufferRect.Size()) {
    // The current buffer is big enough to hold the visible area.
    if (mBufferRect.Contains(visibleBounds)) {
      // We don't need to adjust mBufferRect.
      destBufferRect = mBufferRect;
    } else {
      // The buffer's big enough but doesn't contain everything that's
      // going to be visible. We'll move it.
      destBufferRect = nsIntRect(visibleBounds.TopLeft(), mBufferRect.Size());
    }
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
          (drawBounds.y < yBoundary && yBoundary < drawBounds.YMost())) {
        // The stuff we need to redraw will wrap around an edge of the
        // buffer, so we will need to do a self-copy
        // If mBufferRotation == nsIntPoint(0,0) we could do a real
        // self-copy but we're not going to do that in GL yet.
        // We can't do a real self-copy because the buffer is rotated.
        // So allocate a new buffer for the destination.
        destBufferRect = visibleBounds;
        destBuffer = CreateClampOrRepeatTextureImage(gl(), visibleBounds.Size(), aContentType);
        DEBUG_GL_ERROR_CHECK(gl());
        if (!destBuffer)
          return result;
        if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
          destBufferOnWhite = 
            CreateClampOrRepeatTextureImage(gl(), visibleBounds.Size(), aContentType);
          DEBUG_GL_ERROR_CHECK(gl());
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
    destBufferRect = visibleBounds;
    destBuffer = CreateClampOrRepeatTextureImage(gl(), visibleBounds.Size(), aContentType);
    DEBUG_GL_ERROR_CHECK(gl());
    if (!destBuffer)
      return result;

    if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
      destBufferOnWhite = 
        CreateClampOrRepeatTextureImage(gl(), visibleBounds.Size(), aContentType);
      DEBUG_GL_ERROR_CHECK(gl());
      if (!destBufferOnWhite)
        return result;
    }
  }

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

        destBuffer->Resize(destBufferRect.Size());

        gl()->BlitTextureImage(mTexImage, srcRect,
                               destBuffer, dstRect);
        if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
          destBufferOnWhite->Resize(destBufferRect.Size());
          gl()->BlitTextureImage(mTexImageOnWhite, srcRect,
                                 destBufferOnWhite, dstRect);
        }
      } else {
        // can't blit, just draw everything
        destBufferRect = visibleBounds;
        destBuffer = CreateClampOrRepeatTextureImage(gl(), visibleBounds.Size(), aContentType);
        if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
          destBufferOnWhite = 
            CreateClampOrRepeatTextureImage(gl(), visibleBounds.Size(), aContentType);
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
    nsRefPtr<gfxTeeSurface> surf = new gfxTeeSurface(surfaces, NS_ARRAY_LENGTH(surfaces));

    // XXX If the device offset is set on the individual surfaces instead of on
    // the tee surface, we render in the wrong place. Why?
    gfxPoint deviceOffset = onBlack->GetDeviceOffset();
    onBlack->SetDeviceOffset(gfxPoint(0, 0));
    onWhite->SetDeviceOffset(gfxPoint(0, 0));
    surf->SetDeviceOffset(deviceOffset);

    // Using this surface as a source will likely go horribly wrong, since
    // only the onBlack surface will really be used, so alpha information will
    // be incorrect.
    surf->SetAllowUseAsSource(PR_FALSE);
    result.mContext = new gfxContext(surf);
  } else {
    result.mContext = new gfxContext(mTexImage->BeginUpdate(result.mRegionToDraw));
    if (mTexImage->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA) {
      gfxUtils::ClipToRegion(result.mContext, result.mRegionToDraw);
      result.mContext->SetOperator(gfxContext::OPERATOR_CLEAR);
      result.mContext->Fill();
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
    DEBUG_GL_ERROR_CHECK(gl());

    mDestroyed = PR_TRUE;
  }
}

PRBool
ThebesLayerOGL::CreateSurface()
{
  NS_ASSERTION(!mBuffer, "buffer already created?");

  if (mVisibleRegion.IsEmpty()) {
    return PR_FALSE;
  }

  if (gl()->TextureImageSupportsGetBackingSurface()) {
    // use the ThebesLayerBuffer fast-path
    mBuffer = new SurfaceBufferOGL(this);
  } else {
    mBuffer = new BasicBufferOGL(this);
  }
  return PR_TRUE;
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
  Buffer::PaintState state = mBuffer->BeginPaint(contentType);
  mValidRegion.Sub(mValidRegion, state.mRegionToInvalidate);

  if (state.mContext) {
    state.mRegionToInvalidate.And(state.mRegionToInvalidate, mVisibleRegion);

    LayerManager::DrawThebesLayerCallback callback =
      mOGLManager->GetThebesLayerCallback();
    void* callbackData = mOGLManager->GetThebesLayerCallbackData();
    callback(this, state.mContext, state.mRegionToDraw,
             state.mRegionToInvalidate, callbackData);
    // Everything that's visible has been validated. Do this instead of
    // OR-ing with aRegionToDraw, since that can lead to a very complex region
    // here (OR doesn't automatically simplify to the simplest possible
    // representation of a region.)
    mValidRegion.Or(mValidRegion, mVisibleRegion);
  }

  DEBUG_GL_ERROR_CHECK(gl());

  gl()->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, aPreviousFrameBuffer);
  mBuffer->RenderTo(aOffset, mOGLManager);
  DEBUG_GL_ERROR_CHECK(gl());
}

Layer*
ThebesLayerOGL::GetLayer()
{
  return this;
}

PRBool
ThebesLayerOGL::IsEmpty()
{
  return !mBuffer;
}


#ifdef MOZ_IPC

class ShadowBufferOGL : public ThebesLayerBufferOGL
{
public:
  ShadowBufferOGL(ShadowThebesLayerOGL* aLayer)
    : ThebesLayerBufferOGL(aLayer, aLayer)
  {}

  virtual PaintState BeginPaint(ContentType aContentType) {
    NS_RUNTIMEABORT("can't BeginPaint for a shadow layer");
    return PaintState();
  }

  void
  CreateTexture(ContentType aType, const nsIntSize& aSize)
  {
    NS_ASSERTION(gfxASurface::CONTENT_ALPHA != aType,"ThebesBuffer has color");

    mTexImage = CreateClampOrRepeatTextureImage(gl(), aSize, aType);
  }

  void Upload(gfxASurface* aUpdate, const nsIntRegion& aUpdated,
              const nsIntRect& aRect, const nsIntPoint& aRotation);

protected:
  virtual nsIntPoint GetOriginOffset() {
    return mBufferRect.TopLeft() - mBufferRotation;
  }

private:
  nsIntRect mBufferRect;
  nsIntPoint mBufferRotation;
};

void
ShadowBufferOGL::Upload(gfxASurface* aUpdate, const nsIntRegion& aUpdated,
                        const nsIntRect& aRect, const nsIntPoint& aRotation)
{
  gfxIntSize size = aUpdate->GetSize();
  if (GetSize() != nsIntSize(size.width, size.height)) {
    CreateTexture(aUpdate->GetContentType(),
                  nsIntSize(size.width, size.height));
  }

  nsIntRegion destRegion(aUpdated);
  // aUpdated is in screen coordinates.  Move it so that the layer's
  // top-left is 0,0
  nsIntPoint visTopLeft = mLayer->GetVisibleRegion().GetBounds().TopLeft();
  destRegion.MoveBy(-visTopLeft);

  // |aUpdated|, |aRect|, and |aRotation| are in thebes-layer space,
  // unadjusted for resolution.  The texture is in device space, so
  // first we need to map the update params to device space.
  //
  // XXX this prematurely commits us to updating rects instead of
  // regions here.  This will be a perf penalty on platforms that
  // support region updates.  This is OK for now because the
  // TextureImage backends we care about need to update contiguous
  // rects anyway, and would do this conversion internally.  To fix
  // this, we would need to scale the region instead of its bounds
  // here.
  nsIntRect destBounds = destRegion.GetBounds();
  gfxRect destRect(destBounds.x, destBounds.y, destBounds.width, destBounds.height);
  destRect.Scale(mLayer->GetXResolution(), mLayer->GetYResolution());
  destRect.RoundOut();

  // NB: this gfxContext must not escape EndUpdate() below
  nsIntRegion scaledDestRegion(nsIntRect(destRect.pos.x, destRect.pos.y,
                                         destRect.size.width, destRect.size.height));
  mTexImage->DirectUpdate(aUpdate, scaledDestRegion);

  mBufferRect = aRect;
  mBufferRotation = aRotation;
}

ShadowThebesLayerOGL::ShadowThebesLayerOGL(LayerManagerOGL *aManager)
  : ShadowThebesLayer(aManager, nsnull)
  , LayerOGL(aManager)
{
  mImplData = static_cast<LayerOGL*>(this);
}

ShadowThebesLayerOGL::~ShadowThebesLayerOGL()
{}

void
ShadowThebesLayerOGL::SetFrontBuffer(const OptionalThebesBuffer& aNewFront,
                                     const nsIntRegion& aValidRegion,
                                     float aXResolution, float aYResolution)
{
  if (mDestroyed) {
    return;
  }

  if (!mBuffer) {
    mBuffer = new ShadowBufferOGL(this);
  }

  NS_ASSERTION(OptionalThebesBuffer::Tnull_t == aNewFront.type(),
               "Only one system-memory buffer expected");
}

void
ShadowThebesLayerOGL::Swap(const ThebesBuffer& aNewFront,
                           const nsIntRegion& aUpdatedRegion,
                           ThebesBuffer* aNewBack,
                           nsIntRegion* aNewBackValidRegion,
                           float* aNewXResolution, float* aNewYResolution,
                           OptionalThebesBuffer* aReadOnlyFront,
                           nsIntRegion* aFrontUpdatedRegion)
{
  if (!mDestroyed && mBuffer) {
    nsRefPtr<gfxASurface> surf = ShadowLayerForwarder::OpenDescriptor(aNewFront.buffer());
    mBuffer->Upload(surf, aUpdatedRegion, aNewFront.rect(), aNewFront.rotation());
  }

  *aNewBack = aNewFront;
  *aNewBackValidRegion = mValidRegion;
  *aNewXResolution = mXResolution;
  *aNewYResolution = mYResolution;
  *aReadOnlyFront = null_t();
  aFrontUpdatedRegion->SetEmpty();
}

void
ShadowThebesLayerOGL::DestroyFrontBuffer()
{
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
    mDestroyed = PR_TRUE;
    mBuffer = nsnull;
  }
}

Layer*
ShadowThebesLayerOGL::GetLayer()
{
  return this;
}

PRBool
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
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  DEBUG_GL_ERROR_CHECK(gl());

  gl()->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, aPreviousFrameBuffer);
  mBuffer->RenderTo(aOffset, mOGLManager);
  DEBUG_GL_ERROR_CHECK(gl());
}

#endif  // MOZ_IPC


} /* layers */
} /* mozilla */
