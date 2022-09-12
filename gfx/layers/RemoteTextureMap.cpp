/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/RemoteTextureMap.h"

#include <vector>

#include "mozilla/layers/AsyncImagePipelineManager.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/RemoteTextureHostWrapper.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "mozilla/StaticPrefs_webgl.h"
#include "SharedSurface.h"

namespace mozilla::layers {

RemoteTextureOwnerClient::RemoteTextureOwnerClient(
    const base::ProcessId aForPid)
    : mForPid(aForPid) {}

RemoteTextureOwnerClient::~RemoteTextureOwnerClient() = default;

bool RemoteTextureOwnerClient::IsRegistered(
    const RemoteTextureOwnerId aOwnerId) {
  auto it = mOwnerIds.find(aOwnerId);
  if (it == mOwnerIds.end()) {
    return false;
  }
  return true;
}

void RemoteTextureOwnerClient::RegisterTextureOwner(
    const RemoteTextureOwnerId aOwnerId) {
  MOZ_ASSERT(mOwnerIds.find(aOwnerId) == mOwnerIds.end());
  mOwnerIds.emplace(aOwnerId);
  RemoteTextureMap::Get()->RegisterTextureOwner(aOwnerId, mForPid);
}

void RemoteTextureOwnerClient::UnregisterTextureOwner(
    const RemoteTextureOwnerId aOwnerId) {
  auto it = mOwnerIds.find(aOwnerId);
  if (it == mOwnerIds.end()) {
    return;
  }
  mOwnerIds.erase(it);
  RemoteTextureMap::Get()->UnregisterTextureOwner(aOwnerId, mForPid);
}

void RemoteTextureOwnerClient::UnregisterAllTextureOwners() {
  if (!mOwnerIds.empty()) {
    RemoteTextureMap::Get()->UnregisterTextureOwners(mOwnerIds, mForPid);
    mOwnerIds.clear();
  }
}

void RemoteTextureOwnerClient::PushTexture(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    UniquePtr<TextureData>&& aTextureData,
    const std::shared_ptr<gl::SharedSurface>& aSharedSurface) {
  MOZ_ASSERT(IsRegistered(aOwnerId));
  RemoteTextureMap::Get()->PushTexture(aTextureId, aOwnerId, mForPid,
                                       std::move(aTextureData), aSharedSurface);
}

UniquePtr<TextureData>
RemoteTextureOwnerClient::CreateOrRecycleBufferTextureData(
    const RemoteTextureOwnerId aOwnerId, gfx::IntSize aSize,
    gfx::SurfaceFormat aFormat) {
  auto texture = RemoteTextureMap::Get()->GetRecycledBufferTextureData(
      aOwnerId, mForPid, aSize, aFormat);
  if (texture) {
    return texture;
  }

  auto flags = TextureFlags::DEALLOCATE_CLIENT | TextureFlags::REMOTE_TEXTURE;
  auto* data = BufferTextureData::Create(aSize, aFormat, gfx::BackendType::SKIA,
                                         LayersBackend::LAYERS_WR, flags,
                                         ALLOC_DEFAULT, nullptr);
  return UniquePtr<TextureData>(data);
}

std::shared_ptr<gl::SharedSurface>
RemoteTextureOwnerClient::GetRecycledSharedSurface(
    const RemoteTextureOwnerId aOwnerId) {
  return RemoteTextureMap::Get()->RemoteTextureMap::GetRecycledSharedSurface(
      aOwnerId, mForPid);
}

StaticAutoPtr<RemoteTextureMap> RemoteTextureMap::sInstance;

/* static */
void RemoteTextureMap::Init() {
  MOZ_ASSERT(!sInstance);
  sInstance = new RemoteTextureMap();
}

/* static */
void RemoteTextureMap::Shutdown() {
  if (sInstance) {
    sInstance = nullptr;
  }
}

RemoteTextureMap::RemoteTextureMap() : mMutex("D3D11TextureMap::mMutexd") {}

RemoteTextureMap::~RemoteTextureMap() = default;

void RemoteTextureMap::PushTexture(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    const base::ProcessId aForPid, UniquePtr<TextureData>&& aTextureData,
    const std::shared_ptr<gl::SharedSurface>& aSharedSurface) {
  MOZ_RELEASE_ASSERT(aTextureData);

  RefPtr<TextureHost> textureHost =
      RemoteTextureMap::CreateRemoteTexture(aTextureData.get());
  if (!textureHost) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  {
    MutexAutoLock lock(mMutex);

    auto* owner = GetTextureOwner(lock, aOwnerId, aForPid);
    if (!owner) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return;
    }

    auto textureData = MakeUnique<TextureDataHolder>(
        aTextureId, textureHost, std::move(aTextureData), aSharedSurface);

    MOZ_ASSERT(owner->mLatestTextureId < aTextureId);

    owner->mWaitingTextureDataHolders.push(std::move(textureData));

    // Drop obsoleted remote textures.
    while (!owner->mUsingTextureDataHolders.empty()) {
      auto& front = owner->mUsingTextureDataHolders.front();
      // When compositable ref of TextureHost becomes 0, the TextureHost is not
      // used by WebRender anymore.
      if (front->mTextureHost &&
          front->mTextureHost->NumCompositableRefs() == 0) {
        // Recycle gl::SharedSurface
        if (front->mSharedSurface) {
          owner->mRecycledSharedSurfaces.push(front->mSharedSurface);
          front->mSharedSurface = nullptr;
        }
        // Recycle BufferTextureData
        if (front->mTextureData && front->mTextureData->AsBufferTextureData()) {
          owner->mRecycledTextures.push(std::move(front->mTextureData));
        }
        owner->mUsingTextureDataHolders.pop_front();
      } else if (front->mTextureHost &&
                 front->mTextureHost->NumCompositableRefs() >= 0) {
        // Remote texture is still in use by WebRender.
        break;
      } else {
        MOZ_ASSERT_UNREACHABLE("unexpected to be called");
        owner->mUsingTextureDataHolders.pop_front();
      }
    }
  }
}

