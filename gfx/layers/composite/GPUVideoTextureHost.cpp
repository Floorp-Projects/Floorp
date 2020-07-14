/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GPUVideoTextureHost.h"
#include "mozilla/RemoteDecoderManagerParent.h"
#include "ImageContainer.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/VideoBridgeParent.h"
#include "mozilla/webrender/RenderTextureHostWrapper.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla {
namespace layers {

GPUVideoTextureHost::GPUVideoTextureHost(
    TextureFlags aFlags, const SurfaceDescriptorGPUVideo& aDescriptor)
    : TextureHost(aFlags), mDescriptor(aDescriptor) {
  MOZ_COUNT_CTOR(GPUVideoTextureHost);
}

GPUVideoTextureHost::~GPUVideoTextureHost() {
  MOZ_COUNT_DTOR(GPUVideoTextureHost);
}

GPUVideoTextureHost* GPUVideoTextureHost::CreateFromDescriptor(
    TextureFlags aFlags, const SurfaceDescriptorGPUVideo& aDescriptor) {
  return new GPUVideoTextureHost(aFlags, aDescriptor);
}

TextureHost* GPUVideoTextureHost::EnsureWrappedTextureHost() {
  if (mWrappedTextureHost) {
    return mWrappedTextureHost;
  }

  auto& sd = static_cast<SurfaceDescriptorRemoteDecoder&>(mDescriptor);
  mWrappedTextureHost =
      VideoBridgeParent::GetSingleton(sd.source())->LookupTexture(sd.handle());

  if (!mWrappedTextureHost) {
    return nullptr;
  }

  if (mWrappedTextureHost->AsBufferTextureHost()) {
    // TODO(miko): This code path is taken when WebRenderTextureHost wraps
    // GPUVideoTextureHost, which wraps BufferTextureHost.
    // Because this creates additional copies of the texture data, we should not
    // do this.
    mWrappedTextureHost->AsBufferTextureHost()->DisableExternalTextures();
  }

  if (mExternalImageId.isSome()) {
    // External image id is allocated by mWrappedTextureHost.
    mWrappedTextureHost->EnsureRenderTexture(Nothing());
    MOZ_ASSERT(mWrappedTextureHost->mExternalImageId.isSome());
    auto wrappedId = mWrappedTextureHost->mExternalImageId.ref();

    RefPtr<wr::RenderTextureHost> texture =
        new wr::RenderTextureHostWrapper(wrappedId);
    wr::RenderThread::Get()->RegisterExternalImage(
        wr::AsUint64(mExternalImageId.ref()), texture.forget());
  }

  return mWrappedTextureHost;
}

bool GPUVideoTextureHost::Lock() {
  if (!EnsureWrappedTextureHost()) {
    return false;
  }
  return EnsureWrappedTextureHost()->Lock();
}

void GPUVideoTextureHost::Unlock() {
  if (!EnsureWrappedTextureHost()) {
    return;
  }
  EnsureWrappedTextureHost()->Unlock();
}

void GPUVideoTextureHost::PrepareTextureSource(
    CompositableTextureSourceRef& aTexture) {
  if (!EnsureWrappedTextureHost()) {
    return;
  }
  EnsureWrappedTextureHost()->PrepareTextureSource(aTexture);
}

bool GPUVideoTextureHost::BindTextureSource(
    CompositableTextureSourceRef& aTexture) {
  if (!EnsureWrappedTextureHost()) {
    return false;
  }
  return EnsureWrappedTextureHost()->BindTextureSource(aTexture);
}

bool GPUVideoTextureHost::AcquireTextureSource(
    CompositableTextureSourceRef& aTexture) {
  if (!EnsureWrappedTextureHost()) {
    return false;
  }
  return EnsureWrappedTextureHost()->AcquireTextureSource(aTexture);
}

void GPUVideoTextureHost::SetTextureSourceProvider(
    TextureSourceProvider* aProvider) {
  if (EnsureWrappedTextureHost()) {
    EnsureWrappedTextureHost()->SetTextureSourceProvider(aProvider);
  }
}

gfx::YUVColorSpace GPUVideoTextureHost::GetYUVColorSpace() const {
  if (mWrappedTextureHost) {
    return mWrappedTextureHost->GetYUVColorSpace();
  }
  return gfx::YUVColorSpace::UNKNOWN;
}

gfx::ColorRange GPUVideoTextureHost::GetColorRange() const {
  if (mWrappedTextureHost) {
    return mWrappedTextureHost->GetColorRange();
  }
  return TextureHost::GetColorRange();
}

gfx::IntSize GPUVideoTextureHost::GetSize() const {
  if (!mWrappedTextureHost) {
    return gfx::IntSize();
  }
  return mWrappedTextureHost->GetSize();
}

gfx::SurfaceFormat GPUVideoTextureHost::GetFormat() const {
  if (!mWrappedTextureHost) {
    return gfx::SurfaceFormat::UNKNOWN;
  }
  return mWrappedTextureHost->GetFormat();
}

bool GPUVideoTextureHost::HasIntermediateBuffer() const {
  MOZ_ASSERT(mWrappedTextureHost);
  if (!mWrappedTextureHost) {
    return false;
  }

  return mWrappedTextureHost->HasIntermediateBuffer();
}

void GPUVideoTextureHost::UpdatedInternal(const nsIntRegion* Region) {
  if (!EnsureWrappedTextureHost()) {
    return;
  }
  EnsureWrappedTextureHost()->UpdatedInternal(Region);
}

void GPUVideoTextureHost::CreateRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  MOZ_ASSERT(mExternalImageId.isSome());

