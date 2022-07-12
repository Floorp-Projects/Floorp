/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceANGLE.h"

#include <d3d11.h>
#include "GLContextEGL.h"
#include "GLLibraryEGL.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc

namespace mozilla {
namespace gl {

// Returns `EGL_NO_SURFACE` (`0`) on error.
static EGLSurface CreatePBufferSurface(EglDisplay* egl, EGLConfig config,
                                       const gfx::IntSize& size) {
  const EGLint attribs[] = {LOCAL_EGL_WIDTH, size.width, LOCAL_EGL_HEIGHT,
                            size.height, LOCAL_EGL_NONE};

  EGLSurface surface = egl->fCreatePbufferSurface(config, attribs);
  if (!surface) {
    EGLint err = egl->mLib->fGetError();
    gfxCriticalError() << "Failed to create Pbuffer surface error: "
                       << gfx::hexa(err) << " Size : " << size;
    return 0;
  }

  return surface;
}

/*static*/
UniquePtr<SharedSurface_ANGLEShareHandle>
SharedSurface_ANGLEShareHandle::Create(const SharedSurfaceDesc& desc) {
  const auto& gle = GLContextEGL::Cast(desc.gl);
  const auto& egl = gle->mEgl;
  MOZ_ASSERT(egl);
  MOZ_ASSERT(egl->IsExtensionSupported(
      EGLExtension::ANGLE_surface_d3d_texture_2d_share_handle));

  const auto& config = gle->mConfig;
  MOZ_ASSERT(config);

  EGLSurface pbuffer = CreatePBufferSurface(egl.get(), config, desc.size);
  if (!pbuffer) return nullptr;

  // Declare everything before 'goto's.
  HANDLE shareHandle = nullptr;
  bool ok = egl->fQuerySurfacePointerANGLE(
      pbuffer, LOCAL_EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, &shareHandle);
  if (!ok) {
    egl->fDestroySurface(pbuffer);
    return nullptr;
  }
  void* opaqueKeyedMutex = nullptr;
  egl->fQuerySurfacePointerANGLE(pbuffer, LOCAL_EGL_DXGI_KEYED_MUTEX_ANGLE,
                                 &opaqueKeyedMutex);
  RefPtr<IDXGIKeyedMutex> keyedMutex =
      static_cast<IDXGIKeyedMutex*>(opaqueKeyedMutex);
#ifdef DEBUG
  if (!keyedMutex) {
    std::string envStr("1");
    static auto env = PR_GetEnv("MOZ_REQUIRE_KEYED_MUTEX");
    if (env) {
      envStr = env;
    }
    if (envStr != "0") {
      MOZ_ASSERT(keyedMutex, "set MOZ_REQUIRE_KEYED_MUTEX=0 to allow");
    }
  }
#endif

  return AsUnique(new SharedSurface_ANGLEShareHandle(desc, egl, pbuffer,
                                                     shareHandle, keyedMutex));
}

SharedSurface_ANGLEShareHandle::SharedSurface_ANGLEShareHandle(
    const SharedSurfaceDesc& desc, const std::weak_ptr<EglDisplay>& egl,
    EGLSurface pbuffer, HANDLE shareHandle,
    const RefPtr<IDXGIKeyedMutex>& keyedMutex)
    : SharedSurface(desc, nullptr),
      mEGL(egl),
      mPBuffer(pbuffer),
      mShareHandle(shareHandle),
      mKeyedMutex(keyedMutex) {}

SharedSurface_ANGLEShareHandle::~SharedSurface_ANGLEShareHandle() {
  const auto& gl = mDesc.gl;

  if (gl && GLContextEGL::Cast(gl)->GetEGLSurfaceOverride() == mPBuffer) {
    GLContextEGL::Cast(gl)->SetEGLSurfaceOverride(EGL_NO_SURFACE);
  }
  const auto egl = mEGL.lock();
  if (egl) {
    egl->fDestroySurface(mPBuffer);
  }
}

void SharedSurface_ANGLEShareHandle::LockProdImpl() {
  const auto& gl = mDesc.gl;
  GLContextEGL::Cast(gl)->SetEGLSurfaceOverride(mPBuffer);
}

void SharedSurface_ANGLEShareHandle::UnlockProdImpl() {}

void SharedSurface_ANGLEShareHandle::ProducerAcquireImpl() {
  if (mKeyedMutex) {
    HRESULT hr = mKeyedMutex->AcquireSync(0, 10000);
    if (hr == WAIT_TIMEOUT) {
      MOZ_CRASH("GFX: ANGLE share handle timeout");
    }
  }
}

void SharedSurface_ANGLEShareHandle::ProducerReleaseImpl() {
  const auto& gl = mDesc.gl;
  if (mKeyedMutex) {
    // XXX: ReleaseSync() has an implicit flush of the D3D commands
    // whether we need Flush() or not depends on the ANGLE semantics.
    // For now, we'll just do it
    gl->fFlush();
    mKeyedMutex->ReleaseSync(0);
    return;
  }
  gl->fFinish();
}

void SharedSurface_ANGLEShareHandle::ProducerReadAcquireImpl() {
  ProducerAcquireImpl();
}

void SharedSurface_ANGLEShareHandle::ProducerReadReleaseImpl() {
  if (mKeyedMutex) {
    mKeyedMutex->ReleaseSync(0);
    return;
  }
}

Maybe<layers::SurfaceDescriptor>
SharedSurface_ANGLEShareHandle::ToSurfaceDescriptor() {
  const auto format = gfx::SurfaceFormat::B8G8R8A8;
  return Some(layers::SurfaceDescriptorD3D10(
      (WindowsHandle)mShareHandle, /* gpuProcessTextureId */ Nothing(),
      /* arrayIndex */ 0, format, mDesc.size, mDesc.colorSpace,
      gfx::ColorRange::FULL));
}

class ScopedLockTexture final {
 public:
  explicit ScopedLockTexture(ID3D11Texture2D* texture, bool* succeeded)
      : mIsLocked(false), mTexture(texture) {
    MOZ_ASSERT(NS_IsMainThread(),
               "Must be on the main thread to use d3d11 immediate context");
    MOZ_ASSERT(mTexture);
    MOZ_ASSERT(succeeded);
    *succeeded = false;

    HRESULT hr;
    mTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mMutex));
    if (mMutex) {
      hr = mMutex->AcquireSync(0, 10000);
      if (hr == WAIT_TIMEOUT) {
        MOZ_CRASH("GFX: ANGLE scoped lock timeout");
      }

      if (FAILED(hr)) {
        NS_WARNING("Failed to lock the texture");
        return;
      }
    }

