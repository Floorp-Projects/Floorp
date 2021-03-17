/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DMABUFTextureHostOGL.h"
#include "mozilla/widget/DMABufSurface.h"
#include "mozilla/webrender/RenderDMABUFTextureHost.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "GLContextEGL.h"

namespace mozilla::layers {

DMABUFTextureHostOGL::DMABUFTextureHostOGL(TextureFlags aFlags,
                                           const SurfaceDescriptor& aDesc)
    : TextureHost(aFlags) {
  MOZ_COUNT_CTOR(DMABUFTextureHostOGL);

  // DMABufSurface::CreateDMABufSurface() can fail, for instance when we're run
  // out of file descriptors.
  mSurface =
      DMABufSurface::CreateDMABufSurface(aDesc.get_SurfaceDescriptorDMABuf());
}

DMABUFTextureHostOGL::~DMABUFTextureHostOGL() {
  MOZ_COUNT_DTOR(DMABUFTextureHostOGL);
}

GLTextureSource* DMABUFTextureHostOGL::CreateTextureSourceForPlane(
    size_t aPlane) {
  if (!mSurface) {
    return nullptr;
  }

  if (!mSurface->GetTexture(aPlane)) {
    if (!mSurface->CreateTexture(gl(), aPlane)) {
      return nullptr;
    }
  }

  return new GLTextureSource(
      mProvider, mSurface->GetTexture(aPlane), LOCAL_GL_TEXTURE_2D,
      gfx::IntSize(mSurface->GetWidth(aPlane), mSurface->GetHeight(aPlane)),
      // XXX: This isn't really correct (but isn't used), we should be using the
      // format of the individual plane, not of the whole buffer.
      mSurface->GetFormatGL());
}

bool DMABUFTextureHostOGL::Lock() {
  if (!gl() || !gl()->MakeCurrent() || !mSurface) {
    return false;
  }

  if (!mTextureSource) {
    mTextureSource = CreateTextureSourceForPlane(0);

    RefPtr<TextureSource> prev = mTextureSource;
    for (size_t i = 1; i < mSurface->GetTextureCount(); i++) {
      RefPtr<TextureSource> next = CreateTextureSourceForPlane(i);
      prev->SetNextSibling(next);
      prev = next;
    }
  }

  mSurface->FenceWait();
  return true;
}

void DMABUFTextureHostOGL::Unlock() {}

void DMABUFTextureHostOGL::SetTextureSourceProvider(
    TextureSourceProvider* aProvider) {
  if (!aProvider || !aProvider->GetGLContext()) {
    mTextureSource = nullptr;
    mProvider = nullptr;
    return;
  }

  mProvider = aProvider;

  if (mTextureSource) {
    mTextureSource->SetTextureSourceProvider(aProvider);
  }
}

gfx::SurfaceFormat DMABUFTextureHostOGL::GetFormat() const {
  if (!mSurface) {
    return gfx::SurfaceFormat::UNKNOWN;
  }
  return mSurface->GetFormat();
}

gfx::YUVColorSpace DMABUFTextureHostOGL::GetYUVColorSpace() const {
  if (!mSurface) {
    return gfx::YUVColorSpace::UNKNOWN;
  }
  return mSurface->GetYUVColorSpace();
}

gfx::ColorRange DMABUFTextureHostOGL::GetColorRange() const {
  if (!mSurface) {
    return gfx::ColorRange::LIMITED;
  }
  return mSurface->IsFullRange() ? gfx::ColorRange::FULL
                                 : gfx::ColorRange::LIMITED;
}

uint32_t DMABUFTextureHostOGL::NumSubTextures() {
  return mSurface ? mSurface->GetTextureCount() : 0;
}

gfx::IntSize DMABUFTextureHostOGL::GetSize() const {
  if (!mSurface) {
    return gfx::IntSize();
  }
  return gfx::IntSize(mSurface->GetWidth(), mSurface->GetHeight());
}

gl::GLContext* DMABUFTextureHostOGL::gl() const {
  return mProvider ? mProvider->GetGLContext() : nullptr;
}

void DMABUFTextureHostOGL::CreateRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  if (!mSurface) {
    return;
  }
  RefPtr<wr::RenderTextureHost> texture =
      new wr::RenderDMABUFTextureHost(mSurface);
  wr::RenderThread::Get()->RegisterExternalImage(wr::AsUint64(aExternalImageId),
                                                 texture.forget());
}

