/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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

RenderDXGITextureHostOGL::RenderDXGITextureHostOGL(WindowsHandle aHandle,
                                                   gfx::SurfaceFormat aFormat,
                                                   gfx::IntSize aSize)
    : mHandle(aHandle),
      mSurface(0),
      mStream(0),
      mTextureHandle{0},
      mFormat(aFormat),
      mSize(aSize),
      mLocked(false) {
  MOZ_COUNT_CTOR_INHERITED(RenderDXGITextureHostOGL, RenderTextureHostOGL);
  MOZ_ASSERT((mFormat != gfx::SurfaceFormat::NV12 &&
              mFormat != gfx::SurfaceFormat::P010 &&
              mFormat != gfx::SurfaceFormat::P016) ||
             (mSize.width % 2 == 0 && mSize.height % 2 == 0));
  MOZ_ASSERT(aHandle);
}

RenderDXGITextureHostOGL::~RenderDXGITextureHostOGL() {
  MOZ_COUNT_DTOR_INHERITED(RenderDXGITextureHostOGL, RenderTextureHostOGL);
  DeleteTextureHandle();
}

ID3D11Texture2D* RenderDXGITextureHostOGL::GetD3D11Texture2D() {
  if (!mGL) {
    // SharedGL is always used on Windows with ANGLE.
    mGL = RenderThread::Get()->SharedGL();
  }

  if (!mTexture) {
    if (!EnsureD3D11Texture2D()) {
      return nullptr;
    }
  }
  return mTexture;
}

bool RenderDXGITextureHostOGL::EnsureD3D11Texture2D() {
  if (mTexture) {
    return true;
  }

  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  // Fetch the D3D11 device.
  EGLDeviceEXT eglDevice = nullptr;
  egl->fQueryDisplayAttribEXT(egl->Display(), LOCAL_EGL_DEVICE_EXT,
                              (EGLAttrib*)&eglDevice);
  MOZ_ASSERT(eglDevice);
  ID3D11Device* device = nullptr;
  egl->fQueryDeviceAttribEXT(eglDevice, LOCAL_EGL_D3D11_DEVICE_ANGLE,
                             (EGLAttrib*)&device);
  // There's a chance this might fail if we end up on d3d9 angle for some
  // reason.
  if (!device) {
    gfxCriticalNote << "RenderDXGITextureHostOGL device is not available";
    return false;
  }

  // Get the D3D11 texture from shared handle.
  HRESULT hr = device->OpenSharedResource(
      (HANDLE)mHandle, __uuidof(ID3D11Texture2D),
      (void**)(ID3D11Texture2D**)getter_AddRefs(mTexture));
  if (FAILED(hr)) {
    NS_WARNING(
        "RenderDXGITextureHostOGL::EnsureLockable(): Failed to open shared "
        "texture");
    gfxCriticalNote
        << "RenderDXGITextureHostOGL Failed to open shared texture, hr="
        << gfx::hexa(hr);
    return false;
  }
  MOZ_ASSERT(mTexture.get());
  return true;
}

