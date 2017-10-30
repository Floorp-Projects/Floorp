/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderD3D11TextureHostOGL.h"

#include <d3d11.h>

#include "GLLibraryEGL.h"
#include "GLContextEGL.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/Logging.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace wr {

static EGLint
GetEGLTextureFormat(gfx::SurfaceFormat aFormat)
{
  switch (aFormat) {
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8:
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::A8R8G8B8:
    case gfx::SurfaceFormat::X8R8G8B8:
      return LOCAL_EGL_TEXTURE_RGBA;
    case gfx::SurfaceFormat::R8G8B8:
    case gfx::SurfaceFormat::B8G8R8:
      return LOCAL_EGL_TEXTURE_RGB;
    default:
      gfxCriticalError() << "GetEGLTextureFormat(): unexpected texture format";
      return LOCAL_EGL_TEXTURE_RGBA;
  }
}

RenderDXGITextureHostOGL::RenderDXGITextureHostOGL(WindowsHandle aHandle,
                                                   gfx::SurfaceFormat aFormat,
                                                   gfx::IntSize aSize)
  : mHandle(aHandle)
  , mSurface(0)
  , mStream(0)
  , mTextureHandle{0}
  , mFormat(aFormat)
  , mSize(aSize)
  , mLocked(false)
{
  MOZ_COUNT_CTOR_INHERITED(RenderDXGITextureHostOGL, RenderTextureHostOGL);
  MOZ_ASSERT(mFormat != gfx::SurfaceFormat::NV12 ||
             (mSize.width % 2 == 0 && mSize.height % 2 == 0));
  MOZ_ASSERT(aHandle);
}

RenderDXGITextureHostOGL::~RenderDXGITextureHostOGL()
{
  MOZ_COUNT_DTOR_INHERITED(RenderDXGITextureHostOGL, RenderTextureHostOGL);
  DeleteTextureHandle();
}

void
RenderDXGITextureHostOGL::SetGLContext(gl::GLContext* aContext)
{
  if (mGL.get() != aContext) {
    // Release the texture handle in the previous gl context.
    DeleteTextureHandle();
    mGL = aContext;
    mGL->MakeCurrent();
  }
}

