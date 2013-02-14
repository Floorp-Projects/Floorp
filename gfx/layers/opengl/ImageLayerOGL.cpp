/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxSharedImageSurface.h"
#include "mozilla/layers/ImageContainerParent.h"

#include "ImageContainer.h" // for PlanarYCBCRImage
#include "mozilla/layers/ShmemYCbCrImage.h"
#include "ipc/AutoOpenSurface.h"
#include "ImageLayerOGL.h"
#include "gfxImageSurface.h"
#include "gfxUtils.h"
#include "yuv_convert.h"
#include "GLContextProvider.h"
#if defined(GL_PROVIDER_GLX)
# include "GLXLibrary.h"
# include "gfxXlibSurface.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "nsSurfaceTexture.h"
#endif

#ifdef MOZ_WIDGET_GONK
# include "GonkIOSurfaceImage.h"
# include <ui/GraphicBuffer.h>
using namespace android;
#endif

using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

static void
MakeTextureIfNeeded(GLContext* gl, GLuint& aTexture)
{
  if (aTexture != 0)
    return;

  gl->fGenTextures(1, &aTexture);

  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);

  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
}

/**
 * This is an event used to unref a GLContext on the main thread and
 * optionally delete a texture associated with that context.
 */
class TextureDeleter : public nsRunnable {
public:
  TextureDeleter(already_AddRefed<GLContext> aContext,
                 GLuint aTexture)
      : mContext(aContext), mTexture(aTexture)
  {
    NS_ASSERTION(aTexture, "TextureDeleter instantiated with nothing to do");
  }

  NS_IMETHOD Run() {
    mContext->MakeCurrent();
    mContext->fDeleteTextures(1, &mTexture);

    // Ensure context is released on the main thread
    mContext = nullptr;
    return NS_OK;
  }

  nsRefPtr<GLContext> mContext;
  GLuint mTexture;
};

void
GLTexture::Allocate(GLContext *aContext)
{
  NS_ASSERTION(aContext->IsGlobalSharedContext() || aContext->IsOwningThreadCurrent(),
               "Can only allocate texture on context's owning thread or with cx sharing");

  Release();

  mContext = aContext;

  mContext->MakeCurrent();
  mContext->fGenTextures(1, &mTexture);
}

void
GLTexture::TakeFrom(GLTexture *aOther)
{
  Release();

  mContext = aOther->mContext.forget();
  mTexture = aOther->mTexture;
  aOther->mTexture = 0;
}

void
GLTexture::Release()
{
  if (!mContext) {
    NS_ASSERTION(!mTexture, "Can't delete texture without a context");
    return;
  }

  if (mContext->IsDestroyed() && !mContext->IsGlobalSharedContext()) {
    mContext = mContext->GetSharedContext();
    if (!mContext) {
      NS_ASSERTION(!mTexture, 
                   "Context has been destroyed and couldn't find a shared context!");
      return;
    }
  }

  if (mTexture) {
    if (mContext->IsOwningThreadCurrent() || mContext->IsGlobalSharedContext()) {
      mContext->MakeCurrent();
      mContext->fDeleteTextures(1, &mTexture);
    } else {
      nsCOMPtr<nsIRunnable> runnable =
        new TextureDeleter(mContext.get(), mTexture);
      mContext->DispatchToOwningThread(runnable);
      mContext.forget();
    }

    mTexture = 0;
  }

  mContext = nullptr;
}

TextureRecycleBin::TextureRecycleBin()
  : mLock("mozilla.layers.TextureRecycleBin.mLock")
{
}

void
TextureRecycleBin::RecycleTexture(GLTexture *aTexture, TextureType aType,
                           const gfxIntSize& aSize)
{
  MutexAutoLock lock(mLock);

  if (!aTexture->IsAllocated())
    return;

  if (!mRecycledTextures[aType].IsEmpty() && aSize != mRecycledTextureSizes[aType]) {
    mRecycledTextures[aType].Clear();
  }
  mRecycledTextureSizes[aType] = aSize;
  mRecycledTextures[aType].AppendElement()->TakeFrom(aTexture);
}

void
TextureRecycleBin::GetTexture(TextureType aType, const gfxIntSize& aSize,
                       GLContext *aContext, GLTexture *aOutTexture)
{
  MutexAutoLock lock(mLock);

  if (mRecycledTextures[aType].IsEmpty() || mRecycledTextureSizes[aType] != aSize) {
    aOutTexture->Allocate(aContext);
    return;
  }
  uint32_t last = mRecycledTextures[aType].Length() - 1;
  aOutTexture->TakeFrom(&mRecycledTextures[aType].ElementAt(last));
  mRecycledTextures[aType].RemoveElementAt(last);
}

struct THEBES_API MacIOSurfaceImageOGLBackendData : public ImageBackendData
{
  GLTexture mTexture;
};

#ifdef XP_MACOSX
void
AllocateTextureIOSurface(MacIOSurfaceImage *aIOImage, mozilla::gl::GLContext* aGL)
{
  nsAutoPtr<MacIOSurfaceImageOGLBackendData> backendData(
    new MacIOSurfaceImageOGLBackendData);

  backendData->mTexture.Allocate(aGL);
  aGL->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, backendData->mTexture.GetTextureID());
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                     LOCAL_GL_TEXTURE_MIN_FILTER,
                     LOCAL_GL_NEAREST);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                     LOCAL_GL_TEXTURE_MAG_FILTER,
                     LOCAL_GL_NEAREST);

  void *nativeCtx = aGL->GetNativeData(GLContext::NativeGLContext);

  aIOImage->GetIOSurface()->CGLTexImageIOSurface2D(nativeCtx,
                                     LOCAL_GL_RGBA, LOCAL_GL_BGRA,
                                     LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV, 0);

  aGL->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);

  aIOImage->SetBackendData(LAYERS_OPENGL, backendData.forget());
}
#endif

