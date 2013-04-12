/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureHostOGL.h"
#include "ipc/AutoOpenSurface.h"
#include "gfx2DGlue.h"
#include "ShmemYCbCrImage.h"
#include "GLContext.h"
#include "gfxImageSurface.h"
#include "SurfaceStream.h"
#include "SharedSurface.h"
#include "SharedSurfaceGL.h"
#include "SharedSurfaceEGL.h"
#include "mozilla/layers/CompositorOGL.h"

using namespace mozilla::gl;
using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

TemporaryRef<TextureHost>
CreateTextureHostOGL(SurfaceDescriptorType aDescriptorType,
                     uint32_t aTextureHostFlags,
                     uint32_t aTextureFlags)
{
  RefPtr<TextureHost> result = nullptr;

  if (aDescriptorType == SurfaceDescriptor::TYCbCrImage) {
    result = new YCbCrTextureHostOGL();
  } else if (aDescriptorType == SurfaceDescriptor::TSurfaceStreamDescriptor) {
    result = new SurfaceStreamHostOGL();
  } else if (aDescriptorType == SurfaceDescriptor::TSharedTextureDescriptor) {
    result = new SharedTextureHostOGL();
#ifdef MOZ_WIDGET_GONK
  } else if (aDescriptorType == SurfaceDescriptor::TSurfaceDescriptorGralloc) {
    result = new GrallocTextureHostOGL();
#endif
  } else if (aTextureHostFlags & TEXTURE_HOST_TILED) {
    result = new TiledTextureHostOGL();
  } else {
    result = new TextureImageTextureHostOGL();
  }

  NS_ASSERTION(result, "Result should have been created.");

  result->SetFlags(aTextureFlags);
  return result.forget();
}

static void
MakeTextureIfNeeded(gl::GLContext* gl, GLuint& aTexture)
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

static gl::TextureImage::Flags
FlagsToGLFlags(TextureFlags aFlags)
{
  uint32_t result = TextureImage::NoFlags;

  if (aFlags & UseNearestFilter)
    result |= TextureImage::UseNearestFilter;
  if (aFlags & NeedsYFlip)
    result |= TextureImage::NeedsYFlip;
  if (aFlags & ForceSingleTile)
    result |= TextureImage::ForceSingleTile;

  return static_cast<gl::TextureImage::Flags>(result);
}

GLenum
WrapMode(gl::GLContext *aGl, bool aAllowRepeat)
{
  if (aAllowRepeat &&
      (aGl->IsExtensionSupported(GLContext::ARB_texture_non_power_of_two) ||
       aGl->IsExtensionSupported(GLContext::OES_texture_npot))) {
    return LOCAL_GL_REPEAT;
  }
  return LOCAL_GL_CLAMP_TO_EDGE;
}

gfx::SurfaceFormat
FormatFromShaderType(ShaderProgramType aShaderType)
{
  switch (aShaderType) {
    case RGBALayerProgramType:
    case RGBALayerExternalProgramType:
    case RGBARectLayerProgramType:
    case RGBAExternalLayerProgramType:
      return FORMAT_R8G8B8A8;
    case RGBXLayerProgramType:
      return FORMAT_R8G8B8X8;
    case BGRALayerProgramType:
      return FORMAT_B8G8R8A8;
    case BGRXLayerProgramType:
      return FORMAT_B8G8R8X8;
    default:
      MOZ_NOT_REACHED("Unsupported texture shader type");
      return FORMAT_UNKNOWN;
  }
}

TextureImageTextureHostOGL::~TextureImageTextureHostOGL()
{
  MOZ_COUNT_DTOR(TextureImageTextureHostOGL);
  if (mTexture && mTexture->InUpdate()) {
    mTexture->EndUpdate();
  }
}

gfx::IntSize
TextureImageTextureHostOGL::GetSize() const
{
  if (mTexture) {
    if (mIterating) {
      nsIntRect rect = mTexture->GetTileRect();
      return gfx::IntSize(rect.width, rect.height);
    }
    return gfx::IntSize(mTexture->GetSize().width, mTexture->GetSize().height);
  }
  return gfx::IntSize(0, 0);
}


