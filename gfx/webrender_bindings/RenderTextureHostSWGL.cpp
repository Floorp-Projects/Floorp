/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderTextureHostSWGL.h"

namespace mozilla {
namespace wr {

bool RenderTextureHostSWGL::UpdatePlanes(wr::ImageRendering aRendering) {
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
  for (size_t i = 0; i < planeCount; i++) {
    PlaneInfo& plane = mPlanes[i];
    if (!MapPlane(i, plane)) {
      if (i > 0) {
        UnmapPlanes();
      }
      return false;
    }
    GLenum format = 0;
    switch (plane.mFormat) {
      case gfx::SurfaceFormat::B8G8R8A8:
      case gfx::SurfaceFormat::B8G8R8X8:
        format = LOCAL_GL_RGBA8;
        break;
      case gfx::SurfaceFormat::YUV:
        format = LOCAL_GL_R8;
        break;
      case gfx::SurfaceFormat::NV12:
        format = planeCount == 2 && i > 0 ? LOCAL_GL_RG8 : LOCAL_GL_R8;
        break;
      default:
        MOZ_RELEASE_ASSERT(false, "Unhandled external image format");
        break;
    }
    wr_swgl_set_texture_buffer(mContext, plane.mTexture, format,
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

wr::WrExternalImage RenderTextureHostSWGL::LockSWGL(
    uint8_t aChannelIndex, void* aContext, wr::ImageRendering aRendering) {
  if (mContext != aContext) {
    CleanupPlanes();
    mContext = aContext;
    wr_swgl_reference_context(mContext);
  }
  if (!mContext) {
    return InvalidToWrExternalImage();
  }
  if (!mLocked) {
    if (!UpdatePlanes(aRendering)) {
      return InvalidToWrExternalImage();
    }
    mLocked = true;
  }
  if (aChannelIndex >= mPlanes.size()) {
    return InvalidToWrExternalImage();
  }
  const PlaneInfo& plane = mPlanes[aChannelIndex];
  return NativeTextureToWrExternalImage(plane.mTexture, 0, 0, plane.mSize.width,
                                        plane.mSize.height);
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

}  // namespace wr
}  // namespace mozilla
