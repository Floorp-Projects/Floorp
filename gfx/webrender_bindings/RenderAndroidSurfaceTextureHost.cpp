/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderAndroidSurfaceTextureHost.h"

#include "GLReadTexImageHelper.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/webrender/RenderThread.h"
#include "GLContext.h"
#include "AndroidSurfaceTexture.h"

namespace mozilla {
namespace wr {

RenderAndroidSurfaceTextureHost::RenderAndroidSurfaceTextureHost(
    const java::GeckoSurfaceTexture::GlobalRef& aSurfTex, gfx::IntSize aSize,
    gfx::SurfaceFormat aFormat, bool aContinuousUpdate,
    Maybe<gfx::Matrix4x4> aTransformOverride)
    : mSurfTex(aSurfTex),
      mSize(aSize),
      mFormat(aFormat),
      mContinuousUpdate(aContinuousUpdate),
      mTransformOverride(aTransformOverride),
      mPrepareStatus(STATUS_NONE),
      mAttachedToGLContext(false) {
  MOZ_COUNT_CTOR_INHERITED(RenderAndroidSurfaceTextureHost, RenderTextureHost);

  if (mSurfTex) {
    mSurfTex->IncrementUse();
  }
}

RenderAndroidSurfaceTextureHost::~RenderAndroidSurfaceTextureHost() {
  MOZ_ASSERT(RenderThread::IsInRenderThread());
  MOZ_COUNT_DTOR_INHERITED(RenderAndroidSurfaceTextureHost, RenderTextureHost);
  // The SurfaceTexture gets destroyed when its use count reaches zero.
  if (mSurfTex) {
    mSurfTex->DecrementUse();
  }
}

wr::WrExternalImage RenderAndroidSurfaceTextureHost::Lock(
    uint8_t aChannelIndex, gl::GLContext* aGL, wr::ImageRendering aRendering) {
  MOZ_ASSERT(aChannelIndex == 0);
  MOZ_ASSERT((mPrepareStatus == STATUS_PREPARED) ||
             (!mSurfTex->IsSingleBuffer() &&
              mPrepareStatus == STATUS_UPDATE_TEX_IMAGE_NEEDED));

  if (mGL.get() != aGL) {
    // This should not happen. On android, SingletonGL is used.
    MOZ_ASSERT_UNREACHABLE("Unexpected GL context");
    return InvalidToWrExternalImage();
  }

  if (!mSurfTex || !mGL || !mGL->MakeCurrent()) {
    return InvalidToWrExternalImage();
  }

  MOZ_ASSERT(mAttachedToGLContext);
  if (!mAttachedToGLContext) {
    return InvalidToWrExternalImage();
  }

  if (IsFilterUpdateNecessary(aRendering)) {
    // Cache new rendering filter.
    mCachedRendering = aRendering;
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                                 LOCAL_GL_TEXTURE_EXTERNAL_OES,
                                 mSurfTex->GetTexName(), aRendering);
  }

  if (mContinuousUpdate) {
    MOZ_ASSERT(!mSurfTex->IsSingleBuffer());
    mSurfTex->UpdateTexImage();
  } else if (mPrepareStatus == STATUS_UPDATE_TEX_IMAGE_NEEDED) {
    MOZ_ASSERT(!mSurfTex->IsSingleBuffer());
    // When SurfaceTexture is not single buffer mode, call UpdateTexImage() once
    // just before rendering. During playing video, one SurfaceTexture is used
    // for all RenderAndroidSurfaceTextureHosts of video.
    mSurfTex->UpdateTexImage();
    mPrepareStatus = STATUS_PREPARED;
  }