void
TextureImageTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  GLContext* newGL = glCompositor ? glCompositor->gl() : nullptr;
  if (mGL != newGL) {
    mGL = newGL;
    mTexture = nullptr;
    // if we have a buffer we reupload it with the new gl context
    // Post landing TODO: the new TextureClient/Host model will make this
    // go away.
    if (newGL && mBuffer && IsSurfaceDescriptorValid(*mBuffer)) {
      UpdateImpl(*mBuffer);
    }
  }
}

void
TextureImageTextureHostOGL::UpdateImpl(const SurfaceDescriptor& aImage,
                                       nsIntRegion* aRegion)
{
  if (!mGL) {
    NS_WARNING("trying to update TextureImageTextureHostOGL without a compositor ?");
    return;
  }
  AutoOpenSurface surf(OPEN_READ_ONLY, aImage);
  nsIntSize size = surf.Size();

  if (!mTexture ||
      mTexture->GetSize() != size ||
      mTexture->GetContentType() != surf.ContentType()) {
    mTexture = mGL->CreateTextureImage(size,
                                       surf.ContentType(),
                                       WrapMode(mGL, mFlags & AllowRepeat),
                                       FlagsToGLFlags(mFlags));
  }

  // XXX this is always just ridiculously slow
  nsIntRegion updateRegion;

  if (!aRegion) {
    updateRegion = nsIntRegion(nsIntRect(0, 0, size.width, size.height));
  } else {
    updateRegion = *aRegion;
  }
  mTexture->DirectUpdate(surf.Get(), updateRegion);

  if (mTexture->InUpdate()) {
    mTexture->EndUpdate();
  }
}

bool
TextureImageTextureHostOGL::Lock()
{
  if (!mTexture) {
    NS_WARNING("TextureImageAsTextureHost to be composited without texture");
    return false;
  }

  NS_ASSERTION(mTexture->GetContentType() != gfxASurface::CONTENT_ALPHA,
                "Image layer has alpha image");

  mFormat = FormatFromShaderType(mTexture->GetShaderProgramType());

  return true;
}

void
SharedTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  if (mGL && !glCompositor) {
    DeleteTextures();
  }
  mGL = glCompositor ? glCompositor->gl() : nullptr;
}

void
SharedTextureHostOGL::DeleteTextures()
{
  MOZ_ASSERT(mGL);
  mGL->MakeCurrent();
  if (mSharedHandle) {
    mGL->ReleaseSharedHandle(mShareType, mSharedHandle);
    mSharedHandle = 0;
  }
  if (mTextureHandle) {
    mGL->fDeleteTextures(1, &mTextureHandle);
    mTextureHandle = 0;
  }
}

void
SharedTextureHostOGL::UpdateImpl(const SurfaceDescriptor& aImage,
                                 nsIntRegion* aRegion)
{
  SwapTexturesImpl(aImage, aRegion);
}

void
SharedTextureHostOGL::SwapTexturesImpl(const SurfaceDescriptor& aImage,
                                       nsIntRegion* aRegion)
{
  NS_ASSERTION(aImage.type() == SurfaceDescriptor::TSharedTextureDescriptor,
              "Invalid descriptor");

  SharedTextureDescriptor texture = aImage.get_SharedTextureDescriptor();

  SharedTextureHandle newHandle = texture.handle();
  nsIntSize size = texture.size();
  mSize = gfx::IntSize(size.width, size.height);
  if (texture.inverted()) {
    mFlags |= NeedsYFlip;
  }

  if (mSharedHandle && mSharedHandle != newHandle) {
    mGL->ReleaseSharedHandle(mShareType, mSharedHandle);
  }

  mShareType = texture.shareType();
  mSharedHandle = newHandle;

  GLContext::SharedHandleDetails handleDetails;
  if (mSharedHandle && mGL->GetSharedHandleDetails(mShareType, mSharedHandle, handleDetails)) {
    mTextureTarget = handleDetails.mTarget;
    mShaderProgram = handleDetails.mProgramType;
    mFormat = FormatFromShaderType(mShaderProgram);
    mTextureTransform = handleDetails.mTextureTransform;
  }
}

