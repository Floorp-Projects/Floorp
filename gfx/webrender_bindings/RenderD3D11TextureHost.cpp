/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderD3D11TextureHost.h"

#include <d3d11.h>

#include "GLContextEGL.h"
#include "GLLibraryEGL.h"
#include "ScopedGLHelpers.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {
namespace wr {

RenderDXGITextureHost::RenderDXGITextureHost(
    WindowsHandle aHandle, Maybe<uint64_t>& aGpuProcessTextureId,
    uint32_t aArrayIndex, gfx::SurfaceFormat aFormat,
    gfx::ColorSpace2 aColorSpace, gfx::ColorRange aColorRange,
    gfx::IntSize aSize)
    : mHandle(aHandle),
      mGpuProcessTextureId(aGpuProcessTextureId),
      mArrayIndex(aArrayIndex),
      mSurface(0),
      mStream(0),
      mTextureHandle{0},
      mFormat(aFormat),
      mColorSpace(aColorSpace),
      mColorRange(aColorRange),
      mSize(aSize),
      mLocked(false) {
  MOZ_COUNT_CTOR_INHERITED(RenderDXGITextureHost, RenderTextureHost);
  MOZ_ASSERT((mFormat != gfx::SurfaceFormat::NV12 &&
              mFormat != gfx::SurfaceFormat::P010 &&
              mFormat != gfx::SurfaceFormat::P016) ||
             (mSize.width % 2 == 0 && mSize.height % 2 == 0));
  MOZ_ASSERT((aHandle && aGpuProcessTextureId.isNothing()) ||
             (!aHandle && aGpuProcessTextureId.isSome()));
}

RenderDXGITextureHost::~RenderDXGITextureHost() {
  MOZ_COUNT_DTOR_INHERITED(RenderDXGITextureHost, RenderTextureHost);
  DeleteTextureHandle();
}

ID3D11Texture2D* RenderDXGITextureHost::GetD3D11Texture2DWithGL() {
  if (mTexture) {
    return mTexture;
  }

  if (!mGL) {
    // SingletonGL is always used on Windows with ANGLE.
    mGL = RenderThread::Get()->SingletonGL();
  }

  if (!EnsureD3D11Texture2DWithGL()) {
    return nullptr;
  }

  return mTexture;
}

size_t RenderDXGITextureHost::GetPlaneCount() const {
  if (mFormat == gfx::SurfaceFormat::NV12 ||
      mFormat == gfx::SurfaceFormat::P010 ||
      mFormat == gfx::SurfaceFormat::P016) {
    return 2;
  }
  return 1;
}

template <typename T>
static bool MapTexture(T* aHost, RenderCompositor* aCompositor,
                       RefPtr<ID3D11Texture2D>& aTexture,
                       RefPtr<ID3D11DeviceContext>& aDeviceContext,
                       RefPtr<ID3D11Texture2D>& aCpuTexture,
                       D3D11_MAPPED_SUBRESOURCE& aMappedSubresource) {
  if (!aCompositor) {
    return false;
  }

  RenderCompositorD3D11SWGL* compositor =
      aCompositor->AsRenderCompositorD3D11SWGL();
  if (!compositor) {
    return false;
  }

  if (!aHost->EnsureD3D11Texture2D(compositor->GetDevice())) {
    return false;
  }

  if (!aHost->LockInternal()) {
    return false;
  }

  D3D11_TEXTURE2D_DESC textureDesc = {0};
  aTexture->GetDesc(&textureDesc);

  compositor->GetDevice()->GetImmediateContext(getter_AddRefs(aDeviceContext));

  textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  textureDesc.Usage = D3D11_USAGE_STAGING;
  textureDesc.BindFlags = 0;
  textureDesc.MiscFlags = 0;
  textureDesc.MipLevels = 1;
  HRESULT hr = compositor->GetDevice()->CreateTexture2D(
      &textureDesc, nullptr, getter_AddRefs(aCpuTexture));
  if (FAILED(hr)) {
    return false;
  }

  aDeviceContext->CopyResource(aCpuTexture, aTexture);
  aHost->Unlock();

  hr = aDeviceContext->Map(aCpuTexture, 0, D3D11_MAP_READ, 0,
                           &aMappedSubresource);
  return SUCCEEDED(hr);
}

bool RenderDXGITextureHost::MapPlane(RenderCompositor* aCompositor,
                                     uint8_t aChannelIndex,
                                     PlaneInfo& aPlaneInfo) {
  // TODO: We currently readback from the GPU texture into a new
  // staging texture every time this is mapped. We might be better
  // off retaining the mapped memory to trade performance for memory
  // usage.
  if (!mCpuTexture && !MapTexture(this, aCompositor, mTexture, mDeviceContext,
                                  mCpuTexture, mMappedSubresource)) {
    return false;
  }

  aPlaneInfo.mSize = GetSize(aChannelIndex);
  aPlaneInfo.mStride = mMappedSubresource.RowPitch;
  aPlaneInfo.mData = mMappedSubresource.pData;

  // If this is the second plane, then offset the data pointer by the
  // size of the first plane.
  if (aChannelIndex == 1) {
    aPlaneInfo.mData =
        (uint8_t*)aPlaneInfo.mData + aPlaneInfo.mStride * GetSize(0).height;
  }
  return true;
}

void RenderDXGITextureHost::UnmapPlanes() {
  mMappedSubresource.pData = nullptr;
  if (mCpuTexture) {
    mDeviceContext->Unmap(mCpuTexture, 0);
    mCpuTexture = nullptr;
  }
  mDeviceContext = nullptr;
}

bool RenderDXGITextureHost::EnsureD3D11Texture2DWithGL() {
  if (mTexture) {
    return true;
  }

  if (mGpuProcessTextureId.isSome()) {
    auto* textureMap = layers::GpuProcessD3D11TextureMap::Get();
    if (textureMap) {
      RefPtr<ID3D11Texture2D> texture;
      mTexture = textureMap->GetTexture(mGpuProcessTextureId.ref());
      if (mTexture) {
        return true;
      }
    }
    return false;
  }

  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  // Fetch the D3D11 device.
  EGLDeviceEXT eglDevice = nullptr;
  egl->fQueryDisplayAttribEXT(LOCAL_EGL_DEVICE_EXT, (EGLAttrib*)&eglDevice);
  MOZ_ASSERT(eglDevice);
  ID3D11Device* device = nullptr;
  egl->mLib->fQueryDeviceAttribEXT(eglDevice, LOCAL_EGL_D3D11_DEVICE_ANGLE,
                                   (EGLAttrib*)&device);
  // There's a chance this might fail if we end up on d3d9 angle for some
  // reason.
  if (!device) {
    gfxCriticalNote << "RenderDXGITextureHost device is not available";
    return false;
  }

  return EnsureD3D11Texture2D(device);
}

bool RenderDXGITextureHost::EnsureD3D11Texture2D(ID3D11Device* aDevice) {
  if (mTexture) {
    RefPtr<ID3D11Device> device;
    mTexture->GetDevice(getter_AddRefs(device));
    if (aDevice != device) {
      gfxCriticalNote << "RenderDXGITextureHost uses obsoleted device";
      return false;
    }
    return true;
  }

  // Get the D3D11 texture from shared handle.
  HRESULT hr = aDevice->OpenSharedResource(
      (HANDLE)mHandle, __uuidof(ID3D11Texture2D),
      (void**)(ID3D11Texture2D**)getter_AddRefs(mTexture));
  if (FAILED(hr)) {
    MOZ_ASSERT(false,
               "RenderDXGITextureHost::EnsureLockable(): Failed to open shared "
               "texture");
    gfxCriticalNote
        << "RenderDXGITextureHost Failed to open shared texture, hr="
        << gfx::hexa(hr);
    return false;
  }
  MOZ_ASSERT(mTexture.get());
  mTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mKeyedMutex));
  return true;
}