void RemoteTextureMap::RegisterTextureOwner(const RemoteTextureOwnerId aOwnerId,
                                            const base::ProcessId aForPid) {
  MutexAutoLock lock(mMutex);

  const auto key = std::pair(aForPid, aOwnerId);
  auto it = mTextureOwners.find(key);
  if (it != mTextureOwners.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }
  auto owner = MakeUnique<TextureOwner>();
  mTextureOwners.emplace(key, std::move(owner));
}

void RemoteTextureMap::KeepTextureDataAliveForTextureHostIfNecessary(
    const MutexAutoLock& aProofOfLock,
    std::deque<UniquePtr<TextureDataHolder>>& aHolders) {
  for (auto& holder : aHolders) {
    // If remote texture of TextureHost still exist, keep
    // gl::SharedSurface/TextureData alive while the TextureHost is alive.
    if (holder->mTextureHost &&
        holder->mTextureHost->NumCompositableRefs() >= 0) {
      RefPtr<nsISerialEventTarget> eventTarget =
          MessageLoop::current()->SerialEventTarget();
      RefPtr<Runnable> runnable = NS_NewRunnableFunction(
          "RemoteTextureMap::UnregisterTextureOwner::Runnable",
          [data = std::move(holder->mTextureData),
           surface = std::move(holder->mSharedSurface)]() {});

      auto destroyedCallback = [eventTarget = std::move(eventTarget),
                                runnable = std::move(runnable)]() mutable {
        eventTarget->Dispatch(runnable.forget());
      };

      holder->mTextureHost->SetDestroyedCallback(destroyedCallback);
    }
  }
}

void RemoteTextureMap::UnregisterTextureOwner(
    const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid) {
  UniquePtr<TextureOwner> releasingOwner;  // Release outside the mutex
  RefPtr<TextureHost> releasingTexture;    // Release outside the mutex
  {
    MutexAutoLock lock(mMutex);

    const auto key = std::pair(aForPid, aOwnerId);
    auto it = mTextureOwners.find(key);
    if (it == mTextureOwners.end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return;
    }

    if (it->second->mLatestTextureHost) {
      // Release CompositableRef in mMutex
      releasingTexture = it->second->mLatestTextureHost;
      it->second->mLatestTextureHost = nullptr;
    }

    KeepTextureDataAliveForTextureHostIfNecessary(
        lock, it->second->mUsingTextureDataHolders);

    releasingOwner = std::move(it->second);
    mTextureOwners.erase(it);
  }
}