bool RenderDXGITextureHostOGL::EnsureLockable(wr::ImageRendering aRendering) {
  if (mTextureHandle[0]) {
    // Update filter if filter was changed.
    if (IsFilterUpdateNecessary(aRendering)) {
      ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                                   LOCAL_GL_TEXTURE_EXTERNAL_OES,
                                   mTextureHandle[0], aRendering);
      // Cache new rendering filter.
      mCachedRendering = aRendering;
      // NV12 and P016 uses two handles.
      if (mFormat == gfx::SurfaceFormat::NV12 ||
          mFormat == gfx::SurfaceFormat::P010 ||
          mFormat == gfx::SurfaceFormat::P016) {
        ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE1,
                                     LOCAL_GL_TEXTURE_EXTERNAL_OES,
                                     mTextureHandle[1], aRendering);
      }
    }
    return true;
  }

  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  // We use EGLStream to get the converted gl handle from d3d texture. The
  // NV_stream_consumer_gltexture_yuv and ANGLE_stream_producer_d3d_texture
  // could support nv12 and rgb d3d texture format.
  if (!egl->IsExtensionSupported(
          gl::GLLibraryEGL::NV_stream_consumer_gltexture_yuv) ||
      !egl->IsExtensionSupported(
          gl::GLLibraryEGL::ANGLE_stream_producer_d3d_texture)) {
    gfxCriticalNote
        << "RenderDXGITextureHostOGL egl extensions are not suppored";
    return false;
  }

  // Get the D3D11 texture from shared handle.
  if (!EnsureD3D11Texture2D()) {
    return false;
  }
  mTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mKeyedMutex));

  // Create the EGLStream.
  mStream = egl->fCreateStreamKHR(egl->Display(), nullptr);
  MOZ_ASSERT(mStream);

  if (mFormat != gfx::SurfaceFormat::NV12 &&
      mFormat != gfx::SurfaceFormat::P010 &&
      mFormat != gfx::SurfaceFormat::P016) {
    // The non-nv12 format.

    mGL->fGenTextures(1, mTextureHandle);
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                                 LOCAL_GL_TEXTURE_EXTERNAL_OES,
                                 mTextureHandle[0], aRendering);
    // Cache new rendering filter.
    mCachedRendering = aRendering;
    MOZ_ALWAYS_TRUE(egl->fStreamConsumerGLTextureExternalAttribsNV(
        egl->Display(), mStream, nullptr));
    MOZ_ALWAYS_TRUE(egl->fCreateStreamProducerD3DTextureANGLE(
        egl->Display(), mStream, nullptr));
  } else {
    // The nv12/p016 format.

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
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                                 LOCAL_GL_TEXTURE_EXTERNAL_OES,
                                 mTextureHandle[0], aRendering);
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE1,
                                 LOCAL_GL_TEXTURE_EXTERNAL_OES,
                                 mTextureHandle[1], aRendering);
    // Cache new rendering filter.
    mCachedRendering = aRendering;
    MOZ_ALWAYS_TRUE(egl->fStreamConsumerGLTextureExternalAttribsNV(
        egl->Display(), mStream, consumerAttributes));
    MOZ_ALWAYS_TRUE(egl->fCreateStreamProducerD3DTextureANGLE(
        egl->Display(), mStream, nullptr));
  }

  // Insert the d3d texture.
  MOZ_ALWAYS_TRUE(egl->fStreamPostD3DTextureANGLE(
      egl->Display(), mStream, (void*)mTexture.get(), nullptr));

  // Now, we could get the gl handle from the stream.
  egl->fStreamConsumerAcquireKHR(egl->Display(), mStream);
  MOZ_ASSERT(egl->fGetError() == LOCAL_EGL_SUCCESS);

  return true;
}

wr::WrExternalImage RenderDXGITextureHostOGL::Lock(
    uint8_t aChannelIndex, gl::GLContext* aGL, wr::ImageRendering aRendering) {
  if (mGL.get() != aGL) {
    // Release the texture handle in the previous gl context.
    DeleteTextureHandle();
    mGL = aGL;
    mGL->MakeCurrent();
  }

  if (!EnsureLockable(aRendering)) {
    return InvalidToWrExternalImage();
  }

  if (!mLocked) {
    if (mKeyedMutex) {
      HRESULT hr = mKeyedMutex->AcquireSync(0, 10000);
      if (hr != S_OK) {
        gfxCriticalError()
            << "RenderDXGITextureHostOGL AcquireSync timeout, hr="
            << gfx::hexa(hr);
        return InvalidToWrExternalImage();
      }
    }
    mLocked = true;
  }

  gfx::IntSize size = GetSize(aChannelIndex);
  return NativeTextureToWrExternalImage(GetGLHandle(aChannelIndex), 0, 0,
                                        size.width, size.height);
}

void RenderDXGITextureHostOGL::Unlock() {
  if (mLocked) {
    if (mKeyedMutex) {
      mKeyedMutex->ReleaseSync(0);
    }
    mLocked = false;
  }
}

void RenderDXGITextureHostOGL::ClearCachedResources() {
  DeleteTextureHandle();
  mGL = nullptr;
}

void RenderDXGITextureHostOGL::DeleteTextureHandle() {
  if (mTextureHandle[0] == 0) {
    return;
  }

  MOZ_ASSERT(mGL.get());
  if (!mGL) {
    return;
  }

  if (mGL->MakeCurrent()) {
    mGL->fDeleteTextures(2, mTextureHandle);
  }
  for (int i = 0; i < 2; ++i) {
    mTextureHandle[i] = 0;
  }

  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;
  if (mSurface) {
    egl->fDestroySurface(egl->Display(), mSurface);
    mSurface = 0;
  }
  if (mStream) {
    egl->fDestroyStreamKHR(egl->Display(), mStream);
    mStream = 0;
  }

  mTexture = nullptr;
  mKeyedMutex = nullptr;
}

GLuint RenderDXGITextureHostOGL::GetGLHandle(uint8_t aChannelIndex) const {
  MOZ_ASSERT(((mFormat == gfx::SurfaceFormat::NV12 ||
               mFormat == gfx::SurfaceFormat::P010 ||
               mFormat == gfx::SurfaceFormat::P016) &&
              aChannelIndex < 2) ||
             aChannelIndex < 1);
  return mTextureHandle[aChannelIndex];
}

