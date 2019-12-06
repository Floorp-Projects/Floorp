/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaylandDMABUFTextureHostOGL.h"
#include "mozilla/widget/WaylandDMABufSurface.h"
#include "mozilla/webrender/RenderWaylandDMABUFTextureHostOGL.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "GLContextEGL.h"

namespace mozilla {
namespace layers {

WaylandDMABUFTextureHostOGL::WaylandDMABUFTextureHostOGL(
    TextureFlags aFlags, const SurfaceDescriptor& aDesc)
    : TextureHost(aFlags) {
  MOZ_COUNT_CTOR(WaylandDMABUFTextureHostOGL);

  mSurface = WaylandDMABufSurface::CreateDMABufSurface(
      aDesc.get_SurfaceDescriptorDMABuf());
}

WaylandDMABUFTextureHostOGL::~WaylandDMABUFTextureHostOGL() {
  MOZ_COUNT_DTOR(WaylandDMABUFTextureHostOGL);
  if (mProvider) {
    DeallocateDeviceData();
  }
}

bool WaylandDMABUFTextureHostOGL::Lock() {
  if (!gl() || !gl()->MakeCurrent() || !mSurface) {
    return false;
  }

  if (!mTextureSource && mSurface->CreateEGLImage(gl())) {
    auto format = mSurface->HasAlpha() ? gfx::SurfaceFormat::R8G8B8A8
                                       : gfx::SurfaceFormat::R8G8B8X8;
    mTextureSource = new EGLImageTextureSource(
        mProvider, mSurface->GetEGLImage(), format, LOCAL_GL_TEXTURE_2D,
        LOCAL_GL_CLAMP_TO_EDGE,
        gfx::IntSize(mSurface->GetWidth(), mSurface->GetHeight()));
  }
  return true;
}

void WaylandDMABUFTextureHostOGL::Unlock() {}

void WaylandDMABUFTextureHostOGL::DeallocateDeviceData() {
  mTextureSource = nullptr;
  if (mSurface) {
    mSurface->ReleaseEGLImage();
  }
}

void WaylandDMABUFTextureHostOGL::SetTextureSourceProvider(
    TextureSourceProvider* aProvider) {
  if (!aProvider || !aProvider->GetGLContext()) {
    DeallocateDeviceData();
    mProvider = nullptr;
    return;
  }

  if (mProvider != aProvider) {
    DeallocateDeviceData();
  }

  mProvider = aProvider;

  if (mTextureSource) {
    mTextureSource->SetTextureSourceProvider(aProvider);
  }
}

gfx::SurfaceFormat WaylandDMABUFTextureHostOGL::GetFormat() const {
  MOZ_ASSERT(mTextureSource);
  return mTextureSource ? mTextureSource->GetFormat()
                        : gfx::SurfaceFormat::UNKNOWN;
}

gfx::IntSize WaylandDMABUFTextureHostOGL::GetSize() const {
  if (!mSurface) {
    return gfx::IntSize();
  }
  return gfx::IntSize(mSurface->GetWidth(), mSurface->GetHeight());
}

gl::GLContext* WaylandDMABUFTextureHostOGL::gl() const {
  return mProvider ? mProvider->GetGLContext() : nullptr;
}

void WaylandDMABUFTextureHostOGL::CreateRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  RefPtr<wr::RenderTextureHost> texture =
      new wr::RenderWaylandDMABUFTextureHostOGL(mSurface);
  wr::RenderThread::Get()->RegisterExternalImage(wr::AsUint64(aExternalImageId),
                                                 texture.forget());
}

void WaylandDMABUFTextureHostOGL::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID,
    const bool aPreferCompositorSurface) {
  MOZ_ASSERT(mSurface);

  auto method = aOp == TextureHost::ADD_IMAGE
                    ? &wr::TransactionBuilder::AddExternalImage
                    : &wr::TransactionBuilder::UpdateExternalImage;
  auto imageType =
      wr::ExternalImageType::TextureHandle(wr::TextureTarget::Default);

  gfx::SurfaceFormat format = mSurface->HasAlpha()
                                  ? gfx::SurfaceFormat::R8G8B8A8
                                  : gfx::SurfaceFormat::R8G8B8X8;

  MOZ_ASSERT(aImageKeys.length() == 1);
  // XXX Add RGBA handling. Temporary hack to avoid crash
  // With BGRA format setting, rendering works without problem.
  auto formatTmp = format == gfx::SurfaceFormat::R8G8B8A8
                       ? gfx::SurfaceFormat::B8G8R8A8
                       : gfx::SurfaceFormat::B8G8R8X8;
  wr::ImageDescriptor descriptor(GetSize(), formatTmp,
                                 aPreferCompositorSurface);
  (aResources.*method)(aImageKeys[0], descriptor, aExtID, imageType, 0);
}

void WaylandDMABUFTextureHostOGL::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys) {
  MOZ_ASSERT(aImageKeys.length() == 1);
  aBuilder.PushImage(aBounds, aClip, true, aFilter, aImageKeys[0],
                     !(mFlags & TextureFlags::NON_PREMULTIPLIED));
}

}  // namespace layers
}  // namespace mozilla