void RemoteTextureMap::UnregisterTextureOwners(
    const std::unordered_set<RemoteTextureOwnerId,
                             RemoteTextureOwnerId::HashFn>& aOwnerIds,
    const base::ProcessId aForPid) {
  std::vector<UniquePtr<TextureOwner>>
      releasingOwners;  // Release outside the mutex
  std::vector<RefPtr<TextureHost>>
      releasingTextures;  // Release outside the mutex
  {
    MutexAutoLock lock(mMutex);

    for (auto id : aOwnerIds) {
      const auto key = std::pair(aForPid, id);
      auto it = mTextureOwners.find(key);
      if (it == mTextureOwners.end()) {
        MOZ_ASSERT_UNREACHABLE("unexpected to be called");
        continue;
      }

      if (it->second->mLatestTextureHost) {
        // Release CompositableRef in mMutex
        releasingTextures.emplace_back(it->second->mLatestTextureHost);
        it->second->mLatestTextureHost = nullptr;
      }

      KeepTextureDataAliveForTextureHostIfNecessary(
          lock, it->second->mUsingTextureDataHolders);

      releasingOwners.push_back(std::move(it->second));
      mTextureOwners.erase(it);
    }
  }
}

/* static */
RefPtr<TextureHost> RemoteTextureMap::CreateRemoteTexture(
    TextureData* aTextureData) {
  SurfaceDescriptor desc;
  DebugOnly<bool> ret = aTextureData->Serialize(desc);
  MOZ_ASSERT(ret);
  TextureFlags flags =
      TextureFlags::REMOTE_TEXTURE | TextureFlags::DEALLOCATE_CLIENT;

  Maybe<wr::ExternalImageId> externalImageId = Nothing();
  RefPtr<TextureHost> textureHost =
      TextureHost::Create(desc, null_t(), nullptr, LayersBackend::LAYERS_WR,
                          flags, externalImageId);
  MOZ_ASSERT(textureHost);
  if (!textureHost) {
    gfxCriticalNoteOnce << "Failed to create remote texture";
    return nullptr;
  }

  textureHost->EnsureRenderTexture(Nothing());

  return textureHost;
}

void RemoteTextureMap::UpdateTexture(const MutexAutoLock& aProofOfLock,
                                     RemoteTextureMap::TextureOwner* aOwner,
                                     const RemoteTextureId aTextureId) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aOwner);
  MOZ_ASSERT(aTextureId >= aOwner->mLatestTextureId);

  if (aTextureId == aOwner->mLatestTextureId) {
    // No need to update texture.
    return;
  }

  // Move remote textures to mUsingTextureDataHolders.
  while (!aOwner->mWaitingTextureDataHolders.empty()) {
    auto& front = aOwner->mWaitingTextureDataHolders.front();
    if (aTextureId < front->mTextureId) {
      break;
    }
    MOZ_RELEASE_ASSERT(front->mTextureHost);
    aOwner->mLatestTextureHost = front->mTextureHost;
    aOwner->mLatestTextureId = front->mTextureId;

    UniquePtr<TextureDataHolder> holder = std::move(front);
    aOwner->mWaitingTextureDataHolders.pop();
    aOwner->mUsingTextureDataHolders.push_back(std::move(holder));
  }
}

void RemoteTextureMap::GetRemoteTextureHost(
    RemoteTextureHostWrapper* aTextureHostWrapper) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aTextureHostWrapper);

  const auto& textureId = aTextureHostWrapper->mTextureId;
  const auto& ownerId = aTextureHostWrapper->mOwnerId;
  const auto& forPid = aTextureHostWrapper->mForPid;
  const auto& size = aTextureHostWrapper->mSize;

  RefPtr<TextureHost> textureHost;
  {
    MutexAutoLock lock(mMutex);

    auto* owner = GetTextureOwner(lock, ownerId, forPid);
    if (!owner) {
      return;
    }

    UpdateTexture(lock, owner, textureId);

    if (StaticPrefs::webgl_out_of_process_async_present_force_sync()) {
      // remote texture sync ipc
      if (textureId == owner->mLatestTextureId) {
        MOZ_ASSERT(owner->mLatestTextureHost);
        MOZ_ASSERT(owner->mLatestTextureHost->GetSize() == size);
        textureHost = owner->mLatestTextureHost;
      } else {
        MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      }
    } else {
      // remote texture async ipc
      if (owner->mLatestTextureHost &&
          owner->mLatestTextureHost->GetSize() == size) {
        textureHost = owner->mLatestTextureHost;
      }
    }

    if (textureHost) {
      aTextureHostWrapper->SetRemoteTextureHost(lock, textureHost);
      aTextureHostWrapper->ApplyTextureFlagsToRemoteTexture();
    }
  }
}