gfx::IntSize RenderDXGITextureHostOGL::GetSize(uint8_t aChannelIndex) const {
  MOZ_ASSERT(((mFormat == gfx::SurfaceFormat::NV12 ||
               mFormat == gfx::SurfaceFormat::P010 ||
               mFormat == gfx::SurfaceFormat::P016) &&
              aChannelIndex < 2) ||
             aChannelIndex < 1);

  if (aChannelIndex == 0) {
    return mSize;
  } else {
    // The CbCr channel size is a half of Y channel size in NV12 format.
    return mSize / 2;
  }
}

RenderDXGIYCbCrTextureHostOGL::RenderDXGIYCbCrTextureHostOGL(
    WindowsHandle (&aHandles)[3], gfx::IntSize aSize, gfx::IntSize aSizeCbCr)
    : mHandles{aHandles[0], aHandles[1], aHandles[2]},
      mSurfaces{0},
      mStreams{0},
      mTextureHandles{0},
      mSize(aSize),
      mSizeCbCr(aSizeCbCr),
      mLocked(false) {
  MOZ_COUNT_CTOR_INHERITED(RenderDXGIYCbCrTextureHostOGL, RenderTextureHostOGL);
  // Assume the chroma planes are rounded up if the luma plane is odd sized.
  MOZ_ASSERT((mSizeCbCr.width == mSize.width ||
              mSizeCbCr.width == (mSize.width + 1) >> 1) &&
             (mSizeCbCr.height == mSize.height ||
              mSizeCbCr.height == (mSize.height + 1) >> 1));
  MOZ_ASSERT(aHandles[0] && aHandles[1] && aHandles[2]);
}

RenderDXGIYCbCrTextureHostOGL::~RenderDXGIYCbCrTextureHostOGL() {
  MOZ_COUNT_DTOR_INHERITED(RenderDXGIYCbCrTextureHostOGL, RenderTextureHostOGL);
  DeleteTextureHandle();
}

bool RenderDXGIYCbCrTextureHostOGL::EnsureLockable(
    wr::ImageRendering aRendering) {
  if (mTextureHandles[0]) {
    // Update filter if filter was changed.
    if (IsFilterUpdateNecessary(aRendering)) {
      for (int i = 0; i < 3; ++i) {
        ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0 + i,
                                     LOCAL_GL_TEXTURE_EXTERNAL_OES,
                                     mTextureHandles[i], aRendering);
        // Cache new rendering filter.
        mCachedRendering = aRendering;
      }
    }
    return true;
  }

  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  // The eglCreatePbufferFromClientBuffer doesn't support R8 format, so we
  // use EGLStream to get the converted gl handle from d3d R8 texture.

  if (!egl->IsExtensionSupported(
          gl::GLLibraryEGL::NV_stream_consumer_gltexture_yuv) ||
      !egl->IsExtensionSupported(
          gl::GLLibraryEGL::ANGLE_stream_producer_d3d_texture)) {
    gfxCriticalNote
        << "RenderDXGIYCbCrTextureHostOGL egl extensions are not suppored";
    return false;
  }

  // Fetch the D3D11 device.
  EGLDeviceEXT eglDevice = nullptr;
  egl->fQueryDisplayAttribEXT(egl->Display(), LOCAL_EGL_DEVICE_EXT,
                              (EGLAttrib*)&eglDevice);
  MOZ_ASSERT(eglDevice);
  ID3D11Device* device = nullptr;
  egl->fQueryDeviceAttribEXT(eglDevice, LOCAL_EGL_D3D11_DEVICE_ANGLE,
                             (EGLAttrib*)&device);
  // There's a chance this might fail if we end up on d3d9 angle for some
  // reason.
  if (!device) {
    gfxCriticalNote << "RenderDXGIYCbCrTextureHostOGL device is not available";
    return false;
  }

  for (int i = 0; i < 3; ++i) {
    // Get the R8 D3D11 texture from shared handle.
    HRESULT hr = device->OpenSharedResource(
        (HANDLE)mHandles[i], __uuidof(ID3D11Texture2D),
        (void**)(ID3D11Texture2D**)getter_AddRefs(mTextures[i]));
    if (FAILED(hr)) {
      NS_WARNING(
          "RenderDXGIYCbCrTextureHostOGL::EnsureLockable(): Failed to open "
          "shared "
          "texture");
      gfxCriticalNote
          << "RenderDXGIYCbCrTextureHostOGL Failed to open shared texture, hr="
          << gfx::hexa(hr);
      return false;
    }
  }

  for (int i = 0; i < 3; ++i) {
    mTextures[i]->QueryInterface(
        (IDXGIKeyedMutex**)getter_AddRefs(mKeyedMutexs[i]));
  }

  mGL->fGenTextures(3, mTextureHandles);
  for (int i = 0; i < 3; ++i) {
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0 + i,
                                 LOCAL_GL_TEXTURE_EXTERNAL_OES,
                                 mTextureHandles[i], aRendering);
    // Cache new rendering filter.
    mCachedRendering = aRendering;

    // Create the EGLStream.
    mStreams[i] = egl->fCreateStreamKHR(egl->Display(), nullptr);
    MOZ_ASSERT(mStreams[i]);

    MOZ_ALWAYS_TRUE(egl->fStreamConsumerGLTextureExternalAttribsNV(
        egl->Display(), mStreams[i], nullptr));
    MOZ_ALWAYS_TRUE(egl->fCreateStreamProducerD3DTextureANGLE(
        egl->Display(), mStreams[i], nullptr));

    // Insert the R8 texture.
    MOZ_ALWAYS_TRUE(egl->fStreamPostD3DTextureANGLE(
        egl->Display(), mStreams[i], (void*)mTextures[i].get(), nullptr));

    // Now, we could get the R8 gl handle from the stream.
    egl->fStreamConsumerAcquireKHR(egl->Display(), mStreams[i]);
    MOZ_ASSERT(egl->fGetError() == LOCAL_EGL_SUCCESS);
  }

  return true;
}

