/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderMacIOSurfaceTextureHost.h"

#include "GLContextCGL.h"
#include "mozilla/gfx/Logging.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace wr {

static CGLError CreateTextureForPlane(uint8_t aPlaneID, gl::GLContext* aGL,
                                      MacIOSurface* aSurface,
                                      GLuint* aTexture) {
  MOZ_ASSERT(aGL && aSurface && aTexture);

  aGL->fGenTextures(1, aTexture);
  ActivateBindAndTexParameteri(aGL, LOCAL_GL_TEXTURE0,
                               LOCAL_GL_TEXTURE_RECTANGLE_ARB, *aTexture);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_WRAP_T,
                      LOCAL_GL_CLAMP_TO_EDGE);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_WRAP_S,
                      LOCAL_GL_CLAMP_TO_EDGE);

  CGLError result = kCGLNoError;
  gfx::SurfaceFormat readFormat = gfx::SurfaceFormat::UNKNOWN;
  result = aSurface->CGLTexImageIOSurface2D(
      aGL, gl::GLContextCGL::Cast(aGL)->GetCGLContext(), aPlaneID, &readFormat);
  // If this is a yuv format, the Webrender only supports YUV422 interleaving
  // format.
  MOZ_ASSERT(aSurface->GetFormat() != gfx::SurfaceFormat::YUV422 ||
             readFormat == gfx::SurfaceFormat::YUV422);

  return result;
}

RenderMacIOSurfaceTextureHost::RenderMacIOSurfaceTextureHost(
    MacIOSurface* aSurface)
    : mSurface(aSurface), mTextureHandles{0, 0, 0} {
  MOZ_COUNT_CTOR_INHERITED(RenderMacIOSurfaceTextureHost, RenderTextureHost);
}

RenderMacIOSurfaceTextureHost::~RenderMacIOSurfaceTextureHost() {
  MOZ_COUNT_DTOR_INHERITED(RenderMacIOSurfaceTextureHost, RenderTextureHost);
  DeleteTextureHandle();
}

GLuint RenderMacIOSurfaceTextureHost::GetGLHandle(uint8_t aChannelIndex) const {
  MOZ_ASSERT(mSurface);
  MOZ_ASSERT((mSurface->GetPlaneCount() == 0)
                 ? (aChannelIndex == mSurface->GetPlaneCount())
                 : (aChannelIndex < mSurface->GetPlaneCount()));
  return mTextureHandles[aChannelIndex];
}

gfx::IntSize RenderMacIOSurfaceTextureHost::GetSize(
    uint8_t aChannelIndex) const {
  MOZ_ASSERT(mSurface);
  MOZ_ASSERT((mSurface->GetPlaneCount() == 0)
                 ? (aChannelIndex == mSurface->GetPlaneCount())
                 : (aChannelIndex < mSurface->GetPlaneCount()));

  if (!mSurface) {
    return gfx::IntSize();
  }
  return gfx::IntSize(mSurface->GetDevicePixelWidth(aChannelIndex),
                      mSurface->GetDevicePixelHeight(aChannelIndex));
}

size_t RenderMacIOSurfaceTextureHost::Bytes() {
  return mSurface->GetAllocSize();
}

wr::WrExternalImage RenderMacIOSurfaceTextureHost::Lock(uint8_t aChannelIndex,
                                                        gl::GLContext* aGL) {
  if (mGL.get() != aGL) {
    // release the texture handle in the previous gl context
    DeleteTextureHandle();
    mGL = aGL;
    mGL->MakeCurrent();
  }

  if (!mSurface || !mGL || !mGL->MakeCurrent()) {
    return InvalidToWrExternalImage();
  }

  if (!mTextureHandles[0]) {
    MOZ_ASSERT(gl::GLContextCGL::Cast(mGL.get())->GetCGLContext());

    // The result of GetPlaneCount() is 0 for single plane format, but it will
    // be 2 if the format has 2 planar data.
    CreateTextureForPlane(0, mGL, mSurface, &(mTextureHandles[0]));
    for (size_t i = 1; i < mSurface->GetPlaneCount(); ++i) {
      CreateTextureForPlane(i, mGL, mSurface, &(mTextureHandles[i]));
    }
  }

  const auto uvs = GetUvCoords(GetSize(aChannelIndex));
  return NativeTextureToWrExternalImage(GetGLHandle(aChannelIndex), uvs.first.x,
                                        uvs.first.y, uvs.second.x,
                                        uvs.second.y);
}

void RenderMacIOSurfaceTextureHost::Unlock() {}

void RenderMacIOSurfaceTextureHost::DeleteTextureHandle() {
  if (mTextureHandles[0] != 0 && mGL && mGL->MakeCurrent()) {
    // Calling glDeleteTextures on 0 isn't an error. So, just make them a single
    // call.
    mGL->fDeleteTextures(3, mTextureHandles);
    for (size_t i = 0; i < 3; ++i) {
      mTextureHandles[i] = 0;
    }
  }
}

size_t RenderMacIOSurfaceTextureHost::GetPlaneCount() const {
  size_t planeCount = mSurface->GetPlaneCount();
  return planeCount > 0 ? planeCount : 1;
}

gfx::SurfaceFormat RenderMacIOSurfaceTextureHost::GetFormat() const {
  return mSurface->GetFormat();
}

gfx::ColorDepth RenderMacIOSurfaceTextureHost::GetColorDepth() const {
  return mSurface->GetColorDepth();
}

gfx::YUVRangedColorSpace RenderMacIOSurfaceTextureHost::GetYUVColorSpace()
    const {
  return ToYUVRangedColorSpace(mSurface->GetYUVColorSpace(),
                               mSurface->GetColorRange());
}

bool RenderMacIOSurfaceTextureHost::MapPlane(RenderCompositor* aCompositor,
                                             uint8_t aChannelIndex,
                                             PlaneInfo& aPlaneInfo) {
  if (!aChannelIndex) {
    mSurface->Lock();
  }
  aPlaneInfo.mData = mSurface->GetBaseAddressOfPlane(aChannelIndex);
  aPlaneInfo.mStride = mSurface->GetBytesPerRow(aChannelIndex);
  aPlaneInfo.mSize =
      gfx::IntSize(mSurface->GetDevicePixelWidth(aChannelIndex),
                   mSurface->GetDevicePixelHeight(aChannelIndex));
  return true;
}

void RenderMacIOSurfaceTextureHost::UnmapPlanes() { mSurface->Unlock(); }

}  // namespace wr
}  // namespace mozilla
