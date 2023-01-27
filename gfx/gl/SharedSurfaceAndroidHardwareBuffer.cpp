/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceAndroidHardwareBuffer.h"

#include "GLBlitHelper.h"
#include "GLContextEGL.h"
#include "GLContextProvider.h"
#include "GLLibraryEGL.h"
#include "GLReadTexImageHelper.h"
#include "MozFramebuffer.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/AndroidHardwareBuffer.h"
#include "ScopedGLHelpers.h"
#include "SharedSurface.h"

namespace mozilla {
namespace gl {

/*static*/
UniquePtr<SharedSurface_AndroidHardwareBuffer>
SharedSurface_AndroidHardwareBuffer::Create(const SharedSurfaceDesc& desc) {
  const auto& gle = GLContextEGL::Cast(desc.gl);
  const auto& egl = gle->mEgl;

  RefPtr<layers::AndroidHardwareBuffer> buffer =
      layers::AndroidHardwareBuffer::Create(desc.size,
                                            gfx::SurfaceFormat::R8G8B8A8);
  if (!buffer) {
    return nullptr;
  }

  const EGLint attrs[] = {
      LOCAL_EGL_IMAGE_PRESERVED,
      LOCAL_EGL_TRUE,
      LOCAL_EGL_NONE,
      LOCAL_EGL_NONE,
  };

  EGLClientBuffer clientBuffer =
      egl->mLib->fGetNativeClientBufferANDROID(buffer->GetNativeBuffer());
  const auto image = egl->fCreateImage(
      EGL_NO_CONTEXT, LOCAL_EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attrs);
  if (!image) {
    return nullptr;
  }

  auto tex = MakeUnique<Texture>(*desc.gl);
  {
    ScopedBindTexture texture(gle, tex->name, LOCAL_GL_TEXTURE_2D);
    gle->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER,
                        LOCAL_GL_LINEAR);
    gle->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER,
                        LOCAL_GL_LINEAR);
    gle->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S,
                        LOCAL_GL_CLAMP_TO_EDGE);
    gle->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T,
                        LOCAL_GL_CLAMP_TO_EDGE);
    gle->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, image);
    egl->fDestroyImage(image);
  }

  const GLenum target = LOCAL_GL_TEXTURE_2D;
  auto fb = MozFramebuffer::CreateForBacking(desc.gl, desc.size, 0, false,
                                             target, tex->name);
  if (!fb) {
    return nullptr;
  }

  return AsUnique(new SharedSurface_AndroidHardwareBuffer(
      desc, std::move(fb), std::move(tex), buffer));
}

SharedSurface_AndroidHardwareBuffer::SharedSurface_AndroidHardwareBuffer(
    const SharedSurfaceDesc& desc, UniquePtr<MozFramebuffer> fb,
    UniquePtr<Texture> tex, RefPtr<layers::AndroidHardwareBuffer> buffer)
    : SharedSurface(desc, std::move(fb)),
      mTex(std::move(tex)),
      mAndroidHardwareBuffer(buffer) {}

SharedSurface_AndroidHardwareBuffer::~SharedSurface_AndroidHardwareBuffer() {
  const auto& gl = mDesc.gl;
  if (!gl || !gl->MakeCurrent()) {
    return;
  }
  const auto& gle = GLContextEGL::Cast(gl);
  const auto& egl = gle->mEgl;

  if (mSync) {
    egl->fDestroySync(mSync);
    mSync = 0;
  }
}

void SharedSurface_AndroidHardwareBuffer::ProducerReleaseImpl() {
  const auto& gl = mDesc.gl;
  if (!gl || !gl->MakeCurrent()) {
    return;
  }
  const auto& gle = GLContextEGL::Cast(gl);
  const auto& egl = gle->mEgl;

  if (mSync) {
    MOZ_ALWAYS_TRUE(egl->fDestroySync(mSync));
    mSync = 0;
  }

  mSync = egl->fCreateSync(LOCAL_EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);
  MOZ_ASSERT(mSync);
  int rawFd = egl->fDupNativeFenceFDANDROID(mSync);
  if (rawFd >= 0) {
    auto fenceFd = ipc::FileDescriptor(UniqueFileHandle(rawFd));
    mAndroidHardwareBuffer->SetAcquireFence(std::move(fenceFd));
  }

  gl->fFlush();
}

Maybe<layers::SurfaceDescriptor>
SharedSurface_AndroidHardwareBuffer::ToSurfaceDescriptor() {
  return Some(layers::SurfaceDescriptorAndroidHardwareBuffer(
      mAndroidHardwareBuffer->mId, mAndroidHardwareBuffer->mSize,
      mAndroidHardwareBuffer->mFormat));
}

void SharedSurface_AndroidHardwareBuffer::WaitForBufferOwnership() {
  ipc::FileDescriptor fenceFd =
      mAndroidHardwareBuffer->GetAndResetReleaseFence();
  if (!fenceFd.IsValid()) {
    return;
  }

  const auto& gle = GLContextEGL::Cast(mDesc.gl);
  const auto& egl = gle->mEgl;

  auto rawFD = fenceFd.TakePlatformHandle();
  const EGLint attribs[] = {LOCAL_EGL_SYNC_NATIVE_FENCE_FD_ANDROID, rawFD.get(),
                            LOCAL_EGL_NONE};

  EGLSync sync = egl->fCreateSync(LOCAL_EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
  if (!sync) {
    gfxCriticalNote << "Failed to create EGLSync from fd";
    return;
  }
  // Release fd here, since it is owned by EGLSync
  Unused << rawFD.release();

  egl->fClientWaitSync(sync, 0, LOCAL_EGL_FOREVER);
  egl->fDestroySync(sync);
}

/*static*/
UniquePtr<SurfaceFactory_AndroidHardwareBuffer>
SurfaceFactory_AndroidHardwareBuffer::Create(GLContext& gl) {
  const auto partialDesc = PartialSharedSurfaceDesc{
      &gl,
      SharedSurfaceType::AndroidHardwareBuffer,
      layers::TextureType::AndroidHardwareBuffer,
      true,
  };
  return AsUnique(new SurfaceFactory_AndroidHardwareBuffer(partialDesc));
}

}  // namespace gl

} /* namespace mozilla */
