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

#include "ThebesLayerOGL.h"
#include "ContainerLayerOGL.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#ifdef XP_WIN
#include "gfxWindowsSurface.h"
#endif
#include "GLContextProvider.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gl;

// Returns true if it's OK to save the contents of aLayer in an
// opaque surface (a surface without an alpha channel).
// If we can use a surface without an alpha channel, we should, because
// it will often make painting of antialiased text faster and higher
// quality.
static PRBool
UseOpaqueSurface(Layer* aLayer)
{
  // If the visible content in the layer is opaque, there is no need
  // for an alpha channel.
  if (aLayer->IsOpaqueContent())
    return PR_TRUE;
  // Also, if this layer is the bottommost layer in a container which
  // doesn't need an alpha channel, we can use an opaque surface for this
  // layer too. Any transparent areas must be covered by something else
  // in the container.
  ContainerLayerOGL* parent =
    static_cast<ContainerLayerOGL*>(aLayer->GetParent());
  return parent && parent->GetFirstChild() == aLayer &&
         UseOpaqueSurface(parent);
}


ThebesLayerOGL::ThebesLayerOGL(LayerManagerOGL *aManager)
  : ThebesLayer(aManager, nsnull)
  , LayerOGL(aManager)
  , mTexImage(nsnull)
{
  mImplData = static_cast<LayerOGL*>(this);
}

ThebesLayerOGL::~ThebesLayerOGL()
{
  mTexImage = nsnull;
  DEBUG_GL_ERROR_CHECK(gl());
}

PRBool
ThebesLayerOGL::EnsureSurface()
{
  nsIntSize visibleSize = mVisibleRegion.GetBounds().Size();
  TextureImage::ContentType contentType =
    UseOpaqueSurface(this) ? gfxASurface::CONTENT_COLOR :
                             gfxASurface::CONTENT_COLOR_ALPHA;
  if (!mTexImage ||
      mTexImage->GetSize() != visibleSize ||
      mTexImage->GetContentType() != contentType)
  {
    mValidRegion.SetEmpty();
    mTexImage = nsnull;
    DEBUG_GL_ERROR_CHECK(gl());
  }

  if (!mTexImage && !mVisibleRegion.IsEmpty())
  {
    mTexImage = gl()->CreateTextureImage(visibleSize,
                                         contentType,
                                         LOCAL_GL_CLAMP_TO_EDGE);
    DEBUG_GL_ERROR_CHECK(gl());
  }
  return !!mTexImage;
}

void
ThebesLayerOGL::SetVisibleRegion(const nsIntRegion &aRegion)
{
  if (aRegion.IsEqual(mVisibleRegion))
    return;
  ThebesLayer::SetVisibleRegion(aRegion);
  // FIXME/bug 573829: keep some of these pixels, if we can!
  mValidRegion.SetEmpty();
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
  if (!EnsureSurface())
    return;

  mOGLManager->MakeCurrent();
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

  nsIntRegion rgnToPaint = mVisibleRegion;
  rgnToPaint.Sub(rgnToPaint, mValidRegion);
  PRBool textureBound = PR_FALSE;
  if (!rgnToPaint.IsEmpty()) {
    nsIntRect visibleRect = mVisibleRegion.GetBounds();

    // Offset rgnToPaint by our visible region's origin, before
    // passing to BeginUpdate.  The TextureImage has no concept of an
    // origin, only a size, so it always represents a 0,0 origin area.
    // The layer however has a position, represented by its visible
    // region.  So we have to move things around so that we can
    // interact with the TextureImage.
    rgnToPaint.MoveBy(-visibleRect.TopLeft());

    // BeginUpdate is allowed to modify the given region,
    // if it wants more to be repainted than we request.
    nsRefPtr<gfxContext> ctx = mTexImage->BeginUpdate(rgnToPaint);
    if (!ctx) {
      NS_WARNING("unable to get context for update");
      return;
    }

    // Move rgnToPaint back into position so that the thebes callback
    // gets the right coordintes.
    rgnToPaint.MoveBy(visibleRect.TopLeft());

    // Translate the context so that we're matching the layer's
    // origin, not the 0,0-based TextureImage
    ctx->Translate(-gfxPoint(visibleRect.x, visibleRect.y));

    TextureImage::ContentType contentType = mTexImage->GetContentType();
    //ClipToRegion(ctx, rgnToDraw);
    if (gfxASurface::CONTENT_COLOR_ALPHA == contentType) {
      ctx->SetOperator(gfxContext::OPERATOR_CLEAR);
      ctx->Paint();
      ctx->SetOperator(gfxContext::OPERATOR_OVER);
    }

    mOGLManager->CallThebesLayerDrawCallback(this, ctx, rgnToPaint);

    textureBound = mTexImage->EndUpdate();
    mValidRegion.Or(mValidRegion, rgnToPaint);
  }

  if (!textureBound)
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexImage->Texture());

  // Note BGR: Cairo's image surfaces are always in what
  // OpenGL and our shaders consider BGR format.
  ColorTextureLayerProgram *program =
    UseOpaqueSurface(this)
    ? mOGLManager->GetBGRXLayerProgram()
    : mOGLManager->GetBGRALayerProgram();

  program->Activate();
  program->SetLayerQuadRect(mVisibleRegion.GetBounds());
  program->SetLayerOpacity(GetOpacity());
  program->SetLayerTransform(mTransform);
  program->SetRenderOffset(aOffset);
  program->SetTextureUnit(0);

  mOGLManager->BindAndDrawQuad(program);

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
  return !mTexImage;
}

} /* layers */
} /* mozilla */