  const auto uvs = GetUvCoords(mSize);
  return NativeTextureToWrExternalImage(mSurfTex->GetTexName(), uvs.first.x,
                                        uvs.first.y, uvs.second.x,
                                        uvs.second.y);
}

void RenderAndroidSurfaceTextureHost::Unlock() {}

bool RenderAndroidSurfaceTextureHost::EnsureAttachedToGLContext() {
  // During handling WebRenderError, GeckoSurfaceTexture should not be attached
  // to GLContext.
  if (RenderThread::Get()->IsHandlingWebRenderError()) {
    return false;
  }

  if (mAttachedToGLContext) {
    return true;
  }

  if (!mGL) {
    mGL = RenderThread::Get()->SingletonGL();
  }

  if (!mSurfTex || !mGL || !mGL->MakeCurrent()) {
    return false;
  }

  if (!mSurfTex->IsAttachedToGLContext((int64_t)mGL.get())) {
    GLuint texName;
    mGL->fGenTextures(1, &texName);
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                                 LOCAL_GL_TEXTURE_EXTERNAL_OES, texName,
                                 mCachedRendering);

    if (NS_FAILED(mSurfTex->AttachToGLContext((int64_t)mGL.get(), texName))) {
      MOZ_ASSERT(0);
      mGL->fDeleteTextures(1, &texName);
      return false;
    }
  }

  mAttachedToGLContext = true;
  return true;
}

void RenderAndroidSurfaceTextureHost::PrepareForUse() {
  // When SurfaceTexture is single buffer mode, UpdateTexImage needs to be
  // called only once for each publish. If UpdateTexImage is called more
  // than once, it causes hang on puglish side. And UpdateTexImage needs to
  // be called on render thread, since the SurfaceTexture is consumed on render
  // thread.
  MOZ_ASSERT(RenderThread::IsInRenderThread());
  MOZ_ASSERT(mPrepareStatus == STATUS_NONE);

  if (mContinuousUpdate || !mSurfTex) {
    return;
  }

  mPrepareStatus = STATUS_MIGHT_BE_USED_BY_WR;

  if (mSurfTex->IsSingleBuffer()) {
    EnsureAttachedToGLContext();
    // When SurfaceTexture is single buffer mode, it is OK to call
    // UpdateTexImage() here.
    mSurfTex->UpdateTexImage();
    mPrepareStatus = STATUS_PREPARED;
  }
}

void RenderAndroidSurfaceTextureHost::NotifyForUse() {
  MOZ_ASSERT(RenderThread::IsInRenderThread());

  if (mPrepareStatus == STATUS_MIGHT_BE_USED_BY_WR) {
    // This happens when SurfaceTexture of video is rendered on WebRender.
    // There is a case that SurfaceTexture is not rendered on WebRender, instead
    // it is rendered to WebGL and the SurfaceTexture should not be attached to
    // gl context of WebRender. It is ugly. But it is same as Compositor
    // rendering.
    MOZ_ASSERT(!mSurfTex->IsSingleBuffer());
    if (!EnsureAttachedToGLContext()) {
      return;
    }
    mPrepareStatus = STATUS_UPDATE_TEX_IMAGE_NEEDED;
  }
}

void RenderAndroidSurfaceTextureHost::NotifyNotUsed() {
  MOZ_ASSERT(RenderThread::IsInRenderThread());

  if (!mSurfTex) {
    MOZ_ASSERT(mPrepareStatus == STATUS_NONE);
    return;
  }

  if (mSurfTex->IsSingleBuffer()) {
    MOZ_ASSERT(mPrepareStatus == STATUS_PREPARED);
    MOZ_ASSERT(mAttachedToGLContext);
    // Release SurfaceTexture's buffer to client side.
    mGL->MakeCurrent();
    mSurfTex->ReleaseTexImage();
  } else if (mPrepareStatus == STATUS_UPDATE_TEX_IMAGE_NEEDED) {
    MOZ_ASSERT(mAttachedToGLContext);
    // This could happen when video frame was skipped. UpdateTexImage() neeeds
    // to be called for adjusting SurfaceTexture's buffer status.
    mSurfTex->UpdateTexImage();
  }

  mPrepareStatus = STATUS_NONE;
}

