/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceANGLE.h"

#include <d3d11.h>
#include "gfxWindowsPlatform.h"
#include "GLContextEGL.h"
#include "GLLibraryEGL.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc

namespace mozilla {
namespace gl {

// Returns `EGL_NO_SURFACE` (`0`) on error.
static EGLSurface
CreatePBufferSurface(GLLibraryEGL* egl,
                     EGLDisplay display,
                     EGLConfig config,
                     const gfx::IntSize& size)
{
    auto width = size.width;
    auto height = size.height;

    EGLint attribs[] = {
        LOCAL_EGL_WIDTH, width,
        LOCAL_EGL_HEIGHT, height,
        LOCAL_EGL_NONE
    };

    DebugOnly<EGLint> preCallErr = egl->fGetError();
    MOZ_ASSERT(preCallErr == LOCAL_EGL_SUCCESS);
    EGLSurface surface = egl->fCreatePbufferSurface(display, config, attribs);
    EGLint err = egl->fGetError();
    if (err != LOCAL_EGL_SUCCESS)
        return 0;

    return surface;
}

/*static*/ UniquePtr<SharedSurface_ANGLEShareHandle>
SharedSurface_ANGLEShareHandle::Create(GLContext* gl, EGLConfig config,
                                       const gfx::IntSize& size, bool hasAlpha)
{
    GLLibraryEGL* egl = &sEGLLibrary;
    MOZ_ASSERT(egl);
    MOZ_ASSERT(egl->IsExtensionSupported(
               GLLibraryEGL::ANGLE_surface_d3d_texture_2d_share_handle));
    MOZ_ASSERT(config);

    EGLDisplay display = egl->Display();
    EGLSurface pbuffer = CreatePBufferSurface(egl, display, config, size);
    if (!pbuffer)
        return nullptr;

    // Declare everything before 'goto's.
    HANDLE shareHandle = nullptr;
    bool ok = egl->fQuerySurfacePointerANGLE(display,
                                             pbuffer,
                                             LOCAL_EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
                                             &shareHandle);
    if (!ok) {
        egl->fDestroySurface(egl->Display(), pbuffer);
        return nullptr;
    }
    void* opaqueKeyedMutex = nullptr;
    egl->fQuerySurfacePointerANGLE(display,
                                   pbuffer,
                                   LOCAL_EGL_DXGI_KEYED_MUTEX_ANGLE,
                                   &opaqueKeyedMutex);
    RefPtr<IDXGIKeyedMutex> keyedMutex = static_cast<IDXGIKeyedMutex*>(opaqueKeyedMutex);

    GLuint fence = 0;
    if (gl->IsExtensionSupported(GLContext::NV_fence)) {
        gl->MakeCurrent();
        gl->fGenFences(1, &fence);
    }

    typedef SharedSurface_ANGLEShareHandle ptrT;
    UniquePtr<ptrT> ret( new ptrT(gl, egl, size, hasAlpha, pbuffer, shareHandle,
                                  keyedMutex, fence) );
    return Move(ret);
}

EGLDisplay
SharedSurface_ANGLEShareHandle::Display()
{
    return mEGL->Display();
}

SharedSurface_ANGLEShareHandle::SharedSurface_ANGLEShareHandle(GLContext* gl,
                                                               GLLibraryEGL* egl,
                                                               const gfx::IntSize& size,
                                                               bool hasAlpha,
                                                               EGLSurface pbuffer,
                                                               HANDLE shareHandle,
                                                               const RefPtr<IDXGIKeyedMutex>& keyedMutex,
                                                               GLuint fence)
    : SharedSurface(SharedSurfaceType::EGLSurfaceANGLE,
                    AttachmentType::Screen,
                    gl,
                    size,
                    hasAlpha,
                    true)
    , mEGL(egl)
    , mPBuffer(pbuffer)
    , mShareHandle(shareHandle)
    , mKeyedMutex(keyedMutex)
    , mFence(fence)
{
}


SharedSurface_ANGLEShareHandle::~SharedSurface_ANGLEShareHandle()
{
    mEGL->fDestroySurface(Display(), mPBuffer);

    if (mFence) {
        mGL->MakeCurrent();
        mGL->fDeleteFences(1, &mFence);
    }
}

void
SharedSurface_ANGLEShareHandle::LockProdImpl()
{
    GLContextEGL::Cast(mGL)->SetEGLSurfaceOverride(mPBuffer);
}

void
SharedSurface_ANGLEShareHandle::UnlockProdImpl()
{
}

void
SharedSurface_ANGLEShareHandle::Fence()
{
    mGL->fFinish();
}

bool
SharedSurface_ANGLEShareHandle::WaitSync()
{
    return true;
}

bool
SharedSurface_ANGLEShareHandle::PollSync()
{
    return true;
}

void
SharedSurface_ANGLEShareHandle::ProducerAcquireImpl()
{
    if (mKeyedMutex) {
        HRESULT hr = mKeyedMutex->AcquireSync(0, 10000);
        if (hr == WAIT_TIMEOUT) {
            MOZ_CRASH();
        }
    }
}

void
SharedSurface_ANGLEShareHandle::ProducerReleaseImpl()
{
    if (mKeyedMutex) {
        // XXX: ReleaseSync() has an implicit flush of the D3D commands
        // whether we need Flush() or not depends on the ANGLE semantics.
        // For now, we'll just do it
        mGL->fFlush();
        mKeyedMutex->ReleaseSync(0);
        return;
    }
    Fence();
}

void
SharedSurface_ANGLEShareHandle::ProducerReadAcquireImpl()
{
    ProducerAcquireImpl();
}

void
SharedSurface_ANGLEShareHandle::ProducerReadReleaseImpl()
{
    if (mKeyedMutex) {
        mKeyedMutex->ReleaseSync(0);
        return;
    }
}

void
SharedSurface_ANGLEShareHandle::ConsumerAcquireImpl()
{
    if (!mConsumerTexture) {
        RefPtr<ID3D11Texture2D> tex;
        HRESULT hr = gfxWindowsPlatform::GetPlatform()->GetD3D11Device()->OpenSharedResource(mShareHandle,
                                                                                             __uuidof(ID3D11Texture2D),
                                                                                             (void**)(ID3D11Texture2D**)getter_AddRefs(tex));
        if (SUCCEEDED(hr)) {
            mConsumerTexture = tex;
            RefPtr<IDXGIKeyedMutex> mutex;
            hr = tex->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));

            if (SUCCEEDED(hr)) {
                mConsumerKeyedMutex = mutex;
            }
        }
    }