bool
SharedTextureHostOGL::Lock()
{
  MakeTextureIfNeeded(mGL, mTextureHandle);

  mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
  mGL->fBindTexture(mTextureTarget, mTextureHandle);
  if (!mGL->AttachSharedHandle(mShareType, mSharedHandle)) {
    NS_ERROR("Failed to bind shared texture handle");
    return false;
  }

  return true;
}

void
SharedTextureHostOGL::Unlock()
{
  mGL->DetachSharedHandle(mShareType, mSharedHandle);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, 0);
}

void
SurfaceStreamHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  if (mGL && !glCompositor) {
    DeleteTextures();
  }
  mGL = glCompositor ? glCompositor->gl() : nullptr;
}

void
SurfaceStreamHostOGL::DeleteTextures()
{
  if (mUploadTexture) {
    MOZ_ASSERT(mGL);
    mGL->MakeCurrent();
    mGL->fDeleteTextures(1, &mUploadTexture);
    mUploadTexture = 0;
    mTextureHandle = 0;
  }
}

void
SurfaceStreamHostOGL::SwapTexturesImpl(const SurfaceDescriptor& aImage,
                                       nsIntRegion* aRegion)
{
  MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TSurfaceStreamDescriptor,
             "Invalid descriptor");
}

void
SurfaceStreamHostOGL::Unlock()
{
  // We don't know what this is unless we're locked
  mFormat = gfx::FORMAT_UNKNOWN;
}

bool
SurfaceStreamHostOGL::Lock()
{
  mGL->MakeCurrent();
  SurfaceStream* surfStream = nullptr;
  SharedSurface* sharedSurf = nullptr;
  const SurfaceStreamDescriptor& streamDesc =
    mBuffer->get_SurfaceStreamDescriptor();

  surfStream = SurfaceStream::FromHandle(streamDesc.handle());
  MOZ_ASSERT(surfStream);

  sharedSurf = surfStream->SwapConsumer();
  if (!sharedSurf) {
    // We don't have a valid surf to show yet.
    return false;
  }

  mGL->MakeCurrent();

  mSize = IntSize(sharedSurf->Size().width, sharedSurf->Size().height);

  gfxImageSurface* toUpload = nullptr;
  switch (sharedSurf->Type()) {
    case SharedSurfaceType::GLTextureShare: {
      mTextureHandle = SharedSurface_GLTexture::Cast(sharedSurf)->Texture();
      MOZ_ASSERT(mTextureHandle);
      mShaderProgram = sharedSurf->HasAlpha() ? RGBALayerProgramType
                                              : RGBXLayerProgramType;
      break;
    }
    case SharedSurfaceType::EGLImageShare: {
      SharedSurface_EGLImage* eglImageSurf =
          SharedSurface_EGLImage::Cast(sharedSurf);

      mTextureHandle = eglImageSurf->AcquireConsumerTexture(mGL);
      if (!mTextureHandle) {
        toUpload = eglImageSurf->GetPixels();
        MOZ_ASSERT(toUpload);
      } else {
        mShaderProgram = sharedSurf->HasAlpha() ? RGBALayerProgramType
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
      return false;
  }

  if (toUpload) {
    // mBounds seems to end up as (0,0,0,0) a lot, so don't use it?
    nsIntSize size(toUpload->GetSize());
    nsIntRect rect(nsIntPoint(0,0), size);
    nsIntRegion bounds(rect);
    mShaderProgram = mGL->UploadSurfaceToTexture(toUpload,
                                                 bounds,
                                                 mUploadTexture,
                                                 true);
    mTextureHandle = mUploadTexture;
  }

  mFormat = FormatFromShaderType(mShaderProgram);

  MOZ_ASSERT(mTextureHandle);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextureHandle);
  mGL->fTexParameteri(LOCAL_GL_TEXTURE_2D,
                      LOCAL_GL_TEXTURE_WRAP_S,
                      LOCAL_GL_CLAMP_TO_EDGE);
  mGL->fTexParameteri(LOCAL_GL_TEXTURE_2D,
                      LOCAL_GL_TEXTURE_WRAP_T,
                      LOCAL_GL_CLAMP_TO_EDGE);
  return true;
}


void
YCbCrTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  GLContext* newGL = glCompositor ? glCompositor->gl() : nullptr;
  if (mGL != newGL) {
    mGL = newGL;
    mYTexture->mTexImage = nullptr;
    mCbTexture->mTexImage = nullptr;
    mCrTexture->mTexImage = nullptr;
    // if we have a buffer we reupload it with the new gl context
    if (newGL && mBuffer && mBuffer->type() == SurfaceDescriptor::TYCbCrImage) {
      UpdateImpl(*mBuffer);
    }
  }
}

