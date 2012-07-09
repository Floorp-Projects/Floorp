/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayers.h"
#include "mozilla/layers/ShadowLayers.h"

#include "gfxSharedImageSurface.h"

#include "CanvasLayerOGL.h"

#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "GLContextProvider.h"
#include "gfxPlatform.h"

#ifdef XP_WIN
#include "gfxWindowsSurface.h"
#include "WGLLibrary.h"
#endif

#ifdef XP_MACOSX
#include <OpenGL/OpenGL.h>
#endif

#ifdef MOZ_X11
#include "gfxXlibSurface.h"
#endif

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gl;

void
CanvasLayerOGL::Destroy()
{
  if (!mDestroyed) {
    CleanupResources();
    mDestroyed = true;
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

  mOGLManager->MakeCurrent();

  if (aData.mDrawTarget) {
    mDrawTarget = aData.mDrawTarget;
    mNeedsYFlip = false;
  } else if (aData.mSurface) {
    mCanvasSurface = aData.mSurface;
    mNeedsYFlip = false;
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
    if (aData.mSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
        gfxXlibSurface *xsurf = static_cast<gfxXlibSurface*>(aData.mSurface);
        mPixmap = xsurf->GetGLXPixmap();
        if (mPixmap) {
            if (aData.mSurface->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA) {
                mLayerProgram = gl::RGBALayerProgramType;
            } else {
                mLayerProgram = gl::RGBXLayerProgramType;
            }
            MakeTexture();
        }
    }
#endif
  } else if (aData.mGLContext) {
    if (!aData.mGLContext->IsOffscreen()) {
      NS_WARNING("CanvasLayerOGL with a non-offscreen GL context given");
      return;
    }

    mCanvasGLContext = aData.mGLContext;
    mGLBufferIsPremultiplied = aData.mGLBufferIsPremultiplied;

    mNeedsYFlip = mCanvasGLContext->GetOffscreenTexture() != 0;
  } else {
    NS_WARNING("CanvasLayerOGL::Initialize called without surface or GL context!");
    return;
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);
      
  // Check the maximum texture size supported by GL. glTexImage2D supports
  // images of up to 2 + GL_MAX_TEXTURE_SIZE
  GLint texSize = gl()->GetMaxTextureSize();
  if (mBounds.width > (2 + texSize) || mBounds.height > (2 + texSize)) {
    mDelayedUpdates = true;
    MakeTexture();
    // This should only ever occur with 2d canvas, WebGL can't already have a texture
    // of this size can it?
    NS_ABORT_IF_FALSE(mCanvasSurface || mDrawTarget, 
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

/**
 * Following UpdateSurface(), mTexture on context this->gl() should contain the data we want,
 * unless mDelayedUpdates is true because of a too-large surface.
 */
void
CanvasLayerOGL::UpdateSurface()
{
  if (!mDirty)
    return;
  mDirty = false;

  if (mDestroyed || mDelayedUpdates) {
    return;
  }

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
  if (mPixmap) {
    return;
  }
#endif

  if (mCanvasGLContext &&
      mCanvasGLContext->GetContextType() == gl()->GetContextType())
  {
    DiscardTempSurface();

    // Can texture share, just make sure it's resolved first
    mCanvasGLContext->MakeCurrent();
    mCanvasGLContext->GuaranteeResolve();

    if (gl()->BindOffscreenNeedsTexture(mCanvasGLContext) &&
        mTexture == 0)
    {
      mOGLManager->MakeCurrent();
      MakeTexture();
    }
  } else {
    nsRefPtr<gfxASurface> updatedAreaSurface;

    if (mDrawTarget) {
      // TODO: This is suboptimal - We should have direct handling for the surface types instead of
      // going via a gfxASurface.
      updatedAreaSurface = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(mDrawTarget);
    } else if (mCanvasSurface) {
      updatedAreaSurface = mCanvasSurface;
    } else if (mCanvasGLContext) {
      gfxIntSize size(mBounds.width, mBounds.height);
      nsRefPtr<gfxImageSurface> updatedAreaImageSurface =
        GetTempSurface(size, gfxASurface::ImageFormatARGB32);

      mCanvasGLContext->ReadPixelsIntoImageSurface(0, 0,
                                                   mBounds.width,
                                                   mBounds.height,
                                                   updatedAreaImageSurface);

      updatedAreaSurface = updatedAreaImageSurface;
    }

    mOGLManager->MakeCurrent();
    mLayerProgram = gl()->UploadSurfaceToTexture(updatedAreaSurface,
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

  ShaderProgramOGL *program = nsnull;

  bool useGLContext = mCanvasGLContext &&
    mCanvasGLContext->GetContextType() == gl()->GetContextType();

  nsIntRect drawRect = mBounds;

  if (useGLContext) {
    gl()->BindTex2DOffscreen(mCanvasGLContext);
    program = mOGLManager->GetBasicLayerProgram(CanUseOpaqueSurface(),
                                                true,
                                                GetMaskLayer() ? Mask2d : MaskNone);
  } else if (mDelayedUpdates) {
    NS_ABORT_IF_FALSE(mCanvasSurface || mDrawTarget, "WebGL canvases should always be using full texture upload");
    
    drawRect.IntersectRect(drawRect, GetEffectiveVisibleRegion().GetBounds());

    nsRefPtr<gfxASurface> surf = mCanvasSurface;
    if (mDrawTarget) {
      surf = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(mDrawTarget);
    }

    mLayerProgram =
      gl()->UploadSurfaceToTexture(surf,
                                   nsIntRect(0, 0, drawRect.width, drawRect.height),
                                   mTexture,
                                   true,
                                   drawRect.TopLeft());
  }

  if (!program) {
    program = mOGLManager->GetProgram(mLayerProgram, GetMaskLayer());
  }

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
  if (mPixmap && !mDelayedUpdates) {
    sGLXLibrary.BindTexImage(mPixmap);
  }
#endif

  gl()->ApplyFilterToBoundTexture(mFilter);

  program->Activate();
  program->SetLayerQuadRect(drawRect);
  program->SetLayerTransform(GetEffectiveTransform());
  program->SetLayerOpacity(GetEffectiveOpacity());
  program->SetRenderOffset(aOffset);
  program->SetTextureUnit(0);
  program->LoadMask(GetMaskLayer());

  if (gl()->CanUploadNonPowerOfTwo()) {
    mOGLManager->BindAndDrawQuad(program, mNeedsYFlip ? true : false);
  } else {
    mOGLManager->BindAndDrawQuadWithTextureRect(program, drawRect, drawRect.Size());
  }

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
  if (mPixmap && !mDelayedUpdates) {
    sGLXLibrary.ReleaseTexImage(mPixmap);
  }
#endif

  if (useGLContext) {
    gl()->UnbindTex2DOffscreen(mCanvasGLContext);
  }
}

void
CanvasLayerOGL::CleanupResources()
{
  if (mTexture) {
    gl()->MakeCurrent();
    gl()->fDeleteTextures(1, &mTexture);
  }
}


ShadowCanvasLayerOGL::ShadowCanvasLayerOGL(LayerManagerOGL* aManager)
  : ShadowCanvasLayer(aManager, nsnull)
  , LayerOGL(aManager)
  , mNeedsYFlip(false)
{
  mImplData = static_cast<LayerOGL*>(this);
}
 
ShadowCanvasLayerOGL::~ShadowCanvasLayerOGL()
{}

void
ShadowCanvasLayerOGL::Initialize(const Data& aData)
{
  NS_RUNTIMEABORT("Incompatibe surface type");
}

void
ShadowCanvasLayerOGL::Init(const CanvasSurface& aNewFront, bool needYFlip)
{
  nsRefPtr<gfxASurface> surf = ShadowLayerForwarder::OpenDescriptor(aNewFront);

  mNeedsYFlip = needYFlip;

  mTexImage = gl()->CreateTextureImage(surf->GetSize(),
                                       surf->GetContentType(),
                                       LOCAL_GL_CLAMP_TO_EDGE,
                                       mNeedsYFlip ? TextureImage::NeedsYFlip : TextureImage::NoFlags);
}

void
ShadowCanvasLayerOGL::Swap(const CanvasSurface& aNewFront,
                           bool needYFlip,
                           CanvasSurface* aNewBack)
{
  if (!mDestroyed) {
    nsRefPtr<gfxASurface> surf = ShadowLayerForwarder::OpenDescriptor(aNewFront);
    gfxIntSize sz = surf->GetSize();
    if (!mTexImage || mTexImage->GetSize() != sz ||
        mTexImage->GetContentType() != surf->GetContentType()) {
      Init(aNewFront, needYFlip);
    }
    nsIntRegion updateRegion(nsIntRect(0, 0, sz.width, sz.height));
    mTexImage->DirectUpdate(surf, updateRegion);
  }

  *aNewBack = aNewFront;
}

void
ShadowCanvasLayerOGL::DestroyFrontBuffer()
{
  mTexImage = nsnull;
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
    mDestroyed = true;
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

  ShaderProgramOGL *program =
    mOGLManager->GetProgram(mTexImage->GetShaderProgramType(),
                            GetMaskLayer());


  gfx3DMatrix effectiveTransform = GetEffectiveTransform();
#ifdef ANDROID
  // Bug 691354
  // Using the LINEAR filter we get unexplained artifacts.
  // Use NEAREST when no scaling is required.
  gfxMatrix matrix;
  bool is2D = GetEffectiveTransform().Is2D(&matrix);
  if (is2D && !matrix.HasNonTranslationOrFlip()) {
    mTexImage->SetFilter(gfxPattern::FILTER_NEAREST);
  } else {
    mTexImage->SetFilter(mFilter);
  }
#else
  mTexImage->SetFilter(mFilter);
#endif


  program->Activate();
  program->SetLayerTransform(effectiveTransform);
  program->SetLayerOpacity(GetEffectiveOpacity());
  program->SetRenderOffset(aOffset);
  program->SetTextureUnit(0);
  program->LoadMask(GetMaskLayer());

  mTexImage->BeginTileIteration();
  if (gl()->CanUploadNonPowerOfTwo()) {
    do {
      TextureImage::ScopedBindTextureAndApplyFilter texBind(mTexImage, LOCAL_GL_TEXTURE0);
      program->SetLayerQuadRect(mTexImage->GetTileRect());
      mOGLManager->BindAndDrawQuad(program, mNeedsYFlip); // FIXME flip order of tiles?
    } while (mTexImage->NextTile());
  } else {
    do {
      TextureImage::ScopedBindTextureAndApplyFilter texBind(mTexImage, LOCAL_GL_TEXTURE0);
      program->SetLayerQuadRect(mTexImage->GetTileRect());
      // We can't use BindAndDrawQuad because that always uploads the whole texture from 0.0f -> 1.0f
      // in x and y. We use BindAndDrawQuadWithTextureRect to actually draw a subrect of the texture
      // We need to reset the origin to 0,0 from the tile rect because the tile originates at 0,0 in the
      // actual texture, even though its origin in the composed (tiled) texture is not 0,0
      // FIXME: we need to handle mNeedsYFlip, Bug #728625
      mOGLManager->BindAndDrawQuadWithTextureRect(program,
                                                  nsIntRect(0, 0, mTexImage->GetTileRect().width,
                                                                  mTexImage->GetTileRect().height),
                                                  mTexImage->GetTileRect().Size(),
                                                  mTexImage->GetWrapMode(),
                                                  mNeedsYFlip);
    } while (mTexImage->NextTile());
  }
}

void
ShadowCanvasLayerOGL::CleanupResources()
{
  DestroyFrontBuffer();
}
