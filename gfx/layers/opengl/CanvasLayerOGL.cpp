/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ipc/AutoOpenSurface.h"
#include "mozilla/layers/PLayerTransaction.h"
#include "mozilla/layers/ShadowLayers.h"

#include "gfxSharedImageSurface.h"

#include "CanvasLayerOGL.h"

#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "GLContextProvider.h"
#include "gfxPlatform.h"
#include "SharedSurfaceGL.h"
#include "SharedSurfaceEGL.h"
#include "SurfaceStream.h"
#include "gfxColor.h"

#ifdef XP_MACOSX
#include "mozilla/gfx/MacIOSurface.h"
#endif

#ifdef XP_WIN
#include "gfxWindowsSurface.h"
#include "WGLLibrary.h"
#endif

#ifdef XP_MACOSX
#include <OpenGL/OpenGL.h>
#endif

#ifdef GL_PROVIDER_GLX
#include "gfxXlibSurface.h"
#endif

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gl;
using namespace mozilla::gfx;

static void
MakeTextureIfNeeded(GLContext* gl, GLuint& aTexture)
{
  if (aTexture != 0)
    return;

  gl->fGenTextures(1, &aTexture);

  gl->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);

  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
}

#ifdef XP_MACOSX
static GLuint
MakeIOSurfaceTexture(void* aCGIOSurfaceContext, mozilla::gl::GLContext* aGL)
{
  GLuint ioSurfaceTexture;

  aGL->MakeCurrent();

  aGL->fGenTextures(1, &ioSurfaceTexture);

  aGL->fActiveTexture(LOCAL_GL_TEXTURE0);
  aGL->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, ioSurfaceTexture);

  aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  RefPtr<MacIOSurface> ioSurface = MacIOSurface::IOSurfaceContextGetSurface((CGContextRef)aCGIOSurfaceContext);
  void *nativeCtx = aGL->GetNativeData(GLContext::NativeGLContext);

  ioSurface->CGLTexImageIOSurface2D(nativeCtx,
                                    LOCAL_GL_RGBA, LOCAL_GL_BGRA,
                                    LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV, 0);

  aGL->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);

  return ioSurfaceTexture;
}
#endif

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
  NS_ASSERTION(mCanvasSurface == nullptr, "BasicCanvasLayer::Initialize called twice!");

  if (aData.mGLContext != nullptr &&
      aData.mSurface != nullptr)
  {
    NS_WARNING("CanvasLayerOGL can't have both surface and WebGLContext");
    return;
  }

  mOGLManager->MakeCurrent();

  if (aData.mDrawTarget &&
      aData.mDrawTarget->GetNativeSurface(gfx::NATIVE_SURFACE_CGCONTEXT_ACCELERATED)) {
    mDrawTarget = aData.mDrawTarget;
    mNeedsYFlip = false;
    mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);
    return;
  } else if (aData.mDrawTarget) {
    mDrawTarget = aData.mDrawTarget;
    mCanvasSurface = gfxPlatform::GetPlatform()->CreateThebesSurfaceAliasForDrawTarget_hack(mDrawTarget);
    mNeedsYFlip = false;
  } else if (aData.mSurface) {
    mCanvasSurface = aData.mSurface;
    mNeedsYFlip = false;
#if defined(GL_PROVIDER_GLX)
    if (aData.mSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
        gfxXlibSurface *xsurf = static_cast<gfxXlibSurface*>(aData.mSurface);
        mPixmap = xsurf->GetGLXPixmap();
        if (mPixmap) {
            if (aData.mSurface->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA) {
                mLayerProgram = gl::RGBALayerProgramType;
            } else {
                mLayerProgram = gl::RGBXLayerProgramType;
            }
            MakeTextureIfNeeded(gl(), mUploadTexture);
        }
    }
#endif
  } else if (aData.mGLContext) {
    mGLContext = aData.mGLContext;
    NS_ASSERTION(mGLContext->IsOffscreen(), "Canvas GLContext must be offscreen.");
    mIsGLAlphaPremult = aData.mIsGLAlphaPremult;
    mNeedsYFlip = true;

    // [OGL Layers, MTC] WebGL layer init.

    GLScreenBuffer* screen = mGLContext->Screen();
    SurfaceStreamType streamType =
        SurfaceStream::ChooseGLStreamType(SurfaceStream::MainThread,
                                          screen->PreserveBuffer());
    SurfaceFactory_GL* factory = nullptr;
    if (!mForceReadback) {
      factory = new SurfaceFactory_GLTexture(mGLContext, gl(), screen->Caps());
    }

    if (factory) {
      screen->Morph(factory, streamType);
    }
  } else {
    NS_WARNING("CanvasLayerOGL::Initialize called without surface or GL context!");
    return;
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);
      
  // Check the maximum texture size supported by GL. glTexImage2D supports
  // images of up to 2 + GL_MAX_TEXTURE_SIZE
  GLint texSize = 0;
  gl()->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &texSize);
  MOZ_ASSERT(texSize != 0);
  if (mBounds.width > (2 + texSize) || mBounds.height > (2 + texSize)) {
    mDelayedUpdates = true;
    MakeTextureIfNeeded(gl(), mUploadTexture);
    // This should only ever occur with 2d canvas, WebGL can't already have a texture
    // of this size can it?
    NS_ABORT_IF_FALSE(mCanvasSurface || mDrawTarget, 
                      "Invalid texture size when WebGL surface already exists at that size?");
  }
}

