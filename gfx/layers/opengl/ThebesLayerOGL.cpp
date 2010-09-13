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

// |aTexCoordRect| is the texture rect in unnormalized texture space;
// width, height are the texture's natural
// size. |aTexCoordRect.TopLeft()| is the texture's "rotation": the
// texel that is to be the quad's top-left pixel.  This method
// normalizes |aTexCoordRect| to texture space and applies the
// specified texture rotation before drawing the quad.
static void
BindAndDrawQuadWithTextureRect(LayerProgram *aProg,
                               const nsIntRect& aTexCoordRect,
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

  GLfloat w(aTexCoordRect.width), h(aTexCoordRect.height);
  GLfloat xleft = GLfloat(aTexCoordRect.x) / w;
  GLfloat ytop = GLfloat(aTexCoordRect.y) / h;
  GLfloat texCoords[] = {
    xleft,         ytop,
    1.0f + xleft,  ytop,
    xleft,         1.0f + ytop,
    1.0f + xleft,  1.0f + ytop,
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

  ThebesLayerBufferOGL(ThebesLayerOGL* aLayer, TextureImage* aTexImage)
    : mLayer(aLayer)
    , mTexImage(aTexImage)
  {}
  virtual ~ThebesLayerBufferOGL() {}

  virtual PaintState BeginPaint(ContentType aContentType) = 0;

  void RenderTo(const nsIntPoint& aOffset, LayerManagerOGL* aManager);

protected:
  virtual nsIntRect GetTexCoordRectForRepeat() = 0;

  GLContext* gl() const { return mLayer->gl(); }

  ThebesLayerOGL* mLayer;
  nsRefPtr<TextureImage> mTexImage;
};

void
ThebesLayerBufferOGL::RenderTo(const nsIntPoint& aOffset,
                               LayerManagerOGL* aManager)
{
  // Note BGR: Cairo's image surfaces are always in what
  // OpenGL and our shaders consider BGR format.
  ColorTextureLayerProgram *program =
    aManager->GetBasicLayerProgram(mLayer->CanUseOpaqueSurface(),
                                   mTexImage->IsRGB());

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

  if (!mTexImage->InUpdate() || !mTexImage->EndUpdate()) {
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexImage->Texture());
  }

  nsIntRect quadRect = mLayer->GetVisibleRegion().GetBounds();
  program->Activate();
  program->SetLayerQuadRect(quadRect);
  program->SetLayerOpacity(mLayer->GetOpacity());
  program->SetLayerTransform(mLayer->GetTransform());
  program->SetRenderOffset(aOffset);
  program->SetTextureUnit(0);
  DEBUG_GL_ERROR_CHECK(gl());

  nsIntRect texCoordRect = GetTexCoordRectForRepeat();
  BindAndDrawQuadWithTextureRect(program, texCoordRect, gl());
  DEBUG_GL_ERROR_CHECK(gl());
}


// This implementation is the fast-path for when our TextureImage is
// permanently backed with a server-side ASurface.  We can simply
// reuse the ThebesLayerBuffer logic in its entirety and profit.
class SurfaceBufferOGL : public ThebesLayerBufferOGL, private ThebesLayerBuffer
{
public:
  typedef ThebesLayerBufferOGL::ContentType ContentType;
  typedef ThebesLayerBufferOGL::PaintState PaintState;

  SurfaceBufferOGL(ThebesLayerOGL* aLayer, TextureImage* aTexImage)
    : ThebesLayerBufferOGL(aLayer, aTexImage)
    , ThebesLayerBuffer(SizedToVisibleBounds)
  {
    mTmpSurface = mTexImage->GetBackingSurface();
    NS_ABORT_IF_FALSE(mTmpSurface, "SurfaceBuffer without backing surface??");
  }
  virtual ~SurfaceBufferOGL() {}

  // ThebesLayerBufferOGL interface
  virtual PaintState BeginPaint(ContentType aContentType)
  {
    // Let ThebesLayerBuffer do all the hard work for us! :D
    return ThebesLayerBuffer::BeginPaint(mLayer, aContentType);
  }

  // ThebesLayerBuffer interface
  virtual already_AddRefed<gfxASurface>
  CreateBuffer(ContentType aType, const nsIntSize& aSize)
  {
    NS_ASSERTION(gfxASurface::CONTENT_ALPHA != aType,"ThebesBuffer has color");

    if (mTmpSurface)
    {
      NS_ASSERTION(aSize == mTexImage->GetSize(),
                   "initial TextureImage is the wrong size");
      NS_ASSERTION(aType == mTexImage->GetContentType(),
                   "initial TextureImage has the wrong content type");
      // We were just created, and this is the first buffer paint.
      // This is the first time ThebesLayerBuffer has asked for a
      // buffer, so hand it the surface we already created.  From here
      // on we take the normal path below.
      return mTmpSurface.forget();
    }

    mTexImage = gl()->CreateTextureImage(aSize, aType, LOCAL_GL_REPEAT);
    return mTexImage ? mTexImage->GetBackingSurface() : nsnull;
  }

protected:
  virtual nsIntRect
  GetTexCoordRectForRepeat()
  {
    // BufferRect() mapped to unnormalized texture space, translated by
    // our rotation
    return nsIntRect(BufferRotation(), BufferRect().Size());
  }

private:
  nsRefPtr<gfxASurface> mTmpSurface;
};


