/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/process.h"
#include "GLContext.h"
#include "gfx2DGlue.h"
#include <ui/GraphicBuffer.h>
#include "GrallocImages.h"  // for GrallocImage
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

GrallocTextureSourceOGL::GrallocTextureSourceOGL(CompositorOGL* aCompositor,
                                                 GrallocTextureHostOGL* aTextureHost,
                                                 android::GraphicBuffer* aGraphicBuffer,
                                                 gfx::SurfaceFormat aFormat)
  : mCompositor(aCompositor)
  , mTextureHost(aTextureHost)
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

void
GrallocTextureSourceOGL::BindTexture(GLenum aTextureUnit, gfx::Filter aFilter)
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
  if (!IsValid() || !gl()->MakeCurrent()) {
    return;
  }

  GLuint tex = GetGLTexture();
  GLuint textureTarget = GetTextureTarget();

  gl()->fActiveTexture(aTextureUnit);
  gl()->fBindTexture(textureTarget, tex);

  ApplyFilterToBoundTexture(gl(), aFilter, textureTarget);

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  if (mTextureHost) {
    // Wait until it's ready.
    mTextureHost->WaitAcquireFenceSyncComplete();
  }
#endif
}

bool GrallocTextureSourceOGL::Lock()
{
  MOZ_ASSERT(IsValid());
  if (!IsValid()) {
    return false;
  }
  if (!gl()->MakeCurrent()) {
    NS_WARNING("Failed to make the gl context current");
    return false;
  }

  mTexture = mCompositor->GetTemporaryTexture(GetTextureTarget(), LOCAL_GL_TEXTURE0);

  GLuint textureTarget = GetTextureTarget();

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(textureTarget, mTexture);
  if (!mEGLImage) {
    mEGLImage = EGLImageCreateFromNativeBuffer(gl(), mGraphicBuffer->getNativeBuffer());
  }
  gl()->fEGLImageTargetTexture2D(textureTarget, mEGLImage);
  return true;
}

bool
GrallocTextureSourceOGL::IsValid() const
{
  return !!gl() && !!mGraphicBuffer.get() && !!mCompositor;
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
  MOZ_ASSERT(gl());
  MOZ_ASSERT(mGraphicBuffer.get());

  if (!gl() || !mGraphicBuffer.get()) {
    return LOCAL_GL_TEXTURE_EXTERNAL;
  }

  // SGX has a quirk that only TEXTURE_EXTERNAL works and any other value will
  // result in black pixels when trying to draw from bound textures.
  // Unfortunately, using TEXTURE_EXTERNAL on Adreno has a terrible effect on
  // performance.
  // See Bug 950050.
  if (gl()->Renderer() == gl::GLRenderer::SGX530 ||
      gl()->Renderer() == gl::GLRenderer::SGX540) {
    return LOCAL_GL_TEXTURE_EXTERNAL;
  }

  return TextureTargetForAndroidPixelFormat(mGraphicBuffer->getPixelFormat());
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
    MOZ_ASSERT(mCompositor);
    if (!gl() || !gl()->MakeCurrent()) {
      return;
    }
    EGLImageDestroy(gl(), mEGLImage);
    mEGLImage = EGL_NO_IMAGE;
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
{}

void
GrallocTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  mCompositor = static_cast<CompositorOGL*>(aCompositor);
  if (mTilingTextureSource) {
    mTilingTextureSource->SetCompositor(mCompositor);
  }
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
  if (mTilingTextureSource) {
    mTilingTextureSource->ForgetBuffer();
    mTilingTextureSource = nullptr;
  }
  if (mGLTextureSource) {
    mGLTextureSource = nullptr;
  }

  DestroyEGLImage();

  if (mGrallocHandle.buffer().type() != SurfaceDescriptor::Tnull_t) {
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
  if (mTilingTextureSource) {
    mTilingTextureSource->ForgetBuffer();
    mTilingTextureSource = nullptr;
  }
  if (mGLTextureSource) {
    mGLTextureSource = nullptr;
  }
}

void
GrallocTextureHostOGL::DeallocateDeviceData()
{
  if (mTilingTextureSource) {
    mTilingTextureSource->DeallocateDeviceData();
  }
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
    if (mFlags & TextureFlags::NEEDS_Y_FLIP) {
      flags |= LayerRenderStateFlags::Y_FLIPPED;
    }
    if (mFlags & TextureFlags::RB_SWAPPED) {
      flags |= LayerRenderStateFlags::FORMAT_RB_SWAP;
    }
    return LayerRenderState(graphicBuffer,
                            gfx::ThebesIntSize(mDescriptorSize),
                            flags,
                            this);
  }

  return LayerRenderState();
}

TemporaryRef<gfx::DataSourceSurface>
GrallocTextureHostOGL::GetAsSurface() {
  return mTilingTextureSource ? mTilingTextureSource->GetAsSurface()
                              : nullptr;
}

TemporaryRef<gfx::DataSourceSurface>
GrallocTextureSourceOGL::GetAsSurface() {
  if (!IsValid() || !gl()->MakeCurrent()) {
    return nullptr;
  }

  GLuint tex = GetGLTexture();
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(GetTextureTarget(), tex);
  if (!mEGLImage) {
    mEGLImage = EGLImageCreateFromNativeBuffer(gl(), mGraphicBuffer->getNativeBuffer());
  }
  BindEGLImage();

  RefPtr<gfx::DataSourceSurface> surf =
    IsValid() ? ReadBackSurface(gl(), tex, false, GetFormat())
              : nullptr;

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  return surf.forget();
}

GLuint
GrallocTextureSourceOGL::GetGLTexture()
{
  return mTexture;
}

void
GrallocTextureSourceOGL::BindEGLImage()
{
  gl()->fEGLImageTargetTexture2D(GetTextureTarget(), mEGLImage);
}

TextureSource*
GrallocTextureHostOGL::GetTextureSources()
{
  // This is now only used with tiled layers, and will eventually be removed.
  // Other layer types use BindTextureSource instead.
  MOZ_ASSERT(!mGLTextureSource);
  if (!mTilingTextureSource) {
    android::GraphicBuffer* graphicBuffer = GetGraphicBufferFromDesc(mGrallocHandle).get();
    MOZ_ASSERT(graphicBuffer);
    if (!graphicBuffer) {
      return nullptr;
    }
    mTilingTextureSource = new GrallocTextureSourceOGL(mCompositor, this,
                                                 graphicBuffer, mFormat);
  }
  mTilingTextureSource->Lock();
  return mTilingTextureSource;
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

  MOZ_ASSERT(!mTilingTextureSource);

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
