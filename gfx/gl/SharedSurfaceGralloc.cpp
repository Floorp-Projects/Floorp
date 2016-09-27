/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Preferences.h"
#include "mozilla/UniquePtr.h"

#include "SharedSurfaceGralloc.h"

#include "GLContext.h"
#include "SharedSurface.h"
#include "GLLibraryEGL.h"
#include "mozilla/layers/GrallocTextureClient.h"
#include "mozilla/layers/ShadowLayers.h"

#include "ui/GraphicBuffer.h"
#include "../layers/ipc/ShadowLayers.h"
#include "ScopedGLHelpers.h"

#include "gfxPlatform.h"
#include "gfxPrefs.h"

#define DEBUG_GRALLOC
#ifdef DEBUG_GRALLOC
#define DEBUG_PRINT(...) do { printf_stderr(__VA_ARGS__); } while (0)
#else
#define DEBUG_PRINT(...) do { } while (0)
#endif

namespace mozilla {
namespace gl {

using namespace mozilla::layers;
using namespace android;

SurfaceFactory_Gralloc::SurfaceFactory_Gralloc(GLContext* prodGL, const SurfaceCaps& caps,
                                               const RefPtr<layers::LayersIPCChannel>& allocator,
                                               const layers::TextureFlags& flags)
    : SurfaceFactory(SharedSurfaceType::Gralloc, prodGL, caps, allocator, flags)
{
    MOZ_ASSERT(mAllocator);
}

/*static*/ UniquePtr<SharedSurface_Gralloc>
SharedSurface_Gralloc::Create(GLContext* prodGL,
                              const GLFormats& formats,
                              const gfx::IntSize& size,
                              bool hasAlpha,
                              layers::TextureFlags flags,
                              LayersIPCChannel* allocator)
{
    GLLibraryEGL* egl = &sEGLLibrary;
    MOZ_ASSERT(egl);

    UniquePtr<SharedSurface_Gralloc> ret;

    DEBUG_PRINT("SharedSurface_Gralloc::Create -------\n");

    if (!HasExtensions(egl, prodGL))
        return Move(ret);

    gfxContentType type = hasAlpha ? gfxContentType::COLOR_ALPHA
                                   : gfxContentType::COLOR;

    GrallocTextureData* texData = GrallocTextureData::CreateForGLRendering(
        size, gfxPlatform::GetPlatform()->Optimal2DFormatForContent(type), allocator
    );

    if (!texData) {
        return Move(ret);
    }

    RefPtr<TextureClient> grallocTC = new TextureClient(texData, flags, allocator);

    sp<GraphicBuffer> buffer = texData->GetGraphicBuffer();

    EGLDisplay display = egl->Display();
    EGLClientBuffer clientBuffer = buffer->getNativeBuffer();
    EGLint attrs[] = {
        LOCAL_EGL_NONE, LOCAL_EGL_NONE
    };
    EGLImage image = egl->fCreateImage(display,
                                       EGL_NO_CONTEXT,
                                       LOCAL_EGL_NATIVE_BUFFER_ANDROID,
                                       clientBuffer, attrs);
    if (!image) {
        return Move(ret);
    }

    prodGL->MakeCurrent();
    GLuint prodTex = 0;
    prodGL->fGenTextures(1, &prodTex);
    ScopedBindTexture autoTex(prodGL, prodTex);

    prodGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
    prodGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
    prodGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    prodGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

    prodGL->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, image);

    egl->fDestroyImage(display, image);

    ret.reset( new SharedSurface_Gralloc(prodGL, size, hasAlpha, egl,
                                         allocator, grallocTC,
                                         prodTex) );

    DEBUG_PRINT("SharedSurface_Gralloc::Create: success -- surface %p,"
                " GraphicBuffer %p.\n",
                ret.get(), buffer.get());