#ifdef MOZ_WIDGET_GONK
struct THEBES_API GonkIOSurfaceImageOGLBackendData : public ImageBackendData
{
  GLTexture mTexture;
};

void
AllocateTextureIOSurface(GonkIOSurfaceImage *aIOImage, mozilla::gl::GLContext* aGL)
{
  nsAutoPtr<GonkIOSurfaceImageOGLBackendData> backendData(
    new GonkIOSurfaceImageOGLBackendData);

  backendData->mTexture.Allocate(aGL);
  aIOImage->SetBackendData(LAYERS_OPENGL, backendData.forget());
}
#endif

Layer*
ImageLayerOGL::GetLayer()
{
  return this;
}

void
ImageLayerOGL::RenderLayer(int,
                           const nsIntPoint& aOffset)
{
  nsRefPtr<ImageContainer> container = GetContainer();

  if (!container || mOGLManager->CompositingDisabled())
    return;

  mOGLManager->MakeCurrent();

  AutoLockImage autoLock(container);

  Image *image = autoLock.GetImage();
  if (!image) {
    return;
  }

  NS_ASSERTION(image->GetFormat() != REMOTE_IMAGE_BITMAP,
    "Remote images aren't handled yet in OGL layers!");

  if (image->GetFormat() == PLANAR_YCBCR) {
    PlanarYCbCrImage *yuvImage =
      static_cast<PlanarYCbCrImage*>(image);

    if (!yuvImage->IsValid()) {
      return;
    }

    PlanarYCbCrOGLBackendData *data =
      static_cast<PlanarYCbCrOGLBackendData*>(yuvImage->GetBackendData(LAYERS_OPENGL));

    if (data && data->mTextures->GetGLContext() != gl()) {
      // If these textures were allocated by another layer manager,
      // clear them out and re-allocate below.
      data = nullptr;
      yuvImage->SetBackendData(LAYERS_OPENGL, nullptr);
    }

    if (!data) {
      AllocateTexturesYCbCr(yuvImage);
      data = static_cast<PlanarYCbCrOGLBackendData*>(yuvImage->GetBackendData(LAYERS_OPENGL));
    }

    if (!data || data->mTextures->GetGLContext() != gl()) {
      // XXX - Can this ever happen? If so I need to fix this!
      return;
    }

    gl()->MakeCurrent();
    gl()->fActiveTexture(LOCAL_GL_TEXTURE2);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, data->mTextures[2].GetTextureID());
    gl()->ApplyFilterToBoundTexture(mFilter);
    gl()->fActiveTexture(LOCAL_GL_TEXTURE1);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, data->mTextures[1].GetTextureID());
    gl()->ApplyFilterToBoundTexture(mFilter);
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, data->mTextures[0].GetTextureID());
    gl()->ApplyFilterToBoundTexture(mFilter);
    
    ShaderProgramOGL *program = mOGLManager->GetProgram(YCbCrLayerProgramType,
                                                        GetMaskLayer());

    program->Activate();
    program->SetLayerQuadRect(nsIntRect(0, 0,
                                        yuvImage->GetSize().width,
                                        yuvImage->GetSize().height));
    program->SetLayerTransform(GetEffectiveTransform());
    program->SetLayerOpacity(GetEffectiveOpacity());
    program->SetRenderOffset(aOffset);
    program->SetYCbCrTextureUnits(0, 1, 2);
    program->LoadMask(GetMaskLayer());

    mOGLManager->BindAndDrawQuadWithTextureRect(program,
                                                yuvImage->GetData()->GetPictureRect(),
                                                nsIntSize(yuvImage->GetData()->mYSize.width,
                                                          yuvImage->GetData()->mYSize.height));

    // We shouldn't need to do this, but do it anyway just in case
    // someone else forgets.
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  } else if (image->GetFormat() == CAIRO_SURFACE) {
    CairoImage *cairoImage =
      static_cast<CairoImage*>(image);

    if (!cairoImage->mSurface) {
      return;
    }

    NS_ASSERTION(cairoImage->mSurface->GetContentType() != gfxASurface::CONTENT_ALPHA,
                 "Image layer has alpha image");

    CairoOGLBackendData *data =
      static_cast<CairoOGLBackendData*>(cairoImage->GetBackendData(LAYERS_OPENGL));

    if (data && data->mTexture.GetGLContext() != gl()) {
      // If this texture was allocated by another layer manager, clear
      // it out and re-allocate below.
      data = nullptr;
      cairoImage->SetBackendData(LAYERS_OPENGL, nullptr);
    }

    if (!data) {
      AllocateTexturesCairo(cairoImage);
      data = static_cast<CairoOGLBackendData*>(cairoImage->GetBackendData(LAYERS_OPENGL));
    }

    if (!data || data->mTexture.GetGLContext() != gl()) {
      // XXX - Can this ever happen? If so I need to fix this!
      return;
    }

    gl()->MakeCurrent();

    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, data->mTexture.GetTextureID());

    ShaderProgramOGL *program = 
      mOGLManager->GetProgram(data->mLayerProgram, GetMaskLayer());

    gl()->ApplyFilterToBoundTexture(mFilter);

    program->Activate();
    program->SetLayerQuadRect(nsIntRect(0, 0, 
                                        cairoImage->GetSize().width, 
                                        cairoImage->GetSize().height));
    program->SetLayerTransform(GetEffectiveTransform());
    program->SetLayerOpacity(GetEffectiveOpacity());
    program->SetRenderOffset(aOffset);
    program->SetTextureUnit(0);
    program->LoadMask(GetMaskLayer());

    mOGLManager->BindAndDrawQuad(program);