bool
RenderDXGITextureHostOGL::EnsureLockable()
{
  if (mTextureHandle[0]) {
    return true;
  }

  const auto& egl = &gl::sEGLLibrary;

  if (mFormat != gfx::SurfaceFormat::NV12) {
    // The non-nv12 format.
    // Use eglCreatePbufferFromClientBuffer get the gl handle from the d3d
    // shared handle.
    if (!egl->IsExtensionSupported(gl::GLLibraryEGL::ANGLE_d3d_share_handle_client_buffer)) {
      return false;
    }

    // Get gl texture handle from shared handle.
    EGLint pbufferAttributes[] = {
        LOCAL_EGL_WIDTH, mSize.width,
        LOCAL_EGL_HEIGHT, mSize.height,
        LOCAL_EGL_TEXTURE_TARGET, LOCAL_EGL_TEXTURE_2D,
        LOCAL_EGL_TEXTURE_FORMAT, GetEGLTextureFormat(mFormat),
        LOCAL_EGL_MIPMAP_TEXTURE, LOCAL_EGL_FALSE,
        LOCAL_EGL_NONE
    };
    mSurface = egl->fCreatePbufferFromClientBuffer(egl->Display(),
                                                   LOCAL_EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
                                                   reinterpret_cast<EGLClientBuffer>(mHandle),
                                                   gl::GLContextEGL::Cast(mGL.get())->mConfig,
                                                   pbufferAttributes);
    MOZ_ASSERT(mSurface);

    // Query the keyed-mutex.
    egl->fQuerySurfacePointerANGLE(egl->Display(),
                                   mSurface,
                                   LOCAL_EGL_DXGI_KEYED_MUTEX_ANGLE,
                                   (void**)getter_AddRefs(mKeyedMutex));

    mGL->fGenTextures(1, &mTextureHandle[0]);
    mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
    mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextureHandle[0]);
    mGL->TexParams_SetClampNoMips(LOCAL_GL_TEXTURE_2D);
    egl->fBindTexImage(egl->Display(), mSurface, LOCAL_EGL_BACK_BUFFER);
  } else {
    // The nv12 format.
    // The eglCreatePbufferFromClientBuffer doesn't support nv12 format, so we
    // use EGLStream to get the converted gl handle from d3d nv12 texture.

    if (!egl->IsExtensionSupported(gl::GLLibraryEGL::NV_stream_consumer_gltexture_yuv) ||
        !egl->IsExtensionSupported(gl::GLLibraryEGL::ANGLE_stream_producer_d3d_texture_nv12)) {
      return false;
    }

    // Fetch the D3D11 device.
    EGLDeviceEXT eglDevice = nullptr;
    egl->fQueryDisplayAttribEXT(egl->Display(), LOCAL_EGL_DEVICE_EXT, (EGLAttrib*)&eglDevice);
    MOZ_ASSERT(eglDevice);
    ID3D11Device* device = nullptr;
    egl->fQueryDeviceAttribEXT(eglDevice, LOCAL_EGL_D3D11_DEVICE_ANGLE, (EGLAttrib*)&device);
    // There's a chance this might fail if we end up on d3d9 angle for some reason.
    if (!device) {
      return false;
    }

    // Get the NV12 D3D11 texture from shared handle.
    if (FAILED(device->OpenSharedResource((HANDLE)mHandle,
                                          __uuidof(ID3D11Texture2D),
                                          (void**)(ID3D11Texture2D**)getter_AddRefs(mTexture)))) {
      NS_WARNING("RenderDXGITextureHostOGL::Lock(): Failed to open shared texture");
      return false;
    }

    mTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mKeyedMutex));

    // Create the EGLStream.
    mStream = egl->fCreateStreamKHR(egl->Display(), nullptr);
    MOZ_ASSERT(mStream);

    // Setup the NV12 stream consumer/producer.
    EGLAttrib consumerAttributes[] = {
        LOCAL_EGL_COLOR_BUFFER_TYPE,
        LOCAL_EGL_YUV_BUFFER_EXT,
        LOCAL_EGL_YUV_NUMBER_OF_PLANES_EXT,
        2,
        LOCAL_EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
        0,
        LOCAL_EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
        1,
        LOCAL_EGL_NONE,
    };
    mGL->fGenTextures(2, mTextureHandle);
    mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
    mGL->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL_OES, mTextureHandle[0]);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_EXTERNAL_OES, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
    mGL->fActiveTexture(LOCAL_GL_TEXTURE1);
    mGL->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL_OES, mTextureHandle[1]);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_EXTERNAL_OES, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
    MOZ_ALWAYS_TRUE(egl->fStreamConsumerGLTextureExternalAttribsNV(egl->Display(), mStream, consumerAttributes));
    MOZ_ALWAYS_TRUE(egl->fCreateStreamProducerD3DTextureNV12ANGLE(egl->Display(), mStream, nullptr));

    // Insert the NV12 texture.
    MOZ_ALWAYS_TRUE(egl->fStreamPostD3DTextureNV12ANGLE(egl->Display(), mStream, (void*)mTexture.get(), nullptr));

    // Now, we could get the NV12 gl handle from the stream.
    egl->fStreamConsumerAcquireKHR(egl->Display(), mStream);
    MOZ_ASSERT(egl->fGetError() == LOCAL_EGL_SUCCESS);
  }

  return true;
}

bool
RenderDXGITextureHostOGL::Lock()
{
  if (!EnsureLockable()) {
    return false;
  }

  if (mLocked) {
    return true;
  }

  if (mKeyedMutex) {
    HRESULT hr = mKeyedMutex->AcquireSync(0, 100);
    if (hr != S_OK) {
      gfxCriticalError() << "RenderDXGITextureHostOGL AcquireSync timeout, hr=" << gfx::hexa(hr);
      return false;
    }
  }
  mLocked = true;

  return true;
}

void
RenderDXGITextureHostOGL::Unlock()
{
  if (mLocked) {
    if (mKeyedMutex) {
      mKeyedMutex->ReleaseSync(0);
    }
    mLocked = false;
  }
}

