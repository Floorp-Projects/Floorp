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

#include "ThebesLayerBuffer.h"
#include "ThebesLayerOGL.h"

namespace mozilla {
namespace layers {

using gl::GLContext;
using gl::TextureImage;

// |aTexCoordRect| is the rectangle from the texture that we want to
// draw using the given program.  The program already has a necessary
// offset and scale, so the geometry that needs to be drawn is a unit
// square from 0,0 to 1,1.
//
// |aTexSize| is the actual size of the texture, as it can be larger
// than the rectangle given by |aTexCoordRect|.
static void
BindAndDrawQuadWithTextureRect(LayerProgram *aProg,
                               const nsIntRect& aTexCoordRect,
                               const nsIntSize& aTexSize,
                               GLContext* aGl)
{
  GLuint vertAttribIndex =
    aProg->AttribLocation(LayerProgram::VertexAttrib);
  GLuint texCoordAttribIndex =
    aProg->AttribLocation(LayerProgram::TexCoordAttrib);
  NS_ASSERTION(texCoordAttribIndex != GLuint(-1), "no texture coords?");

  // clear any bound VBO so that glVertexAttribPointer() goes back to
  // "pointer mode"
  aGl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  // NB: quadVertices and texCoords vertices must match
  GLfloat quadVertices[] = {
    0.0f, 0.0f,                 // bottom left
    1.0f, 0.0f,                 // bottom right
    0.0f, 1.0f,                 // top left
    1.0f, 1.0f                  // top right
  };
  aGl->fVertexAttribPointer(vertAttribIndex, 2,
                            LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                            quadVertices);
  DEBUG_GL_ERROR_CHECK(aGl);

  GLfloat xleft = GLfloat(aTexCoordRect.x) / GLfloat(aTexSize.width);
  GLfloat ytop = GLfloat(aTexCoordRect.y) / GLfloat(aTexSize.height);
  GLfloat w = GLfloat(aTexCoordRect.width) / GLfloat(aTexSize.width);
  GLfloat h = GLfloat(aTexCoordRect.height) / GLfloat(aTexSize.height);
  GLfloat texCoords[] = {
    xleft,     ytop,
    w + xleft, ytop,
    xleft,     h + ytop,
    w + xleft, h + ytop,
  };

  aGl->fVertexAttribPointer(texCoordAttribIndex, 2,
                            LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                            texCoords);
  DEBUG_GL_ERROR_CHECK(aGl);

  {
    aGl->fEnableVertexAttribArray(texCoordAttribIndex);
    {
      aGl->fEnableVertexAttribArray(vertAttribIndex);

      aGl->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
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

  ThebesLayerBufferOGL(ThebesLayerOGL* aLayer)
    : mLayer(aLayer)
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

  GLContext* gl() const { return mLayer->gl(); }

  ThebesLayerOGL* mLayer;
  nsRefPtr<TextureImage> mTexImage;
};

void
ThebesLayerBufferOGL::RenderTo(const nsIntPoint& aOffset,
                               LayerManagerOGL* aManager)
{
  if (!mTexImage)
    return;

  // Note BGR: Cairo's image surfaces are always in what
  // OpenGL and our shaders consider BGR format.
  ColorTextureLayerProgram *program =
    aManager->GetBasicLayerProgram(mLayer->CanUseOpaqueSurface(),
                                   mTexImage->IsRGB());

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

  if (!mTexImage->InUpdate() || !mTexImage->EndUpdate()) {
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexImage->Texture());
  }

  float xres = mLayer->GetXResolution();
  float yres = mLayer->GetYResolution();

  nsIntRegionRectIterator iter(mLayer->GetVisibleRegion());
  while (const nsIntRect *iterRect = iter.Next()) {
    nsIntRect quadRect = *iterRect;
    program->Activate();
    program->SetLayerQuadRect(quadRect);
    program->SetLayerOpacity(mLayer->GetOpacity());
    program->SetLayerTransform(mLayer->GetTransform());
    program->SetRenderOffset(aOffset);
    program->SetTextureUnit(0);
    DEBUG_GL_ERROR_CHECK(gl());

    quadRect.MoveBy(-GetOriginOffset());

    // The buffer rect and rotation are resolution-neutral; with a
    // non-1.0 resolution, only the texture size is scaled by the
    // resolution.  So map the quadrent rect into the space scaled to
    // the texture size and let GL do the rest.
    gfxRect sqr(quadRect.x, quadRect.y, quadRect.width, quadRect.height);
    sqr.Scale(xres, yres);
    sqr.RoundOut();
    nsIntRect scaledQuadRect(sqr.pos.x, sqr.pos.y, sqr.size.width, sqr.size.height);

    BindAndDrawQuadWithTextureRect(program, scaledQuadRect, mTexImage->GetSize(), gl());
    DEBUG_GL_ERROR_CHECK(gl());
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
    : ThebesLayerBufferOGL(aLayer)
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

    mTexImage = gl()->CreateTextureImage(aSize, aType, LOCAL_GL_REPEAT);
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
    : ThebesLayerBufferOGL(aLayer)
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

BasicBufferOGL::PaintState
BasicBufferOGL::BeginPaint(ContentType aContentType)
{
  PaintState result;

  result.mRegionToDraw.Sub(mLayer->GetVisibleRegion(), mLayer->GetValidRegion());

  if (!mTexImage || mTexImage->GetContentType() != aContentType) {
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
    mBufferRect.SetRect(0, 0, 0, 0);
    mBufferRotation.MoveTo(0, 0);
  }

  if (result.mRegionToDraw.IsEmpty())
    return result;

  nsIntRect drawBounds = result.mRegionToDraw.GetBounds();
  nsIntRect visibleBounds = mLayer->GetVisibleRegion().GetBounds();
  nsRefPtr<TextureImage> destBuffer;
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
        destBuffer = gl()->CreateTextureImage(visibleBounds.Size(), aContentType,
                                              LOCAL_GL_REPEAT);
        DEBUG_GL_ERROR_CHECK(gl());
        if (!destBuffer)
          return result;
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
    destBuffer = gl()->CreateTextureImage(visibleBounds.Size(), aContentType,
                                          LOCAL_GL_REPEAT);
    DEBUG_GL_ERROR_CHECK(gl());
    if (!destBuffer)
      return result;
  }

  if (!destBuffer && !mTexImage) {
    return result;
  }

  if (destBuffer) {
    if (mTexImage) {
      // BlitTextureImage depends on the FBO texture target being
      // TEXTURE_2D.  This isn't the case on some older X1600-era Radeons.
      if (mLayer->mOGLManager->FBOTextureTarget() == LOCAL_GL_TEXTURE_2D) {
        nsIntRect overlap;
        overlap.IntersectRect(mBufferRect, destBufferRect);

        nsIntRect srcRect(overlap), dstRect(overlap);
        srcRect.MoveBy(- mBufferRect.TopLeft() + mBufferRotation);
        dstRect.MoveBy(- destBufferRect.TopLeft());

        destBuffer->Resize(destBufferRect.Size());

        gl()->BlitTextureImage(mTexImage, srcRect,
                               destBuffer, dstRect);
      } else {
        // can't blit, just draw everything
        destBufferRect = visibleBounds;
        destBuffer = gl()->CreateTextureImage(visibleBounds.Size(), aContentType,
                                              LOCAL_GL_REPEAT);
      }
    }

    mTexImage = destBuffer.forget();
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
  result.mContext = mTexImage->BeginUpdate(result.mRegionToDraw);
  result.mContext->Translate(-gfxPoint(quadrantRect.x, quadrantRect.y));
  if (!result.mContext) {
    NS_WARNING("unable to get context for update");
    return result;
  }
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
    mValidRegion.Or(mValidRegion, state.mRegionToDraw);
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

} /* layers */
} /* mozilla */