    RefPtr<ID3D11Device> device =
        gfx::DeviceManagerDx::Get()->GetContentDevice();
    if (!device) {
      return;
    }

    device->GetImmediateContext(getter_AddRefs(mDeviceContext));

    mTexture->GetDesc(&mDesc);
    mDesc.BindFlags = 0;
    mDesc.Usage = D3D11_USAGE_STAGING;
    mDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    mDesc.MiscFlags = 0;

    hr = device->CreateTexture2D(&mDesc, nullptr,
                                 getter_AddRefs(mCopiedTexture));

    if (FAILED(hr)) {
      return;
    }

    mDeviceContext->CopyResource(mCopiedTexture, mTexture);

    hr = mDeviceContext->Map(mCopiedTexture, 0, D3D11_MAP_READ, 0,
                             &mSubresource);
    if (FAILED(hr)) {
      return;
    }

    *succeeded = true;
    mIsLocked = true;
  }

  ~ScopedLockTexture() {
    mDeviceContext->Unmap(mCopiedTexture, 0);
    if (mMutex) {
      HRESULT hr = mMutex->ReleaseSync(0);
      if (FAILED(hr)) {
        NS_WARNING("Failed to unlock the texture");
      }
    }
    mIsLocked = false;
  }

  bool mIsLocked;
  RefPtr<ID3D11Texture2D> mTexture;
  RefPtr<ID3D11Texture2D> mCopiedTexture;
  RefPtr<IDXGIKeyedMutex> mMutex;
  RefPtr<ID3D11DeviceContext> mDeviceContext;
  D3D11_TEXTURE2D_DESC mDesc;
  D3D11_MAPPED_SUBRESOURCE mSubresource;
};

////////////////////////////////////////////////////////////////////////////////
// Factory

/*static*/
UniquePtr<SurfaceFactory_ANGLEShareHandle>
SurfaceFactory_ANGLEShareHandle::Create(GLContext& gl) {
  if (!gl.IsANGLE()) return nullptr;

  const auto& gle = *GLContextEGL::Cast(&gl);
  const auto& egl = gle.mEgl;

  if (!egl->IsExtensionSupported(
          EGLExtension::ANGLE_surface_d3d_texture_2d_share_handle)) {
    return nullptr;
  }

  if (XRE_IsContentProcess()) {
    gfxPlatform::GetPlatform()->EnsureDevicesInitialized();
  }

  gfx::DeviceManagerDx* dm = gfx::DeviceManagerDx::Get();
  MOZ_ASSERT(dm);
  if (gl.IsWARP() != dm->IsWARP() || !dm->TextureSharingWorks()) {
    return nullptr;
  }

  return AsUnique(new SurfaceFactory_ANGLEShareHandle(
      {&gl, SharedSurfaceType::EGLSurfaceANGLE, layers::TextureType::D3D11,
       true}));
}

} /* namespace gl */
} /* namespace mozilla */
