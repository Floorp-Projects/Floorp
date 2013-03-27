/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ipc/AutoOpenSurface.h"
#include "mozilla/layers/PLayers.h"
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

#ifdef MOZ_X11
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
#endif // XP_MACOSX

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
  GLint texSize;
  gl()->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &texSize);
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

static bool
IsValidSharedTexDescriptor(const SurfaceDescriptor& aDescriptor)
{
  return aDescriptor.type() == SurfaceDescriptor::TSharedTextureDescriptor;
}

static bool
IsValidSurfaceStreamDescriptor(const SurfaceDescriptor& aDescriptor)
{
  return aDescriptor.type() == SurfaceDescriptor::TSurfaceStreamDescriptor;
}

ShadowCanvasLayerOGL::ShadowCanvasLayerOGL(LayerManagerOGL* aManager)
  : ShadowCanvasLayer(aManager, nullptr)
  , LayerOGL(aManager)
  , mNeedsYFlip(false)
  , mUploadTexture(0)
  , mCurTexture(0)
  , mGrallocImage(EGL_NO_IMAGE)
  , mShaderType(RGBXLayerProgramType)
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
  AutoOpenSurface autoSurf(OPEN_READ_ONLY, aNewFront);

  mNeedsYFlip = needYFlip;

  mTexImage = gl()->CreateTextureImage(autoSurf.Size(),
                                       autoSurf.ContentType(),
                                       LOCAL_GL_CLAMP_TO_EDGE,
                                       mNeedsYFlip ? TextureImage::NeedsYFlip : TextureImage::NoFlags);
}

void
ShadowCanvasLayerOGL::Swap(const CanvasSurface& aNewFront,
                           bool needYFlip,
                           CanvasSurface* aNewBack)
{
  if (mDestroyed) {
    *aNewBack = aNewFront;
    return;
  }

  const SurfaceDescriptor& frontDesc = aNewFront.get_SurfaceDescriptor();
  nsRefPtr<TextureImage> texImage =
      ShadowLayerManager::OpenDescriptorForDirectTexturing(
          gl(), frontDesc, LOCAL_GL_CLAMP_TO_EDGE);


  if (texImage) {
    if (mTexImage &&
        (mTexImage->GetSize() != texImage->GetSize() ||
         mTexImage->GetContentType() != texImage->GetContentType()))
    {
      mTexImage = nullptr;
      DestroyFrontBuffer();
    }

    mTexImage = texImage;
    *aNewBack = IsSurfaceDescriptorValid(mFrontBufferDescriptor) ?
                CanvasSurface(mFrontBufferDescriptor) : CanvasSurface(null_t());
    mFrontBufferDescriptor = aNewFront;
    mNeedsYFlip = needYFlip;
  } else if (IsValidSharedTexDescriptor(aNewFront)) {
    MakeTextureIfNeeded(gl(), mUploadTexture);
    if (!IsValidSharedTexDescriptor(mFrontBufferDescriptor)) {
      mFrontBufferDescriptor = SharedTextureDescriptor(GLContext::SameProcess,
                                                       0,
                                                       nsIntSize(0, 0),
                                                       false);
    }
    *aNewBack = mFrontBufferDescriptor;
    mFrontBufferDescriptor = aNewFront;
    mNeedsYFlip = needYFlip;
  } else if (IsValidSurfaceStreamDescriptor(frontDesc)) {
    // Check our previous desc.
    if (!IsValidSurfaceStreamDescriptor(mFrontBufferDescriptor)) {
      SurfaceStreamHandle handle = (SurfaceStreamHandle)nullptr;
      mFrontBufferDescriptor = SurfaceStreamDescriptor(handle, false);
    }
    *aNewBack = mFrontBufferDescriptor;
    mFrontBufferDescriptor = aNewFront;
    mNeedsYFlip = needYFlip;
  } else {
    AutoOpenSurface autoSurf(OPEN_READ_ONLY, aNewFront);
    gfxIntSize autoSurfSize = autoSurf.Size();
    if (!mTexImage ||
        mTexImage->GetSize() != autoSurfSize ||
        mTexImage->GetContentType() != autoSurf.ContentType())
    {
      Init(aNewFront, needYFlip);
    }
    nsIntRegion updateRegion(nsIntRect(0, 0, autoSurfSize.width, autoSurfSize.height));
    mTexImage->DirectUpdate(autoSurf.Get(), updateRegion);
    *aNewBack = aNewFront;
  }
}

void
ShadowCanvasLayerOGL::DestroyFrontBuffer()
{
  mTexImage = nullptr;

  if (mUploadTexture) {
    gl()->MakeCurrent();
    gl()->fDeleteTextures(1, &mUploadTexture);
  }

  MOZ_ASSERT(!IsValidSharedTexDescriptor(mFrontBufferDescriptor));

  gl()->EmptyTexGarbageBin();

  if (mGrallocImage) {
    GLLibraryEGL* egl = (GLLibraryEGL*)gl()->GetLibraryEGL();
    MOZ_ASSERT(egl);
    egl->fDestroyImage(egl->Display(), mGrallocImage);
    mGrallocImage = 0;
  }

  if (IsValidSurfaceStreamDescriptor(mFrontBufferDescriptor)) {
    // Nothing needed.
    mFrontBufferDescriptor = SurfaceDescriptor();
  } else if (IsSurfaceDescriptorValid(mFrontBufferDescriptor)) {
    mAllocator->DestroySharedSurface(&mFrontBufferDescriptor);
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
    mDestroyed = true;
    DestroyFrontBuffer();
  }
}

