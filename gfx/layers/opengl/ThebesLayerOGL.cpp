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
  : ThebesLayer(aManager, NULL)
  , LayerOGL(aManager)
  , mTexture(0)
  , mOffscreenFormat(gfxASurface::ImageFormatUnknown)
  , mOffscreenSize(-1,-1)
{
  mImplData = static_cast<LayerOGL*>(this);
}

ThebesLayerOGL::~ThebesLayerOGL()
{
  mOGLManager->MakeCurrent();
  if (mOffscreenSurfaceAsGLContext)
    mOffscreenSurfaceAsGLContext->ReleaseTexImage();
  if (mTexture) {
    gl()->fDeleteTextures(1, &mTexture);
  }
}

PRBool
ThebesLayerOGL::EnsureSurface()
{
  gfxASurface::gfxImageFormat imageFormat = gfxASurface::ImageFormatARGB32;
  if (UseOpaqueSurface(this))
    imageFormat = gfxASurface::ImageFormatRGB24;

  if (mInvalidatedRect.IsEmpty())
    return mOffScreenSurface ? PR_TRUE : PR_FALSE;

  if ((mOffscreenSize == gfxIntSize(mInvalidatedRect.width, mInvalidatedRect.height)
      && imageFormat == mOffscreenFormat)
      || mInvalidatedRect.IsEmpty())
    return mOffScreenSurface ? PR_TRUE : PR_FALSE;

  mOffScreenSurface =
    gfxPlatform::GetPlatform()->
      CreateOffscreenSurface(gfxIntSize(mInvalidatedRect.width, mInvalidatedRect.height),
                             imageFormat);
  if (!mOffScreenSurface)
    return PR_FALSE;

  if (mOffScreenSurface) {
    mOffscreenSize.width = mInvalidatedRect.width;
    mOffscreenSize.height = mInvalidatedRect.height;
    mOffscreenFormat = imageFormat;
  }


  mOGLManager->MakeCurrent();

  if (!mTexture)
    gl()->fGenTextures(1, &mTexture);

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
  gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  // Try bind our offscreen surface directly to texture
  mOffscreenSurfaceAsGLContext = sGLContextProvider.CreateForNativePixmapSurface(mOffScreenSurface);
  // Bind GL surface to the texture, and return
  if (mOffscreenSurfaceAsGLContext)
    return mOffscreenSurfaceAsGLContext->BindTexImage();

  // Otherwise allocate new texture
  gl()->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                    0,
                    LOCAL_GL_RGBA,
                    mInvalidatedRect.width,
                    mInvalidatedRect.height,
                    0,
                    LOCAL_GL_RGBA,
                    LOCAL_GL_UNSIGNED_BYTE,
                    NULL);

  DEBUG_GL_ERROR_CHECK(gl());
  return PR_TRUE;
}

void
ThebesLayerOGL::SetVisibleRegion(const nsIntRegion &aRegion)
{
  if (aRegion.IsEqual(mVisibleRegion))
    return;

  ThebesLayer::SetVisibleRegion(aRegion);

  mInvalidatedRect = mVisibleRegion.GetBounds();
}

void
ThebesLayerOGL::InvalidateRegion(const nsIntRegion &aRegion)
{
  nsIntRegion invalidatedRegion;
  invalidatedRegion.Or(aRegion, mInvalidatedRect);
  invalidatedRegion.And(invalidatedRegion, mVisibleRegion);
  mInvalidatedRect = invalidatedRegion.GetBounds();
}

LayerOGL::LayerType
ThebesLayerOGL::GetType()
{
  return TYPE_THEBES;
}

void
ThebesLayerOGL::RenderLayer(int aPreviousFrameBuffer,
                            const nsIntPoint& aOffset)
{
  if (!EnsureSurface())
    return;

  if (!mTexture)
    return;

  mOGLManager->MakeCurrent();
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

  bool needsTextureBind = true;
  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  nsRefPtr<gfxASurface> surface = mOffScreenSurface;
  gfxASurface::gfxImageFormat imageFormat = mOffscreenFormat;

  if (!mInvalidatedRect.IsEmpty()) {
    nsRefPtr<gfxContext> ctx = new gfxContext(surface);
    ctx->Translate(gfxPoint(-mInvalidatedRect.x, -mInvalidatedRect.y));

    /* Call the thebes layer callback */
    mOGLManager->CallThebesLayerDrawCallback(this, ctx, mInvalidatedRect);
  }

  // If draw callback happend and we don't have native surface
  if (!mInvalidatedRect.IsEmpty() && !mOffscreenSurfaceAsGLContext) {
    /* Then take its results and put it in an image surface,
     * in preparation for a texture upload */
    nsRefPtr<gfxImageSurface> imageSurface;

    switch (surface->GetType()) {
      case gfxASurface::SurfaceTypeImage:
        imageSurface = static_cast<gfxImageSurface*>(surface.get());
        break;
#ifdef XP_WIN
      case gfxASurface::SurfaceTypeWin32:
        imageSurface =
          static_cast<gfxWindowsSurface*>(surface.get())->
            GetImageSurface();
        break;
#endif
      default:
        /** 
         * XXX - This is very undesirable. Implement this for other platforms in
         * a more efficient way as well!
         */
        {
          imageSurface = new gfxImageSurface(gfxIntSize(mInvalidatedRect.width,
                                                        mInvalidatedRect.height),
                                             imageFormat);
          nsRefPtr<gfxContext> tmpContext = new gfxContext(imageSurface);
          tmpContext->SetSource(surface);
          tmpContext->SetOperator(gfxContext::OPERATOR_SOURCE);
          tmpContext->Paint();
        }
        break;
    }

    // Upload image to texture (slow)
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    gl()->fTexSubImage2D(LOCAL_GL_TEXTURE_2D,
                         0,
                         mInvalidatedRect.x - visibleRect.x,
                         mInvalidatedRect.y - visibleRect.y,
                         mInvalidatedRect.width,
                         mInvalidatedRect.height,
                         LOCAL_GL_RGBA,
                         LOCAL_GL_UNSIGNED_BYTE,
                         imageSurface->Data());

    needsTextureBind = false;
  }

  if (needsTextureBind)
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

  // Note BGR: Cairo's image surfaces are always in what
  // OpenGL and our shaders consider BGR format.
  ColorTextureLayerProgram *program =
    UseOpaqueSurface(this)
    ? mOGLManager->GetBGRXLayerProgram()
    : mOGLManager->GetBGRALayerProgram();

  program->Activate();
  program->SetLayerQuadRect(visibleRect);
  program->SetLayerOpacity(GetOpacity());
  program->SetLayerTransform(mTransform);
  program->SetRenderOffset(aOffset);
  program->SetTextureUnit(0);

  mOGLManager->BindAndDrawQuad(program);

  DEBUG_GL_ERROR_CHECK(gl());
}

const nsIntRect&
ThebesLayerOGL::GetInvalidatedRect()
{
  return mInvalidatedRect;
}

Layer*
ThebesLayerOGL::GetLayer()
{
  return this;
}

PRBool
ThebesLayerOGL::IsEmpty()
{
  return !mTexture;
}

} /* layers */
} /* mozilla */
