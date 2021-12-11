/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderTextureHostWrapper.h"

#include "mozilla/gfx/Logging.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla {
namespace wr {

RenderTextureHostWrapper::RenderTextureHostWrapper(
    ExternalImageId aExternalImageId)
    : mExternalImageId(aExternalImageId) {
  MOZ_COUNT_CTOR_INHERITED(RenderTextureHostWrapper, RenderTextureHost);
}

RenderTextureHostWrapper::~RenderTextureHostWrapper() {
  MOZ_COUNT_DTOR_INHERITED(RenderTextureHostWrapper, RenderTextureHost);
}

void RenderTextureHostWrapper::EnsureTextureHost() const {
  if (!mTextureHost) {
    mTextureHost = RenderThread::Get()->GetRenderTexture(mExternalImageId);
    MOZ_ASSERT(mTextureHost);
    if (!mTextureHost) {
      gfxCriticalNoteOnce << "Failed to get RenderTextureHost for extId:"
                          << AsUint64(mExternalImageId);
    }
  }
}

wr::WrExternalImage RenderTextureHostWrapper::Lock(
    uint8_t aChannelIndex, gl::GLContext* aGL, wr::ImageRendering aRendering) {
  EnsureTextureHost();
  if (!mTextureHost) {
    return InvalidToWrExternalImage();
  }

  return mTextureHost->Lock(aChannelIndex, aGL, aRendering);
}

void RenderTextureHostWrapper::Unlock() {
  if (mTextureHost) {
    mTextureHost->Unlock();
  }
}

void RenderTextureHostWrapper::ClearCachedResources() {
  if (mTextureHost) {
    mTextureHost->ClearCachedResources();
  }
}

RenderMacIOSurfaceTextureHost*
RenderTextureHostWrapper::AsRenderMacIOSurfaceTextureHost() {
  EnsureTextureHost();
  if (!mTextureHost) {
    return nullptr;
  }
  return mTextureHost->AsRenderMacIOSurfaceTextureHost();
}

RenderDXGITextureHost* RenderTextureHostWrapper::AsRenderDXGITextureHost() {
  EnsureTextureHost();
  if (!mTextureHost) {
    return nullptr;
  }
  return mTextureHost->AsRenderDXGITextureHost();
}

RenderDXGIYCbCrTextureHost*
RenderTextureHostWrapper::AsRenderDXGIYCbCrTextureHost() {
  EnsureTextureHost();
  if (!mTextureHost) {
    return nullptr;
  }
  return mTextureHost->AsRenderDXGIYCbCrTextureHost();
}

RenderTextureHostSWGL* RenderTextureHostWrapper::EnsureRenderTextureHostSWGL()
    const {
  EnsureTextureHost();
  if (!mTextureHost) {
    return nullptr;
  }
  return mTextureHost->AsRenderTextureHostSWGL();
}

size_t RenderTextureHostWrapper::GetPlaneCount() const {
  if (RenderTextureHostSWGL* swglHost = EnsureRenderTextureHostSWGL()) {
    return swglHost->GetPlaneCount();
  }
  return 0;
}

gfx::SurfaceFormat RenderTextureHostWrapper::GetFormat() const {
  if (RenderTextureHostSWGL* swglHost = EnsureRenderTextureHostSWGL()) {
    return swglHost->GetFormat();
  }
  return gfx::SurfaceFormat::UNKNOWN;
}

gfx::ColorDepth RenderTextureHostWrapper::GetColorDepth() const {
  if (RenderTextureHostSWGL* swglHost = EnsureRenderTextureHostSWGL()) {
    return swglHost->GetColorDepth();
  }
  return gfx::ColorDepth::COLOR_8;
}

gfx::YUVRangedColorSpace RenderTextureHostWrapper::GetYUVColorSpace() const {
  if (RenderTextureHostSWGL* swglHost = EnsureRenderTextureHostSWGL()) {
    return swglHost->GetYUVColorSpace();
  }
  return gfx::YUVRangedColorSpace::Default;
}

bool RenderTextureHostWrapper::MapPlane(RenderCompositor* aCompositor,
                                        uint8_t aChannelIndex,
                                        PlaneInfo& aPlaneInfo) {
  if (RenderTextureHostSWGL* swglHost = EnsureRenderTextureHostSWGL()) {
    return swglHost->MapPlane(aCompositor, aChannelIndex, aPlaneInfo);
  }
  return false;
}

void RenderTextureHostWrapper::UnmapPlanes() {
  if (RenderTextureHostSWGL* swglHost = EnsureRenderTextureHostSWGL()) {
    swglHost->UnmapPlanes();
  }
}

}  // namespace wr
}  // namespace mozilla
