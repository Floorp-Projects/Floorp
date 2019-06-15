/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceEGL.h"

#include "GLBlitHelper.h"
#include "GLContextEGL.h"
#include "GLContextProvider.h"
#include "GLLibraryEGL.h"
#include "GLReadTexImageHelper.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "SharedSurface.h"

namespace mozilla {
namespace gl {

/*static*/
UniquePtr<SharedSurface_EGLImage> SharedSurface_EGLImage::Create(
    GLContext* prodGL, const GLFormats& formats, const gfx::IntSize& size,
    bool hasAlpha, EGLContext context) {
  const auto& gle = GLContextEGL::Cast(prodGL);
  const auto& egl = gle->mEgl;
  MOZ_ASSERT(egl);
  MOZ_ASSERT(context);

  UniquePtr<SharedSurface_EGLImage> ret;

  if (!HasExtensions(egl, prodGL)) {
    return ret;
  }

  MOZ_ALWAYS_TRUE(prodGL->MakeCurrent());
  GLuint prodTex = CreateTextureForOffscreen(prodGL, formats, size);
  if (!prodTex) {
    return ret;
  }

  EGLClientBuffer buffer =
      reinterpret_cast<EGLClientBuffer>(uintptr_t(prodTex));
  EGLImage image = egl->fCreateImage(egl->Display(), context,
                                     LOCAL_EGL_GL_TEXTURE_2D, buffer, nullptr);
  if (!image) {
    prodGL->fDeleteTextures(1, &prodTex);
    return ret;
  }

  ret.reset(new SharedSurface_EGLImage(prodGL, size, hasAlpha, formats, prodTex,
                                       image));
  return ret;
}

bool SharedSurface_EGLImage::HasExtensions(GLLibraryEGL* egl, GLContext* gl) {
  return egl->HasKHRImageBase() &&
         egl->IsExtensionSupported(GLLibraryEGL::KHR_gl_texture_2D_image) &&
         (gl->IsExtensionSupported(GLContext::OES_EGL_image_external) ||
          gl->IsExtensionSupported(GLContext::OES_EGL_image));
}

SharedSurface_EGLImage::SharedSurface_EGLImage(GLContext* gl,
                                               const gfx::IntSize& size,
                                               bool hasAlpha,
                                               const GLFormats& formats,
                                               GLuint prodTex, EGLImage image)
    : SharedSurface(
          SharedSurfaceType::EGLImageShare, AttachmentType::GLTexture, gl, size,
          hasAlpha,
          false)  // Can't recycle, as mSync changes never update TextureHost.
      ,
      mMutex("SharedSurface_EGLImage mutex"),
      mFormats(formats),
      mProdTex(prodTex),
      mImage(image),
      mSync(0) {}

SharedSurface_EGLImage::~SharedSurface_EGLImage() {
  const auto& gle = GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;
  egl->fDestroyImage(egl->Display(), mImage);

  if (mSync) {
    // We can't call this unless we have the ext, but we will always have
    // the ext if we have something to destroy.
    egl->fDestroySync(egl->Display(), mSync);
    mSync = 0;
  }

  if (!mGL || !mGL->MakeCurrent()) return;

  mGL->fDeleteTextures(1, &mProdTex);
  mProdTex = 0;
}

void SharedSurface_EGLImage::ProducerReleaseImpl() {
  const auto& gle = GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  MutexAutoLock lock(mMutex);
  mGL->MakeCurrent();

  if (egl->IsExtensionSupported(GLLibraryEGL::KHR_fence_sync) &&
      mGL->IsExtensionSupported(GLContext::OES_EGL_sync)) {
    if (mSync) {
      MOZ_RELEASE_ASSERT(false, "GFX: Non-recycleable should not Fence twice.");
      MOZ_ALWAYS_TRUE(egl->fDestroySync(egl->Display(), mSync));
      mSync = 0;
    }

    mSync = egl->fCreateSync(egl->Display(), LOCAL_EGL_SYNC_FENCE, nullptr);
    if (mSync) {
      mGL->fFlush();
      return;
    }
  }

  MOZ_ASSERT(!mSync);
  mGL->fFinish();
}

void SharedSurface_EGLImage::ProducerReadAcquireImpl() {
  const auto& gle = GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;
  // Wait on the fence, because presumably we're going to want to read this
  // surface
  if (mSync) {
    egl->fClientWaitSync(egl->Display(), mSync, 0, LOCAL_EGL_FOREVER);
  }
}

bool SharedSurface_EGLImage::ToSurfaceDescriptor(
    layers::SurfaceDescriptor* const out_descriptor) {
  *out_descriptor = layers::EGLImageDescriptor(
      (uintptr_t)mImage, (uintptr_t)mSync, mSize, mHasAlpha);
  return true;
}

bool SharedSurface_EGLImage::ReadbackBySharedHandle(
    gfx::DataSourceSurface* out_surface) {
  const auto& gle = GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;
  MOZ_ASSERT(out_surface);
  MOZ_ASSERT(NS_IsMainThread());
  return egl->ReadbackEGLImage(mImage, out_surface);
}

////////////////////////////////////////////////////////////////////////

/*static*/
UniquePtr<SurfaceFactory_EGLImage> SurfaceFactory_EGLImage::Create(
    GLContext* prodGL, const SurfaceCaps& caps,
    const RefPtr<layers::LayersIPCChannel>& allocator,
    const layers::TextureFlags& flags) {
  const auto& gle = GLContextEGL::Cast(prodGL);
  const auto& egl = gle->mEgl;
  const auto& context = gle->mContext;

  typedef SurfaceFactory_EGLImage ptrT;
  UniquePtr<ptrT> ret;

  if (SharedSurface_EGLImage::HasExtensions(egl, prodGL)) {
    ret.reset(new ptrT(prodGL, caps, allocator, flags, context));
  }

  return ret;
}

////////////////////////////////////////////////////////////////////////

#ifdef MOZ_WIDGET_ANDROID

/*static*/
UniquePtr<SharedSurface_SurfaceTexture> SharedSurface_SurfaceTexture::Create(
    GLContext* prodGL, const GLFormats& formats, const gfx::IntSize& size,
    bool hasAlpha, java::GeckoSurface::Param surface) {
  MOZ_ASSERT(surface);

  UniquePtr<SharedSurface_SurfaceTexture> ret;

  AndroidNativeWindow window(surface);
  const auto& gle = GLContextEGL::Cast(prodGL);
  MOZ_ASSERT(gle);
  EGLSurface eglSurface = gle->CreateCompatibleSurface(window.NativeWindow());
  if (!eglSurface) {
    return ret;
  }

  ret.reset(new SharedSurface_SurfaceTexture(prodGL, size, hasAlpha, formats,
                                             surface, eglSurface));
  return ret;
}

SharedSurface_SurfaceTexture::SharedSurface_SurfaceTexture(
    GLContext* gl, const gfx::IntSize& size, bool hasAlpha,
    const GLFormats& formats, java::GeckoSurface::Param surface,
    EGLSurface eglSurface)
    : SharedSurface(SharedSurfaceType::AndroidSurfaceTexture,
                    AttachmentType::Screen, gl, size, hasAlpha, true),
      mSurface(surface),
      mEglSurface(eglSurface) {}

SharedSurface_SurfaceTexture::~SharedSurface_SurfaceTexture() {
  GLContextProviderEGL::DestroyEGLSurface(mEglSurface);
  java::SurfaceAllocator::DisposeSurface(mSurface);
}

void SharedSurface_SurfaceTexture::LockProdImpl() {
  MOZ_RELEASE_ASSERT(mSurface->GetAvailable());

  GLContextEGL* gl = GLContextEGL::Cast(mGL);
  mOrigEglSurface = gl->GetEGLSurfaceOverride();
  gl->SetEGLSurfaceOverride(mEglSurface);
}

void SharedSurface_SurfaceTexture::UnlockProdImpl() {
  MOZ_RELEASE_ASSERT(mSurface->GetAvailable());

  GLContextEGL* gl = GLContextEGL::Cast(mGL);
  MOZ_ASSERT(gl->GetEGLSurfaceOverride() == mEglSurface);

  gl->SetEGLSurfaceOverride(mOrigEglSurface);
  mOrigEglSurface = nullptr;
}

void SharedSurface_SurfaceTexture::Commit() {
  MOZ_RELEASE_ASSERT(mSurface->GetAvailable());

  LockProdImpl();
  mGL->SwapBuffers();
  UnlockProdImpl();
  mSurface->SetAvailable(false);
}

void SharedSurface_SurfaceTexture::WaitForBufferOwnership() {
  mSurface->SetAvailable(true);
}

bool SharedSurface_SurfaceTexture::IsBufferAvailable() const {
  return mSurface->GetAvailable();
}

bool SharedSurface_SurfaceTexture::ToSurfaceDescriptor(
    layers::SurfaceDescriptor* const out_descriptor) {
  *out_descriptor = layers::SurfaceTextureDescriptor(
      mSurface->GetHandle(), mSize, gfx::SurfaceFormat::R8G8B8A8,
      false /* NOT continuous */, false /* Do not ignore transform */);
  return true;
}

////////////////////////////////////////////////////////////////////////

/*static*/
UniquePtr<SurfaceFactory_SurfaceTexture> SurfaceFactory_SurfaceTexture::Create(
    GLContext* prodGL, const SurfaceCaps& caps,
    const RefPtr<layers::LayersIPCChannel>& allocator,
    const layers::TextureFlags& flags) {
  UniquePtr<SurfaceFactory_SurfaceTexture> ret(
      new SurfaceFactory_SurfaceTexture(prodGL, caps, allocator, flags));
  return ret;
}

UniquePtr<SharedSurface> SurfaceFactory_SurfaceTexture::CreateShared(
    const gfx::IntSize& size) {
  bool hasAlpha = mReadCaps.alpha;

  jni::Object::LocalRef surface =
      java::SurfaceAllocator::AcquireSurface(size.width, size.height, true);
  if (!surface) {
    // Try multi-buffer mode
    surface =
        java::SurfaceAllocator::AcquireSurface(size.width, size.height, false);
    if (!surface) {
      // Give up
      NS_WARNING("Failed to allocate SurfaceTexture!");
      return nullptr;
    }
  }

  return SharedSurface_SurfaceTexture::Create(
      mGL, mFormats, size, hasAlpha, java::GeckoSurface::Ref::From(surface));
}

#endif  // MOZ_WIDGET_ANDROID

}  // namespace gl

} /* namespace mozilla */