bool RenderDXGITextureHost::EnsureLockable(wr::ImageRendering aRendering) {
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
          gl::EGLExtension::NV_stream_consumer_gltexture_yuv) ||
      !egl->IsExtensionSupported(
          gl::EGLExtension::ANGLE_stream_producer_d3d_texture)) {
    gfxCriticalNote << "RenderDXGITextureHost egl extensions are not suppored";
    return false;
  }

  // Get the D3D11 texture from shared handle.
  if (!EnsureD3D11Texture2DWithGL()) {
    return false;
  }

  // Create the EGLStream.
  mStream = egl->fCreateStreamKHR(nullptr);
  MOZ_ASSERT(mStream);

  bool ok = true;
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
    ok &=
        bool(egl->fStreamConsumerGLTextureExternalAttribsNV(mStream, nullptr));
    ok &= bool(egl->fCreateStreamProducerD3DTextureANGLE(mStream, nullptr));
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
    ok &= bool(egl->fStreamConsumerGLTextureExternalAttribsNV(
        mStream, consumerAttributes));
    ok &= bool(egl->fCreateStreamProducerD3DTextureANGLE(mStream, nullptr));
  }

  const EGLAttrib frameAttributes[] = {
      LOCAL_EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE,
      static_cast<EGLAttrib>(mArrayIndex),
      LOCAL_EGL_NONE,
  };

  // Insert the d3d texture.
  ok &= bool(egl->fStreamPostD3DTextureANGLE(mStream, (void*)mTexture.get(),
                                             frameAttributes));

  if (!ok) {
    gfxCriticalNote << "RenderDXGITextureHost init stream failed";
    DeleteTextureHandle();
    return false;
  }

  // Now, we could get the gl handle from the stream.
  MOZ_ALWAYS_TRUE(egl->fStreamConsumerAcquireKHR(mStream));

  return true;
}