    return Move(ret);
}


SharedSurface_Gralloc::SharedSurface_Gralloc(GLContext* prodGL,
                                             const gfx::IntSize& size,
                                             bool hasAlpha,
                                             GLLibraryEGL* egl,
                                             layers::LayersIPCChannel* allocator,
                                             layers::TextureClient* textureClient,
                                             GLuint prodTex)
    : SharedSurface(SharedSurfaceType::Gralloc,
                    AttachmentType::GLTexture,
                    prodGL,
                    size,
                    hasAlpha,
                    true)
    , mEGL(egl)
    , mSync(0)
    , mAllocator(allocator)
    , mTextureClient(textureClient)
    , mProdTex(prodTex)
{
}

bool
SharedSurface_Gralloc::HasExtensions(GLLibraryEGL* egl, GLContext* gl)
{
    return egl->HasKHRImageBase() &&
           gl->IsExtensionSupported(GLContext::OES_EGL_image);
}

SharedSurface_Gralloc::~SharedSurface_Gralloc()
{
    DEBUG_PRINT("[SharedSurface_Gralloc %p] destroyed\n", this);

    if (!mGL->MakeCurrent())
        return;

    mGL->fDeleteTextures(1, &mProdTex);

    if (mSync) {
        MOZ_ALWAYS_TRUE( mEGL->fDestroySync(mEGL->Display(), mSync) );
        mSync = 0;
    }
}

void
SharedSurface_Gralloc::ProducerReleaseImpl()
{
    if (mSync) {
        MOZ_ALWAYS_TRUE( mEGL->fDestroySync(mEGL->Display(), mSync) );
        mSync = 0;
    }

    bool disableSyncFence = false;
    // Disable sync fence on AdrenoTM200.
    // AdrenoTM200's sync fence does not work correctly. See Bug 1022205.
    if (mGL->Renderer() == GLRenderer::AdrenoTM200) {
        disableSyncFence = true;
    }

    // When Android native fences are available, try
    // them first since they're more likely to work.
    // Android native fences are also likely to perform better.
    if (!disableSyncFence &&
        mEGL->IsExtensionSupported(GLLibraryEGL::ANDROID_native_fence_sync))
    {
        mGL->MakeCurrent();
        EGLSync sync = mEGL->fCreateSync(mEGL->Display(),
                                         LOCAL_EGL_SYNC_NATIVE_FENCE_ANDROID,
                                         nullptr);
        if (sync) {
            mGL->fFlush();
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
            int fenceFd = mEGL->fDupNativeFenceFDANDROID(mEGL->Display(), sync);
            if (fenceFd != -1) {
                mEGL->fDestroySync(mEGL->Display(), sync);
                mTextureClient->SetAcquireFenceHandle(FenceHandle(new FenceHandle::FdObj(fenceFd)));
            } else {
                mSync = sync;
            }
#else
            mSync = sync;
#endif
            return;
        }
    }

    if (!disableSyncFence &&
        mEGL->IsExtensionSupported(GLLibraryEGL::KHR_fence_sync))
    {
        mGL->MakeCurrent();
        mSync = mEGL->fCreateSync(mEGL->Display(),
                                  LOCAL_EGL_SYNC_FENCE,
                                  nullptr);
        if (mSync) {
            mGL->fFlush();
            return;
        }
    }

    // We should be able to rely on genlock write locks/read locks.
    // But they're broken on some configs, and even a glFinish doesn't
    // work.  glReadPixels seems to, though.
    if (gfxPrefs::GrallocFenceWithReadPixels()) {
        mGL->MakeCurrent();
        UniquePtr<char[]> buf = MakeUnique<char[]>(4);
        mGL->fReadPixels(0, 0, 1, 1, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, buf.get());
    }
}

void
SharedSurface_Gralloc::WaitForBufferOwnership()
{
    mTextureClient->WaitForBufferOwnership();
}

bool
SharedSurface_Gralloc::ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor)
{
    mTextureClient->mWorkaroundAnnoyingSharedSurfaceOwnershipIssues = true;
    return mTextureClient->ToSurfaceDescriptor(*out_descriptor);
}

bool
SharedSurface_Gralloc::ReadbackBySharedHandle(gfx::DataSourceSurface* out_surface)
{
    MOZ_ASSERT(out_surface);
    sp<GraphicBuffer> buffer = static_cast<GrallocTextureData*>(
        mTextureClient->GetInternalData()
    )->GetGraphicBuffer();

    const uint8_t* grallocData = nullptr;
    auto result = buffer->lock(
        GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_NEVER,
        const_cast<void**>(reinterpret_cast<const void**>(&grallocData))
    );

    if (result == BAD_VALUE) {
        return false;
    }

    gfx::DataSourceSurface::ScopedMap map(out_surface, gfx::DataSourceSurface::WRITE);
    if (!map.IsMapped()) {
        buffer->unlock();
        return false;
    }

    uint32_t stride = buffer->getStride() * android::bytesPerPixel(buffer->getPixelFormat());
    uint32_t height = buffer->getHeight();
    uint32_t width = buffer->getWidth();
    for (uint32_t i = 0; i < height; i++) {
        memcpy(map.GetData() + i * map.GetStride(),
               grallocData + i * stride, width * 4);
    }

    buffer->unlock();

    android::PixelFormat srcFormat = buffer->getPixelFormat();
    MOZ_ASSERT(srcFormat == PIXEL_FORMAT_RGBA_8888 ||
               srcFormat == PIXEL_FORMAT_BGRA_8888 ||
               srcFormat == PIXEL_FORMAT_RGBX_8888);
    bool isSrcRGB = srcFormat == PIXEL_FORMAT_RGBA_8888 ||
                    srcFormat == PIXEL_FORMAT_RGBX_8888;

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

} // namespace gl
} // namespace mozilla
