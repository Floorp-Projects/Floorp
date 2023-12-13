/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLImages.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "ScopedGLHelpers.h"
#include "GLImages.h"
#include "GLBlitHelper.h"
#include "GLReadTexImageHelper.h"
#include "GLLibraryEGL.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/LayersSurfaces.h"

using namespace mozilla;
using namespace mozilla::gl;

namespace mozilla::layers {

static RefPtr<GLContext> sSnapshotContext;

nsresult GLImage::ReadIntoBuffer(uint8_t* aData, int32_t aStride,
                                 const gfx::IntSize& aSize,
                                 gfx::SurfaceFormat aFormat) {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main thread");

  if (!sSnapshotContext) {
    nsCString discardFailureId;
    sSnapshotContext = GLContextProvider::CreateHeadless({}, &discardFailureId);
    if (!sSnapshotContext) {
      NS_WARNING("Failed to create snapshot GLContext");
      return NS_ERROR_FAILURE;
    }
  }

  sSnapshotContext->MakeCurrent();
  ScopedTexture scopedTex(sSnapshotContext);
  ScopedBindTexture boundTex(sSnapshotContext, scopedTex.Texture());

  sSnapshotContext->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA,
                                aSize.width, aSize.height, 0, LOCAL_GL_RGBA,
                                LOCAL_GL_UNSIGNED_BYTE, nullptr);

  ScopedFramebufferForTexture autoFBForTex(sSnapshotContext,
                                           scopedTex.Texture());
  if (!autoFBForTex.IsComplete()) {
    gfxCriticalError()
        << "GetAsSourceSurface: ScopedFramebufferForTexture failed.";
    return NS_ERROR_FAILURE;
  }

  const gl::OriginPos destOrigin = gl::OriginPos::TopLeft;
  {
    const ScopedBindFramebuffer bindFB(sSnapshotContext, autoFBForTex.FB());
    if (!sSnapshotContext->BlitHelper()->BlitImageToFramebuffer(this, aSize,
                                                                destOrigin)) {
      return NS_ERROR_FAILURE;
    }
  }

  ScopedBindFramebuffer bind(sSnapshotContext, autoFBForTex.FB());
  ReadPixelsIntoBuffer(sSnapshotContext, aData, aStride, aSize, aFormat);
  return NS_OK;
}

already_AddRefed<gfx::SourceSurface> GLImage::GetAsSourceSurface() {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main thread");

  gfx::IntSize size = GetSize();
  auto format = gfx::SurfaceFormat::B8G8R8A8;
  RefPtr<gfx::DataSourceSurface> dest =
      gfx::Factory::CreateDataSourceSurface(size, format);
  if (NS_WARN_IF(!dest)) {
    return nullptr;
  }

  gfx::DataSourceSurface::ScopedMap map(dest, gfx::DataSourceSurface::WRITE);
  if (NS_WARN_IF(!map.IsMapped())) {
    return nullptr;
  }

  nsresult rv = ReadIntoBuffer(map.GetData(), map.GetStride(), size, format);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return dest.forget();
}

nsresult GLImage::BuildSurfaceDescriptorBuffer(
    SurfaceDescriptorBuffer& aSdBuffer, BuildSdbFlags aFlags,
    const std::function<MemoryOrShmem(uint32_t)>& aAllocate) {
  gfx::IntSize size = GetSize();
  auto format = gfx::SurfaceFormat::B8G8R8A8;

  uint8_t* buffer = nullptr;
  int32_t stride = 0;
  nsresult rv = AllocateSurfaceDescriptorBufferRgb(
      size, format, buffer, aSdBuffer, stride, aAllocate);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return ReadIntoBuffer(buffer, stride, size, format);
}

#ifdef MOZ_WIDGET_ANDROID
SurfaceTextureImage::SurfaceTextureImage(
    AndroidSurfaceTextureHandle aHandle, const gfx::IntSize& aSize,
    bool aContinuous, gl::OriginPos aOriginPos, bool aHasAlpha,
    bool aForceBT709ColorSpace, Maybe<gfx::Matrix4x4> aTransformOverride)
    : GLImage(ImageFormat::SURFACE_TEXTURE),
      mHandle(aHandle),
      mSize(aSize),
      mContinuous(aContinuous),
      mOriginPos(aOriginPos),
      mHasAlpha(aHasAlpha),
      mForceBT709ColorSpace(aForceBT709ColorSpace),
      mTransformOverride(aTransformOverride) {
  MOZ_ASSERT(mHandle);
}

Maybe<SurfaceDescriptor> SurfaceTextureImage::GetDesc() {
  SurfaceDescriptor sd = SurfaceTextureDescriptor(
      mHandle, mSize,
      mHasAlpha ? gfx::SurfaceFormat::R8G8B8A8 : gfx::SurfaceFormat::R8G8B8X8,
      mForceBT709ColorSpace, false /* NOT continuous */, mTransformOverride);
  return Some(sd);
}
#endif

}  // namespace mozilla::layers