gfx::SurfaceFormat RenderAndroidSurfaceTextureHost::GetFormat() const {
  MOZ_ASSERT(mFormat == gfx::SurfaceFormat::R8G8B8A8 ||
             mFormat == gfx::SurfaceFormat::R8G8B8X8);

  if (mFormat == gfx::SurfaceFormat::R8G8B8A8) {
    return gfx::SurfaceFormat::B8G8R8A8;
  }

  if (mFormat == gfx::SurfaceFormat::R8G8B8X8) {
    return gfx::SurfaceFormat::B8G8R8X8;
  }

  gfxCriticalNoteOnce
      << "Unexpected color format of RenderAndroidSurfaceTextureHost";

  return gfx::SurfaceFormat::UNKNOWN;
}

already_AddRefed<DataSourceSurface>
RenderAndroidSurfaceTextureHost::ReadTexImage() {
  if (!mGL) {
    mGL = RenderThread::Get()->SingletonGL();
    if (!mGL) {
      return nullptr;
    }
  }

  /* Allocate resulting image surface */
  int32_t stride = mSize.width * BytesPerPixel(GetFormat());
  RefPtr<DataSourceSurface> surf =
      Factory::CreateDataSourceSurfaceWithStride(mSize, GetFormat(), stride);
  if (!surf) {
    return nullptr;
  }

  layers::ShaderConfigOGL config = layers::ShaderConfigFromTargetAndFormat(
      LOCAL_GL_TEXTURE_EXTERNAL, mFormat);
  int shaderConfig = config.mFeatures;

  bool ret = mGL->ReadTexImageHelper()->ReadTexImage(
      surf, mSurfTex->GetTexName(), LOCAL_GL_TEXTURE_EXTERNAL, mSize,
      shaderConfig, /* aYInvert */ false);
  if (!ret) {
    return nullptr;
  }

  return surf.forget();
}

bool RenderAndroidSurfaceTextureHost::MapPlane(RenderCompositor* aCompositor,
                                               uint8_t aChannelIndex,
                                               PlaneInfo& aPlaneInfo) {
  RefPtr<gfx::DataSourceSurface> readback = ReadTexImage();
  if (!readback) {
    return false;
  }

  DataSourceSurface::MappedSurface map;
  if (!readback->Map(DataSourceSurface::MapType::READ, &map)) {
    return false;
  }

  mReadback = readback;
  aPlaneInfo.mSize = mSize;
  aPlaneInfo.mStride = map.mStride;
  aPlaneInfo.mData = map.mData;
  return true;
}

void RenderAndroidSurfaceTextureHost::UnmapPlanes() {
  if (mReadback) {
    mReadback->Unmap();
    mReadback = nullptr;
  }
}

std::pair<gfx::Point, gfx::Point> RenderAndroidSurfaceTextureHost::GetUvCoords(
    gfx::IntSize aTextureSize) const {
  gfx::Matrix4x4 transform;

  // GetTransformMatrix() returns the transform set by the producer side of the
  // SurfaceTexture that must be applied to texture coordinates when
  // sampling. In some cases we may have set an override value, such as in
  // AndroidNativeWindowTextureData where we own the producer side, or for
  // MediaCodec output on devices where where we know the value is incorrect.
  if (mTransformOverride) {
    transform = *mTransformOverride;
  } else if (mSurfTex) {
    const auto& surf = java::sdk::SurfaceTexture::LocalRef(
        java::sdk::SurfaceTexture::Ref::From(mSurfTex));
    gl::AndroidSurfaceTexture::GetTransformMatrix(surf, &transform);
  }

  // We expect this transform to always be rectilinear, usually just a
  // y-flip and sometimes an x and y scale. This allows this function
  // to simply transform and return 2 points here instead of 4.
  MOZ_ASSERT(transform.IsRectilinear(),
             "Unexpected non-rectilinear transform returned from "
             "SurfaceTexture.GetTransformMatrix()");

  transform.PostScale(aTextureSize.width, aTextureSize.height, 0.0);

  gfx::Point uv0 = gfx::Point(0.0, 0.0);
  gfx::Point uv1 = gfx::Point(1.0, 1.0);
  uv0 = transform.TransformPoint(uv0);
  uv1 = transform.TransformPoint(uv1);

  return std::make_pair(uv0, uv1);
}

}  // namespace wr
}  // namespace mozilla