#ifdef XP_MACOSX
  } else if (image->GetFormat() == MAC_IO_SURFACE) {
     MacIOSurfaceImage *ioImage =
       static_cast<MacIOSurfaceImage*>(image);

     if (!mOGLManager->GetThebesLayerCallback()) {
       // If its an empty transaction we still need to update
       // the plugin IO Surface and make sure we grab the
       // new image
       ioImage->Update(GetContainer());
       image = nullptr;
       autoLock.Refresh();
       image = autoLock.GetImage();
       gl()->MakeCurrent();
       ioImage = static_cast<MacIOSurfaceImage*>(image);
     }

     if (!ioImage) {
       return;
     }

     gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

     if (!ioImage->GetBackendData(LAYERS_OPENGL)) {
       AllocateTextureIOSurface(ioImage, gl());
     }

     MacIOSurfaceImageOGLBackendData *data =
      static_cast<MacIOSurfaceImageOGLBackendData*>(ioImage->GetBackendData(LAYERS_OPENGL));

     gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
     gl()->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, data->mTexture.GetTextureID());

     ShaderProgramOGL *program =
       mOGLManager->GetProgram(gl::RGBARectLayerProgramType, GetMaskLayer());

     program->Activate();
     if (program->GetTexCoordMultiplierUniformLocation() != -1) {
       // 2DRect case, get the multiplier right for a sampler2DRect
       program->SetTexCoordMultiplier(ioImage->GetSize().width, ioImage->GetSize().height);
     } else {
       NS_ASSERTION(0, "no rects?");
     }

     program->SetLayerQuadRect(nsIntRect(0, 0,
                                         ioImage->GetSize().width,
                                         ioImage->GetSize().height));
     program->SetLayerTransform(GetEffectiveTransform());
     program->SetLayerOpacity(GetEffectiveOpacity());
     program->SetRenderOffset(aOffset);
     program->SetTextureUnit(0);
     program->LoadMask(GetMaskLayer());

     mOGLManager->BindAndDrawQuad(program);
     gl()->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);
#endif
#ifdef MOZ_WIDGET_GONK
  } else if (image->GetFormat() == GONK_IO_SURFACE) {

    GonkIOSurfaceImage *ioImage = static_cast<GonkIOSurfaceImage*>(image);
    if (!ioImage) {
      return;
    }

    gl()->MakeCurrent();
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

    if (!ioImage->GetBackendData(LAYERS_OPENGL)) {
      AllocateTextureIOSurface(ioImage, gl());
    }
    GonkIOSurfaceImageOGLBackendData *data =
      static_cast<GonkIOSurfaceImageOGLBackendData*>(ioImage->GetBackendData(LAYERS_OPENGL));

    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->BindExternalBuffer(data->mTexture.GetTextureID(), ioImage->GetNativeBuffer());

    ShaderProgramOGL *program = mOGLManager->GetProgram(RGBAExternalLayerProgramType, GetMaskLayer());

    gl()->ApplyFilterToBoundTexture(mFilter);

    program->Activate();
    program->SetLayerQuadRect(nsIntRect(0, 0, 
                                        ioImage->GetSize().width, 
                                        ioImage->GetSize().height));
    program->SetLayerTransform(GetEffectiveTransform());
    program->SetLayerOpacity(GetEffectiveOpacity());
    program->SetRenderOffset(aOffset);
    program->SetTextureUnit(0);
    program->LoadMask(GetMaskLayer());

    mOGLManager->BindAndDrawQuadWithTextureRect(program,
                                                GetVisibleRegion().GetBounds(),
                                                nsIntSize(ioImage->GetSize().width,
                                                          ioImage->GetSize().height));
#endif
  }
  GetContainer()->NotifyPaintedImage(image);
}

static void
SetClamping(GLContext* aGL, GLuint aTexture)
{
  aGL->fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
}

static void
UploadYUVToTexture(GLContext* gl, const PlanarYCbCrImage::Data& aData, 
                   GLTexture* aYTexture,
                   GLTexture* aUTexture,
                   GLTexture* aVTexture)
{
  nsIntRect size(0, 0, aData.mYSize.width, aData.mYSize.height);
  GLuint texture = aYTexture->GetTextureID();
  nsRefPtr<gfxASurface> surf = new gfxImageSurface(aData.mYChannel,
                                                   aData.mYSize,
                                                   aData.mYStride,
                                                   gfxASurface::ImageFormatA8);
  gl->UploadSurfaceToTexture(surf, size, texture, true);
  
  size = nsIntRect(0, 0, aData.mCbCrSize.width, aData.mCbCrSize.height);
  texture = aUTexture->GetTextureID();
  surf = new gfxImageSurface(aData.mCbChannel,
                             aData.mCbCrSize,
                             aData.mCbCrStride,
                             gfxASurface::ImageFormatA8);
  gl->UploadSurfaceToTexture(surf, size, texture, true);

  texture = aVTexture->GetTextureID();
  surf = new gfxImageSurface(aData.mCrChannel,
                             aData.mCbCrSize,
                             aData.mCbCrStride,
                             gfxASurface::ImageFormatA8);
  gl->UploadSurfaceToTexture(surf, size, texture, true);
}

