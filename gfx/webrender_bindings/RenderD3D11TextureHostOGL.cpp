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
  , mTextureHandle{0}
  , mSurface(0)
  , mStream(0)
  , mFormat(aFormat)
  , mSize(aSize)
{
  MOZ_COUNT_CTOR_INHERITED(RenderDXGITextureHostOGL, RenderTextureHostOGL);
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
    MOZ_ASSERT(egl->fGetError() == LOCAL_EGL_SUCCESS);
    MOZ_ASSERT(mStream);

    DebugOnly<EGLBoolean> eglResult;

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

    // Now, we could check the stream status and use the NV12 gl handle.
    EGLint state;
    egl->fQueryStreamKHR(egl->Display(), mStream, LOCAL_EGL_STREAM_STATE_KHR, &state);
    MOZ_ASSERT(state == LOCAL_EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR);
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

  if (mKeyedMutex) {
    HRESULT hr = mKeyedMutex->AcquireSync(0, 100);
    if (hr != S_OK) {
      gfxCriticalError() << "RenderDXGITextureHostOGL AcquireSync timeout, hr=" << gfx::hexa(hr);
      return false;
    }
  }

  return true;
}

void
RenderDXGITextureHostOGL::Unlock()
{
  if (mKeyedMutex) {
    mKeyedMutex->ReleaseSync(0);
  }
}

void
RenderDXGITextureHostOGL::DeleteTextureHandle()
{
  if (mTextureHandle[0] != 0) {
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

} // namespace wr
} // namespace mozilla