void DMABUFTextureHostOGL::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID) {
  if (!mSurface) {
    return;
  }

  auto method = aOp == TextureHost::ADD_IMAGE
                    ? &wr::TransactionBuilder::AddExternalImage
                    : &wr::TransactionBuilder::UpdateExternalImage;
  auto imageType =
      wr::ExternalImageType::TextureHandle(wr::ImageBufferKind::Texture2D);

  switch (mSurface->GetFormat()) {
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::B8G8R8X8:
    case gfx::SurfaceFormat::B8G8R8A8: {
      MOZ_ASSERT(aImageKeys.length() == 1);
      // XXX Add RGBA handling. Temporary hack to avoid crash
      // With BGRA format setting, rendering works without problem.
      wr::ImageDescriptor descriptor(GetSize(), mSurface->GetFormat());
      (aResources.*method)(aImageKeys[0], descriptor, aExtID, imageType, 0);
      break;
    }
    case gfx::SurfaceFormat::NV12: {
      MOZ_ASSERT(aImageKeys.length() == 2);
      MOZ_ASSERT(mSurface->GetTextureCount() == 2);
      wr::ImageDescriptor descriptor0(
          gfx::IntSize(mSurface->GetWidth(0), mSurface->GetHeight(0)),
          gfx::SurfaceFormat::A8);
      wr::ImageDescriptor descriptor1(
          gfx::IntSize(mSurface->GetWidth(1), mSurface->GetHeight(1)),
          gfx::SurfaceFormat::R8G8);
      (aResources.*method)(aImageKeys[0], descriptor0, aExtID, imageType, 0);
      (aResources.*method)(aImageKeys[1], descriptor1, aExtID, imageType, 1);
      break;
    }
    case gfx::SurfaceFormat::YUV: {
      MOZ_ASSERT(aImageKeys.length() == 3);
      MOZ_ASSERT(mSurface->GetTextureCount() == 3);
      wr::ImageDescriptor descriptor0(
          gfx::IntSize(mSurface->GetWidth(0), mSurface->GetHeight(0)),
          gfx::SurfaceFormat::A8);
      wr::ImageDescriptor descriptor1(
          gfx::IntSize(mSurface->GetWidth(1), mSurface->GetHeight(1)),
          gfx::SurfaceFormat::A8);
      (aResources.*method)(aImageKeys[0], descriptor0, aExtID, imageType, 0);
      (aResources.*method)(aImageKeys[1], descriptor1, aExtID, imageType, 1);
      (aResources.*method)(aImageKeys[2], descriptor1, aExtID, imageType, 2);
      break;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    }
  }
}

void DMABUFTextureHostOGL::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys, PushDisplayItemFlagSet aFlags) {
  if (!mSurface) {
    return;
  }
  bool preferCompositorSurface =
      aFlags.contains(PushDisplayItemFlag::PREFER_COMPOSITOR_SURFACE);
  switch (mSurface->GetFormat()) {
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8: {
      MOZ_ASSERT(aImageKeys.length() == 1);
      aBuilder.PushImage(aBounds, aClip, true, aFilter, aImageKeys[0],
                         !(mFlags & TextureFlags::NON_PREMULTIPLIED),
                         wr::ColorF{1.0f, 1.0f, 1.0f, 1.0f},
                         preferCompositorSurface);
      break;
    }
    case gfx::SurfaceFormat::NV12: {
      MOZ_ASSERT(aImageKeys.length() == 2);
      MOZ_ASSERT(mSurface->GetTextureCount() == 2);
      // Those images can only be generated at present by the VAAPI H264 decoder
      // which only supports 8 bits color depth.
      aBuilder.PushNV12Image(aBounds, aClip, true, aImageKeys[0], aImageKeys[1],
                             wr::ColorDepth::Color8,
                             wr::ToWrYuvColorSpace(GetYUVColorSpace()),
                             wr::ToWrColorRange(GetColorRange()), aFilter,
                             preferCompositorSurface);
      break;
    }
    case gfx::SurfaceFormat::YUV: {
      MOZ_ASSERT(aImageKeys.length() == 3);
      MOZ_ASSERT(mSurface->GetTextureCount() == 3);
      // Those images can only be generated at present by the VAAPI vp8 decoder
      // which only supports 8 bits color depth.
      aBuilder.PushYCbCrPlanarImage(
          aBounds, aClip, true, aImageKeys[0], aImageKeys[1], aImageKeys[2],
          wr::ColorDepth::Color8, wr::ToWrYuvColorSpace(GetYUVColorSpace()),
          wr::ToWrColorRange(GetColorRange()), aFilter,
          preferCompositorSurface);
      break;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    }
  }
}

}  // namespace mozilla::layers
