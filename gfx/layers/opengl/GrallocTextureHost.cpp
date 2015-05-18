/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/process.h"
#include "GLContext.h"
#include "gfx2DGlue.h"
#include <ui/GraphicBuffer.h>
#include "GrallocImages.h"  // for GrallocImage
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/layers/GrallocTextureHost.h"
#include "mozilla/layers/SharedBufferManagerParent.h"
#include "EGLImageHelpers.h"
#include "GLReadTexImageHelper.h"

namespace mozilla {
namespace layers {

using namespace android;

static gfx::SurfaceFormat
SurfaceFormatForAndroidPixelFormat(android::PixelFormat aFormat,
                                   TextureFlags aFlags)
{
  bool swapRB = bool(aFlags & TextureFlags::RB_SWAPPED);
  switch (aFormat) {
  case android::PIXEL_FORMAT_BGRA_8888:
    return swapRB ? gfx::SurfaceFormat::R8G8B8A8 : gfx::SurfaceFormat::B8G8R8A8;
  case android::PIXEL_FORMAT_RGBA_8888:
    return swapRB ? gfx::SurfaceFormat::B8G8R8A8 : gfx::SurfaceFormat::R8G8B8A8;
  case android::PIXEL_FORMAT_RGBX_8888:
    return swapRB ? gfx::SurfaceFormat::B8G8R8X8 : gfx::SurfaceFormat::R8G8B8X8;
  case android::PIXEL_FORMAT_RGB_565:
    return gfx::SurfaceFormat::R5G6B5;
  case HAL_PIXEL_FORMAT_YCbCr_422_SP:
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
  case HAL_PIXEL_FORMAT_YCbCr_422_I:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
  case HAL_PIXEL_FORMAT_YV12:
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
#endif
    return gfx::SurfaceFormat::R8G8B8A8; // yup, use SurfaceFormat::R8G8B8A8 even though it's a YUV texture. This is an external texture.
  default:
    if (aFormat >= 0x100 && aFormat <= 0x1FF) {
      // Reserved range for HAL specific formats.
      return gfx::SurfaceFormat::R8G8B8A8;
    } else {
      // This is not super-unreachable, there's a bunch of hypothetical pixel
      // formats we don't deal with.
      // We only want to abort in debug builds here, since if we crash here
      // we'll take down the compositor process and thus the phone. This seems
      // like undesirable behaviour. We'd rather have a subtle artifact.
      printf_stderr(" xxxxx unknow android format %i\n", (int)aFormat);
      MOZ_ASSERT(false, "Unknown Android pixel format.");
      return gfx::SurfaceFormat::UNKNOWN;
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
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
#endif
    return LOCAL_GL_TEXTURE_EXTERNAL;
  case android::PIXEL_FORMAT_BGRA_8888:
  case android::PIXEL_FORMAT_RGBA_8888:
  case android::PIXEL_FORMAT_RGBX_8888:
  case android::PIXEL_FORMAT_RGB_565:
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

GrallocTextureHostOGL::GrallocTextureHostOGL(TextureFlags aFlags,
                                             const NewSurfaceDescriptorGralloc& aDescriptor)
  : TextureHost(aFlags)
  , mGrallocHandle(aDescriptor)
  , mSize(0, 0)
  , mDescriptorSize(aDescriptor.size())
  , mFormat(gfx::SurfaceFormat::UNKNOWN)
  , mEGLImage(EGL_NO_IMAGE)
  , mIsOpaque(aDescriptor.isOpaque())
{
  android::GraphicBuffer* graphicBuffer = GetGraphicBufferFromDesc(mGrallocHandle).get();
  MOZ_ASSERT(graphicBuffer);

  if (graphicBuffer) {
    mFormat =
      SurfaceFormatForAndroidPixelFormat(graphicBuffer->getPixelFormat(),
                                         aFlags & TextureFlags::RB_SWAPPED);
    mSize = gfx::IntSize(graphicBuffer->getWidth(), graphicBuffer->getHeight());
  } else {
    printf_stderr("gralloc buffer is nullptr");
  }
}

GrallocTextureHostOGL::~GrallocTextureHostOGL()
{
  DestroyEGLImage();
}

void
GrallocTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  MOZ_ASSERT(aCompositor);
  mCompositor = static_cast<CompositorOGL*>(aCompositor);
  if (mGLTextureSource) {
    mGLTextureSource->SetCompositor(mCompositor);
  }

  if (mCompositor && aCompositor != mCompositor) {
    DestroyEGLImage();
  }
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
  android::GraphicBuffer* graphicBuffer = GetGraphicBufferFromDesc(mGrallocHandle).get();
  return graphicBuffer != nullptr;
}

gfx::SurfaceFormat
GrallocTextureHostOGL::GetFormat() const
{
  return mFormat;
}

void
GrallocTextureHostOGL::DeallocateSharedData()
{
  if (mGLTextureSource) {
    mGLTextureSource = nullptr;
  }

  DestroyEGLImage();

  if (mGrallocHandle.buffer().type() != MaybeMagicGrallocBufferHandle::Tnull_t) {
    MaybeMagicGrallocBufferHandle handle = mGrallocHandle.buffer();
    base::ProcessId owner;
    if (handle.type() == MaybeMagicGrallocBufferHandle::TGrallocBufferRef) {
      owner = handle.get_GrallocBufferRef().mOwner;
    }
    else {
      owner = handle.get_MagicGrallocBufferHandle().mRef.mOwner;
    }

    SharedBufferManagerParent::DropGrallocBuffer(owner, mGrallocHandle);
  }
}

void
GrallocTextureHostOGL::ForgetSharedData()
{
  if (mGLTextureSource) {
    mGLTextureSource = nullptr;
  }
}

void
GrallocTextureHostOGL::DeallocateDeviceData()
{
  if (mGLTextureSource) {
    mGLTextureSource = nullptr;
  }
  DestroyEGLImage();
}

LayerRenderState
GrallocTextureHostOGL::GetRenderState()
{
  android::GraphicBuffer* graphicBuffer = GetGraphicBufferFromDesc(mGrallocHandle).get();

  if (graphicBuffer) {
    LayerRenderStateFlags flags = LayerRenderStateFlags::LAYER_RENDER_STATE_DEFAULT;
    if (mIsOpaque) {
      flags |= LayerRenderStateFlags::OPAQUE;
    }
    if (mFlags & TextureFlags::ORIGIN_BOTTOM_LEFT) {
      flags |= LayerRenderStateFlags::ORIGIN_BOTTOM_LEFT;
    }
    if (mFlags & TextureFlags::RB_SWAPPED) {
      flags |= LayerRenderStateFlags::FORMAT_RB_SWAP;
    }
    return LayerRenderState(graphicBuffer,
                            mDescriptorSize,
                            flags,
                            this);
  }

  return LayerRenderState();
}

TemporaryRef<gfx::DataSourceSurface>
GrallocTextureHostOGL::GetAsSurface() {
  android::GraphicBuffer* graphicBuffer = GetGraphicBufferFromDesc(mGrallocHandle).get();
  if (!graphicBuffer) {
    return nullptr;
  }
  uint8_t* grallocData;
  graphicBuffer->lock(GRALLOC_USAGE_SW_READ_OFTEN, reinterpret_cast<void**>(&grallocData));
  RefPtr<gfx::DataSourceSurface> grallocTempSurf =
    gfx::Factory::CreateWrappingDataSourceSurface(grallocData,
                                                  graphicBuffer->getStride() * android::bytesPerPixel(graphicBuffer->getPixelFormat()),
                                                  GetSize(), GetFormat());
  RefPtr<gfx::DataSourceSurface> surf = CreateDataSourceSurfaceByCloning(grallocTempSurf);

  graphicBuffer->unlock();

  return surf.forget();
}

void
GrallocTextureHostOGL::UnbindTextureSource()
{
  // Clear the reference to the TextureSource (if any), because we know that
  // another TextureHost is being bound to the TextureSource. This means that
  // we will have to re-do gl->fEGLImageTargetTexture2D next time we go through
  // BindTextureSource (otherwise we would have skipped it).
  // Note that this doesn't "unlock" the gralloc buffer or force it to be
  // detached, Although decreasing the refcount of the TextureSource may lead
  // to the gl handle being destroyed, which would unlock the gralloc buffer.
  // That said, this method is called before another TextureHost attaches to the
  // TextureSource, which has the effect of unlocking the gralloc buffer. So when
  // this is called we know we are going to be unlocked soon.
  mGLTextureSource = nullptr;
}

FenceHandle
GrallocTextureHostOGL::GetAndResetReleaseFenceHandle()
{
  return GetAndResetReleaseFence();
}

GLenum GetTextureTarget(gl::GLContext* aGL, android::PixelFormat aFormat) {
  MOZ_ASSERT(aGL);
  if (aGL->Renderer() == gl::GLRenderer::SGX530 ||
      aGL->Renderer() == gl::GLRenderer::SGX540) {
    // SGX has a quirk that only TEXTURE_EXTERNAL works and any other value will
    // result in black pixels when trying to draw from bound textures.
    // Unfortunately, using TEXTURE_EXTERNAL on Adreno has a terrible effect on
    // performance.
    // See Bug 950050.
    return LOCAL_GL_TEXTURE_EXTERNAL;
  } else {
    return TextureTargetForAndroidPixelFormat(aFormat);
  }
}

void
GrallocTextureHostOGL::DestroyEGLImage()
{
  // Only called when we want to get rid of the gralloc buffer, usually
  // around the end of life of the TextureHost.
  if (mEGLImage != EGL_NO_IMAGE && GetGLContext()) {
    EGLImageDestroy(GetGLContext(), mEGLImage);
    mEGLImage = EGL_NO_IMAGE;
  }
}

void
GrallocTextureHostOGL::PrepareTextureSource(CompositableTextureSourceRef& aTextureSource)
{
  // This happens during the layers transaction.
  // All of the gralloc magic goes here. The only thing that happens externally
  // and that is good to keep in mind is that when the TextureSource is deleted,
  // it destroys its gl texture handle which is important for genlock.

  // If this TextureHost's mGLTextureSource member is non-null, it means we are
  // still bound to the TextureSource, in which case we can skip the driver
  // overhead of binding the texture again (fEGLImageTargetTexture2D)
  // As a result, if the TextureHost is used with several CompositableHosts,
  // it will be bound to only one TextureSource, and we'll do the driver work
  // only once, which is great. This means that all of the compositables that
  // use this TextureHost will keep a reference to this TextureSource at least
  // for the duration of this frame.

  // If the compositable already has a TextureSource (the aTextureSource parameter),
  // that is compatible and is not in use by several compositable, we try to
  // attach to it. This has the effect of unlocking the previous TextureHost that
  // we attached to the TextureSource (the previous frame)

  // If the TextureSource used by the compositable is also used by other
  // compositables (see NumCompositableRefs), we have to create a new TextureSource,
  // because otherwise we would be modifying the content of every layer that uses
  // the TextureSource in question, even thoug they don't use this TextureHost.

  android::GraphicBuffer* graphicBuffer = GetGraphicBufferFromDesc(mGrallocHandle).get();

  MOZ_ASSERT(graphicBuffer);
  if (!graphicBuffer) {
    mGLTextureSource = nullptr;
    return;
  }

  if (mGLTextureSource && !mGLTextureSource->IsValid()) {
    mGLTextureSource = nullptr;
  }

  if (mGLTextureSource) {
    // We are already attached to a TextureSource, nothing to do except tell
    // the compositable to use it.
    aTextureSource = mGLTextureSource.get();
    return;
  }

  gl::GLContext* gl = GetGLContext();
  if (!gl || !gl->MakeCurrent()) {
    mGLTextureSource = nullptr;
    return;
  }

  if (mEGLImage == EGL_NO_IMAGE) {
    // Should only happen the first time.
    mEGLImage = EGLImageCreateFromNativeBuffer(gl, graphicBuffer->getNativeBuffer());
  }

  GLenum textureTarget = GetTextureTarget(gl, graphicBuffer->getPixelFormat());

  GLTextureSource* glSource = aTextureSource.get() ?
    aTextureSource->AsSourceOGL()->AsGLTextureSource() : nullptr;

  bool shouldCreateTextureSource = !glSource  || !glSource->IsValid()
                                 || glSource->NumCompositableRefs() > 1
                                 || glSource->GetTextureTarget() != textureTarget;

  if (shouldCreateTextureSource) {
    GLuint textureHandle;
    gl->fGenTextures(1, &textureHandle);
    gl->fBindTexture(textureTarget, textureHandle);
    gl->fTexParameteri(textureTarget, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
    gl->fTexParameteri(textureTarget, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    gl->fEGLImageTargetTexture2D(textureTarget, mEGLImage);

    mGLTextureSource = new GLTextureSource(mCompositor, textureHandle, textureTarget,
                                           mSize, mFormat);
    aTextureSource = mGLTextureSource.get();
  } else {
    gl->fBindTexture(textureTarget, glSource->GetTextureHandle());

    gl->fEGLImageTargetTexture2D(textureTarget, mEGLImage);
    glSource->SetSize(mSize);
    glSource->SetFormat(mFormat);
    mGLTextureSource = glSource;
  }
}

bool
GrallocTextureHostOGL::BindTextureSource(CompositableTextureSourceRef& aTextureSource)
{
  // This happens at composition time.

  // If mGLTextureSource is null it means PrepareTextureSource failed.
  if (!mGLTextureSource) {
    return false;
  }

  // If Prepare didn't fail, we expect our TextureSource to be the same as aTextureSource,
  // otherwise it means something has fiddled with the TextureSource between Prepare and
  // now.
  MOZ_ASSERT(mGLTextureSource == aTextureSource);
  aTextureSource = mGLTextureSource.get();

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  // Wait until it's ready.
  WaitAcquireFenceSyncComplete();
#endif
  return true;
}

} // namepsace layers
} // namepsace mozilla
