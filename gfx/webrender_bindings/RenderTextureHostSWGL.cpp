/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderTextureHostSWGL.h"

#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/TextureHost.h"
#include "RenderThread.h"

namespace mozilla {
namespace wr {

bool RenderTextureHostSWGL::UpdatePlanes(RenderCompositor* aCompositor,
                                         wr::ImageRendering aRendering) {
  wr_swgl_make_current(mContext);
  size_t planeCount = GetPlaneCount();
  bool filterUpdate = IsFilterUpdateNecessary(aRendering);
  if (mPlanes.size() < planeCount) {
    mPlanes.reserve(planeCount);
    while (mPlanes.size() < planeCount) {
      mPlanes.push_back(PlaneInfo(wr_swgl_gen_texture(mContext)));
    }
    filterUpdate = true;
  }
  gfx::SurfaceFormat format = GetFormat();
  gfx::ColorDepth colorDepth = GetColorDepth();
  for (size_t i = 0; i < planeCount; i++) {
    PlaneInfo& plane = mPlanes[i];
    if (!MapPlane(aCompositor, i, plane)) {
      if (i > 0) {
        UnmapPlanes();
      }
      return false;
    }
    GLenum internalFormat = 0;
    switch (format) {
      case gfx::SurfaceFormat::B8G8R8A8:
      case gfx::SurfaceFormat::B8G8R8X8:
        MOZ_ASSERT(colorDepth == gfx::ColorDepth::COLOR_8);
        internalFormat = LOCAL_GL_RGBA8;
        break;
      case gfx::SurfaceFormat::YUV:
        switch (colorDepth) {
          case gfx::ColorDepth::COLOR_8:
            internalFormat = LOCAL_GL_R8;
            break;
          case gfx::ColorDepth::COLOR_10:
          case gfx::ColorDepth::COLOR_12:
          case gfx::ColorDepth::COLOR_16:
            internalFormat = LOCAL_GL_R16;
            break;
          default:
            MOZ_RELEASE_ASSERT(false, "Unhandled YUV color depth");
            break;
        }
        break;
      case gfx::SurfaceFormat::NV12:
        MOZ_ASSERT(colorDepth == gfx::ColorDepth::COLOR_8);
        internalFormat = i > 0 ? LOCAL_GL_RG8 : LOCAL_GL_R8;
        break;
      case gfx::SurfaceFormat::YUV422:
        MOZ_ASSERT(colorDepth == gfx::ColorDepth::COLOR_8);
        internalFormat = LOCAL_GL_RGB_RAW_422_APPLE;
        break;
      default:
        MOZ_RELEASE_ASSERT(false, "Unhandled external image format");
        break;
    }
    wr_swgl_set_texture_buffer(mContext, plane.mTexture, internalFormat,
                               plane.mSize.width, plane.mSize.height,
                               plane.mStride, plane.mData, 0, 0);
  }
  if (filterUpdate) {
    mCachedRendering = aRendering;
    GLenum filter = aRendering == wr::ImageRendering::Pixelated
                        ? LOCAL_GL_NEAREST
                        : LOCAL_GL_LINEAR;
    for (const auto& plane : mPlanes) {
      wr_swgl_set_texture_parameter(mContext, plane.mTexture,
                                    LOCAL_GL_TEXTURE_MIN_FILTER, filter);
      wr_swgl_set_texture_parameter(mContext, plane.mTexture,
                                    LOCAL_GL_TEXTURE_MAG_FILTER, filter);
    }
  }
  return true;
}

bool RenderTextureHostSWGL::SetContext(void* aContext) {
  if (mContext != aContext) {
    CleanupPlanes();
    mContext = aContext;
    wr_swgl_reference_context(mContext);
  }
  return mContext != nullptr;
}

wr::WrExternalImage RenderTextureHostSWGL::LockSWGL(
    uint8_t aChannelIndex, void* aContext, RenderCompositor* aCompositor,
    wr::ImageRendering aRendering) {
  if (!SetContext(aContext)) {
    return InvalidToWrExternalImage();
  }
  if (!mLocked) {
    if (!UpdatePlanes(aCompositor, aRendering)) {
      return InvalidToWrExternalImage();
    }
    mLocked = true;
  }
  if (aChannelIndex >= mPlanes.size()) {
    return InvalidToWrExternalImage();
  }
  const PlaneInfo& plane = mPlanes[aChannelIndex];

  // Prefer native textures, unless our backend forbids it.
  layers::TextureHost::NativeTexturePolicy policy =
      layers::TextureHost::BackendNativeTexturePolicy(
          layers::WebRenderBackend::SOFTWARE, plane.mSize);
  return policy == layers::TextureHost::NativeTexturePolicy::FORBID
             ? RawDataToWrExternalImage((uint8_t*)plane.mData,
                                        plane.mStride * plane.mSize.height)
             : NativeTextureToWrExternalImage(
                   plane.mTexture, 0, 0, plane.mSize.width, plane.mSize.height);
}

void RenderTextureHostSWGL::UnlockSWGL() {
  if (mLocked) {
    mLocked = false;
    UnmapPlanes();
  }
}

void RenderTextureHostSWGL::CleanupPlanes() {
  if (!mContext) {
    return;
  }
  if (!mPlanes.empty()) {
    wr_swgl_make_current(mContext);
    for (const auto& plane : mPlanes) {
      wr_swgl_delete_texture(mContext, plane.mTexture);
    }
    mPlanes.clear();
  }
  wr_swgl_destroy_context(mContext);
  mContext = nullptr;
}

RenderTextureHostSWGL::~RenderTextureHostSWGL() { CleanupPlanes(); }

bool RenderTextureHostSWGL::LockSWGLCompositeSurface(
    void* aContext, wr::SWGLCompositeSurfaceInfo* aInfo) {
  if (!SetContext(aContext)) {
    return false;
  }
  if (!mLocked) {
    if (!UpdatePlanes(nullptr, mCachedRendering)) {
      return false;
    }
    mLocked = true;
  }
  MOZ_ASSERT(mPlanes.size() <= 3);
  for (size_t i = 0; i < mPlanes.size(); i++) {
    aInfo->textures[i] = mPlanes[i].mTexture;
  }
  switch (GetFormat()) {
    case gfx::SurfaceFormat::YUV:
    case gfx::SurfaceFormat::NV12:
    case gfx::SurfaceFormat::YUV422: {
      aInfo->yuv_planes = mPlanes.size();
      auto colorSpace = GetYUVColorSpace();
      MOZ_ASSERT(colorSpace != gfx::YUVColorSpace::UNKNOWN);
      aInfo->color_space = ToWrYuvColorSpace(colorSpace);
      auto colorDepth = GetColorDepth();
      MOZ_ASSERT(colorDepth != gfx::ColorDepth::UNKNOWN);
      aInfo->color_depth = ToWrColorDepth(colorDepth);
      break;
    }
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8:
      break;
    default:
      gfxCriticalNote << "Unhandled external image format: " << GetFormat();
      MOZ_RELEASE_ASSERT(false, "Unhandled external image format");
      break;
  }
  aInfo->size.width = mPlanes[0].mSize.width;
  aInfo->size.height = mPlanes[0].mSize.height;
  return true;
}

bool wr_swgl_lock_composite_surface(void* aContext, wr::ExternalImageId aId,
                                    wr::SWGLCompositeSurfaceInfo* aInfo) {
  RenderTextureHost* texture = RenderThread::Get()->GetRenderTexture(aId);
  if (!texture) {
    return false;
  }
  RenderTextureHostSWGL* swglTex = texture->AsRenderTextureHostSWGL();
  if (!swglTex) {
    return false;
  }
  return swglTex->LockSWGLCompositeSurface(aContext, aInfo);
}

void wr_swgl_unlock_composite_surface(void* aContext, wr::ExternalImageId aId) {
  RenderTextureHost* texture = RenderThread::Get()->GetRenderTexture(aId);
  if (!texture) {
    return;
  }
  RenderTextureHostSWGL* swglTex = texture->AsRenderTextureHostSWGL();
  if (!swglTex) {
    return;
  }
  swglTex->UnlockSWGL();
}

}  // namespace wr
}  // namespace mozilla
