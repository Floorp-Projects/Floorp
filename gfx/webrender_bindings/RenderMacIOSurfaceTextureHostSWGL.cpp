/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderMacIOSurfaceTextureHostSWGL.h"

#include "MacIOSurfaceHelpers.h"

namespace mozilla {
namespace wr {

RenderMacIOSurfaceTextureHostSWGL::RenderMacIOSurfaceTextureHostSWGL(
    MacIOSurface* aSurface)
    : mSurface(aSurface) {
  MOZ_COUNT_CTOR_INHERITED(RenderMacIOSurfaceTextureHostSWGL,
                           RenderTextureHostSWGL);
  MOZ_RELEASE_ASSERT(mSurface);
}

RenderMacIOSurfaceTextureHostSWGL::~RenderMacIOSurfaceTextureHostSWGL() {
  MOZ_COUNT_DTOR_INHERITED(RenderMacIOSurfaceTextureHostSWGL,
                           RenderTextureHostSWGL);
}

size_t RenderMacIOSurfaceTextureHostSWGL::GetPlaneCount() {
  size_t planeCount = mSurface->GetPlaneCount();
  return planeCount > 0 ? planeCount : 1;
}

bool RenderMacIOSurfaceTextureHostSWGL::MapPlane(uint8_t aChannelIndex,
                                                 PlaneInfo& aPlaneInfo) {
  if (!aChannelIndex) {
    mSurface->Lock();
  }
  aPlaneInfo.mFormat = mSurface->GetFormat();
  aPlaneInfo.mData = mSurface->GetBaseAddressOfPlane(aChannelIndex);
  aPlaneInfo.mStride = mSurface->GetBytesPerRow(aChannelIndex);
  aPlaneInfo.mSize =
      gfx::IntSize(mSurface->GetDevicePixelWidth(aChannelIndex),
                   mSurface->GetDevicePixelHeight(aChannelIndex));
  return true;
}

void RenderMacIOSurfaceTextureHostSWGL::UnmapPlanes() { mSurface->Unlock(); }

}  // namespace wr
}  // namespace mozilla