  // When mWrappedTextureHost already exist, call CreateRenderTexture() here.
  // In other cases, EnsureWrappedTextureHost() handles CreateRenderTexture().

  if (mWrappedTextureHost) {
    // External image id is allocated by mWrappedTextureHost.
    mWrappedTextureHost->EnsureRenderTexture(Nothing());
    MOZ_ASSERT(mWrappedTextureHost->mExternalImageId.isSome());
    auto wrappedId = mWrappedTextureHost->mExternalImageId.ref();

    RefPtr<wr::RenderTextureHost> texture =
        new wr::RenderTextureHostWrapper(wrappedId);
    wr::RenderThread::Get()->RegisterExternalImage(
        wr::AsUint64(mExternalImageId.ref()), texture.forget());
    return;
  }

  MOZ_ASSERT(EnsureWrappedTextureHost());
  EnsureWrappedTextureHost();
}

void GPUVideoTextureHost::MaybeDestroyRenderTexture() {
  if (mExternalImageId.isNothing() || !mWrappedTextureHost) {
    // RenderTextureHost was not created
    return;
  }
  // When GPUVideoTextureHost created RenderTextureHost, delete it here.
  TextureHost::DestroyRenderTexture(mExternalImageId.ref());
}

uint32_t GPUVideoTextureHost::NumSubTextures() {
  MOZ_ASSERT(EnsureWrappedTextureHost());
  if (!EnsureWrappedTextureHost()) {
    return 0;
  }
  return EnsureWrappedTextureHost()->NumSubTextures();
}

void GPUVideoTextureHost::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID) {
  MOZ_ASSERT(EnsureWrappedTextureHost());
  if (!EnsureWrappedTextureHost()) {
    return;
  }
  EnsureWrappedTextureHost()->PushResourceUpdates(aResources, aOp, aImageKeys,
                                                  aExtID);
}

void GPUVideoTextureHost::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys,
    const bool aPreferCompositorSurface) {
  MOZ_ASSERT(EnsureWrappedTextureHost());
  MOZ_ASSERT(aImageKeys.length() > 0);
  if (!EnsureWrappedTextureHost()) {
    return;
  }

  EnsureWrappedTextureHost()->PushDisplayItems(
      aBuilder, aBounds, aClip, aFilter, aImageKeys, aPreferCompositorSurface);
}

}  // namespace layers
}  // namespace mozilla