ImageLayerOGL::ImageLayerOGL(LayerManagerOGL *aManager)
  : ImageLayer(aManager, NULL)
  , LayerOGL(aManager)
  , mTextureRecycleBin(new TextureRecycleBin())
{ 
  mImplData = static_cast<LayerOGL*>(this);
}

void
ImageLayerOGL::AllocateTexturesYCbCr(PlanarYCbCrImage *aImage)
{
  if (!aImage->IsValid())
    return;

  nsAutoPtr<PlanarYCbCrOGLBackendData> backendData(
    new PlanarYCbCrOGLBackendData);

  const PlanarYCbCrImage::Data *data = aImage->GetData();

  gl()->MakeCurrent();

  mTextureRecycleBin->GetTexture(TextureRecycleBin::TEXTURE_Y, data->mYSize, gl(), &backendData->mTextures[0]);
  SetClamping(gl(), backendData->mTextures[0].GetTextureID());

  mTextureRecycleBin->GetTexture(TextureRecycleBin::TEXTURE_C, data->mCbCrSize, gl(), &backendData->mTextures[1]);
  SetClamping(gl(), backendData->mTextures[1].GetTextureID());

  mTextureRecycleBin->GetTexture(TextureRecycleBin::TEXTURE_C, data->mCbCrSize, gl(), &backendData->mTextures[2]);
  SetClamping(gl(), backendData->mTextures[2].GetTextureID());

  UploadYUVToTexture(gl(), *data,
                     &backendData->mTextures[0],
                     &backendData->mTextures[1],
                     &backendData->mTextures[2]);

  backendData->mYSize = data->mYSize;
  backendData->mCbCrSize = data->mCbCrSize;
  backendData->mTextureRecycleBin = mTextureRecycleBin;

  aImage->SetBackendData(LAYERS_OPENGL, backendData.forget());
}

void
ImageLayerOGL::AllocateTexturesCairo(CairoImage *aImage)
{
  nsAutoPtr<CairoOGLBackendData> backendData(
    new CairoOGLBackendData);

  GLTexture &texture = backendData->mTexture;

  texture.Allocate(gl());

  if (!texture.IsAllocated()) {
    return;
  }

  mozilla::gl::GLContext *gl = texture.GetGLContext();
  gl->MakeCurrent();

  GLuint tex = texture.GetTextureID();
  gl->fActiveTexture(LOCAL_GL_TEXTURE0);

  SetClamping(gl, tex);

#if defined(GL_PROVIDER_GLX)
  if (aImage->mSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
    gfxXlibSurface *xsurf =
      static_cast<gfxXlibSurface*>(aImage->mSurface.get());
    GLXPixmap pixmap = xsurf->GetGLXPixmap();
    if (pixmap) {
      if (aImage->mSurface->GetContentType()
          == gfxASurface::CONTENT_COLOR_ALPHA) {
        backendData->mLayerProgram = gl::RGBALayerProgramType;
      } else {
        backendData->mLayerProgram = gl::RGBXLayerProgramType;
      }

      aImage->SetBackendData(LAYERS_OPENGL, backendData.forget());

      sDefGLXLib.BindTexImage(pixmap);

      return;
    }
  }
#endif
  backendData->mLayerProgram =
    gl->UploadSurfaceToTexture(aImage->mSurface,
                               nsIntRect(0,0, aImage->mSize.width, aImage->mSize.height),
                               tex, true);

  aImage->SetBackendData(LAYERS_OPENGL, backendData.forget());
}

/*
 * Returns a size that is larger than and closest to aSize where both
 * width and height are powers of two.
 * If the OpenGL setup is capable of using non-POT textures, then it
 * will just return aSize.
 */
gfxIntSize CalculatePOTSize(const gfxIntSize& aSize, GLContext* gl)
{
  if (gl->CanUploadNonPowerOfTwo())
    return aSize;

  return gfxIntSize(NextPowerOfTwo(aSize.width), NextPowerOfTwo(aSize.height));
}

bool
ImageLayerOGL::LoadAsTexture(GLuint aTextureUnit, gfxIntSize* aSize)
{
  // this method shares a lot of code with RenderLayer, but it doesn't seem
  // to be possible to factor it out into a helper method

  if (!GetContainer()) {
    return false;
  }

  AutoLockImage autoLock(GetContainer());

  Image *image = autoLock.GetImage();
  if (!image) {
    return false;
  }

  if (image->GetFormat() != CAIRO_SURFACE) {
    return false;
  }

  CairoImage* cairoImage = static_cast<CairoImage*>(image);

  if (!cairoImage->mSurface) {
    return false;
  }

  CairoOGLBackendData *data = static_cast<CairoOGLBackendData*>(
    cairoImage->GetBackendData(LAYERS_OPENGL));

  if (!data) {
    NS_ASSERTION(cairoImage->mSurface->GetContentType() == gfxASurface::CONTENT_ALPHA,
                 "OpenGL mask layers must be backed by alpha surfaces");

    // allocate a new texture and save the details in the backend data
    data = new CairoOGLBackendData;
    data->mTextureSize = CalculatePOTSize(cairoImage->mSize, gl());

    GLTexture &texture = data->mTexture;
    texture.Allocate(mOGLManager->gl());

    if (!texture.IsAllocated()) {
      return false;
    }

    mozilla::gl::GLContext *texGL = texture.GetGLContext();
    texGL->MakeCurrent();

    GLuint texID = texture.GetTextureID();

    data->mLayerProgram =
      texGL->UploadSurfaceToTexture(cairoImage->mSurface,
                                    nsIntRect(0,0,
                                              data->mTextureSize.width,
                                              data->mTextureSize.height),
                                    texID, true, nsIntPoint(0,0), false,
                                    aTextureUnit);

    cairoImage->SetBackendData(LAYERS_OPENGL, data);

    gl()->MakeCurrent();
    gl()->fActiveTexture(aTextureUnit);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, texID);
    gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER,
                         LOCAL_GL_LINEAR);
    gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER,
                         LOCAL_GL_LINEAR);
    gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S,
                         LOCAL_GL_CLAMP_TO_EDGE);
    gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T,
                         LOCAL_GL_CLAMP_TO_EDGE);
  } else {
    gl()->fActiveTexture(aTextureUnit);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, data->mTexture.GetTextureID());
  }

  *aSize = data->mTextureSize;
  return true;
}

