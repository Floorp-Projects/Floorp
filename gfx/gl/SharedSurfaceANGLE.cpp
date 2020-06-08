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
static EGLSurface CreatePBufferSurface(GLLibraryEGL* egl, EGLDisplay display,
                                       EGLConfig config,
                                       const gfx::IntSize& size) {
  const EGLint attribs[] = {LOCAL_EGL_WIDTH, size.width, LOCAL_EGL_HEIGHT,
                            size.height, LOCAL_EGL_NONE};

  DebugOnly<EGLint> preCallErr = egl->fGetError();
  MOZ_ASSERT(preCallErr == LOCAL_EGL_SUCCESS);
  EGLSurface surface = egl->fCreatePbufferSurface(display, config, attribs);
  EGLint err = egl->fGetError();
  if (err != LOCAL_EGL_SUCCESS) {
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
      GLLibraryEGL::ANGLE_surface_d3d_texture_2d_share_handle));

  const auto& config = gle->mConfig;
  MOZ_ASSERT(config);

  EGLDisplay display = egl->Display();
  EGLSurface pbuffer = CreatePBufferSurface(egl, display, config, desc.size);
  if (!pbuffer) return nullptr;

  // Declare everything before 'goto's.
  HANDLE shareHandle = nullptr;
  bool ok = egl->fQuerySurfacePointerANGLE(
      display, pbuffer, LOCAL_EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
      &shareHandle);
  if (!ok) {
    egl->fDestroySurface(egl->Display(), pbuffer);
    return nullptr;
  }
  void* opaqueKeyedMutex = nullptr;
  egl->fQuerySurfacePointerANGLE(
      display, pbuffer, LOCAL_EGL_DXGI_KEYED_MUTEX_ANGLE, &opaqueKeyedMutex);
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

EGLDisplay SharedSurface_ANGLEShareHandle::Display() const {
  return mEGL->Display();
}

SharedSurface_ANGLEShareHandle::SharedSurface_ANGLEShareHandle(
    const SharedSurfaceDesc& desc, GLLibraryEGL* egl, EGLSurface pbuffer,
    HANDLE shareHandle, const RefPtr<IDXGIKeyedMutex>& keyedMutex)
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
  mEGL->fDestroySurface(Display(), mPBuffer);
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
      (WindowsHandle)mShareHandle, format, mDesc.size,
      gfx::YUVColorSpace::UNKNOWN, gfx::ColorRange::FULL));
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

bool SharedSurface_ANGLEShareHandle::ReadbackBySharedHandle(
    gfx::DataSourceSurface* out_surface) {
  MOZ_ASSERT(out_surface);

  RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetContentDevice();
  if (!device) {
    return false;
  }

  RefPtr<ID3D11Texture2D> tex;
  HRESULT hr = device->OpenSharedResource(
      mShareHandle, __uuidof(ID3D11Texture2D),
      (void**)(ID3D11Texture2D**)getter_AddRefs(tex));

  if (FAILED(hr)) {
    return false;
  }

  bool succeeded = false;
  ScopedLockTexture scopedLock(tex, &succeeded);
  if (!succeeded) {
    return false;
  }

  const uint8_t* data =
      reinterpret_cast<uint8_t*>(scopedLock.mSubresource.pData);
  uint32_t srcStride = scopedLock.mSubresource.RowPitch;

  gfx::DataSourceSurface::ScopedMap map(out_surface,
                                        gfx::DataSourceSurface::WRITE);
  if (!map.IsMapped()) {
    return false;
  }

  if (map.GetStride() == srcStride) {
    memcpy(map.GetData(), data,
           out_surface->GetSize().height * map.GetStride());
  } else {
    const uint8_t bytesPerPixel = BytesPerPixel(out_surface->GetFormat());
    for (int32_t i = 0; i < out_surface->GetSize().height; i++) {
      memcpy(map.GetData() + i * map.GetStride(), data + i * srcStride,
             bytesPerPixel * out_surface->GetSize().width);
    }
  }

  DXGI_FORMAT srcFormat = scopedLock.mDesc.Format;
  MOZ_ASSERT(srcFormat == DXGI_FORMAT_B8G8R8A8_UNORM ||
             srcFormat == DXGI_FORMAT_B8G8R8X8_UNORM ||
             srcFormat == DXGI_FORMAT_R8G8B8A8_UNORM);
  bool isSrcRGB = srcFormat == DXGI_FORMAT_R8G8B8A8_UNORM;

  gfx::SurfaceFormat destFormat = out_surface->GetFormat();
  MOZ_ASSERT(destFormat == gfx::SurfaceFormat::R8G8B8X8 ||
             destFormat == gfx::SurfaceFormat::R8G8B8A8 ||
             destFormat == gfx::SurfaceFormat::B8G8R8X8 ||
             destFormat == gfx::SurfaceFormat::B8G8R8A8);
  bool isDestRGB = destFormat == gfx::SurfaceFormat::R8G8B8X8 ||
                   destFormat == gfx::SurfaceFormat::R8G8B8A8;

  if (isSrcRGB != isDestRGB) {
    SwapRAndBComponents(out_surface);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Factory

/*static*/
UniquePtr<SurfaceFactory_ANGLEShareHandle>
SurfaceFactory_ANGLEShareHandle::Create(GLContext& gl) {
  if (!gl.IsANGLE()) return nullptr;

  const auto& gle = *GLContextEGL::Cast(&gl);
  const auto& egl = gle.mEgl;

  auto ext = GLLibraryEGL::ANGLE_surface_d3d_texture_2d_share_handle;
  if (!egl->IsExtensionSupported(ext)) return nullptr;

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