void
YCbCrTextureHostOGL::UpdateImpl(const SurfaceDescriptor& aImage,
                                nsIntRegion* aRegion)
{
  if (!mGL) {
    return;
  }
  NS_ASSERTION(aImage.type() == SurfaceDescriptor::TYCbCrImage, "SurfaceDescriptor mismatch");

  ShmemYCbCrImage shmemImage(aImage.get_YCbCrImage().data(),
                             aImage.get_YCbCrImage().offset());

  gfxIntSize gfxSize = shmemImage.GetYSize();
  gfxIntSize gfxCbCrSize = shmemImage.GetCbCrSize();

  if (!mYTexture->mTexImage || mYTexture->mTexImage->GetSize() != gfxSize) {
    mYTexture->mTexImage = CreateBasicTextureImage(mGL,
                                                   gfxSize,
                                                   gfxASurface::CONTENT_ALPHA,
                                                   WrapMode(mGL, mFlags & AllowRepeat),
                                                   FlagsToGLFlags(mFlags));
  }
  if (!mCbTexture->mTexImage || mCbTexture->mTexImage->GetSize() != gfxCbCrSize) {
    mCbTexture->mTexImage = CreateBasicTextureImage(mGL,
                                                    gfxCbCrSize,
                                                    gfxASurface::CONTENT_ALPHA,
                                                    WrapMode(mGL, mFlags & AllowRepeat),
                                                    FlagsToGLFlags(mFlags));
  }
  if (!mCrTexture->mTexImage || mCrTexture->mTexImage->GetSize() != gfxCbCrSize) {
    mCrTexture->mTexImage = CreateBasicTextureImage(mGL,
                                                    gfxCbCrSize,
                                                    gfxASurface::CONTENT_ALPHA,
                                                    WrapMode(mGL, mFlags & AllowRepeat),
                                                    FlagsToGLFlags(mFlags));
  }

  RefPtr<gfxImageSurface> tempY = new gfxImageSurface(shmemImage.GetYData(),
                                      gfxSize, shmemImage.GetYStride(),
                                      gfxASurface::ImageFormatA8);
  RefPtr<gfxImageSurface> tempCb = new gfxImageSurface(shmemImage.GetCbData(),
                                       gfxCbCrSize, shmemImage.GetCbCrStride(),
                                       gfxASurface::ImageFormatA8);
  RefPtr<gfxImageSurface> tempCr = new gfxImageSurface(shmemImage.GetCrData(),
                                       gfxCbCrSize, shmemImage.GetCbCrStride(),
                                       gfxASurface::ImageFormatA8);

  nsIntRegion yRegion(nsIntRect(0, 0, gfxSize.width, gfxSize.height));
  nsIntRegion cbCrRegion(nsIntRect(0, 0, gfxCbCrSize.width, gfxCbCrSize.height));

  mYTexture->mTexImage->DirectUpdate(tempY, yRegion);
  mCbTexture->mTexImage->DirectUpdate(tempCb, cbCrRegion);
  mCrTexture->mTexImage->DirectUpdate(tempCr, cbCrRegion);
}

