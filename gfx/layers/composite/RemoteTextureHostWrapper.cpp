/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteTextureHostWrapper.h"

#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/AsyncImagePipelineManager.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/RemoteTextureMap.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "mozilla/StaticPrefs_webgl.h"
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

bool RemoteTextureHostWrapper::IsValid() {
  return !!mRemoteTextureForDisplayList;
}

gfx::YUVColorSpace RemoteTextureHostWrapper::GetYUVColorSpace() const {
  MOZ_ASSERT(mRemoteTextureForDisplayList, "TextureHost isn't valid yet");
  if (!mRemoteTextureForDisplayList) {
    return TextureHost::GetYUVColorSpace();
  }
  return mRemoteTextureForDisplayList->GetYUVColorSpace();
}

gfx::ColorDepth RemoteTextureHostWrapper::GetColorDepth() const {
  MOZ_ASSERT(mRemoteTextureForDisplayList, "TextureHost isn't valid yet");
  if (!mRemoteTextureForDisplayList) {
    return TextureHost::GetColorDepth();
  }
  return mRemoteTextureForDisplayList->GetColorDepth();
}

gfx::ColorRange RemoteTextureHostWrapper::GetColorRange() const {
  MOZ_ASSERT(mRemoteTextureForDisplayList, "TextureHost isn't valid yet");
  if (!mRemoteTextureForDisplayList) {
    return TextureHost::GetColorRange();
  }
  return mRemoteTextureForDisplayList->GetColorRange();
}

gfx::IntSize RemoteTextureHostWrapper::GetSize() const {
  MOZ_ASSERT(mRemoteTextureForDisplayList, "TextureHost isn't valid yet");
  if (!mRemoteTextureForDisplayList) {
    return gfx::IntSize();
  }
  return mRemoteTextureForDisplayList->GetSize();
}

gfx::SurfaceFormat RemoteTextureHostWrapper::GetFormat() const {
  MOZ_ASSERT(mRemoteTextureForDisplayList, "TextureHost isn't valid yet");
  if (!mRemoteTextureForDisplayList) {
    return gfx::SurfaceFormat::UNKNOWN;
  }
  return mRemoteTextureForDisplayList->GetFormat();
}

void RemoteTextureHostWrapper::CreateRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  MOZ_ASSERT(mExternalImageId.isSome());
  MOZ_ASSERT(mRemoteTextureForDisplayList);
  MOZ_ASSERT(mRemoteTextureForDisplayList->mExternalImageId.isSome());

  if (mIsSyncMode) {
    // sync mode
    // mRemoteTextureForDisplayList is also used for WebRender rendering.
    auto wrappedId = mRemoteTextureForDisplayList->mExternalImageId.ref();
    RefPtr<wr::RenderTextureHost> texture =
        new wr::RenderTextureHostWrapper(wrappedId);
    wr::RenderThread::Get()->RegisterExternalImage(mExternalImageId.ref(),
                                                   texture.forget());
  } else {
    // async mode
    // mRemoteTextureForDisplayList could be previous remote texture's
    // TextureHost that is compatible to the mTextureId's TextureHost.
    // mRemoteTextureForDisplayList might not be used WebRender rendering.
    RefPtr<wr::RenderTextureHost> texture =
        new wr::RenderTextureHostWrapper(mTextureId, mOwnerId, mForPid);
    wr::RenderThread::Get()->RegisterExternalImage(mExternalImageId.ref(),
                                                   texture.forget());
  }
}

uint32_t RemoteTextureHostWrapper::NumSubTextures() {
  if (!mRemoteTextureForDisplayList) {
    return 0;
  }
  return mRemoteTextureForDisplayList->NumSubTextures();
}

void RemoteTextureHostWrapper::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID) {
  MOZ_ASSERT(mRemoteTextureForDisplayList, "TextureHost isn't valid yet");
  if (!mRemoteTextureForDisplayList) {
    return;
  }
  mRemoteTextureForDisplayList->PushResourceUpdates(aResources, aOp, aImageKeys,
                                                    aExtID);
}

void RemoteTextureHostWrapper::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys, PushDisplayItemFlagSet aFlags) {
  MOZ_ASSERT(mRemoteTextureForDisplayList, "TextureHost isn't valid yet");
  MOZ_ASSERT(aImageKeys.length() > 0);
  if (!mRemoteTextureForDisplayList) {
    return;
  }
  mRemoteTextureForDisplayList->PushDisplayItems(aBuilder, aBounds, aClip,
                                                 aFilter, aImageKeys, aFlags);
}

bool RemoteTextureHostWrapper::SupportsExternalCompositing(
    WebRenderBackend aBackend) {
  if (!mRemoteTextureForDisplayList) {
    return false;
  }
  return mRemoteTextureForDisplayList->SupportsExternalCompositing(aBackend);
}

void RemoteTextureHostWrapper::UnbindTextureSource() {}

void RemoteTextureHostWrapper::NotifyNotUsed() {
  if (mRemoteTextureForDisplayList) {
    // Release mRemoteTextureForDisplayList.
    RemoteTextureMap::Get()->ReleaseRemoteTextureHostForDisplayList(this);
  }
  MOZ_ASSERT(!mRemoteTextureForDisplayList);

  RemoteTextureMap::Get()->UnregisterRemoteTextureHostWrapper(
      mTextureId, mOwnerId, mForPid);
}

TextureHostType RemoteTextureHostWrapper::GetTextureHostType() {
  if (!mRemoteTextureForDisplayList) {
    return TextureHostType::Unknown;
  }
  return mRemoteTextureForDisplayList->GetTextureHostType();
}

bool RemoteTextureHostWrapper::IsReadyForRendering() {
  return !!mRemoteTextureForDisplayList;
}

void RemoteTextureHostWrapper::ApplyTextureFlagsToRemoteTexture() {
  if (!mRemoteTextureForDisplayList) {
    return;
  }
  mRemoteTextureForDisplayList->SetFlags(mFlags |
                                         TextureFlags::DEALLOCATE_CLIENT);
}

TextureHost* RemoteTextureHostWrapper::GetRemoteTextureHostForDisplayList(
    const MonitorAutoLock& aProofOfLock) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mRemoteTextureForDisplayList;
}

void RemoteTextureHostWrapper::SetRemoteTextureHostForDisplayList(
    const MonitorAutoLock& aProofOfLock, TextureHost* aTextureHost,
    bool aIsSyncMode) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mRemoteTextureForDisplayList = aTextureHost;
  mIsSyncMode = aIsSyncMode;
}

void RemoteTextureHostWrapper::ClearRemoteTextureHostForDisplayList(
    const MonitorAutoLock& aProofOfLoc) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mRemoteTextureForDisplayList = nullptr;
}

bool RemoteTextureHostWrapper::IsWrappingSurfaceTextureHost() {
  if (!mRemoteTextureForDisplayList) {
    return false;
  }
  return mRemoteTextureForDisplayList->IsWrappingSurfaceTextureHost();
}

bool RemoteTextureHostWrapper::NeedsDeferredDeletion() const {
  if (!mRemoteTextureForDisplayList) {
    return true;
  }
  return mRemoteTextureForDisplayList->NeedsDeferredDeletion();
}

AndroidHardwareBuffer* RemoteTextureHostWrapper::GetAndroidHardwareBuffer()
    const {
  if (!mRemoteTextureForDisplayList) {
    return nullptr;
  }
  return mRemoteTextureForDisplayList->GetAndroidHardwareBuffer();
}

}  // namespace mozilla::layers
