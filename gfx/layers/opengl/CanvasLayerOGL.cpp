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

void
CanvasLayerOGL::Destroy()
{
  if (!mDestroyed) {
    if (mTexture) {
      GLContext *cx = mOGLManager->glForResources();
      cx->MakeCurrent();
      cx->fDeleteTextures(1, &mTexture);
    }

    mDestroyed = PR_TRUE;
  }
}

void
CanvasLayerOGL::Initialize(const Data& aData)
{
  NS_ASSERTION(mCanvasSurface == nsnull, "BasicCanvasLayer::Initialize called twice!");

  if (aData.mGLContext != nsnull &&
      aData.mSurface != nsnull)
  {
    NS_WARNING("CanvasLayerOGL can't have both surface and GLContext");
    return;
  }

  if (aData.mSurface) {
    mCanvasSurface = aData.mSurface;
    mNeedsYFlip = PR_FALSE;
  } else if (aData.mGLContext) {
    if (!aData.mGLContext->IsOffscreen()) {
      NS_WARNING("CanvasLayerOGL with a non-offscreen GL context given");
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
CanvasLayerOGL::MakeTexture()
{
  if (mTexture != 0)
    return;

  gl()->fGenTextures(1, &mTexture);

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

  gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
}

void
CanvasLayerOGL::Updated(const nsIntRect& aRect)
{
  if (mDestroyed) {
    return;
  }

  NS_ASSERTION(mUpdatedRect.IsEmpty(),
               "CanvasLayer::Updated called more than once during a transaction!");

  mOGLManager->MakeCurrent();

  mUpdatedRect.UnionRect(mUpdatedRect, aRect);

  if (mCanvasGLContext &&
      mCanvasGLContext->GetContextType() == gl()->GetContextType())
  {
    if (gl()->BindOffscreenNeedsTexture(mCanvasGLContext) &&
        mTexture == 0)
    {
      MakeTexture();
    }
  } else {
    PRBool newTexture = mTexture == 0;
    if (newTexture) {
      MakeTexture();
      mUpdatedRect = mBounds;
    } else {
      gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
      gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    }

    nsRefPtr<gfxImageSurface> updatedAreaImageSurface;
    if (mCanvasSurface) {
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
    } else if (mCanvasGLContext) {
      updatedAreaImageSurface =
        new gfxImageSurface(gfxIntSize(mUpdatedRect.width, mUpdatedRect.height),
                            gfxASurface::ImageFormatARGB32);
      mCanvasGLContext->ReadPixelsIntoImageSurface(mUpdatedRect.x, mUpdatedRect.y,
                                                   mUpdatedRect.width,
                                                   mUpdatedRect.height,
                                                   updatedAreaImageSurface);
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

  if (mTexture) {
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
  }

  bool useGLContext = mCanvasGLContext &&
    mCanvasGLContext->GetContextType() == gl()->GetContextType();

  if (useGLContext) {
    gl()->BindTex2DOffscreen(mCanvasGLContext);
    DEBUG_GL_ERROR_CHECK(gl());
    program = mOGLManager->GetRGBALayerProgram();
  } else {
    program = mOGLManager->GetBGRALayerProgram();
  }

  ApplyFilter(mFilter);

  program->Activate();
  program->SetLayerQuadRect(mBounds);
  program->SetLayerTransform(mTransform);
  program->SetLayerOpacity(GetOpacity());
  program->SetRenderOffset(aOffset);
  program->SetTextureUnit(0);

  mOGLManager->BindAndDrawQuad(program, mNeedsYFlip ? true : false);

  DEBUG_GL_ERROR_CHECK(gl());

  if (useGLContext) {
    gl()->UnbindTex2DOffscreen(mCanvasGLContext);
  }

  mUpdatedRect.Empty();
}