ShadowImageLayerOGL::ShadowImageLayerOGL(LayerManagerOGL* aManager)
  : ShadowImageLayer(aManager, nullptr)
  , LayerOGL(aManager)
  , mSharedHandle(0)
  , mInverted(false)
  , mTexture(0)
{
  mImplData = static_cast<LayerOGL*>(this);
}

ShadowImageLayerOGL::~ShadowImageLayerOGL()
{}

bool
ShadowImageLayerOGL::Init(const SharedImage& aFront)
{
  if (aFront.type() == SharedImage::TSurfaceDescriptor) {
    SurfaceDescriptor surface = aFront.get_SurfaceDescriptor();
    if (surface.type() == SurfaceDescriptor::TSharedTextureDescriptor) {
      SharedTextureDescriptor texture = surface.get_SharedTextureDescriptor();
      mSize = texture.size();
      mSharedHandle = texture.handle();
      mShareType = texture.shareType();
      mInverted = texture.inverted();
    } else {
      AutoOpenSurface autoSurf(OPEN_READ_ONLY, surface);
      mSize = autoSurf.Size();
      mTexImage = gl()->CreateTextureImage(nsIntSize(mSize.width, mSize.height),
                                           autoSurf.ContentType(),
                                           LOCAL_GL_CLAMP_TO_EDGE,
                                           mForceSingleTile
                                            ? TextureImage::ForceSingleTile
                                            : TextureImage::NoFlags);
    }
  } else {
    YUVImage yuv = aFront.get_YUVImage();

    AutoOpenSurface surfY(OPEN_READ_ONLY, yuv.Ydata());
    AutoOpenSurface surfU(OPEN_READ_ONLY, yuv.Udata());

    mSize = surfY.Size();
    mCbCrSize = surfU.Size();

    if (!mYUVTexture[0].IsAllocated()) {
      mYUVTexture[0].Allocate(gl());
      mYUVTexture[1].Allocate(gl());
      mYUVTexture[2].Allocate(gl());
    }

    NS_ASSERTION(mYUVTexture[0].IsAllocated() &&
                 mYUVTexture[1].IsAllocated() &&
                 mYUVTexture[2].IsAllocated(),
                 "Texture allocation failed!");

    gl()->MakeCurrent();
    SetClamping(gl(), mYUVTexture[0].GetTextureID());
    SetClamping(gl(), mYUVTexture[1].GetTextureID());
    SetClamping(gl(), mYUVTexture[2].GetTextureID());
    return true;
  }
  return false;
}

void
ShadowImageLayerOGL::Swap(const SharedImage& aNewFront,
                          SharedImage* aNewBack)
{
  if (!mDestroyed) {

    if (aNewFront.type() == SharedImage::TSharedImageID) {
      // We are using ImageBridge protocol. The image data will be queried at render
      // time in the parent side.
      uint64_t newID = aNewFront.get_SharedImageID().id();
      if (newID != mImageContainerID) {
        mImageContainerID = newID;
        mImageVersion = 0;
      }
    } else if (aNewFront.type() == SharedImage::TSurfaceDescriptor) {
      SurfaceDescriptor surface = aNewFront.get_SurfaceDescriptor();

      if (surface.type() == SurfaceDescriptor::TSharedTextureDescriptor) {
        SharedTextureDescriptor texture = surface.get_SharedTextureDescriptor();

        SharedTextureHandle newHandle = texture.handle();
        mSize = texture.size();
        mInverted = texture.inverted();

        if (mSharedHandle && newHandle != mSharedHandle)
          gl()->ReleaseSharedHandle(mShareType, mSharedHandle);

        mSharedHandle = newHandle;
        mShareType = texture.shareType();
      } else {
        AutoOpenSurface surf(OPEN_READ_ONLY, surface);
        gfxIntSize size = surf.Size();
        if (mSize != size || !mTexImage ||
            mTexImage->GetContentType() != surf.ContentType()) {
          Init(aNewFront);
        }
        // XXX this is always just ridiculously slow
        nsIntRegion updateRegion(nsIntRect(0, 0, size.width, size.height));
        mTexImage->DirectUpdate(surf.Get(), updateRegion);
      }
    } else {
      const YUVImage& yuv = aNewFront.get_YUVImage();
      UploadSharedYUVToTexture(yuv);
    }
  }

  *aNewBack = aNewFront;
}

void
ShadowImageLayerOGL::Disconnect()
{
  Destroy();
}

void
ShadowImageLayerOGL::Destroy()
{
  if (!mDestroyed) {
    mDestroyed = true;
    CleanupResources();
  }
}