bool
YCbCrTextureHostOGL::Lock()
{
  return true;
}

TiledTextureHostOGL::~TiledTextureHostOGL()
{
  DeleteTextures();
}

static void
GetFormatAndTileForImageFormat(gfxASurface::gfxImageFormat aFormat,
                               GLenum& aOutFormat,
                               GLenum& aOutType)
{
  if (aFormat == gfxASurface::ImageFormatRGB16_565) {
    aOutFormat = LOCAL_GL_RGB;
    aOutType = LOCAL_GL_UNSIGNED_SHORT_5_6_5;
  } else {
    aOutFormat = LOCAL_GL_RGBA;
    aOutType = LOCAL_GL_UNSIGNED_BYTE;
  }
}

void
TiledTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  if (mGL && !glCompositor) {
    DeleteTextures();
  }
  mGL = glCompositor ? glCompositor->gl() : nullptr;
}

void
TiledTextureHostOGL::DeleteTextures()
{
  if (mTextureHandle) {
    mGL->MakeCurrent();
    mGL->fDeleteTextures(1, &mTextureHandle);

    gl::GLContext::UpdateTextureMemoryUsage(gl::GLContext::MemoryFreed, mGLFormat,
                                            GetTileType(), TILEDLAYERBUFFER_TILE_SIZE);
    mTextureHandle = 0;
  }
}

void
TiledTextureHostOGL::Update(gfxReusableSurfaceWrapper* aReusableSurface, TextureFlags aFlags, const gfx::IntSize& aSize)
{
  mSize = aSize;
  mGL->MakeCurrent();
  if (aFlags & NewTile) {
    SetFlags(aFlags);
    mGL->fGenTextures(1, &mTextureHandle);
    mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextureHandle);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
  } else {
    mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextureHandle);
    // We're re-using a texture, but the format may change. Update the memory
    // reporter with a free and alloc (below) using the old and new formats.
    gl::GLContext::UpdateTextureMemoryUsage(gl::GLContext::MemoryFreed, mGLFormat,
                                            GetTileType(), TILEDLAYERBUFFER_TILE_SIZE);
  }

  GLenum type;
  GetFormatAndTileForImageFormat(aReusableSurface->Format(), mGLFormat, type);

  const unsigned char* buf = aReusableSurface->GetReadOnlyData();
  mGL->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, mGLFormat,
                   TILEDLAYERBUFFER_TILE_SIZE, TILEDLAYERBUFFER_TILE_SIZE, 0,
                   mGLFormat, type, buf);

  gl::GLContext::UpdateTextureMemoryUsage(gl::GLContext::MemoryAllocated, mGLFormat,
                                          type, TILEDLAYERBUFFER_TILE_SIZE);

  if (mGLFormat == LOCAL_GL_RGB) {
    mFormat = FORMAT_R8G8B8X8;
  } else {
    mFormat = FORMAT_B8G8R8A8;
  }
}

bool
TiledTextureHostOGL::Lock()
{
  if (!mTextureHandle) {
    NS_WARNING("TiledTextureHostOGL not ready to be composited");
    return false;
  }

  mGL->MakeCurrent();
  mGL->fActiveTexture(LOCAL_GL_TEXTURE0);

  return true;
}

#ifdef MOZ_WIDGET_GONK
static gfx::SurfaceFormat
SurfaceFormatForAndroidPixelFormat(android::PixelFormat aFormat)
{
  switch (aFormat) {
  case android::PIXEL_FORMAT_RGBA_8888:
    return FORMAT_B8G8R8A8;
  case android::PIXEL_FORMAT_RGBX_8888:
    return FORMAT_B8G8R8X8;
  case android::PIXEL_FORMAT_RGB_565:
    return FORMAT_R5G6B5;
  case android::PIXEL_FORMAT_A_8:
    return FORMAT_A8;
  default:
    MOZ_NOT_REACHED("Unknown Android pixel format");
    return FORMAT_B8G8R8A8;
  }
}

void GrallocTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  if (mGL && !glCompositor) {
    DeleteTextures();
  }
  mGL = glCompositor ? glCompositor->gl() : nullptr;
}

void
GrallocTextureHostOGL::DeleteTextures()
{
  if (mGLTexture || mEGLImage) {
    mGL->MakeCurrent();
    if (mGLTexture) {
      mGL->fDeleteTextures(1, &mGLTexture);
      mGLTexture= 0;
    }
    if (mEGLImage) {
      mGL->DestroyEGLImage(mEGLImage);
      mEGLImage = 0;
    }
  }
}

void
GrallocTextureHostOGL::UpdateImpl(const SurfaceDescriptor& aImage,
                                 nsIntRegion* aRegion)
{
  SwapTexturesImpl(aImage, aRegion);
}

void
GrallocTextureHostOGL::SwapTexturesImpl(const SurfaceDescriptor& aImage,
                                        nsIntRegion*)
{
  android::sp<android::GraphicBuffer> buffer = GrallocBufferActor::GetFrom(aImage);
  MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TSurfaceDescriptorGralloc);

  const SurfaceDescriptorGralloc& desc = aImage.get_SurfaceDescriptorGralloc();
  mGraphicBuffer = GrallocBufferActor::GetFrom(desc);
  mFormat = SurfaceFormatForAndroidPixelFormat(mGraphicBuffer->getPixelFormat());

  DeleteTextures();
}

void GrallocTextureHostOGL::BindTexture(GLenum aTextureUnit)
{
  MOZ_ASSERT(mGLTexture);

  mGL->MakeCurrent();
  mGL->fActiveTexture(aTextureUnit);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mGLTexture);
  mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
}

bool
GrallocTextureHostOGL::IsValid() const
{
  return !!mGraphicBuffer.get();
}

GrallocTextureHostOGL::~GrallocTextureHostOGL()
{
  DeleteTextures();
}

bool
GrallocTextureHostOGL::Lock()
{
  /*
   * The job of this function is to ensure that the texture is tied to the
   * android::GraphicBuffer, so that texturing will source the GraphicBuffer.
   *
   * To this effect we create an EGLImage wrapping this GraphicBuffer,
   * using CreateEGLImageForNativeBuffer, and then we tie this EGLImage to our
   * texture using fEGLImageTargetTexture2D.
   *
   * We try to avoid re-creating the EGLImage everytime, by keeping it around
   * as the mEGLImage member of this class.
   */
  MOZ_ASSERT(mGraphicBuffer.get());

  mGL->MakeCurrent();

  if (!mGLTexture) {
    mGL->fGenTextures(1, &mGLTexture);
  }
  mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mGLTexture);
  if (!mEGLImage) {
    mEGLImage = mGL->CreateEGLImageForNativeBuffer(mGraphicBuffer->getNativeBuffer());
  }
  mGL->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, mEGLImage);
  return true;
}

void
GrallocTextureHostOGL::Unlock()
{
  /*
   * The job of this function is to ensure that we release any read lock placed on
   * our android::GraphicBuffer by any drawing code that sourced it via this TextureHost.
   *
   * Indeed, as soon as we draw with a texture that's tied to a android::GraphicBuffer,
   * the GL may place read locks on it. We must ensure that we release them early enough,
   * i.e. before the next time that we will try to acquire a write lock on the same buffer,
   * because read and write locks on gralloc buffers are mutually exclusive.
   *
   * Unfortunately there does not seem to exist an EGL function to dissociate a gralloc
   * buffer from a texture that it was tied to. Failing that, we achieve the same result
   * by uploading a 1x1 dummy texture image to the same texture, replacing the existing
   * gralloc buffer attachment.
   */
  mGL->MakeCurrent();
  mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mGLTexture);
  mGL->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0,
                   LOCAL_GL_RGBA,
                   1, 1, 0,
                   LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE,
                   nullptr);
}

gfx::SurfaceFormat
GrallocTextureHostOGL::GetFormat() const
{
  return mFormat;
}


#endif

} // namespace
} // namespace
