/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxSharedImageSurface.h"

#include "ImageLayerOGL.h"
#include "gfxImageSurface.h"
#include "gfxUtils.h"
#include "yuv_convert.h"
#include "GLContextProvider.h"
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
# include "GLXLibrary.h"
# include "mozilla/X11Util.h"
#endif

using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

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
    mContext = nsnull;
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

  mContext = nsnull;
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
  PRUint32 last = mRecycledTextures[aType].Length() - 1;
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

  aIOImage->SetBackendData(LayerManager::LAYERS_OPENGL, backendData.forget());
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

  if (!container)
    return;

  mOGLManager->MakeCurrent();

  AutoLockImage autoLock(container);

  Image *image = autoLock.GetImage();
  if (!image) {
    return;
  }

  NS_ASSERTION(image->GetFormat() != Image::REMOTE_IMAGE_BITMAP,
    "Remote images aren't handled yet in OGL layers!");
  NS_ASSERTION(mScaleMode == SCALE_NONE,
    "Scale modes other than none not handled yet in OGL layers!");

  if (image->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImage *yuvImage =
      static_cast<PlanarYCbCrImage*>(image);

    if (!yuvImage->mBufferSize) {
      return;
    }

    PlanarYCbCrOGLBackendData *data =
      static_cast<PlanarYCbCrOGLBackendData*>(yuvImage->GetBackendData(LayerManager::LAYERS_OPENGL));

    if (data && data->mTextures->GetGLContext() != gl()) {
      // If these textures were allocated by another layer manager,
      // clear them out and re-allocate below.
      data = nsnull;
      yuvImage->SetBackendData(LayerManager::LAYERS_OPENGL, nsnull);
    }

    if (!data) {
      AllocateTexturesYCbCr(yuvImage);
      data = static_cast<PlanarYCbCrOGLBackendData*>(yuvImage->GetBackendData(LayerManager::LAYERS_OPENGL));
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
                                        yuvImage->mSize.width,
                                        yuvImage->mSize.height));
    program->SetLayerTransform(GetEffectiveTransform());
    program->SetLayerOpacity(GetEffectiveOpacity());
    program->SetRenderOffset(aOffset);
    program->SetYCbCrTextureUnits(0, 1, 2);
    program->LoadMask(GetMaskLayer());

    mOGLManager->BindAndDrawQuadWithTextureRect(program,
                                                yuvImage->mData.GetPictureRect(),
                                                nsIntSize(yuvImage->mData.mYSize.width,
                                                          yuvImage->mData.mYSize.height));

    // We shouldn't need to do this, but do it anyway just in case
    // someone else forgets.
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  } else if (image->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImage *cairoImage =
      static_cast<CairoImage*>(image);

    if (!cairoImage->mSurface) {
      return;
    }

    NS_ASSERTION(cairoImage->mSurface->GetContentType() != gfxASurface::CONTENT_ALPHA,
                 "Image layer has alpha image");

    CairoOGLBackendData *data =
      static_cast<CairoOGLBackendData*>(cairoImage->GetBackendData(LayerManager::LAYERS_OPENGL));

    if (data && data->mTexture.GetGLContext() != gl()) {
      // If this texture was allocated by another layer manager, clear
      // it out and re-allocate below.
      data = nsnull;
      cairoImage->SetBackendData(LayerManager::LAYERS_OPENGL, nsnull);
    }

    if (!data) {
      AllocateTexturesCairo(cairoImage);
      data = static_cast<CairoOGLBackendData*>(cairoImage->GetBackendData(LayerManager::LAYERS_OPENGL));
    }

    if (!data || data->mTexture.GetGLContext() != gl()) {
      // XXX - Can this ever happen? If so I need to fix this!
      return;
    }

    gl()->MakeCurrent();
    unsigned int iwidth  = cairoImage->mSize.width;
    unsigned int iheight = cairoImage->mSize.height;

    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, data->mTexture.GetTextureID());

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
    GLXPixmap pixmap;

    if (cairoImage->mSurface) {
        pixmap = sGLXLibrary.CreatePixmap(cairoImage->mSurface);
        NS_ASSERTION(pixmap, "Failed to create pixmap!");
        if (pixmap) {
            sGLXLibrary.BindTexImage(pixmap);
        }
    }