wr::WrExternalImage RenderDXGITextureHost::Lock(uint8_t aChannelIndex,
                                                gl::GLContext* aGL,
                                                wr::ImageRendering aRendering) {
  if (mGL.get() != aGL) {
    // Release the texture handle in the previous gl context.
    DeleteTextureHandle();
    mGL = aGL;
  }

  if (!mGL) {
    // XXX Software WebRender is not handled yet.
    // Software WebRender does not provide GLContext
    gfxCriticalNoteOnce
        << "Software WebRender is not suppored by RenderDXGITextureHost.";
    return InvalidToWrExternalImage();
  }

  if (!EnsureLockable(aRendering)) {
    return InvalidToWrExternalImage();
  }

  if (!LockInternal()) {
    return InvalidToWrExternalImage();
  }

  const auto uvs = GetUvCoords(GetSize(aChannelIndex));
  return NativeTextureToWrExternalImage(GetGLHandle(aChannelIndex), uvs.first.x,
                                        uvs.first.y, uvs.second.x,
                                        uvs.second.y);
}

bool RenderDXGITextureHost::LockInternal() {
  if (!mLocked) {
    if (mKeyedMutex) {
      HRESULT hr = mKeyedMutex->AcquireSync(0, 10000);
      if (hr != S_OK) {
        gfxCriticalError() << "RenderDXGITextureHost AcquireSync timeout, hr="
                           << gfx::hexa(hr);
        return false;
      }
    }
    mLocked = true;
  }
  return true;
}

void RenderDXGITextureHost::Unlock() {
  if (mLocked) {
    if (mKeyedMutex) {
      mKeyedMutex->ReleaseSync(0);
    }
    mLocked = false;
  }
}

void RenderDXGITextureHost::ClearCachedResources() {
  DeleteTextureHandle();
  mGL = nullptr;
}

void RenderDXGITextureHost::DeleteTextureHandle() {
  if (mTextureHandle[0] == 0) {
    return;
  }

  MOZ_ASSERT(mGL.get());
  if (!mGL) {
    return;
  }

  if (mGL->MakeCurrent()) {
    mGL->fDeleteTextures(2, mTextureHandle);

    const auto& gle = gl::GLContextEGL::Cast(mGL);
    const auto& egl = gle->mEgl;
    if (mSurface) {
      egl->fDestroySurface(mSurface);
    }
    if (mStream) {
      egl->fDestroyStreamKHR(mStream);
    }
  }

  for (int i = 0; i < 2; ++i) {
    mTextureHandle[i] = 0;
  }

  mTexture = nullptr;
  mKeyedMutex = nullptr;
  mSurface = 0;
  mStream = 0;
}

GLuint RenderDXGITextureHost::GetGLHandle(uint8_t aChannelIndex) const {
  MOZ_ASSERT(((mFormat == gfx::SurfaceFormat::NV12 ||
               mFormat == gfx::SurfaceFormat::P010 ||
               mFormat == gfx::SurfaceFormat::P016) &&
              aChannelIndex < 2) ||
             aChannelIndex < 1);
  return mTextureHandle[aChannelIndex];
}