// This implementation is (currently) the slow-path for when we can't
// implement pixel retaining using thebes.  This implementation and
// the above could be unified by abstracting buffer-copy operations
// and implementing them here using GL hacketry.
class BasicBufferOGL : public ThebesLayerBufferOGL
{
public:
  BasicBufferOGL(ThebesLayerOGL* aLayer, TextureImage* aTexImage)
    : ThebesLayerBufferOGL(aLayer, aTexImage)
  {}
  virtual ~BasicBufferOGL() {}

  virtual PaintState BeginPaint(ContentType aContentType);

protected:
  virtual nsIntRect
  GetTexCoordRectForRepeat()
  {
    // we don't rotate yet
    return nsIntRect(nsIntPoint(0, 0), mBufferRect.Size());
  }

private:
  nsIntRect mBufferRect;
};

BasicBufferOGL::PaintState
BasicBufferOGL::BeginPaint(ContentType aContentType)
{
  PaintState state;
  nsIntRect visibleRect = mLayer->GetVisibleRegion().GetBounds();

  if (aContentType != mTexImage->GetContentType() ||
      visibleRect.Size() != mTexImage->GetSize())
  {
    mBufferRect = nsIntRect();
    mTexImage = gl()->CreateTextureImage(visibleRect.Size(), aContentType,
                                         LOCAL_GL_REPEAT);
    DEBUG_GL_ERROR_CHECK(gl());
    if (!mTexImage) {
      return state;
    }
  }
  NS_ABORT_IF_FALSE((mTexImage->GetContentType() == aContentType &&
                     mTexImage->GetSize() == visibleRect.Size()),
                    "TextureImage matches layer attributes");

  state.mRegionToDraw = mLayer->GetVisibleRegion();
  if (mBufferRect != visibleRect) {
    // FIXME/bug 573829: keep some of these pixels, if we can!
    state.mRegionToInvalidate = mLayer->GetValidRegion();
    mBufferRect = visibleRect;
  } else {
    state.mRegionToDraw.Sub(state.mRegionToDraw, mLayer->GetValidRegion());
  }
  if (state.mRegionToDraw.IsEmpty()) {
    return state;
  }

  // Offset the region to draw by our visible region's origin, before
  // passing to BeginUpdate.  The TextureImage has no concept of an
  // origin, only a size, so it always represents a 0,0 origin area.
  // The layer however has a position, represented by its visible
  // region.  So we have to move things around so that we can interact
  // with the TextureImage.
  state.mRegionToDraw.MoveBy(-visibleRect.TopLeft());

  // BeginUpdate is allowed to modify the given region,
  // if it wants more to be repainted than we request.
  state.mContext = mTexImage->BeginUpdate(state.mRegionToDraw);
  if (!state.mContext) {
    NS_WARNING("unable to get context for update");
    return state;
  }

  // Move rgnToPaint back into position so that the thebes callback
  // gets the right coordintes.
  state.mRegionToDraw.MoveBy(visibleRect.TopLeft());

  // Translate the context so that we're matching the layer's
  // origin, not the 0,0-based TextureImage
  state.mContext->Translate(-gfxPoint(visibleRect.x, visibleRect.y));

  //ClipToRegion(ctx, rgnToDraw);
  if (gfxASurface::CONTENT_COLOR_ALPHA == aContentType) {
    state.mContext->SetOperator(gfxContext::OPERATOR_CLEAR);
    state.mContext->Paint();
    state.mContext->SetOperator(gfxContext::OPERATOR_OVER);
  }
  return state;
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

  nsIntSize visibleSize = mVisibleRegion.GetBounds().Size();
  TextureImage::ContentType contentType =
    CanUseOpaqueSurface() ? gfxASurface::CONTENT_COLOR :
                            gfxASurface::CONTENT_COLOR_ALPHA;
  nsRefPtr<TextureImage> teximage(
    gl()->CreateTextureImage(visibleSize, contentType,
                             LOCAL_GL_CLAMP_TO_EDGE));
  if (!teximage) {
    return PR_FALSE;
  }

  nsRefPtr<gfxASurface> surf = teximage->GetBackingSurface();
  if (surf) {
    // use the ThebesLayerBuffer fast-path
    mBuffer = new SurfaceBufferOGL(this, teximage);
  } else {
    mBuffer = new BasicBufferOGL(this, teximage);
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
ThebesLayerOGL::RenderLayer(int /*unused aPreviousFrameBuffer*/,
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