#endif

    ShaderProgramOGL *program = 
      mOGLManager->GetProgram(data->mLayerProgram, GetMaskLayer());

    gl()->ApplyFilterToBoundTexture(mFilter);

    program->Activate();
    // The following uniform controls the scaling of the vertex coords.
    // Instead of setting the scale here and using coords in the range [0,1], we
    // set an identity transform and use pixel coordinates below
    program->SetLayerQuadRect(nsIntRect(0, 0, 1, 1));
    program->SetLayerTransform(GetEffectiveTransform());
    program->SetLayerOpacity(GetEffectiveOpacity());
    program->SetRenderOffset(aOffset);
    program->SetTextureUnit(0);
    program->LoadMask(GetMaskLayer());

    nsIntRect rect = GetVisibleRegion().GetBounds();

    GLContext::RectTriangles triangleBuffer;

    float tex_offset_u = float(rect.x % iwidth) / iwidth;
    float tex_offset_v = float(rect.y % iheight) / iheight;
    triangleBuffer.addRect(rect.x, rect.y,
                           rect.x + rect.width, rect.y + rect.height,
                           tex_offset_u, tex_offset_v,
                           tex_offset_u + float(rect.width) / float(iwidth),
                           tex_offset_v + float(rect.height) / float(iheight));

    GLuint vertAttribIndex =
        program->AttribLocation(ShaderProgramOGL::VertexCoordAttrib);
    GLuint texCoordAttribIndex =
        program->AttribLocation(ShaderProgramOGL::TexCoordAttrib);
    NS_ASSERTION(texCoordAttribIndex != GLuint(-1), "no texture coords?");

    gl()->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
    gl()->fVertexAttribPointer(vertAttribIndex, 2,
                               LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                               triangleBuffer.vertexPointer());

    gl()->fVertexAttribPointer(texCoordAttribIndex, 2,
                               LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                               triangleBuffer.texCoordPointer());
    {
        gl()->fEnableVertexAttribArray(texCoordAttribIndex);
        {
            gl()->fEnableVertexAttribArray(vertAttribIndex);
            gl()->fDrawArrays(LOCAL_GL_TRIANGLES, 0, triangleBuffer.elements());
            gl()->fDisableVertexAttribArray(vertAttribIndex);
        }
        gl()->fDisableVertexAttribArray(texCoordAttribIndex);
    }
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
    if (cairoImage->mSurface && pixmap) {
        sGLXLibrary.ReleaseTexImage(pixmap);
        sGLXLibrary.DestroyPixmap(pixmap);
    }
