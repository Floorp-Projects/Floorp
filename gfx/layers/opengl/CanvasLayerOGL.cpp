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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "CanvasLayerOGL.h"

#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "GLContextProvider.h"

#ifdef XP_WIN
#include "gfxWindowsSurface.h"
#include "WGLLibrary.h"
#endif

#ifdef XP_MACOSX
#include <OpenGL/OpenGL.h>
#endif

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gl;

CanvasLayerOGL::~CanvasLayerOGL()
{
  mOGLManager->MakeCurrent();

  if (mTexture) {
    gl()->fDeleteTextures(1, &mTexture);
  }
}

void
CanvasLayerOGL::Initialize(const Data& aData)
{
  NS_ASSERTION(mCanvasSurface == nsnull, "BasicCanvasLayer::Initialize called twice!");

  if (aData.mSurface) {
    mCanvasSurface = aData.mSurface;
    NS_ASSERTION(aData.mGLContext == nsnull,
                 "CanvasLayerOGL can't have both surface and GLContext");
    mNeedsYFlip = PR_FALSE;
    if (mCanvasSurface->GetType() == gfxASurface::SurfaceTypeXlib)
      mCanvasSurfaceAsGLContext = sGLContextProvider.CreateForNativePixmapSurface(mCanvasSurface);
  } else if (aData.mGLContext) {
    // this must be a pbuffer context
    void *pbuffer = aData.mGLContext->GetNativeData(GLContext::NativePBuffer);
    if (!pbuffer) {
      NS_WARNING("CanvasLayerOGL with GL context without NativePBuffer");
      return;
    }

    mCanvasGLContext = aData.mGLContext;
    mGLBufferIsPremultiplied = aData.mGLBufferIsPremultiplied;
    mNeedsYFlip = PR_TRUE;
  } else {
    NS_WARNING("CanvasLayerOGL::Initialize called without surface or GL context!");
    return;
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);
}