Layer*
ShadowImageLayerOGL::GetLayer()
{
  return this;
}

LayerRenderState
ShadowImageLayerOGL::GetRenderState()
{
  if (!mImageContainerID) {
    return LayerRenderState();
  }

  // Update the associated compositor ID in case Composer2D succeeds,
  // because we won't enter RenderLayer() if so ...
  ImageContainerParent::SetCompositorIDForImage(
    mImageContainerID, mOGLManager->GetCompositorID());
  // ... but do *not* try to update the local image version.  We need
  // to retain that information in case we fall back on GL, so that we
  // can upload / attach buffers properly.

  SharedImage* img = ImageContainerParent::GetSharedImage(mImageContainerID);
  if (img && img->type() == SharedImage::TSurfaceDescriptor) {
    return LayerRenderState(&img->get_SurfaceDescriptor());
  }
  return LayerRenderState();
}

void ShadowImageLayerOGL::UploadSharedYUVToTexture(const YUVImage& yuv)
{
  AutoOpenSurface asurfY(OPEN_READ_ONLY, yuv.Ydata());
  AutoOpenSurface asurfU(OPEN_READ_ONLY, yuv.Udata());
  AutoOpenSurface asurfV(OPEN_READ_ONLY, yuv.Vdata());
  nsRefPtr<gfxImageSurface> surfY = asurfY.GetAsImage();
  nsRefPtr<gfxImageSurface> surfU = asurfU.GetAsImage();
  nsRefPtr<gfxImageSurface> surfV = asurfV.GetAsImage();
  mPictureRect = yuv.picture();

  gfxIntSize size = surfY->GetSize();
  gfxIntSize CbCrSize = surfU->GetSize();
  if (size != mSize || mCbCrSize != CbCrSize || !mYUVTexture[0].IsAllocated()) {
    
    mSize = surfY->GetSize();
    mCbCrSize = surfU->GetSize();

    if (!mYUVTexture[0].IsAllocated()) {
      mYUVTexture[0].Allocate(gl());
      mYUVTexture[1].Allocate(gl());
      mYUVTexture[2].Allocate(gl());
    }

    NS_ASSERTION(mYUVTexture[0].IsAllocated() &&
                 mYUVTexture[1].IsAllocated() &&
                 mYUVTexture[2].IsAllocated(),
                 "Texture allocation failed!");

    gl()->MakeCurrent();
    SetClamping(gl(), mYUVTexture[0].GetTextureID());
    SetClamping(gl(), mYUVTexture[1].GetTextureID());
    SetClamping(gl(), mYUVTexture[2].GetTextureID());
  }

  PlanarYCbCrImage::Data data;
  data.mYChannel = surfY->Data();
  data.mYStride = surfY->Stride();
  data.mYSize = surfY->GetSize();
  data.mCbChannel = surfU->Data();
  data.mCrChannel = surfV->Data();
  data.mCbCrStride = surfU->Stride();
  data.mCbCrSize = surfU->GetSize();

  UploadYUVToTexture(gl(), data, &mYUVTexture[0], &mYUVTexture[1], &mYUVTexture[2]);
}

void ShadowImageLayerOGL::UploadSharedYCbCrToTexture(ShmemYCbCrImage& aImage,
                                                     nsIntRect aPictureRect)
{
  mPictureRect = aPictureRect;

  gfxIntSize size = aImage.GetYSize();
  gfxIntSize CbCrSize = aImage.GetCbCrSize();
  if (size != mSize || mCbCrSize != CbCrSize || !mYUVTexture[0].IsAllocated()) {

    mSize = size;
    mCbCrSize = CbCrSize;

    if (!mYUVTexture[0].IsAllocated()) {
        mYUVTexture[0].Allocate(gl());
        mYUVTexture[1].Allocate(gl());
        mYUVTexture[2].Allocate(gl());
    }

    NS_ASSERTION(mYUVTexture[0].IsAllocated() &&
                 mYUVTexture[1].IsAllocated() &&
                 mYUVTexture[2].IsAllocated(),
                 "Texture allocation failed!");

    gl()->MakeCurrent();
    SetClamping(gl(), mYUVTexture[0].GetTextureID());
    SetClamping(gl(), mYUVTexture[1].GetTextureID());
    SetClamping(gl(), mYUVTexture[2].GetTextureID());
  }

  PlanarYCbCrImage::Data data;
  data.mYChannel = aImage.GetYData();
  data.mYStride = aImage.GetYStride();
  data.mYSize = aImage.GetYSize();
  data.mCbChannel = aImage.GetCbData();
  data.mCrChannel = aImage.GetCrData();
  data.mCbCrStride = aImage.GetCbCrStride();
  data.mCbCrSize = aImage.GetCbCrSize();

  UploadYUVToTexture(gl(), data, &mYUVTexture[0], &mYUVTexture[1], &mYUVTexture[2]);
}

