/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderAndroidHardwareBufferTextureHost.h"

#include "mozilla/layers/AndroidHardwareBuffer.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/gfx/2D.h"
#include "GLContextEGL.h"
#include "GLLibraryEGL.h"
#include "GLReadTexImageHelper.h"
#include "OGLShaderConfig.h"

namespace mozilla {
namespace wr {

RenderAndroidHardwareBufferTextureHost::RenderAndroidHardwareBufferTextureHost(
    layers::AndroidHardwareBuffer* aAndroidHardwareBuffer)
    : mAndroidHardwareBuffer(aAndroidHardwareBuffer),
      mEGLImage(EGL_NO_IMAGE),
      mTextureHandle(0) {
  MOZ_ASSERT(mAndroidHardwareBuffer);
  MOZ_COUNT_CTOR_INHERITED(RenderAndroidHardwareBufferTextureHost,
                           RenderTextureHost);
}

RenderAndroidHardwareBufferTextureHost::
    ~RenderAndroidHardwareBufferTextureHost() {
  MOZ_COUNT_DTOR_INHERITED(RenderAndroidHardwareBufferTextureHost,
                           RenderTextureHost);
  DeleteTextureHandle();
  DestroyEGLImage();
}

gfx::IntSize RenderAndroidHardwareBufferTextureHost::GetSize() const {
  if (mAndroidHardwareBuffer) {
    return mAndroidHardwareBuffer->mSize;
  }
  return gfx::IntSize();
}

bool RenderAndroidHardwareBufferTextureHost::EnsureLockable() {
  if (!mAndroidHardwareBuffer) {
    return false;
  }

  auto fenceFd = mAndroidHardwareBuffer->GetAndResetAcquireFence();
  if (fenceFd.IsValid()) {
    const auto& gle = gl::GLContextEGL::Cast(mGL);
    const auto& egl = gle->mEgl;

    auto rawFD = fenceFd.TakePlatformHandle();
    const EGLint attribs[] = {LOCAL_EGL_SYNC_NATIVE_FENCE_FD_ANDROID,
                              rawFD.get(), LOCAL_EGL_NONE};

    EGLSync sync =
        egl->fCreateSync(LOCAL_EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
    if (sync) {
      // Release fd here, since it is owned by EGLSync
      Unused << rawFD.release();

      if (egl->IsExtensionSupported(gl::EGLExtension::KHR_wait_sync)) {
        egl->fWaitSync(sync, 0);
      } else {
        egl->fClientWaitSync(sync, 0, LOCAL_EGL_FOREVER);
      }
      egl->fDestroySync(sync);
    } else {
      gfxCriticalNote << "Failed to create EGLSync from acquire fence fd";
    }
  }

  if (mTextureHandle) {
    return true;
  }

  if (!mEGLImage) {
    // XXX add crop handling for video
    // Should only happen the first time.
    const auto& gle = gl::GLContextEGL::Cast(mGL);
    const auto& egl = gle->mEgl;

    const EGLint attrs[] = {
        LOCAL_EGL_IMAGE_PRESERVED,
        LOCAL_EGL_TRUE,
        LOCAL_EGL_NONE,
        LOCAL_EGL_NONE,
    };

    EGLClientBuffer clientBuffer = egl->mLib->fGetNativeClientBufferANDROID(
        mAndroidHardwareBuffer->GetNativeBuffer());
    mEGLImage = egl->fCreateImage(
        EGL_NO_CONTEXT, LOCAL_EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attrs);
  }
  MOZ_ASSERT(mEGLImage);

  mGL->fGenTextures(1, &mTextureHandle);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, mTextureHandle);
  mGL->fTexParameteri(LOCAL_GL_TEXTURE_EXTERNAL, LOCAL_GL_TEXTURE_WRAP_T,
                      LOCAL_GL_CLAMP_TO_EDGE);
  mGL->fTexParameteri(LOCAL_GL_TEXTURE_EXTERNAL, LOCAL_GL_TEXTURE_WRAP_S,
                      LOCAL_GL_CLAMP_TO_EDGE);
  mGL->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_EXTERNAL, mEGLImage);

  ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                               LOCAL_GL_TEXTURE_EXTERNAL_OES, mTextureHandle);
  return true;
}