void
CanvasLayerOGL::Updated(const nsIntRect& aRect)
{
  NS_ASSERTION(mUpdatedRect.IsEmpty(),
               "CanvasLayer::Updated called more than once during a transaction!");

  mOGLManager->MakeCurrent();

  mUpdatedRect.UnionRect(mUpdatedRect, aRect);

  if (mCanvasSurfaceAsGLContext) {
    PRBool newTexture = mTexture == 0;
    if (newTexture) {
      gl()->fGenTextures(1, &mTexture);

      gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
      gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

      gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
      gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
      gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
      gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

      mUpdatedRect = mBounds;
    } else {
      gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
      gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    }
    mCanvasSurfaceAsGLContext->BindTexImage();
  } else if (mCanvasSurface) {
    PRBool newTexture = mTexture == 0;
    if (newTexture) {
      gl()->fGenTextures(1, &mTexture);

      gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
      gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

      gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
      gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
      gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
      gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

      mUpdatedRect = mBounds;
    } else {
      gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
      gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    }

    nsRefPtr<gfxImageSurface> updatedAreaImageSurface;
    nsRefPtr<gfxASurface> sourceSurface = mCanvasSurface;

#ifdef XP_WIN
    if (sourceSurface->GetType() == gfxASurface::SurfaceTypeWin32) {
      sourceSurface = static_cast<gfxWindowsSurface*>(sourceSurface.get())->GetImageSurface();
      if (!sourceSurface)
        sourceSurface = mCanvasSurface;
    }
#endif

#if 0
    // XXX don't copy, blah.
    // but need to deal with stride on the gl side; do this later.
    if (mCanvasSurface->GetType() == gfxASurface::SurfaceTypeImage) {
      gfxImageSurface *s = static_cast<gfxImageSurface*>(mCanvasSurface.get());
      if (s->Format() == gfxASurface::ImageFormatARGB32 ||
          s->Format() == gfxASurface::ImageFormatRGB24)
      {
        updatedAreaImageSurface = ...;
      } else {
        NS_WARNING("surface with format that we can't handle");
        return;
      }
    } else
#endif
    {
      updatedAreaImageSurface =
        new gfxImageSurface(gfxIntSize(mUpdatedRect.width, mUpdatedRect.height),
                            gfxASurface::ImageFormatARGB32);
      nsRefPtr<gfxContext> ctx = new gfxContext(updatedAreaImageSurface);
      ctx->Translate(gfxPoint(-mUpdatedRect.x, -mUpdatedRect.y));
      ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
      ctx->SetSource(sourceSurface);
      ctx->Paint();
    }

    if (newTexture) {
      gl()->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                        0,
                        LOCAL_GL_RGBA,
                        mUpdatedRect.width,
                        mUpdatedRect.height,
                        0,
                        LOCAL_GL_RGBA,
                        LOCAL_GL_UNSIGNED_BYTE,
                        updatedAreaImageSurface->Data());
    } else {
      gl()->fTexSubImage2D(LOCAL_GL_TEXTURE_2D,
                           0,
                           mUpdatedRect.x,
                           mUpdatedRect.y,
                           mUpdatedRect.width,
                           mUpdatedRect.height,
                           LOCAL_GL_RGBA,
                           LOCAL_GL_UNSIGNED_BYTE,
                           updatedAreaImageSurface->Data());
    }
  } else if (mCanvasGLContext) {
    // we just need to create a texture that we'll use, the first time through
    PRBool newTexture = mTexture == 0;
    if (newTexture) {
      gl()->fGenTextures(1, (GLuint*)&mTexture);

      gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

      gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
      gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
      gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
      gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

      mUpdatedRect = mBounds;
    }

#if defined(XP_MACOSX)
    // We only need to do this for the first time we set up the texture
    if (newTexture) {
      CGLError err;
      err = CGLTexImagePBuffer((CGLContextObj) gl()->GetNativeData(GLContext::NativeCGLContext),
                               (CGLPBufferObj) mCanvasGLContext->GetNativeData(GLContext::NativePBuffer),
                               LOCAL_GL_BACK);
      if (err) {
        NS_WARNING("CanvasLayerOGL::Updated CGLTexImagePBuffer failed");
      }
    }
#elif defined(XP_WIN)
    // We need to do this every time before we paint
    if (!sWGLLibrary.fBindTexImage((HANDLE) mCanvasGLContext->GetNativeData(GLContext::NativePBuffer),
                                   LOCAL_WGL_FRONT_LEFT_ARB)) {
      NS_WARNING("CanvasLayerOGL::Updated wglBindTexImageARB failed");
    }
#else
    NS_WARNING("CanvasLayerOGL::Updated with GL context, but I don't know how to render on this platform!");
#endif
  }

  // sanity
  NS_ASSERTION(mBounds.Contains(mUpdatedRect),
               "CanvasLayer: Updated rect bigger than bounds!");
}

void
CanvasLayerOGL::RenderLayer(int aPreviousDestination,
                            const nsIntPoint& aOffset)
{
  mOGLManager->MakeCurrent();

  // XXX We're going to need a different program depending on if
  // mGLBufferIsPremultiplied is TRUE or not.  The RGBLayerProgram
  // assumes that it's true.

  ColorTextureLayerProgram *program = nsnull;

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

  if (mCanvasGLContext || mCanvasSurfaceAsGLContext) {
    program = mOGLManager->GetRGBALayerProgram();
  } else {
    program = mOGLManager->GetBGRALayerProgram();
  }

  program->Activate();
  program->SetLayerQuadRect(mBounds);
  program->SetLayerTransform(mTransform);
  program->SetLayerOpacity(GetOpacity());
  program->SetRenderOffset(aOffset);
  program->SetTextureUnit(0);

  mOGLManager->BindAndDrawQuad(program, mNeedsYFlip ? true : false);

#if defined(XP_WIN)
  // We need to do this ever time after we paint, before anyone
  // draws into the pbuffer again
  if (mCanvasGLContext) {
    sWGLLibrary.fReleaseTexImage((HANDLE) mCanvasGLContext->GetNativeData(GLContext::NativePBuffer),
                                 LOCAL_WGL_FRONT_LEFT_ARB);
  }
#endif

  mUpdatedRect.Empty();
}
