/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteTextureHostWrapper.h"

#include "mozilla/layers/AsyncImagePipelineManager.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/RemoteTextureMap.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "mozilla/webrender/RenderTextureHostWrapper.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla::layers {

/* static */
RefPtr<TextureHost> RemoteTextureHostWrapper::Create(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    const base::ProcessId aForPid, const gfx::IntSize aSize,
    const TextureFlags aFlags) {
  RefPtr<TextureHost> textureHost =
      new RemoteTextureHostWrapper(aTextureId, aOwnerId, aForPid, aSize,
                                   aFlags | TextureFlags::REMOTE_TEXTURE);
  auto externalImageId = AsyncImagePipelineManager::GetNextExternalImageId();
  textureHost = new WebRenderTextureHost(aFlags, textureHost, externalImageId);
  return textureHost;
}

RemoteTextureHostWrapper::RemoteTextureHostWrapper(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    const base::ProcessId aForPid, const gfx::IntSize aSize,
    const TextureFlags aFlags)
    : TextureHost(TextureHostType::Unknown, aFlags),
      mTextureId(aTextureId),
      mOwnerId(aOwnerId),
      mForPid(aForPid),
      mSize(aSize) {
  MOZ_COUNT_CTOR(RemoteTextureHostWrapper);
}

RemoteTextureHostWrapper::~RemoteTextureHostWrapper() {
  MOZ_COUNT_DTOR(RemoteTextureHostWrapper);
}

bool RemoteTextureHostWrapper::IsValid() { return !!mRemoteTextureHost; }

gfx::YUVColorSpace RemoteTextureHostWrapper::GetYUVColorSpace() const {
  MOZ_ASSERT(mRemoteTextureHost, "TextureHost isn't valid yet");
  if (!mRemoteTextureHost) {
    return TextureHost::GetYUVColorSpace();
  }
  return mRemoteTextureHost->GetYUVColorSpace();
}

gfx::ColorDepth RemoteTextureHostWrapper::GetColorDepth() const {
  MOZ_ASSERT(mRemoteTextureHost, "TextureHost isn't valid yet");
  if (!mRemoteTextureHost) {
    return TextureHost::GetColorDepth();
  }
  return mRemoteTextureHost->GetColorDepth();
}

gfx::ColorRange RemoteTextureHostWrapper::GetColorRange() const {
  MOZ_ASSERT(mRemoteTextureHost, "TextureHost isn't valid yet");
  if (!mRemoteTextureHost) {
    return TextureHost::GetColorRange();
  }
  return mRemoteTextureHost->GetColorRange();
}

gfx::IntSize RemoteTextureHostWrapper::GetSize() const {
  MOZ_ASSERT(mRemoteTextureHost, "TextureHost isn't valid yet");
  if (!mRemoteTextureHost) {
    return gfx::IntSize();
  }
  return mRemoteTextureHost->GetSize();
}

gfx::SurfaceFormat RemoteTextureHostWrapper::GetFormat() const {
  MOZ_ASSERT(mRemoteTextureHost, "TextureHost isn't valid yet");
  if (!mRemoteTextureHost) {
    return gfx::SurfaceFormat::UNKNOWN;
  }
  return mRemoteTextureHost->GetFormat();
}

void RemoteTextureHostWrapper::CreateRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  MOZ_ASSERT(mExternalImageId.isSome());
  MOZ_ASSERT(mRemoteTextureHost);
  MOZ_ASSERT(mRemoteTextureHost->mExternalImageId.isSome());

  auto wrappedId = mRemoteTextureHost->mExternalImageId.ref();

  RefPtr<wr::RenderTextureHost> texture =
      new wr::RenderTextureHostWrapper(wrappedId);
  wr::RenderThread::Get()->RegisterExternalImage(mExternalImageId.ref(),
                                                 texture.forget());
}

void RemoteTextureHostWrapper::MaybeDestroyRenderTexture() {
  if (mExternalImageId.isNothing() || !mRemoteTextureHost) {
    // RenderTextureHost was not created
    return;
  }
  TextureHost::DestroyRenderTexture(mExternalImageId.ref());
}

uint32_t RemoteTextureHostWrapper::NumSubTextures() {
  if (!mRemoteTextureHost) {
    return 0;
  }
  return mRemoteTextureHost->NumSubTextures();
}

void RemoteTextureHostWrapper::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID) {
  MOZ_ASSERT(mRemoteTextureHost, "TextureHost isn't valid yet");
  if (!mRemoteTextureHost) {
    return;
  }
  mRemoteTextureHost->PushResourceUpdates(aResources, aOp, aImageKeys, aExtID);
}

void RemoteTextureHostWrapper::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys, PushDisplayItemFlagSet aFlags) {
  MOZ_ASSERT(mRemoteTextureHost, "TextureHost isn't valid yet");
  MOZ_ASSERT(aImageKeys.length() > 0);
  if (!mRemoteTextureHost) {
    return;
  }
  mRemoteTextureHost->PushDisplayItems(aBuilder, aBounds, aClip, aFilter,
                                       aImageKeys, aFlags);
}

bool RemoteTextureHostWrapper::SupportsExternalCompositing(
    WebRenderBackend aBackend) {
  if (!mRemoteTextureHost) {
    return false;
  }
  return mRemoteTextureHost->SupportsExternalCompositing(aBackend);
}

void RemoteTextureHostWrapper::UnbindTextureSource() {}

void RemoteTextureHostWrapper::NotifyNotUsed() {
  if (mRemoteTextureHost) {
    // Release mRemoteTextureHost.
    RemoteTextureMap::Get()->ReleaseRemoteTextureHost(this);
  }
  MOZ_ASSERT(!mRemoteTextureHost);

  RemoteTextureMap::Get()->UnregisterRemoteTextureHostWrapper(
      mTextureId, mOwnerId, mForPid);
}

TextureHostType RemoteTextureHostWrapper::GetTextureHostType() {
  if (!mRemoteTextureHost) {
    return TextureHostType::Unknown;
  }
  return mRemoteTextureHost->GetTextureHostType();
}

bool RemoteTextureHostWrapper::CheckIsReadyForRendering() {
  if (!mRemoteTextureHost) {
    // mRemoteTextureHost might be updated.
    RemoteTextureMap::Get()->GetRemoteTextureHost(this);
  }
  return !!mRemoteTextureHost;
}

void RemoteTextureHostWrapper::ApplyTextureFlagsToRemoteTexture() {
  if (!mRemoteTextureHost) {
    return;
  }
  mRemoteTextureHost->SetFlags(mFlags | TextureFlags::DEALLOCATE_CLIENT);
}

TextureHost* RemoteTextureHostWrapper::GetRemoteTextureHost(
    const MutexAutoLock& aProofOfLock) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mRemoteTextureHost;
}

void RemoteTextureHostWrapper::SetRemoteTextureHost(
    const MutexAutoLock& aProofOfLock, TextureHost* aTextureHost) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mRemoteTextureHost = aTextureHost;
}

}  // namespace mozilla::layers