Layer*
ShadowCanvasLayerOGL::GetLayer()
{
  return this;
}

LayerRenderState
ShadowCanvasLayerOGL::GetRenderState()
{
  if (mDestroyed || !IsValidSharedTexDescriptor(mFrontBufferDescriptor)) {
    return LayerRenderState();
  }
  return LayerRenderState(&mFrontBufferDescriptor,
                          mNeedsYFlip ? LAYER_RENDER_STATE_Y_FLIPPED : 0);
}

void
ShadowCanvasLayerOGL::RenderLayer(int aPreviousFrameBuffer,
                                  const nsIntPoint& aOffset)
{
  if (!mTexImage &&
      !IsValidSharedTexDescriptor(mFrontBufferDescriptor) &&
      !IsValidSurfaceStreamDescriptor(mFrontBufferDescriptor))
  {
    return;
  }

  if (mOGLManager->CompositingDisabled()) {
    return;
  }
  mOGLManager->MakeCurrent();
  gl()->EmptyTexGarbageBin();
  //ScopedBindTexture autoTex(gl());

  gfx3DMatrix effectiveTransform = GetEffectiveTransform();
  gfxPattern::GraphicsFilter filter = mFilter;
#ifdef ANDROID
  // Bug 691354
  // Using the LINEAR filter we get unexplained artifacts.
  // Use NEAREST when no scaling is required.
  gfxMatrix matrix;
  bool is2D = GetEffectiveTransform().Is2D(&matrix);
  if (is2D && !matrix.HasNonTranslationOrFlip()) {
    filter = gfxPattern::FILTER_NEAREST;
  }
#endif
  SurfaceStream* surfStream = nullptr;
  SharedSurface* sharedSurf = nullptr;
  if (IsValidSurfaceStreamDescriptor(mFrontBufferDescriptor)) {
    const SurfaceStreamDescriptor& streamDesc =
        mFrontBufferDescriptor.get_SurfaceStreamDescriptor();

    surfStream = SurfaceStream::FromHandle(streamDesc.handle());
    MOZ_ASSERT(surfStream);

    sharedSurf = surfStream->SwapConsumer();
    if (!sharedSurf) {
      // We don't have a valid surf to show yet.
      return;
    }

    gfxImageSurface* toUpload = nullptr;
    switch (sharedSurf->Type()) {
      case SharedSurfaceType::GLTextureShare: {
        mCurTexture = SharedSurface_GLTexture::Cast(sharedSurf)->Texture();
        MOZ_ASSERT(mCurTexture);
        mShaderType = sharedSurf->HasAlpha() ? RGBALayerProgramType
                                             : RGBXLayerProgramType;
        break;
      }
      case SharedSurfaceType::EGLImageShare: {
        SharedSurface_EGLImage* eglImageSurf =
            SharedSurface_EGLImage::Cast(sharedSurf);

        mCurTexture = eglImageSurf->AcquireConsumerTexture(gl());
        if (!mCurTexture) {
          toUpload = eglImageSurf->GetPixels();
          MOZ_ASSERT(toUpload);
        } else {
          mShaderType = sharedSurf->HasAlpha() ? RGBALayerProgramType
                                               : RGBXLayerProgramType;
        }
        break;
      }
      case SharedSurfaceType::Basic: {
        toUpload = SharedSurface_Basic::Cast(sharedSurf)->GetData();
        MOZ_ASSERT(toUpload);
        break;
      }
      default:
        MOZ_NOT_REACHED("Invalid SharedSurface type.");
        return;
    }

    if (toUpload) {
      // mBounds seems to end up as (0,0,0,0) a lot, so don't use it?
      nsIntSize size(toUpload->GetSize());
      nsIntRect rect(nsIntPoint(0,0), size);
      nsIntRegion bounds(rect);
      mShaderType = gl()->UploadSurfaceToTexture(toUpload,
                                                 bounds,
                                                 mUploadTexture,
                                                 true);
      mCurTexture = mUploadTexture;
    }

    MOZ_ASSERT(mCurTexture);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mCurTexture);
    gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D,
                         LOCAL_GL_TEXTURE_WRAP_S,
                         LOCAL_GL_CLAMP_TO_EDGE);
    gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D,
                         LOCAL_GL_TEXTURE_WRAP_T,
                         LOCAL_GL_CLAMP_TO_EDGE);
  } else if (mTexImage) {
    mShaderType = mTexImage->GetShaderProgramType();
  } else {
    MOZ_NOT_REACHED("What can we do?");
    return;
  }

  ShaderProgramOGL* program = mOGLManager->GetProgram(mShaderType, GetMaskLayer());

  program->Activate();
  program->SetLayerTransform(effectiveTransform);
  program->SetLayerOpacity(GetEffectiveOpacity());
  program->SetRenderOffset(aOffset);
  program->SetTextureUnit(0);
  program->LoadMask(GetMaskLayer());

  if (surfStream) {
    MOZ_ASSERT(sharedSurf);

    gl()->ApplyFilterToBoundTexture(filter);
    program->SetLayerQuadRect(nsIntRect(nsIntPoint(0, 0), sharedSurf->Size()));

    mOGLManager->BindAndDrawQuad(program, mNeedsYFlip);

    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, 0);
  } else {
    // Tiled texture image rendering path
    mTexImage->SetFilter(filter);
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
}

void
ShadowCanvasLayerOGL::CleanupResources()
{
  DestroyFrontBuffer();
}
