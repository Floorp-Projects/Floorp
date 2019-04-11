/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GPUVideoTextureHost.h"
#include "mozilla/VideoDecoderManagerParent.h"
#include "ImageContainer.h"
#include "mozilla/layers/VideoBridgeParent.h"

namespace mozilla {
namespace layers {

GPUVideoTextureHost::GPUVideoTextureHost(TextureFlags aFlags,
                                         TextureHost* aWrappedTextureHost)
    : TextureHost(aFlags), mWrappedTextureHost(aWrappedTextureHost) {
  MOZ_COUNT_CTOR(GPUVideoTextureHost);
}

GPUVideoTextureHost::~GPUVideoTextureHost() {
  MOZ_COUNT_DTOR(GPUVideoTextureHost);
}

GPUVideoTextureHost* GPUVideoTextureHost::CreateFromDescriptor(
    TextureFlags aFlags, const SurfaceDescriptorGPUVideo& aDescriptor) {
  TextureHost* wrappedTextureHost =
      VideoBridgeParent::GetSingleton()->LookupTexture(aDescriptor.handle());
  if (!wrappedTextureHost) {
    return nullptr;
  }
  return new GPUVideoTextureHost(aFlags, wrappedTextureHost);
}

bool GPUVideoTextureHost::Lock() {
  if (!mWrappedTextureHost) {
    return false;
  }
  return mWrappedTextureHost->Lock();
}

void GPUVideoTextureHost::Unlock() {
  if (!mWrappedTextureHost) {
    return;
  }
  mWrappedTextureHost->Unlock();
}

bool GPUVideoTextureHost::BindTextureSource(
    CompositableTextureSourceRef& aTexture) {
  if (!mWrappedTextureHost) {
    return false;
  }
  return mWrappedTextureHost->BindTextureSource(aTexture);
}

bool GPUVideoTextureHost::AcquireTextureSource(
    CompositableTextureSourceRef& aTexture) {
  if (!mWrappedTextureHost) {
    return false;
  }
  return mWrappedTextureHost->AcquireTextureSource(aTexture);
}

void GPUVideoTextureHost::SetTextureSourceProvider(
    TextureSourceProvider* aProvider) {
  if (mWrappedTextureHost) {
    mWrappedTextureHost->SetTextureSourceProvider(aProvider);
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

  return mWrappedTextureHost->HasIntermediateBuffer();
}

void GPUVideoTextureHost::CreateRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  MOZ_ASSERT(mWrappedTextureHost);

  mWrappedTextureHost->CreateRenderTexture(aExternalImageId);
}

uint32_t GPUVideoTextureHost::NumSubTextures() const {
  MOZ_ASSERT(mWrappedTextureHost);
  return mWrappedTextureHost->NumSubTextures();
}

void GPUVideoTextureHost::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID) {
  MOZ_ASSERT(mWrappedTextureHost);
  mWrappedTextureHost->PushResourceUpdates(aResources, aOp, aImageKeys, aExtID);
}

void GPUVideoTextureHost::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys) {
  MOZ_ASSERT(mWrappedTextureHost);
  MOZ_ASSERT(aImageKeys.length() > 0);

  mWrappedTextureHost->PushDisplayItems(aBuilder, aBounds, aClip, aFilter,
                                        aImageKeys);
}

bool GPUVideoTextureHost::SupportsWrNativeTexture() {
  MOZ_ASSERT(mWrappedTextureHost);
  return mWrappedTextureHost->SupportsWrNativeTexture();
}

}  // namespace layers
}  // namespace mozilla