void ShadowImageLayerOGL::UploadSharedRGBToTexture(ipc::Shmem *aShmem,
                                                   nsIntRect aPictureRect,
                                                   uint32_t aRgbFormat)
{
  mPictureRect = aPictureRect;

  if (aPictureRect.width != mSize.width ||
      aPictureRect.height != mSize.height ||
      !mRGBTexture.IsAllocated()) {
    mSize = gfxIntSize(aPictureRect.width, aPictureRect.height);

    if (!mRGBTexture.IsAllocated()) {
      mRGBTexture.Allocate(gl());
    }

    gl()->MakeCurrent();
    SetClamping(gl(), mRGBTexture.GetTextureID());
  }

  gfxASurface::gfxImageFormat rgbFormat = (gfxASurface::gfxImageFormat)aRgbFormat;
  GLuint texture = mRGBTexture.GetTextureID();
  uint32_t stride = gfxASurface::BytesPerPixel(rgbFormat) * aPictureRect.width;
  nsRefPtr<gfxASurface> surface = new gfxImageSurface(aShmem->get<uint8_t>(),
                                                      mSize,
                                                      stride,
                                                      rgbFormat);

  gl()->UploadSurfaceToTexture(surface,
                               nsIntRect(0, 0, mSize.width, mSize.height),
                               texture, true);
}


void
ShadowImageLayerOGL::RenderLayer(int aPreviousFrameBuffer,
                                 const nsIntPoint& aOffset)
{
  if (mOGLManager->CompositingDisabled()) {
    return;
  }
  mOGLManager->MakeCurrent();
  if (mImageContainerID) {
    ImageContainerParent::SetCompositorIDForImage(mImageContainerID,
                                                  mOGLManager->GetCompositorID());
    uint32_t imgVersion = ImageContainerParent::GetSharedImageVersion(mImageContainerID);
    SharedImage* img = ImageContainerParent::GetSharedImage(mImageContainerID);
    if (imgVersion != mImageVersion) {
      if (img && (img->type() == SharedImage::TYUVImage)) {
        UploadSharedYUVToTexture(img->get_YUVImage());
  
        mImageVersion = imgVersion;
      } else if (img && (img->type() == SharedImage::TYCbCrImage)) {
        ShmemYCbCrImage shmemImage(img->get_YCbCrImage().data(),
                                   img->get_YCbCrImage().offset());
        UploadSharedYCbCrToTexture(shmemImage, img->get_YCbCrImage().picture());

        mImageVersion = imgVersion;
      } else if (img && (img->type() == SharedImage::TRGBImage)) {
        UploadSharedRGBToTexture(&img->get_RGBImage().data(),
                                 img->get_RGBImage().picture(),
                                 img->get_RGBImage().rgbFormat());
        mImageVersion = imgVersion;
      }
    }
#ifdef MOZ_WIDGET_GONK
    if (img
        && (img->type() == SharedImage::TSurfaceDescriptor)
        && (img->get_SurfaceDescriptor().type() == SurfaceDescriptor::TSurfaceDescriptorGralloc)) {
      const SurfaceDescriptorGralloc& desc = img->get_SurfaceDescriptor().get_SurfaceDescriptorGralloc();
      sp<GraphicBuffer> graphicBuffer = GrallocBufferActor::GetFrom(desc);
      mSize = gfxIntSize(graphicBuffer->getWidth(), graphicBuffer->getHeight());
      if (!mExternalBufferTexture.IsAllocated()) {
        mExternalBufferTexture.Allocate(gl());
      }
      gl()->MakeCurrent();
      gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
      gl()->BindExternalBuffer(mExternalBufferTexture.GetTextureID(), graphicBuffer->getNativeBuffer());
      mImageVersion = imgVersion;
    }
#endif
  }


  if (mTexImage) {
    NS_ASSERTION(mTexImage->GetContentType() != gfxASurface::CONTENT_ALPHA,
                 "Image layer has alpha image");

    ShaderProgramOGL *colorProgram =
      mOGLManager->GetProgram(mTexImage->GetShaderProgramType(), GetMaskLayer());

    colorProgram->Activate();
    colorProgram->SetTextureUnit(0);
    colorProgram->SetLayerTransform(GetEffectiveTransform());
    colorProgram->SetLayerOpacity(GetEffectiveOpacity());
    colorProgram->SetRenderOffset(aOffset);
    colorProgram->LoadMask(GetMaskLayer());

    mTexImage->SetFilter(mFilter);
    mTexImage->BeginTileIteration();

    if (gl()->CanUploadNonPowerOfTwo()) {
      do {
        nsIntRect rect = mTexImage->GetTileRect();
        if (!rect.IsEmpty()) {
          TextureImage::ScopedBindTextureAndApplyFilter texBind(mTexImage, LOCAL_GL_TEXTURE0);
          colorProgram->SetLayerQuadRect(rect);
          mOGLManager->BindAndDrawQuad(colorProgram);
        }
      } while (mTexImage->NextTile());
    } else {
      do {
        nsIntRect rect = mTexImage->GetTileRect();
        if (!rect.IsEmpty()) {
          TextureImage::ScopedBindTextureAndApplyFilter texBind(mTexImage, LOCAL_GL_TEXTURE0);
          colorProgram->SetLayerQuadRect(rect);
          // We can't use BindAndDrawQuad because that always uploads the whole texture from 0.0f -> 1.0f
          // in x and y. We use BindAndDrawQuadWithTextureRect to actually draw a subrect of the texture
          mOGLManager->BindAndDrawQuadWithTextureRect(colorProgram,
                                                      nsIntRect(0, 0, mTexImage->GetTileRect().width,
                                                                mTexImage->GetTileRect().height),
                                                      mTexImage->GetTileRect().Size());
        }
      } while (mTexImage->NextTile());
    }
#ifdef MOZ_WIDGET_GONK
  } else if (mExternalBufferTexture.IsAllocated()) {
    gl()->MakeCurrent();
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, mExternalBufferTexture.GetTextureID());

    ShaderProgramOGL *program = mOGLManager->GetProgram(RGBAExternalLayerProgramType, GetMaskLayer());

    gl()->ApplyFilterToBoundTexture(LOCAL_GL_TEXTURE_EXTERNAL, mFilter);

    program->Activate();
    program->SetLayerQuadRect(nsIntRect(0, 0,
                                        mSize.width, mSize.height));
    program->SetLayerTransform(GetEffectiveTransform());
    program->SetLayerOpacity(GetEffectiveOpacity());
    program->SetRenderOffset(aOffset);
    program->SetTextureUnit(0);
    program->LoadMask(GetMaskLayer());

    mOGLManager->BindAndDrawQuad(program);

    // Make sure that we release the underlying external image
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, 0);
    mExternalBufferTexture.Release();
