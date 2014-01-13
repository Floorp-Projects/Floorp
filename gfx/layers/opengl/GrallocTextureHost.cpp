/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"
#include "gfxImageSurface.h"
#include "gfx2DGlue.h"
#include <ui/GraphicBuffer.h>
#include "GrallocImages.h"  // for GrallocImage
#include "mozilla/layers/GrallocTextureHost.h"
#include "mozilla/layers/CompositorOGL.h"
#include "EGLImageHelpers.h"
#include "GLReadTexImageHelper.h"

namespace mozilla {
namespace layers {

using namespace android;

static gfx::SurfaceFormat
SurfaceFormatForAndroidPixelFormat(android::PixelFormat aFormat,
                                   bool swapRB = false)
{
  switch (aFormat) {
  case android::PIXEL_FORMAT_BGRA_8888:
    return swapRB ? gfx::FORMAT_R8G8B8A8 : gfx::FORMAT_B8G8R8A8;
  case android::PIXEL_FORMAT_RGBA_8888:
    return swapRB ? gfx::FORMAT_B8G8R8A8 : gfx::FORMAT_R8G8B8A8;
  case android::PIXEL_FORMAT_RGBX_8888:
    return swapRB ? gfx::FORMAT_B8G8R8X8 : gfx::FORMAT_R8G8B8X8;
  case android::PIXEL_FORMAT_RGB_565:
    return gfx::FORMAT_R5G6B5;
  case android::PIXEL_FORMAT_A_8:
    return gfx::FORMAT_A8;
  case HAL_PIXEL_FORMAT_YCbCr_422_SP:
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
  case HAL_PIXEL_FORMAT_YCbCr_422_I:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
  case HAL_PIXEL_FORMAT_YV12:
    return gfx::FORMAT_B8G8R8A8; // yup, use FORMAT_B8G8R8A8 even though it's a YUV texture. This is an external texture.
  default:
    if (aFormat >= 0x100 && aFormat <= 0x1FF) {
      // Reserved range for HAL specific formats.
      return gfx::FORMAT_B8G8R8A8;
    } else {
      // This is not super-unreachable, there's a bunch of hypothetical pixel
      // formats we don't deal with.
      // We only want to abort in debug builds here, since if we crash here
      // we'll take down the compositor process and thus the phone. This seems
      // like undesirable behaviour. We'd rather have a subtle artifact.
      printf_stderr(" xxxxx unknow android format %i\n", (int)aFormat);
      MOZ_ASSERT(false, "Unknown Android pixel format.");
      return gfx::FORMAT_UNKNOWN;
    }
  }
}

static GLenum
TextureTargetForAndroidPixelFormat(android::PixelFormat aFormat)
{
  switch (aFormat) {
  case HAL_PIXEL_FORMAT_YCbCr_422_SP:
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
  case HAL_PIXEL_FORMAT_YCbCr_422_I:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
  case HAL_PIXEL_FORMAT_YV12:
    return LOCAL_GL_TEXTURE_EXTERNAL;
  case android::PIXEL_FORMAT_RGBA_8888:
  case android::PIXEL_FORMAT_RGBX_8888:
  case android::PIXEL_FORMAT_RGB_565:
  case android::PIXEL_FORMAT_A_8:
    return LOCAL_GL_TEXTURE_2D;
  default:
    if (aFormat >= 0x100 && aFormat <= 0x1FF) {
      // Reserved range for HAL specific formats.
      return LOCAL_GL_TEXTURE_EXTERNAL;
    } else {
      // This is not super-unreachable, there's a bunch of hypothetical pixel
      // formats we don't deal with.
      // We only want to abort in debug builds here, since if we crash here
      // we'll take down the compositor process and thus the phone. This seems
      // like undesirable behaviour. We'd rather have a subtle artifact.
      MOZ_ASSERT(false, "Unknown Android pixel format.");
      return LOCAL_GL_TEXTURE_EXTERNAL;
    }
  }
}

GrallocTextureSourceOGL::GrallocTextureSourceOGL(CompositorOGL* aCompositor,
                                                 android::GraphicBuffer* aGraphicBuffer,
                                                 gfx::SurfaceFormat aFormat)
  : mCompositor(aCompositor)
  , mGraphicBuffer(aGraphicBuffer)
  , mEGLImage(0)
  , mFormat(aFormat)
  , mNeedsReset(true)
{
  MOZ_ASSERT(mGraphicBuffer.get());
}

GrallocTextureSourceOGL::~GrallocTextureSourceOGL()
{
  DeallocateDeviceData();
  mCompositor = nullptr;
}

void GrallocTextureSourceOGL::BindTexture(GLenum aTextureUnit)
{
  /*
   * The job of this function is to ensure that the texture is tied to the
   * android::GraphicBuffer, so that texturing will source the GraphicBuffer.
   *
   * To this effect we create an EGLImage wrapping this GraphicBuffer,
   * using EGLImageCreateFromNativeBuffer, and then we tie this EGLImage to our
   * texture using fEGLImageTargetTexture2D.
   */
  MOZ_ASSERT(gl());
  gl()->MakeCurrent();

  GLuint tex = GetGLTexture();
  GLuint textureTarget = GetTextureTarget();

  gl()->fActiveTexture(aTextureUnit);
  gl()->fBindTexture(textureTarget, tex);
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
}

bool
GrallocTextureSourceOGL::IsValid() const
{
  return !!gl() && !!mGraphicBuffer.get();
}

gl::GLContext*
GrallocTextureSourceOGL::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

void
GrallocTextureSourceOGL::SetCompositor(Compositor* aCompositor)
{
  if (mCompositor && !aCompositor) {
    DeallocateDeviceData();
  }
  mCompositor = static_cast<CompositorOGL*>(aCompositor);
}


GLenum
GrallocTextureSourceOGL::GetTextureTarget() const
{
  MOZ_ASSERT(mGraphicBuffer.get());
  return TextureTargetForAndroidPixelFormat(mGraphicBuffer->getPixelFormat());
}

gfx::SurfaceFormat
GrallocTextureSourceOGL::GetFormat() const {
  if (!mGraphicBuffer.get()) {
    return gfx::FORMAT_UNKNOWN;
  }
  if (GetTextureTarget() == LOCAL_GL_TEXTURE_EXTERNAL) {
    return gfx::FORMAT_R8G8B8A8;
  }
  return mFormat;
}

void
GrallocTextureSourceOGL::SetCompositableBackendSpecificData(CompositableBackendSpecificData* aBackendData)
{
  if (mCompositableBackendData != aBackendData) {
    mNeedsReset = true;
  }

  if (!mNeedsReset) {
    return;
  }

  mCompositableBackendData = aBackendData;

  if (!mCompositor) {
    return;
  }

  // delete old EGLImage
  DeallocateDeviceData();

  gl()->MakeCurrent();
  GLuint tex = GetGLTexture();
  GLuint textureTarget = GetTextureTarget();

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(textureTarget, tex);
  // create new EGLImage
  mEGLImage = EGLImageCreateFromNativeBuffer(gl(), mGraphicBuffer->getNativeBuffer());
  gl()->fEGLImageTargetTexture2D(textureTarget, mEGLImage);
  mNeedsReset = false;
}

gfx::IntSize
GrallocTextureSourceOGL::GetSize() const
{
  if (!IsValid()) {
    NS_WARNING("Trying to access the size of an invalid GrallocTextureSourceOGL");
    return gfx::IntSize(0, 0);
  }
  return gfx::IntSize(mGraphicBuffer->getWidth(), mGraphicBuffer->getHeight());
}

void
GrallocTextureSourceOGL::DeallocateDeviceData()
{
  if (mEGLImage) {
    MOZ_ASSERT(gl());
    gl()->MakeCurrent();
    EGLImageDestroy(gl(), mEGLImage);
    mEGLImage = EGL_NO_IMAGE;
  }
}

GrallocTextureHostOGL::GrallocTextureHostOGL(TextureFlags aFlags,
                                             const NewSurfaceDescriptorGralloc& aDescriptor)
  : TextureHost(aFlags)
{
  mGrallocActor =
    static_cast<GrallocBufferActor*>(aDescriptor.bufferParent());

  android::GraphicBuffer* graphicBuffer = mGrallocActor->GetGraphicBuffer();

  mSize = aDescriptor.size();
  gfx::SurfaceFormat format =
    SurfaceFormatForAndroidPixelFormat(graphicBuffer->getPixelFormat(),
                                     aFlags & TEXTURE_RB_SWAPPED);
  mTextureSource = new GrallocTextureSourceOGL(nullptr,
                                               graphicBuffer,
                                               format);
}

GrallocTextureHostOGL::~GrallocTextureHostOGL()
{
  mTextureSource = nullptr;
}

void
GrallocTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  mTextureSource->SetCompositor(static_cast<CompositorOGL*>(aCompositor));
}

bool
GrallocTextureHostOGL::Lock()
{
  return IsValid();
}

void
GrallocTextureHostOGL::Unlock()
{
  // Unlock is done internally by binding the texture to another gralloc buffer
}

bool
GrallocTextureHostOGL::IsValid() const
{
  return mTextureSource->IsValid();
}

gfx::SurfaceFormat
GrallocTextureHostOGL::GetFormat() const
{
  return mTextureSource->GetFormat();
}

void
GrallocTextureHostOGL::DeallocateSharedData()
{
  if (mTextureSource) {
    mTextureSource->ForgetBuffer();
  }
  PGrallocBufferParent::Send__delete__(mGrallocActor);
}

void
GrallocTextureHostOGL::ForgetSharedData()
{
  if (mTextureSource) {
    mTextureSource->ForgetBuffer();
  }
}

void
GrallocTextureHostOGL::DeallocateDeviceData()
{
  mTextureSource->DeallocateDeviceData();
}

LayerRenderState
GrallocTextureHostOGL::GetRenderState()
{
  if (IsValid()) {
    uint32_t flags = 0;
    if (mFlags & TEXTURE_NEEDS_Y_FLIP) {
      flags |= LAYER_RENDER_STATE_Y_FLIPPED;
    }
    if (mFlags & TEXTURE_RB_SWAPPED) {
      flags |= LAYER_RENDER_STATE_FORMAT_RB_SWAP;
    }
    return LayerRenderState(mTextureSource->mGraphicBuffer.get(),
                            gfx::ThebesIntSize(mSize),
                            flags);
  }

  return LayerRenderState();
}

TemporaryRef<gfx::DataSourceSurface>
GrallocTextureHostOGL::GetAsSurface() {
  return mTextureSource ? mTextureSource->GetAsSurface()
                        : nullptr;
}

TemporaryRef<gfx::DataSourceSurface>
GrallocTextureSourceOGL::GetAsSurface() {
  MOZ_ASSERT(gl());
  gl()->MakeCurrent();

  GLuint tex = GetGLTexture();
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(GetTextureTarget(), tex);
  if (!mEGLImage) {
    mEGLImage = EGLImageCreateFromNativeBuffer(gl(), mGraphicBuffer->getNativeBuffer());
  }
  gl()->fEGLImageTargetTexture2D(GetTextureTarget(), mEGLImage);

  RefPtr<gfx::DataSourceSurface> surf =
    IsValid() ? ReadBackSurface(gl(), tex, false, GetFormat())
              : nullptr;

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  return surf.forget();
}

GLuint
GrallocTextureSourceOGL::GetGLTexture()
{
  mCompositableBackendData->SetCompositor(mCompositor);
  return static_cast<CompositableDataGonkOGL*>(mCompositableBackendData.get())->GetTexture();
}

void
GrallocTextureHostOGL::SetCompositableBackendSpecificData(CompositableBackendSpecificData* aBackendData)
{
  mCompositableBackendData = aBackendData;
  if (mTextureSource) {
    mTextureSource->SetCompositableBackendSpecificData(aBackendData);
  }
}

} // namepsace layers
} // namepsace mozilla