    if (mConsumerKeyedMutex) {
      HRESULT hr = mConsumerKeyedMutex->AcquireSync(0, 10000);
      if (hr == WAIT_TIMEOUT) {
        MOZ_CRASH();
      }
    }
}

void
SharedSurface_ANGLEShareHandle::ConsumerReleaseImpl()
{
    if (mConsumerKeyedMutex) {
        mConsumerKeyedMutex->ReleaseSync(0);
    }
}

void
SharedSurface_ANGLEShareHandle::Fence_ContentThread_Impl()
{
    if (mFence) {
        MOZ_ASSERT(mGL->IsExtensionSupported(GLContext::NV_fence));
        mGL->fSetFence(mFence, LOCAL_GL_ALL_COMPLETED_NV);
        mGL->fFlush();
        return;
    }

    Fence();
}

bool
SharedSurface_ANGLEShareHandle::WaitSync_ContentThread_Impl()
{
    if (mFence) {
        mGL->MakeCurrent();
        mGL->fFinishFence(mFence);
        return true;
    }

    return WaitSync();
}

bool
SharedSurface_ANGLEShareHandle::PollSync_ContentThread_Impl()
{
    if (mFence) {
        mGL->MakeCurrent();
        return mGL->fTestFence(mFence);
    }

    return PollSync();
}

bool
SharedSurface_ANGLEShareHandle::ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor)
{
    gfx::SurfaceFormat format = mHasAlpha ? gfx::SurfaceFormat::B8G8R8A8
                                          : gfx::SurfaceFormat::B8G8R8X8;
    *out_descriptor = layers::SurfaceDescriptorD3D10((WindowsHandle)mShareHandle, format,
                                                     mSize);
    return true;
}