gfx::IntSize RenderDXGITextureHost::GetSize(uint8_t aChannelIndex) const {
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

RenderDXGIYCbCrTextureHost::RenderDXGIYCbCrTextureHost(
    WindowsHandle (&aHandles)[3], gfx::YUVColorSpace aYUVColorSpace,
    gfx::ColorDepth aColorDepth, gfx::ColorRange aColorRange,
    gfx::IntSize aSizeY, gfx::IntSize aSizeCbCr)
    : mHandles{aHandles[0], aHandles[1], aHandles[2]},
      mSurfaces{0},
      mStreams{0},
      mTextureHandles{0},
      mYUVColorSpace(aYUVColorSpace),
      mColorDepth(aColorDepth),
      mColorRange(aColorRange),
      mSizeY(aSizeY),
      mSizeCbCr(aSizeCbCr),
      mLocked(false) {
  MOZ_COUNT_CTOR_INHERITED(RenderDXGIYCbCrTextureHost, RenderTextureHost);
  // Assume the chroma planes are rounded up if the luma plane is odd sized.
  MOZ_ASSERT((mSizeCbCr.width == mSizeY.width ||
              mSizeCbCr.width == (mSizeY.width + 1) >> 1) &&
             (mSizeCbCr.height == mSizeY.height ||
              mSizeCbCr.height == (mSizeY.height + 1) >> 1));
  MOZ_ASSERT(aHandles[0] && aHandles[1] && aHandles[2]);
}

bool RenderDXGIYCbCrTextureHost::MapPlane(RenderCompositor* aCompositor,
                                          uint8_t aChannelIndex,
                                          PlaneInfo& aPlaneInfo) {
  D3D11_MAPPED_SUBRESOURCE mappedSubresource;
  if (!MapTexture(this, aCompositor, mTextures[aChannelIndex], mDeviceContext,
                  mCpuTexture[aChannelIndex], mappedSubresource)) {
    return false;
  }

  aPlaneInfo.mSize = GetSize(aChannelIndex);
  aPlaneInfo.mStride = mappedSubresource.RowPitch;
  aPlaneInfo.mData = mappedSubresource.pData;
  return true;
}

void RenderDXGIYCbCrTextureHost::UnmapPlanes() {
  for (uint32_t i = 0; i < 3; i++) {
    if (mCpuTexture[i]) {
      mDeviceContext->Unmap(mCpuTexture[i], 0);
      mCpuTexture[i] = nullptr;
    }
  }
  mDeviceContext = nullptr;
}

RenderDXGIYCbCrTextureHost::~RenderDXGIYCbCrTextureHost() {
  MOZ_COUNT_DTOR_INHERITED(RenderDXGIYCbCrTextureHost, RenderTextureHost);
  DeleteTextureHandle();
}

bool RenderDXGIYCbCrTextureHost::EnsureLockable(wr::ImageRendering aRendering) {
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
          gl::EGLExtension::NV_stream_consumer_gltexture_yuv) ||
      !egl->IsExtensionSupported(
          gl::EGLExtension::ANGLE_stream_producer_d3d_texture)) {
    gfxCriticalNote
        << "RenderDXGIYCbCrTextureHost egl extensions are not suppored";
    return false;
  }

  // Fetch the D3D11 device.
  EGLDeviceEXT eglDevice = nullptr;
  egl->fQueryDisplayAttribEXT(LOCAL_EGL_DEVICE_EXT, (EGLAttrib*)&eglDevice);
  MOZ_ASSERT(eglDevice);
  ID3D11Device* device = nullptr;
  egl->mLib->fQueryDeviceAttribEXT(eglDevice, LOCAL_EGL_D3D11_DEVICE_ANGLE,
                                   (EGLAttrib*)&device);
  // There's a chance this might fail if we end up on d3d9 angle for some
  // reason.
  if (!device) {
    gfxCriticalNote << "RenderDXGIYCbCrTextureHost device is not available";
    return false;
  }

  EnsureD3D11Texture2D(device);

  mGL->fGenTextures(3, mTextureHandles);
  bool ok = true;
  for (int i = 0; i < 3; ++i) {
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0 + i,
                                 LOCAL_GL_TEXTURE_EXTERNAL_OES,
                                 mTextureHandles[i], aRendering);
    // Cache new rendering filter.
    mCachedRendering = aRendering;

    // Create the EGLStream.
    mStreams[i] = egl->fCreateStreamKHR(nullptr);
    MOZ_ASSERT(mStreams[i]);

    ok &= bool(
        egl->fStreamConsumerGLTextureExternalAttribsNV(mStreams[i], nullptr));
    ok &= bool(egl->fCreateStreamProducerD3DTextureANGLE(mStreams[i], nullptr));

    // Insert the R8 texture.
    ok &= bool(egl->fStreamPostD3DTextureANGLE(
        mStreams[i], (void*)mTextures[i].get(), nullptr));

    // Now, we could get the R8 gl handle from the stream.
    MOZ_ALWAYS_TRUE(egl->fStreamConsumerAcquireKHR(mStreams[i]));
  }

  if (!ok) {
    gfxCriticalNote << "RenderDXGIYCbCrTextureHost init stream failed";
    DeleteTextureHandle();
    return false;
  }

  return true;
}

