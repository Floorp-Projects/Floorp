/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GPUVideoTextureHost.h"

#include "ImageContainer.h"
#include "mozilla/RemoteDecoderManagerParent.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/VideoBridgeParent.h"
#include "mozilla/webrender/RenderTextureHostWrapper.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla {
namespace layers {

GPUVideoTextureHost::GPUVideoTextureHost(
    TextureFlags aFlags, const SurfaceDescriptorGPUVideo& aDescriptor)
    : TextureHost(TextureHostType::Unknown, aFlags), mDescriptor(aDescriptor) {
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

  const auto& sd =
      static_cast<const SurfaceDescriptorRemoteDecoder&>(mDescriptor);
  VideoBridgeParent* parent = VideoBridgeParent::GetSingleton(sd.source());
  if (!parent) {
    // The VideoBridge went away. This can happen if the RDD process
    // crashes.
    return nullptr;
  }
  mWrappedTextureHost = parent->LookupTexture(sd.handle());

  if (!mWrappedTextureHost) {
    // The TextureHost hasn't been registered yet. This is due to a race
    // between the ImageBridge (content) and the VideoBridge (RDD) and the
    // ImageBridge won. See bug
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1630733#c14 for more
    // details.
    return nullptr;
  }

  if (mExternalImageId.isSome()) {
    // External image id is allocated by mWrappedTextureHost.
    mWrappedTextureHost->EnsureRenderTexture(Nothing());
    MOZ_ASSERT(mWrappedTextureHost->mExternalImageId.isSome());
    auto wrappedId = mWrappedTextureHost->mExternalImageId.ref();

    RefPtr<wr::RenderTextureHost> texture =
        new wr::RenderTextureHostWrapper(wrappedId);
    wr::RenderThread::Get()->RegisterExternalImage(mExternalImageId.ref(),
                                                   texture.forget());
  }

  return mWrappedTextureHost;
}

bool GPUVideoTextureHost::IsValid() { return !!EnsureWrappedTextureHost(); }

gfx::YUVColorSpace GPUVideoTextureHost::GetYUVColorSpace() const {
  MOZ_ASSERT(mWrappedTextureHost, "Image isn't valid yet");
  if (!mWrappedTextureHost) {
    return TextureHost::GetYUVColorSpace();
  }
  return mWrappedTextureHost->GetYUVColorSpace();
}

gfx::ColorDepth GPUVideoTextureHost::GetColorDepth() const {
  MOZ_ASSERT(mWrappedTextureHost, "Image isn't valid yet");
  if (!mWrappedTextureHost) {
    return TextureHost::GetColorDepth();
  }
  return mWrappedTextureHost->GetColorDepth();
}

gfx::ColorRange GPUVideoTextureHost::GetColorRange() const {
  MOZ_ASSERT(mWrappedTextureHost, "Image isn't valid yet");
  if (!mWrappedTextureHost) {
    return TextureHost::GetColorRange();
  }
  return mWrappedTextureHost->GetColorRange();
}

gfx::IntSize GPUVideoTextureHost::GetSize() const {
  MOZ_ASSERT(mWrappedTextureHost, "Image isn't valid yet");
  if (!mWrappedTextureHost) {
    return gfx::IntSize();
  }
  return mWrappedTextureHost->GetSize();
}

gfx::SurfaceFormat GPUVideoTextureHost::GetFormat() const {
  MOZ_ASSERT(mWrappedTextureHost, "Image isn't valid yet");
  if (!mWrappedTextureHost) {
    return gfx::SurfaceFormat::UNKNOWN;
  }
  return mWrappedTextureHost->GetFormat();
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
    wr::RenderThread::Get()->RegisterExternalImage(mExternalImageId.ref(),
                                                   texture.forget());
    return;
  }

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
  if (!EnsureWrappedTextureHost()) {
    return 0;
  }
  return EnsureWrappedTextureHost()->NumSubTextures();
}

void GPUVideoTextureHost::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID) {
  MOZ_ASSERT(EnsureWrappedTextureHost(), "Image isn't valid yet");
  if (!EnsureWrappedTextureHost()) {
    return;
  }
  EnsureWrappedTextureHost()->PushResourceUpdates(aResources, aOp, aImageKeys,
                                                  aExtID);
}

void GPUVideoTextureHost::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys, PushDisplayItemFlagSet aFlags) {
  MOZ_ASSERT(EnsureWrappedTextureHost(), "Image isn't valid yet");
  MOZ_ASSERT(aImageKeys.length() > 0);
  if (!EnsureWrappedTextureHost()) {
    return;
  }

  EnsureWrappedTextureHost()->PushDisplayItems(aBuilder, aBounds, aClip,
                                               aFilter, aImageKeys, aFlags);
}

bool GPUVideoTextureHost::SupportsExternalCompositing(
    WebRenderBackend aBackend) {
  if (!EnsureWrappedTextureHost()) {
    return false;
  }
  return EnsureWrappedTextureHost()->SupportsExternalCompositing(aBackend);
}

void GPUVideoTextureHost::UnbindTextureSource() {
  if (EnsureWrappedTextureHost()) {
    EnsureWrappedTextureHost()->UnbindTextureSource();
  }
  // Handle read unlock
  TextureHost::UnbindTextureSource();
}

void GPUVideoTextureHost::NotifyNotUsed() {
  if (EnsureWrappedTextureHost()) {
    EnsureWrappedTextureHost()->NotifyNotUsed();
  }
  TextureHost::NotifyNotUsed();
}

bool GPUVideoTextureHost::IsWrappingBufferTextureHost() {
  if (EnsureWrappedTextureHost()) {
    return EnsureWrappedTextureHost()->IsWrappingBufferTextureHost();
  }
  return false;
}

TextureHostType GPUVideoTextureHost::GetTextureHostType() {
  if (!mWrappedTextureHost) {
    return TextureHostType::Unknown;
  }
  return mWrappedTextureHost->GetTextureHostType();
}

}  // namespace layers
}  // namespace mozilla