wr::WrExternalImage RenderDXGIYCbCrTextureHostOGL::Lock(
    uint8_t aChannelIndex, gl::GLContext* aGL, wr::ImageRendering aRendering) {
  if (mGL.get() != aGL) {
    // Release the texture handle in the previous gl context.
    DeleteTextureHandle();
    mGL = aGL;
    mGL->MakeCurrent();
  }

  if (!EnsureLockable(aRendering)) {
    return InvalidToWrExternalImage();
  }

  if (!mLocked) {
    if (mKeyedMutexs[0]) {
      for (const auto& mutex : mKeyedMutexs) {
        HRESULT hr = mutex->AcquireSync(0, 10000);
        if (hr != S_OK) {
          gfxCriticalError()
              << "RenderDXGIYCbCrTextureHostOGL AcquireSync timeout, hr="
              << gfx::hexa(hr);
          return InvalidToWrExternalImage();
        }
      }
    }
    mLocked = true;
  }

  gfx::IntSize size = GetSize(aChannelIndex);
  return NativeTextureToWrExternalImage(GetGLHandle(aChannelIndex), 0, 0,
                                        size.width, size.height);
}

void RenderDXGIYCbCrTextureHostOGL::Unlock() {
  if (mLocked) {
    if (mKeyedMutexs[0]) {
      for (const auto& mutex : mKeyedMutexs) {
        mutex->ReleaseSync(0);
      }
    }
    mLocked = false;
  }
}

void RenderDXGIYCbCrTextureHostOGL::ClearCachedResources() {
  DeleteTextureHandle();
  mGL = nullptr;
}

GLuint RenderDXGIYCbCrTextureHostOGL::GetGLHandle(uint8_t aChannelIndex) const {
  MOZ_ASSERT(aChannelIndex < 3);

  return mTextureHandles[aChannelIndex];
}

gfx::IntSize RenderDXGIYCbCrTextureHostOGL::GetSize(
    uint8_t aChannelIndex) const {
  MOZ_ASSERT(aChannelIndex < 3);

  if (aChannelIndex == 0) {
    return mSize;
  } else {
    return mSizeCbCr;
  }
}

void RenderDXGIYCbCrTextureHostOGL::DeleteTextureHandle() {
  if (mTextureHandles[0] == 0) {
    return;
  }

  MOZ_ASSERT(mGL.get());
  if (!mGL) {
    return;
  }

  if (mGL->MakeCurrent()) {
    mGL->fDeleteTextures(3, mTextureHandles);
  }
  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;
  for (int i = 0; i < 3; ++i) {
    mTextureHandles[i] = 0;
    mTextures[i] = nullptr;
    mKeyedMutexs[i] = nullptr;

    if (mSurfaces[i]) {
      egl->fDestroySurface(egl->Display(), mSurfaces[i]);
      mSurfaces[i] = 0;
    }
    if (mStreams[i]) {
      egl->fDestroyStreamKHR(egl->Display(), mStreams[i]);
      mStreams[i] = 0;
    }
  }
}

}  // namespace wr
}  // namespace mozilla