void
RenderDXGITextureHostOGL::DeleteTextureHandle()
{
  if (mTextureHandle[0] == 0) {
    return;
  }

  if (mGL && mGL->MakeCurrent()) {
    mGL->fDeleteTextures(2, mTextureHandle);
  }
  for(int i = 0; i < 2; ++i) {
    mTextureHandle[i] = 0;
  }

  const auto& egl = &gl::sEGLLibrary;
  if (mSurface) {
    egl->fDestroySurface(egl->Display(), mSurface);
    mSurface = 0;
  }
  if (mStream) {
    egl->fStreamConsumerReleaseKHR(egl->Display(), mStream);
    mStream = 0;
  }

  mTexture = nullptr;
  mKeyedMutex = nullptr;
}

GLuint
RenderDXGITextureHostOGL::GetGLHandle(uint8_t aChannelIndex) const
{
  MOZ_ASSERT(mFormat != gfx::SurfaceFormat::NV12 || aChannelIndex < 2);
  MOZ_ASSERT(mFormat == gfx::SurfaceFormat::NV12 || aChannelIndex < 1);

  return mTextureHandle[aChannelIndex];
}

gfx::IntSize
RenderDXGITextureHostOGL::GetSize(uint8_t aChannelIndex) const
{
  MOZ_ASSERT(mFormat != gfx::SurfaceFormat::NV12 || aChannelIndex < 2);
  MOZ_ASSERT(mFormat == gfx::SurfaceFormat::NV12 || aChannelIndex < 1);

  if (aChannelIndex == 0) {
    return mSize;
  } else {
    // The CbCr channel size is a half of Y channel size in NV12 format.
    return mSize / 2;
  }
}

RenderDXGIYCbCrTextureHostOGL::RenderDXGIYCbCrTextureHostOGL(WindowsHandle (&aHandles)[3],
                                                             gfx::IntSize aSize)
  : mHandles{ aHandles[0], aHandles[1], aHandles[2] }
  , mSurfaces{0}
  , mStreams{0}
  , mTextureHandles{0}
  , mSize(aSize)
  , mLocked(false)
{
    MOZ_COUNT_CTOR_INHERITED(RenderDXGIYCbCrTextureHostOGL, RenderTextureHostOGL);
    // The size should be even.
    MOZ_ASSERT(mSize.width % 2 == 0);
    MOZ_ASSERT(mSize.height % 2 == 0);
    MOZ_ASSERT(aHandles[0] && aHandles[1] && aHandles[2]);
}

RenderDXGIYCbCrTextureHostOGL::~RenderDXGIYCbCrTextureHostOGL()
{
  MOZ_COUNT_CTOR_INHERITED(RenderDXGIYCbCrTextureHostOGL, RenderTextureHostOGL);
  DeleteTextureHandle();
}

void
RenderDXGIYCbCrTextureHostOGL::SetGLContext(gl::GLContext* aContext)
{
  if (mGL.get() != aContext) {
    // Release the texture handle in the previous gl context.
    DeleteTextureHandle();
    mGL = aContext;
    mGL->MakeCurrent();
  }
}