bool RenderDXGIYCbCrTextureHost::EnsureD3D11Texture2D(ID3D11Device* aDevice) {
  if (mTextures[0]) {
    RefPtr<ID3D11Device> device;
    mTextures[0]->GetDevice(getter_AddRefs(device));
    if (aDevice != device) {
      gfxCriticalNote << "RenderDXGIYCbCrTextureHost uses obsoleted device";
      return false;
    }
  }

  if (mTextureHandles[0]) {
    return true;
  }

  for (int i = 0; i < 3; ++i) {
    // Get the R8 D3D11 texture from shared handle.
    HRESULT hr = aDevice->OpenSharedResource(
        (HANDLE)mHandles[i], __uuidof(ID3D11Texture2D),
        (void**)(ID3D11Texture2D**)getter_AddRefs(mTextures[i]));
    if (FAILED(hr)) {
      NS_WARNING(
          "RenderDXGIYCbCrTextureHost::EnsureLockable(): Failed to open "
          "shared "
          "texture");
      gfxCriticalNote
          << "RenderDXGIYCbCrTextureHost Failed to open shared texture, hr="
          << gfx::hexa(hr);
      return false;
    }
  }

  for (int i = 0; i < 3; ++i) {
    mTextures[i]->QueryInterface(
        (IDXGIKeyedMutex**)getter_AddRefs(mKeyedMutexs[i]));
  }
  return true;
}

bool RenderDXGIYCbCrTextureHost::LockInternal() {
  if (!mLocked) {
    if (mKeyedMutexs[0]) {
      for (const auto& mutex : mKeyedMutexs) {
        HRESULT hr = mutex->AcquireSync(0, 10000);
        if (hr != S_OK) {
          gfxCriticalError()
              << "RenderDXGIYCbCrTextureHost AcquireSync timeout, hr="
              << gfx::hexa(hr);
          return false;
        }
      }
    }
    mLocked = true;
  }
  return true;
}

wr::WrExternalImage RenderDXGIYCbCrTextureHost::Lock(
    uint8_t aChannelIndex, gl::GLContext* aGL, wr::ImageRendering aRendering) {
  if (mGL.get() != aGL) {
    // Release the texture handle in the previous gl context.
    DeleteTextureHandle();
    mGL = aGL;
  }

  if (!mGL) {
    // XXX Software WebRender is not handled yet.
    // Software WebRender does not provide GLContext
    gfxCriticalNoteOnce << "Software WebRender is not suppored by "
                           "RenderDXGIYCbCrTextureHost.";
    return InvalidToWrExternalImage();
  }

  if (!EnsureLockable(aRendering)) {
    return InvalidToWrExternalImage();
  }

  if (!LockInternal()) {
    return InvalidToWrExternalImage();
  }

  const auto uvs = GetUvCoords(GetSize(aChannelIndex));
  return NativeTextureToWrExternalImage(GetGLHandle(aChannelIndex), uvs.first.x,
                                        uvs.first.y, uvs.second.x,
                                        uvs.second.y);
}

void RenderDXGIYCbCrTextureHost::Unlock() {
  if (mLocked) {
    if (mKeyedMutexs[0]) {
      for (const auto& mutex : mKeyedMutexs) {
        mutex->ReleaseSync(0);
      }
    }
    mLocked = false;
  }
}

void RenderDXGIYCbCrTextureHost::ClearCachedResources() {
  DeleteTextureHandle();
  mGL = nullptr;
}

GLuint RenderDXGIYCbCrTextureHost::GetGLHandle(uint8_t aChannelIndex) const {
  MOZ_ASSERT(aChannelIndex < 3);

  return mTextureHandles[aChannelIndex];
}

gfx::IntSize RenderDXGIYCbCrTextureHost::GetSize(uint8_t aChannelIndex) const {
  MOZ_ASSERT(aChannelIndex < 3);

  if (aChannelIndex == 0) {
    return mSizeY;
  } else {
    return mSizeCbCr;
  }
}

void RenderDXGIYCbCrTextureHost::DeleteTextureHandle() {
  if (mTextureHandles[0] == 0) {
    return;
  }

  MOZ_ASSERT(mGL.get());
  if (!mGL) {
    return;
  }

  if (mGL->MakeCurrent()) {
    mGL->fDeleteTextures(3, mTextureHandles);

    const auto& gle = gl::GLContextEGL::Cast(mGL);
    const auto& egl = gle->mEgl;
    for (int i = 0; i < 3; ++i) {
      mTextureHandles[i] = 0;
      mTextures[i] = nullptr;
      mKeyedMutexs[i] = nullptr;

      if (mSurfaces[i]) {
        egl->fDestroySurface(mSurfaces[i]);
        mSurfaces[i] = 0;
      }
      if (mStreams[i]) {
        egl->fDestroyStreamKHR(mStreams[i]);
        mStreams[i] = 0;
      }
    }
  }
}

}  // namespace wr
}  // namespace mozilla
