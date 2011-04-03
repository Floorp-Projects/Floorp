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

#include "gfxSharedImageSurface.h"

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
      
  // Check the maximum texture size supported by GL. glTexImage2D supports
  // images of up to 2 + GL_MAX_TEXTURE_SIZE
  GLint texSize = gl()->GetMaxTextureSize();
  if (mBounds.width > (2 + texSize) || mBounds.height > (2 + texSize)) {
    mDelayedUpdates = PR_TRUE;
    MakeTexture();
    // This should only ever occur with 2d canvas, WebGL can't already have a texture
    // of this size can it?
    NS_ABORT_IF_FALSE(mCanvasSurface, 
                      "Invalid texture size when WebGL surface already exists at that size?");
  }
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
CanvasLayerOGL::UpdateSurface()
{
  if (!mDirty)
    return;
  mDirty = PR_FALSE;

  if (mDestroyed || mDelayedUpdates) {
    return;
  }

  mOGLManager->MakeCurrent();

  if (mCanvasGLContext &&
      mCanvasGLContext->GetContextType() == gl()->GetContextType())
  {
    if (gl()->BindOffscreenNeedsTexture(mCanvasGLContext) &&
        mTexture == 0)
    {
      MakeTexture();
    }
  } else {
    nsRefPtr<gfxASurface> updatedAreaSurface;
    if (mCanvasSurface) {
      updatedAreaSurface = mCanvasSurface;
    } else if (mCanvasGLContext) {
      nsRefPtr<gfxImageSurface> updatedAreaImageSurface =
        new gfxImageSurface(gfxIntSize(mBounds.width, mBounds.height),
                            gfxASurface::ImageFormatARGB32);
      mCanvasGLContext->ReadPixelsIntoImageSurface(0, 0,
                                                   mBounds.width,
                                                   mBounds.height,
                                                   updatedAreaImageSurface);
      updatedAreaSurface = updatedAreaImageSurface;
    }

    mLayerProgram =
      gl()->UploadSurfaceToTexture(updatedAreaSurface,
                                   mBounds,
                                   mTexture,
                                   false,
                                   nsIntPoint(0, 0));
  }
}

void
CanvasLayerOGL::RenderLayer(int aPreviousDestination,
                            const nsIntPoint& aOffset)
{
  UpdateSurface();
  FireDidTransactionCallback();

  mOGLManager->MakeCurrent();

  // XXX We're going to need a different program depending on if
  // mGLBufferIsPremultiplied is TRUE or not.  The RGBLayerProgram
  // assumes that it's true.

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

  if (mTexture) {
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
  }

  ColorTextureLayerProgram *program = nsnull;

  bool useGLContext = mCanvasGLContext &&
    mCanvasGLContext->GetContextType() == gl()->GetContextType();

  nsIntRect drawRect = mBounds;

  if (useGLContext) {
    mCanvasGLContext->MakeCurrent();
    mCanvasGLContext->fFlush();

    gl()->MakeCurrent();
    gl()->BindTex2DOffscreen(mCanvasGLContext);
    program = mOGLManager->GetBasicLayerProgram(CanUseOpaqueSurface(), PR_TRUE);
  } else if (mDelayedUpdates) {
    NS_ABORT_IF_FALSE(mCanvasSurface, "WebGL canvases should always be using full texture upload");
    
    drawRect.IntersectRect(drawRect, GetEffectiveVisibleRegion().GetBounds());

    mLayerProgram =
      gl()->UploadSurfaceToTexture(mCanvasSurface,
                                   nsIntRect(0, 0, drawRect.width, drawRect.height),
                                   mTexture,
                                   true,
                                   drawRect.TopLeft());
  }
  if (!program) { 
    program = mOGLManager->GetColorTextureLayerProgram(mLayerProgram);
  }

  ApplyFilter(mFilter);

  program->Activate();
  program->SetLayerQuadRect(drawRect);
  program->SetLayerTransform(GetEffectiveTransform());
  program->SetLayerOpacity(GetEffectiveOpacity());
  program->SetRenderOffset(aOffset);
  program->SetTextureUnit(0);

  mOGLManager->BindAndDrawQuad(program, mNeedsYFlip ? true : false);

  if (useGLContext) {
    gl()->UnbindTex2DOffscreen(mCanvasGLContext);
  }
}


ShadowCanvasLayerOGL::ShadowCanvasLayerOGL(LayerManagerOGL* aManager)
  : ShadowCanvasLayer(aManager, nsnull)
  , LayerOGL(aManager)
{
  mImplData = static_cast<LayerOGL*>(this);
}
 
ShadowCanvasLayerOGL::~ShadowCanvasLayerOGL()
{}

void
ShadowCanvasLayerOGL::Initialize(const Data& aData)
{
  mDeadweight = static_cast<gfxSharedImageSurface*>(aData.mSurface);
  gfxSize sz = mDeadweight->GetSize();
  mTexImage = gl()->CreateTextureImage(nsIntSize(sz.width, sz.height),
                                       mDeadweight->GetContentType(),
                                       LOCAL_GL_CLAMP_TO_EDGE);
}

already_AddRefed<gfxSharedImageSurface>
ShadowCanvasLayerOGL::Swap(gfxSharedImageSurface* aNewFront)
{
  if (!mDestroyed && mTexImage) {
    // XXX this is always just ridiculously slow

    gfxSize sz = aNewFront->GetSize();
    nsIntRegion updateRegion(nsIntRect(0, 0, sz.width, sz.height));
    mTexImage->DirectUpdate(aNewFront, updateRegion);
  }

  return aNewFront;
}

void
ShadowCanvasLayerOGL::DestroyFrontBuffer()
{
  mTexImage = nsnull;
  if (mDeadweight) {
    mOGLManager->DestroySharedSurface(mDeadweight, mAllocator);
    mDeadweight = nsnull;
  }
}

void
ShadowCanvasLayerOGL::Disconnect()
{
  Destroy();
}

void
ShadowCanvasLayerOGL::Destroy()
{
  if (!mDestroyed) {
    mDestroyed = PR_TRUE;
    mTexImage = nsnull;
  }
}

Layer*
ShadowCanvasLayerOGL::GetLayer()
{
  return this;
}

void
ShadowCanvasLayerOGL::RenderLayer(int aPreviousFrameBuffer,
                                  const nsIntPoint& aOffset)
{
  mOGLManager->MakeCurrent();

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexImage->Texture());
  ColorTextureLayerProgram *program =
    mOGLManager->GetColorTextureLayerProgram(mTexImage->GetShaderProgramType());

  ApplyFilter(mFilter);

  program->Activate();
  program->SetLayerQuadRect(nsIntRect(nsIntPoint(0, 0), mTexImage->GetSize()));
  program->SetLayerTransform(GetEffectiveTransform());
  program->SetLayerOpacity(GetEffectiveOpacity());
  program->SetRenderOffset(aOffset);
  program->SetTextureUnit(0);

  mOGLManager->BindAndDrawQuad(program);
}