bool
RenderDXGIYCbCrTextureHostOGL::EnsureLockable()
{
  if (mTextureHandles[0]) {
    return true;
  }

  const auto& egl = &gl::sEGLLibrary;

  // The eglCreatePbufferFromClientBuffer doesn't support R8 format, so we
  // use EGLStream to get the converted gl handle from d3d R8 texture.

  if (!egl->IsExtensionSupported(gl::GLLibraryEGL::NV_stream_consumer_gltexture_yuv) ||
      !egl->IsExtensionSupported(gl::GLLibraryEGL::ANGLE_stream_producer_d3d_texture_nv12))
  {
      return false;
  }

  // Fetch the D3D11 device.
  EGLDeviceEXT eglDevice = nullptr;
  egl->fQueryDisplayAttribEXT(egl->Display(), LOCAL_EGL_DEVICE_EXT, (EGLAttrib*)&eglDevice);
  MOZ_ASSERT(eglDevice);
  ID3D11Device* device = nullptr;
  egl->fQueryDeviceAttribEXT(eglDevice, LOCAL_EGL_D3D11_DEVICE_ANGLE, (EGLAttrib*)&device);
  // There's a chance this might fail if we end up on d3d9 angle for some reason.
  if (!device) {
    return false;
  }

  for (int i = 0; i < 3; ++i) {
    // Get the R8 D3D11 texture from shared handle.
    if (FAILED(device->OpenSharedResource((HANDLE)mHandles[i],
                                          __uuidof(ID3D11Texture2D),
                                          (void**)(ID3D11Texture2D**)getter_AddRefs(mTextures[i])))) {
      NS_WARNING("RenderDXGITextureHostOGL::Lock(): Failed to open shared texture");
      return false;
    }
  }

  for (int i = 0; i < 3; ++i) {
    mTextures[i]->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mKeyedMutexs[i]));
  }

  mGL->fGenTextures(3, mTextureHandles);
  for (int i = 0; i < 3; ++i) {
    mGL->fActiveTexture(LOCAL_GL_TEXTURE0 + i);
    mGL->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL_OES, mTextureHandles[i]);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_EXTERNAL_OES, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);

    // Create the EGLStream.
    mStreams[i] = egl->fCreateStreamKHR(egl->Display(), nullptr);
    MOZ_ASSERT(mStreams[i]);

    MOZ_ALWAYS_TRUE(egl->fStreamConsumerGLTextureExternalAttribsNV(egl->Display(), mStreams[i], nullptr));
    MOZ_ALWAYS_TRUE(egl->fCreateStreamProducerD3DTextureNV12ANGLE(egl->Display(), mStreams[i], nullptr));

    // Insert the R8 texture.
    MOZ_ALWAYS_TRUE(egl->fStreamPostD3DTextureNV12ANGLE(egl->Display(), mStreams[i], (void*)mTextures[i].get(), nullptr));

    // Now, we could get the R8 gl handle from the stream.
    egl->fStreamConsumerAcquireKHR(egl->Display(), mStreams[i]);
    MOZ_ASSERT(egl->fGetError() == LOCAL_EGL_SUCCESS);
  }

  return true;
}

bool
RenderDXGIYCbCrTextureHostOGL::Lock()
{
  if (!EnsureLockable()) {
    return false;
  }

  if (mLocked) {
    return true;
  }

  if (mKeyedMutexs[0]) {
    for (const auto& mutex : mKeyedMutexs) {
      HRESULT hr = mutex->AcquireSync(0, 100);
      if (hr != S_OK) {
        gfxCriticalError() << "RenderDXGIYCbCrTextureHostOGL AcquireSync timeout, hr=" << gfx::hexa(hr);
        return false;
      }
    }
  }
  mLocked = true;

  return true;
}

void
RenderDXGIYCbCrTextureHostOGL::Unlock()
{
  if (mLocked) {
    if (mKeyedMutexs[0]) {
      for (const auto& mutex : mKeyedMutexs) {
        mutex->ReleaseSync(0);
      }
    }
    mLocked = false;
  }
}

GLuint
RenderDXGIYCbCrTextureHostOGL::GetGLHandle(uint8_t aChannelIndex) const
{
  MOZ_ASSERT(aChannelIndex < 3);

  return mTextureHandles[aChannelIndex];
}

gfx::IntSize
RenderDXGIYCbCrTextureHostOGL::GetSize(uint8_t aChannelIndex) const
{
  MOZ_ASSERT(aChannelIndex < 3);

  if (aChannelIndex == 0) {
    return mSize;
  } else {
    // The CbCr channel size is a half of Y channel size.
    return mSize / 2;
  }
}

void
RenderDXGIYCbCrTextureHostOGL::DeleteTextureHandle()
{
  if (mTextureHandles[0] == 0) {
    return;
  }

  if (mGL && mGL->MakeCurrent()) {
    mGL->fDeleteTextures(3, mTextureHandles);
  }
  for (int i = 0; i < 3; ++i) {
    mTextureHandles[i] = 0;
    mTextures[i] = nullptr;
    mKeyedMutexs[i] = nullptr;

    const auto& egl = &gl::sEGLLibrary;
    if (mSurfaces[i]) {
      egl->fDestroySurface(egl->Display(), mSurfaces[i]);
      mSurfaces[i] = 0;
    }
    if (mStreams[i]) {
      egl->fStreamConsumerReleaseKHR(egl->Display(), mStreams[i]);
      mStreams[i] = 0;
    }
  }
}

} // namespace wr
} // namespace mozilla