#endif
#ifdef XP_MACOSX
  } else if (image->GetFormat() == Image::MAC_IO_SURFACE) {
     MacIOSurfaceImage *ioImage =
       static_cast<MacIOSurfaceImage*>(image);

     if (!mOGLManager->GetThebesLayerCallback()) {
       // If its an empty transaction we still need to update
       // the plugin IO Surface and make sure we grab the
       // new image
       ioImage->Update(GetContainer());
       image = nsnull;
       autoLock.Refresh();
       image = autoLock.GetImage();
       gl()->MakeCurrent();
       ioImage = static_cast<MacIOSurfaceImage*>(image);
     }

     if (!ioImage) {
       return;
     }

     gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

     if (!ioImage->GetBackendData(LayerManager::LAYERS_OPENGL)) {
       AllocateTextureIOSurface(ioImage, gl());
     }

     MacIOSurfaceImageOGLBackendData *data =
      static_cast<MacIOSurfaceImageOGLBackendData*>(ioImage->GetBackendData(LayerManager::LAYERS_OPENGL));

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
  if (!aImage->mBufferSize)
    return;

  nsAutoPtr<PlanarYCbCrOGLBackendData> backendData(
    new PlanarYCbCrOGLBackendData);

  PlanarYCbCrImage::Data &data = aImage->mData;

  gl()->MakeCurrent();
 
  mTextureRecycleBin->GetTexture(TextureRecycleBin::TEXTURE_Y, data.mYSize, gl(), &backendData->mTextures[0]);
  SetClamping(gl(), backendData->mTextures[0].GetTextureID());

  mTextureRecycleBin->GetTexture(TextureRecycleBin::TEXTURE_C, data.mCbCrSize, gl(), &backendData->mTextures[1]);
  SetClamping(gl(), backendData->mTextures[1].GetTextureID());

  mTextureRecycleBin->GetTexture(TextureRecycleBin::TEXTURE_C, data.mCbCrSize, gl(), &backendData->mTextures[2]);
  SetClamping(gl(), backendData->mTextures[2].GetTextureID());

  UploadYUVToTexture(gl(), aImage->mData,
                     &backendData->mTextures[0],
                     &backendData->mTextures[1],
                     &backendData->mTextures[2]);

  backendData->mYSize = aImage->mData.mYSize;
  backendData->mCbCrSize = aImage->mData.mCbCrSize;
  backendData->mTextureRecycleBin = mTextureRecycleBin;

  aImage->SetBackendData(LayerManager::LAYERS_OPENGL, backendData.forget());
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

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
  if (sGLXLibrary.SupportsTextureFromPixmap(aImage->mSurface)) {
    if (aImage->mSurface->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA) {
      backendData->mLayerProgram = gl::RGBALayerProgramType;
    } else {
      backendData->mLayerProgram = gl::RGBXLayerProgramType;
    }

    aImage->SetBackendData(LayerManager::LAYERS_OPENGL, backendData.forget());
    return;
  }
#endif
  backendData->mLayerProgram =
    gl->UploadSurfaceToTexture(aImage->mSurface,
                               nsIntRect(0,0, aImage->mSize.width, aImage->mSize.height),
                               tex, true);

  aImage->SetBackendData(LayerManager::LAYERS_OPENGL, backendData.forget());
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

  if (image->GetFormat() != Image::CAIRO_SURFACE) {
    return false;
  }

  CairoImage* cairoImage = static_cast<CairoImage*>(image);

  if (!cairoImage->mSurface) {
    return false;
  }

  CairoOGLBackendData *data = static_cast<CairoOGLBackendData*>(
    cairoImage->GetBackendData(LayerManager::LAYERS_OPENGL));

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

    cairoImage->SetBackendData(LayerManager::LAYERS_OPENGL, data);

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
  : ShadowImageLayer(aManager, nsnull)
  , LayerOGL(aManager)
{
  mImplData = static_cast<LayerOGL*>(this);
}

ShadowImageLayerOGL::~ShadowImageLayerOGL()
{}

bool
ShadowImageLayerOGL::Init(const SharedImage& aFront)
{
  if (aFront.type() == SharedImage::TSurfaceDescriptor) {
    SurfaceDescriptor desc = aFront.get_SurfaceDescriptor();
    nsRefPtr<gfxASurface> surf =
      ShadowLayerForwarder::OpenDescriptor(desc);
    mSize = surf->GetSize();
    mTexImage = gl()->CreateTextureImage(nsIntSize(mSize.width, mSize.height),
                                         surf->GetContentType(),
                                         LOCAL_GL_CLAMP_TO_EDGE,
                                         mForceSingleTile
                                          ? TextureImage::ForceSingleTile
                                          : TextureImage::NoFlags);
    return true;
  } else {
    YUVImage yuv = aFront.get_YUVImage();

    nsRefPtr<gfxSharedImageSurface> surfY =
      gfxSharedImageSurface::Open(yuv.Ydata());
    nsRefPtr<gfxSharedImageSurface> surfU =
      gfxSharedImageSurface::Open(yuv.Udata());
    nsRefPtr<gfxSharedImageSurface> surfV =
      gfxSharedImageSurface::Open(yuv.Vdata());

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
    return true;
  }
  return false;
}

void
ShadowImageLayerOGL::Swap(const SharedImage& aNewFront,
                          SharedImage* aNewBack)
{
  if (!mDestroyed) {
    if (aNewFront.type() == SharedImage::TSurfaceDescriptor) {
      nsRefPtr<gfxASurface> surf =
        ShadowLayerForwarder::OpenDescriptor(aNewFront.get_SurfaceDescriptor());
      gfxIntSize size = surf->GetSize();
      if (mSize != size || !mTexImage ||
          mTexImage->GetContentType() != surf->GetContentType()) {
        Init(aNewFront);
      }
      // XXX this is always just ridiculously slow
      nsIntRegion updateRegion(nsIntRect(0, 0, size.width, size.height));
      mTexImage->DirectUpdate(surf, updateRegion);
    } else {
      const YUVImage& yuv = aNewFront.get_YUVImage();

      nsRefPtr<gfxSharedImageSurface> surfY =
        gfxSharedImageSurface::Open(yuv.Ydata());
      nsRefPtr<gfxSharedImageSurface> surfU =
        gfxSharedImageSurface::Open(yuv.Udata());
      nsRefPtr<gfxSharedImageSurface> surfV =
        gfxSharedImageSurface::Open(yuv.Vdata());
      mPictureRect = yuv.picture();

      gfxIntSize size = surfY->GetSize();
      gfxIntSize CbCrSize = surfU->GetSize();
      if (size != mSize || mCbCrSize != CbCrSize || !mYUVTexture[0].IsAllocated()) {
        Init(aNewFront);
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

void
ShadowImageLayerOGL::RenderLayer(int aPreviousFrameBuffer,
                                 const nsIntPoint& aOffset)
{
  mOGLManager->MakeCurrent();

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
        TextureImage::ScopedBindTextureAndApplyFilter texBind(mTexImage, LOCAL_GL_TEXTURE0);
        colorProgram->SetLayerQuadRect(mTexImage->GetTileRect());
        mOGLManager->BindAndDrawQuad(colorProgram);
      } while (mTexImage->NextTile());
    } else {
      do {
        TextureImage::ScopedBindTextureAndApplyFilter texBind(mTexImage, LOCAL_GL_TEXTURE0);
        colorProgram->SetLayerQuadRect(mTexImage->GetTileRect());
        // We can't use BindAndDrawQuad because that always uploads the whole texture from 0.0f -> 1.0f
        // in x and y. We use BindAndDrawQuadWithTextureRect to actually draw a subrect of the texture
        mOGLManager->BindAndDrawQuadWithTextureRect(colorProgram,
                                                    nsIntRect(0, 0, mTexImage->GetTileRect().width,
                                                                    mTexImage->GetTileRect().height),
                                                    mTexImage->GetTileRect().Size());
      } while (mTexImage->NextTile());
    }

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
  mYUVTexture[0].Release();
  mYUVTexture[1].Release();
  mYUVTexture[2].Release();
  mTexImage = nsnull;
}

} /* layers */
} /* mozilla */
