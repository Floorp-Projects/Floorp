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

using namespace mozilla;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

static RefPtr<GLContext> sSnapshotContext;

already_AddRefed<gfx::SourceSurface> GLImage::GetAsSourceSurface() {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main thread");

  if (!sSnapshotContext) {
    nsCString discardFailureId;
    sSnapshotContext = GLContextProvider::CreateHeadless(
        CreateContextFlags::NONE, &discardFailureId);
    if (!sSnapshotContext) {
      NS_WARNING("Failed to create snapshot GLContext");
      return nullptr;
    }
  }

  sSnapshotContext->MakeCurrent();
  ScopedTexture scopedTex(sSnapshotContext);
  ScopedBindTexture boundTex(sSnapshotContext, scopedTex.Texture());

  gfx::IntSize size = GetSize();
  sSnapshotContext->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA,
                                size.width, size.height, 0, LOCAL_GL_RGBA,
                                LOCAL_GL_UNSIGNED_BYTE, nullptr);

  ScopedFramebufferForTexture autoFBForTex(sSnapshotContext,
                                           scopedTex.Texture());
  if (!autoFBForTex.IsComplete()) {
    gfxCriticalError()
        << "GetAsSourceSurface: ScopedFramebufferForTexture failed.";
    return nullptr;
  }

  const gl::OriginPos destOrigin = gl::OriginPos::TopLeft;
  {
    const ScopedBindFramebuffer bindFB(sSnapshotContext, autoFBForTex.FB());
    if (!sSnapshotContext->BlitHelper()->BlitImageToFramebuffer(this, size,
                                                                destOrigin)) {
      return nullptr;
    }
  }

  RefPtr<gfx::DataSourceSurface> source =
      gfx::Factory::CreateDataSourceSurface(size, gfx::SurfaceFormat::B8G8R8A8);
  if (NS_WARN_IF(!source)) {
    return nullptr;
  }

  ScopedBindFramebuffer bind(sSnapshotContext, autoFBForTex.FB());
  ReadPixelsIntoDataSurface(sSnapshotContext, source);
  return source.forget();
}

#ifdef MOZ_WIDGET_ANDROID
SurfaceTextureImage::SurfaceTextureImage(AndroidSurfaceTextureHandle aHandle,
                                         const gfx::IntSize& aSize,
                                         bool aContinuous,
                                         gl::OriginPos aOriginPos,
                                         bool aHasAlpha /* = true */)
    : GLImage(ImageFormat::SURFACE_TEXTURE),
      mHandle(aHandle),
      mSize(aSize),
      mContinuous(aContinuous),
      mOriginPos(aOriginPos),
      mHasAlpha(aHasAlpha) {
  MOZ_ASSERT(mHandle);
}
#endif

}  // namespace layers
}  // namespace mozilla