#endif
  } else if (mSharedHandle) {
    GLContext::SharedHandleDetails handleDetails;
    if (!gl()->GetSharedHandleDetails(mShareType, mSharedHandle, handleDetails)) {
      NS_ERROR("Failed to get shared handle details");
      return;
    }

    ShaderProgramOGL* program = mOGLManager->GetProgram(handleDetails.mProgramType, GetMaskLayer());
   
    program->Activate();
    program->SetLayerTransform(GetEffectiveTransform());
    program->SetLayerOpacity(GetEffectiveOpacity());
    program->SetRenderOffset(aOffset);
    program->SetTextureUnit(0);
    program->SetTextureTransform(handleDetails.mTextureTransform);
    program->LoadMask(GetMaskLayer());

    MakeTextureIfNeeded(gl(), mTexture);
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(handleDetails.mTarget, mTexture);
    
    if (!gl()->AttachSharedHandle(mShareType, mSharedHandle)) {
      NS_ERROR("Failed to bind shared texture handle");
      return;
    }

    gl()->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                             LOCAL_GL_ONE, LOCAL_GL_ONE);
    gl()->ApplyFilterToBoundTexture(mFilter);
    program->SetLayerQuadRect(nsIntRect(nsIntPoint(0, 0), mSize));
    mOGLManager->BindAndDrawQuad(program, mInverted);
    gl()->fBindTexture(handleDetails.mTarget, 0);
    gl()->DetachSharedHandle(mShareType, mSharedHandle);
  } else if (mRGBTexture.IsAllocated()) {
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mRGBTexture.GetTextureID());
    gl()->ApplyFilterToBoundTexture(mFilter);

    ShaderProgramOGL *shader = mOGLManager->GetProgram(RGBALayerProgramType, GetMaskLayer());
    shader->Activate();

    shader->SetLayerQuadRect(nsIntRect(0, 0,
                                           mPictureRect.width,
                                           mPictureRect.height));
    shader->SetTextureUnit(0);
    shader->SetLayerTransform(GetEffectiveTransform());
    shader->SetLayerOpacity(GetEffectiveOpacity());
    shader->SetRenderOffset(aOffset);
    shader->LoadMask(GetMaskLayer());

    mOGLManager->BindAndDrawQuadWithTextureRect(shader,
                                                mPictureRect,
                                                nsIntSize(mSize.width, mSize.height));
  } else {
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mYUVTexture[0].GetTextureID());
    gl()->ApplyFilterToBoundTexture(mFilter);
    gl()->fActiveTexture(LOCAL_GL_TEXTURE1);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mYUVTexture[1].GetTextureID());
    gl()->ApplyFilterToBoundTexture(mFilter);
    gl()->fActiveTexture(LOCAL_GL_TEXTURE2);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mYUVTexture[2].GetTextureID());
    gl()->ApplyFilterToBoundTexture(mFilter);

    ShaderProgramOGL *yuvProgram = mOGLManager->GetProgram(YCbCrLayerProgramType, GetMaskLayer());

    yuvProgram->Activate();
    yuvProgram->SetLayerQuadRect(nsIntRect(0, 0,
                                           mPictureRect.width,
                                           mPictureRect.height));
    yuvProgram->SetYCbCrTextureUnits(0, 1, 2);
    yuvProgram->SetLayerTransform(GetEffectiveTransform());
    yuvProgram->SetLayerOpacity(GetEffectiveOpacity());
    yuvProgram->SetRenderOffset(aOffset);
    yuvProgram->LoadMask(GetMaskLayer());

    mOGLManager->BindAndDrawQuadWithTextureRect(yuvProgram,
                                                mPictureRect,
                                                nsIntSize(mSize.width, mSize.height));
 }
}

bool
ShadowImageLayerOGL::LoadAsTexture(GLuint aTextureUnit, gfxIntSize* aSize)
{
  if (!mTexImage) {
    return false;
  }

  mTexImage->BindTextureAndApplyFilter(aTextureUnit);

  NS_ASSERTION(mTexImage->GetContentType() == gfxASurface::CONTENT_ALPHA,
               "OpenGL mask layers must be backed by alpha surfaces");

  // We're assuming that the gl backend won't cheat and use NPOT
  // textures when glContext says it can't (which seems to happen
  // on a mac when you force POT textures)
  *aSize = CalculatePOTSize(mTexImage->GetSize(), gl());
  return true;
}

void
ShadowImageLayerOGL::CleanupResources()
{
  if (mSharedHandle) {
    gl()->ReleaseSharedHandle(mShareType, mSharedHandle);
    mSharedHandle = 0;
  }

  mExternalBufferTexture.Release();
  mYUVTexture[0].Release();
  mYUVTexture[1].Release();
  mYUVTexture[2].Release();
  mRGBTexture.Release();
  mTexImage = nullptr;
}

} /* layers */
} /* mozilla */