class ScopedLockTexture final
{
public:
    explicit ScopedLockTexture(ID3D11Texture2D* texture, bool* succeeded)
      : mIsLocked(false)
      , mTexture(texture)
    {
        MOZ_ASSERT(mTexture);
        MOZ_ASSERT(succeeded);
        *succeeded = false;

        HRESULT hr;
        mTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mMutex));
        if (mMutex) {
            hr = mMutex->AcquireSync(0, 10000);
            if (hr == WAIT_TIMEOUT) {
                MOZ_CRASH();
            }

            if (FAILED(hr)) {
                NS_WARNING("Failed to lock the texture");
                return;
            }
        }

        ID3D11Device* device = gfxWindowsPlatform::GetPlatform()->GetD3D11Device();
        device->GetImmediateContext(getter_AddRefs(mDeviceContext));

        mTexture->GetDesc(&mDesc);
        mDesc.BindFlags = 0;
        mDesc.Usage = D3D11_USAGE_STAGING;
        mDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        mDesc.MiscFlags = 0;

        hr = device->CreateTexture2D(&mDesc, nullptr, getter_AddRefs(mCopiedTexture));

        if (FAILED(hr)) {
            return;
        }

        mDeviceContext->CopyResource(mCopiedTexture, mTexture);

        hr = mDeviceContext->Map(mCopiedTexture, 0, D3D11_MAP_READ, 0, &mSubresource);
        if (FAILED(hr)) {
            return;
        }

        *succeeded = true;
        mIsLocked = true;
    }

    ~ScopedLockTexture()
    {
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

bool
SharedSurface_ANGLEShareHandle::ReadbackBySharedHandle(gfx::DataSourceSurface* out_surface)
{
    MOZ_ASSERT(out_surface);
    RefPtr<ID3D11Texture2D> tex;
    ID3D11Device* device = gfxWindowsPlatform::GetPlatform()->GetD3D11Device();
    HRESULT hr = device->OpenSharedResource(mShareHandle,
                                            __uuidof(ID3D11Texture2D),
                                            (void**)(ID3D11Texture2D**)getter_AddRefs(tex));

    if (FAILED(hr)) {
        return false;
    }

    bool succeeded = false;
    ScopedLockTexture scopedLock(tex, &succeeded);
    if (!succeeded) {
        return false;
    }

    const uint8_t* data = reinterpret_cast<uint8_t*>(scopedLock.mSubresource.pData);
    uint32_t srcStride = scopedLock.mSubresource.RowPitch;

    gfx::DataSourceSurface::ScopedMap map(out_surface, gfx::DataSourceSurface::WRITE);
    if (!map.IsMapped()) {
        return false;
    }

    if (map.GetStride() == srcStride) {
        memcpy(map.GetData(), data, out_surface->GetSize().height * map.GetStride());
    } else {
        const uint8_t bytesPerPixel = BytesPerPixel(out_surface->GetFormat());
        for (int32_t i = 0; i < out_surface->GetSize().height; i++) {
            memcpy(map.GetData() + i * map.GetStride(),
                   data + i * srcStride,
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

/*static*/ UniquePtr<SurfaceFactory_ANGLEShareHandle>
SurfaceFactory_ANGLEShareHandle::Create(GLContext* gl, const SurfaceCaps& caps,
                                        const RefPtr<layers::ISurfaceAllocator>& allocator,
                                        const layers::TextureFlags& flags)
{
    GLLibraryEGL* egl = &sEGLLibrary;
    if (!egl)
        return nullptr;

    auto ext = GLLibraryEGL::ANGLE_surface_d3d_texture_2d_share_handle;
    if (!egl->IsExtensionSupported(ext))
        return nullptr;

    EGLConfig config = GLContextEGL::Cast(gl)->mConfig;

    typedef SurfaceFactory_ANGLEShareHandle ptrT;
    UniquePtr<ptrT> ret( new ptrT(gl, caps, allocator, flags, egl, config) );
    return Move(ret);
}

SurfaceFactory_ANGLEShareHandle::SurfaceFactory_ANGLEShareHandle(GLContext* gl,
                                                                 const SurfaceCaps& caps,
                                                                 const RefPtr<layers::ISurfaceAllocator>& allocator,
                                                                 const layers::TextureFlags& flags,
                                                                 GLLibraryEGL* egl,
                                                                 EGLConfig config)
    : SurfaceFactory(SharedSurfaceType::EGLSurfaceANGLE, gl, caps, allocator, flags)
    , mProdGL(gl)
    , mEGL(egl)
    , mConfig(config)
{ }

} /* namespace gl */
} /* namespace mozilla */