wr::WrExternalImage RenderAndroidHardwareBufferTextureHost::Lock(
    uint8_t aChannelIndex, gl::GLContext* aGL) {
  MOZ_ASSERT(aChannelIndex == 0);

  if (mGL.get() != aGL) {
    if (mGL) {
      // This should not happen.
      MOZ_ASSERT_UNREACHABLE("Unexpected GL context");
      return InvalidToWrExternalImage();
    }
    mGL = aGL;
  }

  if (!mGL || !mGL->MakeCurrent()) {
    return InvalidToWrExternalImage();
  }

  if (!EnsureLockable()) {
    return InvalidToWrExternalImage();
  }

  const auto uvs = GetUvCoords(GetSize());
  return NativeTextureToWrExternalImage(
      mTextureHandle, uvs.first.x, uvs.first.y, uvs.second.x, uvs.second.y);
}

void RenderAndroidHardwareBufferTextureHost::Unlock() {}

size_t RenderAndroidHardwareBufferTextureHost::Bytes() {
  return GetSize().width * GetSize().height *
         BytesPerPixel(mAndroidHardwareBuffer->mFormat);
}

void RenderAndroidHardwareBufferTextureHost::DeleteTextureHandle() {
  if (!mTextureHandle) {
    return;
  }
  MOZ_ASSERT(mGL);
  mGL->fDeleteTextures(1, &mTextureHandle);
  mTextureHandle = 0;
}

void RenderAndroidHardwareBufferTextureHost::DestroyEGLImage() {
  if (!mEGLImage) {
    return;
  }
  MOZ_ASSERT(mGL);
  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;
  egl->fDestroyImage(mEGLImage);
  mEGLImage = EGL_NO_IMAGE;
}

gfx::SurfaceFormat RenderAndroidHardwareBufferTextureHost::GetFormat() const {
  MOZ_ASSERT(mAndroidHardwareBuffer->mFormat == gfx::SurfaceFormat::R8G8B8A8 ||
             mAndroidHardwareBuffer->mFormat == gfx::SurfaceFormat::R8G8B8X8);

  if (mAndroidHardwareBuffer->mFormat == gfx::SurfaceFormat::R8G8B8A8) {
    return gfx::SurfaceFormat::B8G8R8A8;
  }

  if (mAndroidHardwareBuffer->mFormat == gfx::SurfaceFormat::R8G8B8X8) {
    return gfx::SurfaceFormat::B8G8R8X8;
  }

  gfxCriticalNoteOnce
      << "Unexpected color format of RenderAndroidSurfaceTextureHost";

  return gfx::SurfaceFormat::UNKNOWN;
}

already_AddRefed<gfx::DataSourceSurface>
RenderAndroidHardwareBufferTextureHost::ReadTexImage() {
  if (!mGL) {
    mGL = RenderThread::Get()->SingletonGL();
    if (!mGL) {
      return nullptr;
    }
  }

  if (!EnsureLockable()) {
    return nullptr;
  }

  /* Allocate resulting image surface */
  int32_t stride = GetSize().width * BytesPerPixel(GetFormat());
  RefPtr<gfx::DataSourceSurface> surf =
      gfx::Factory::CreateDataSourceSurfaceWithStride(GetSize(), GetFormat(),
                                                      stride);
  if (!surf) {
    return nullptr;
  }

  layers::ShaderConfigOGL config = layers::ShaderConfigFromTargetAndFormat(
      LOCAL_GL_TEXTURE_EXTERNAL, mAndroidHardwareBuffer->mFormat);
  int shaderConfig = config.mFeatures;

  bool ret = mGL->ReadTexImageHelper()->ReadTexImage(
      surf, mTextureHandle, LOCAL_GL_TEXTURE_EXTERNAL, GetSize(), shaderConfig,
      /* aYInvert */ false);
  if (!ret) {
    return nullptr;
  }

  return surf.forget();
}

bool RenderAndroidHardwareBufferTextureHost::MapPlane(
    RenderCompositor* aCompositor, uint8_t aChannelIndex,
    PlaneInfo& aPlaneInfo) {
  RefPtr<gfx::DataSourceSurface> readback = ReadTexImage();
  if (!readback) {
    return false;
  }

  gfx::DataSourceSurface::MappedSurface map;
  if (!readback->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
    return false;
  }

  mReadback = readback;
  aPlaneInfo.mSize = GetSize();
  aPlaneInfo.mStride = map.mStride;
  aPlaneInfo.mData = map.mData;
  return true;
}

void RenderAndroidHardwareBufferTextureHost::UnmapPlanes() {
  if (mReadback) {
    mReadback->Unmap();
    mReadback = nullptr;
  }
}

}  // namespace wr
}  // namespace mozilla
