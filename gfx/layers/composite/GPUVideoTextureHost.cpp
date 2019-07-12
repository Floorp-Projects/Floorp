/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GPUVideoTextureHost.h"
#include "mozilla/RemoteDecoderManagerParent.h"
#include "ImageContainer.h"
#include "mozilla/layers/VideoBridgeParent.h"

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

  // In the future when the RDD process has a PVideoBridge connection,
  // then there might be two VideoBridgeParents (one within the GPU process,
  // one from RDD). We'll need to flag which one to use to lookup our
  // descriptor, or just try both.
  mWrappedTextureHost =
      VideoBridgeParent::GetSingleton()->LookupTexture(mDescriptor.handle());
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

void GPUVideoTextureHost::CreateRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  MOZ_ASSERT(EnsureWrappedTextureHost());
  if (!EnsureWrappedTextureHost()) {
    return;
  }

  EnsureWrappedTextureHost()->CreateRenderTexture(aExternalImageId);
}

uint32_t GPUVideoTextureHost::NumSubTextures() const {
  MOZ_ASSERT(mWrappedTextureHost);
  if (!mWrappedTextureHost) {
    return 0;
  }
  return mWrappedTextureHost->NumSubTextures();
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
    const Range<wr::ImageKey>& aImageKeys) {
  MOZ_ASSERT(EnsureWrappedTextureHost());
  MOZ_ASSERT(aImageKeys.length() > 0);
  if (!EnsureWrappedTextureHost()) {
    return;
  }

  EnsureWrappedTextureHost()->PushDisplayItems(aBuilder, aBounds, aClip,
                                               aFilter, aImageKeys);
}

bool GPUVideoTextureHost::SupportsWrNativeTexture() {
  MOZ_ASSERT(EnsureWrappedTextureHost());
  if (!EnsureWrappedTextureHost()) {
    return false;
  }
  return EnsureWrappedTextureHost()->SupportsWrNativeTexture();
}

}  // namespace layers
}  // namespace mozilla