/**
 * Following UpdateSurface(), mTexture on context this->gl() should contain the data we want,
 * unless mDelayedUpdates is true because of a too-large surface.
 */
void
CanvasLayerOGL::UpdateSurface()
{
  if (!IsDirty())
    return;
  Painted();

  if (mDestroyed || mDelayedUpdates) {
    return;
  }

#if defined(GL_PROVIDER_GLX)
  if (mPixmap) {
    return;
  }
#endif

  gfxASurface* updatedSurface = nullptr;
  gfxImageSurface* temporarySurface = nullptr;
  bool nothingToShow = false;
  if (mGLContext) {
    SharedSurface* surf = mGLContext->RequestFrame();
    if (surf) {
      mLayerProgram = surf->HasAlpha() ? RGBALayerProgramType
                                       : RGBXLayerProgramType;
      switch (surf->Type()) {
        case SharedSurfaceType::Basic: {
          SharedSurface_Basic* readbackSurf = SharedSurface_Basic::Cast(surf);
          updatedSurface = readbackSurf->GetData();
          break;
        }
        case SharedSurfaceType::GLTextureShare: {
          SharedSurface_GLTexture* textureSurf = SharedSurface_GLTexture::Cast(surf);
          mTexture = textureSurf->Texture();
          break;
        }
        default:
          MOZ_NOT_REACHED("Unacceptable SharedSurface type.");
          return;
      }
    } else {
      nothingToShow = true;
    }
  } else if (mCanvasSurface) {
#ifdef XP_MACOSX
    if (mDrawTarget && mDrawTarget->GetNativeSurface(gfx::NATIVE_SURFACE_CGCONTEXT_ACCELERATED)) {
      if (!mTexture) {
        mTexture = MakeIOSurfaceTexture((CGContextRef)mDrawTarget->GetNativeSurface(
                                        gfx::NATIVE_SURFACE_CGCONTEXT_ACCELERATED),
                                        gl());
        mTextureTarget = LOCAL_GL_TEXTURE_RECTANGLE_ARB;
        mLayerProgram = gl::RGBARectLayerProgramType;
      }
      mDrawTarget->Flush();
      return;
    }
#endif
    updatedSurface = mCanvasSurface;
  } else {
    MOZ_NOT_REACHED("Unhandled canvas layer type.");
    return;
  }

  if (updatedSurface) {
    mOGLManager->MakeCurrent();
    mLayerProgram = gl()->UploadSurfaceToTexture(updatedSurface,
                                                 mBounds,
                                                 mUploadTexture,
                                                 true,//false,
                                                 nsIntPoint(0, 0));
    mTexture = mUploadTexture;

    if (temporarySurface)
      delete temporarySurface;
  }

  MOZ_ASSERT(mTexture || nothingToShow);
}

void
CanvasLayerOGL::RenderLayer(int aPreviousDestination,
                            const nsIntPoint& aOffset)
{
  FirePreTransactionCallback();
  UpdateSurface();
  if (mOGLManager->CompositingDisabled()) {
    return;
  }
  FireDidTransactionCallback();

  mOGLManager->MakeCurrent();

  // XXX We're going to need a different program depending on if
  // mGLBufferIsPremultiplied is TRUE or not.  The RGBLayerProgram
  // assumes that it's true.

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

  if (mTexture) {
    gl()->fBindTexture(mTextureTarget, mTexture);
  }

  ShaderProgramOGL *program = nullptr;

  nsIntRect drawRect = mBounds;
  if (mDelayedUpdates) {
    NS_ABORT_IF_FALSE(mCanvasSurface || mDrawTarget, "WebGL canvases should always be using full texture upload");
    
    drawRect.IntersectRect(drawRect, GetEffectiveVisibleRegion().GetBounds());

    mLayerProgram =
      gl()->UploadSurfaceToTexture(mCanvasSurface,
                                   nsIntRect(0, 0, drawRect.width, drawRect.height),
                                   mUploadTexture,
                                   true,
                                   drawRect.TopLeft());
    mTexture = mUploadTexture;
  }

  if (!program) {
    program = mOGLManager->GetProgram(mLayerProgram, GetMaskLayer());
  }

#if defined(GL_PROVIDER_GLX)
  if (mPixmap && !mDelayedUpdates) {
    sDefGLXLib.BindTexImage(mPixmap);
  }
#endif

  gl()->ApplyFilterToBoundTexture(mFilter);

  program->Activate();
  if (mLayerProgram == gl::RGBARectLayerProgramType) {
    // This is used by IOSurface that use 0,0...w,h coordinate rather then 0,0..1,1.
    program->SetTexCoordMultiplier(mDrawTarget->GetSize().width, mDrawTarget->GetSize().height);
  }
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

#if defined(GL_PROVIDER_GLX)
  if (mPixmap && !mDelayedUpdates) {
    sDefGLXLib.ReleaseTexImage(mPixmap);
  }
#endif
}

void
CanvasLayerOGL::CleanupResources()
{
  if (mUploadTexture) {
    gl()->MakeCurrent();
    gl()->fDeleteTextures(1, &mUploadTexture);
    mUploadTexture = 0;
  }
}