void RemoteTextureMap::ReleaseRemoteTextureHost(
    RemoteTextureHostWrapper* aTextureHostWrapper) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aTextureHostWrapper);

  RefPtr<TextureHost> releasingTexture;  // Release outside the mutex
  {
    MutexAutoLock lock(mMutex);
    releasingTexture = aTextureHostWrapper->GetRemoteTextureHost(lock);
    aTextureHostWrapper->SetRemoteTextureHost(lock, nullptr);
  }
}

RefPtr<TextureHost> RemoteTextureMap::GetOrCreateRemoteTextureHostWrapper(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    const base::ProcessId aForPid, const gfx::IntSize aSize,
    const TextureFlags aFlags) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MutexAutoLock lock(mMutex);

  const auto key = std::pair(aForPid, aTextureId);
  auto it = mRemoteTextureHostWrappers.find(key);
  if (it != mRemoteTextureHostWrappers.end()) {
    return it->second;
  }

  auto textureHost = RemoteTextureHostWrapper::Create(aTextureId, aOwnerId,
                                                      aForPid, aSize, aFlags);
  mRemoteTextureHostWrappers.emplace(key, textureHost);

  return textureHost;
}

void RemoteTextureMap::UnregisterRemoteTextureHostWrapper(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    const base::ProcessId aForPid) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  {
    MutexAutoLock lock(mMutex);

    const auto key = std::pair(aForPid, aTextureId);
    auto it = mRemoteTextureHostWrappers.find(key);
    if (it == mRemoteTextureHostWrappers.end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return;
    }
    mRemoteTextureHostWrappers.erase(it);
  }
}

UniquePtr<TextureData> RemoteTextureMap::GetRecycledBufferTextureData(
    const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid,
    gfx::IntSize aSize, gfx::SurfaceFormat aFormat) {
  std::stack<UniquePtr<TextureData>>
      releasingTextures;  // Release outside the mutex
  UniquePtr<TextureData> texture;
  {
    MutexAutoLock lock(mMutex);

    auto* owner = GetTextureOwner(lock, aOwnerId, aForPid);
    if (!owner) {
      return nullptr;
    }

    if (owner->mRecycledTextures.empty()) {
      return nullptr;
    }

    if (!owner->mRecycledTextures.empty()) {
      auto& top = owner->mRecycledTextures.top();
      auto* bufferTexture = top->AsBufferTextureData();
      if (bufferTexture && bufferTexture->GetSize() == aSize &&
          bufferTexture->GetFormat() == aFormat) {
        texture = std::move(top);
        owner->mRecycledTextures.pop();
      } else {
        // If size or format are different, release all textures.
        owner->mRecycledTextures.swap(releasingTextures);
      }
    }
  }
  return texture;
}

std::shared_ptr<gl::SharedSurface> RemoteTextureMap::GetRecycledSharedSurface(
    const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid) {
  std::shared_ptr<gl::SharedSurface> sharedSurface;
  {
    MutexAutoLock lock(mMutex);

    auto* owner = GetTextureOwner(lock, aOwnerId, aForPid);
    if (!owner) {
      return nullptr;
    }

    if (owner->mRecycledSharedSurfaces.empty()) {
      return nullptr;
    }

    if (!owner->mRecycledSharedSurfaces.empty()) {
      sharedSurface = owner->mRecycledSharedSurfaces.top();
      owner->mRecycledSharedSurfaces.pop();
    }
  }
  return sharedSurface;
}

RemoteTextureMap::TextureOwner* RemoteTextureMap::GetTextureOwner(
    const MutexAutoLock& aProofOfLock, const RemoteTextureOwnerId aOwnerId,
    const base::ProcessId aForPid) {
  const auto key = std::pair(aForPid, aOwnerId);
  auto it = mTextureOwners.find(key);
  if (it == mTextureOwners.end()) {
    return nullptr;
  }
  return it->second.get();
}

RemoteTextureMap::TextureDataHolder::TextureDataHolder(
    const RemoteTextureId aTextureId, RefPtr<TextureHost> aTextureHost,
    UniquePtr<TextureData>&& aTextureData,
    const std::shared_ptr<gl::SharedSurface>& aSharedSurface)
    : mTextureId(aTextureId),
      mTextureHost(aTextureHost),
      mTextureData(std::move(aTextureData)),
      mSharedSurface(aSharedSurface) {}

}  // namespace mozilla::layers
