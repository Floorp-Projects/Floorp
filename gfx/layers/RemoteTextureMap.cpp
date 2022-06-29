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
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/WebRenderTextureHost.h"
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

void RemoteTextureOwnerClient::PushTexure(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    UniquePtr<TextureData>&& aTextureData,
    const std::shared_ptr<gl::SharedSurface>& aSharedSurface) {
  MOZ_ASSERT(IsRegistered(aOwnerId));
  RemoteTextureMap::Get()->PushTexure(aTextureId, aOwnerId, mForPid,
                                      std::move(aTextureData), aSharedSurface);
}

UniquePtr<TextureData>
RemoteTextureOwnerClient::CreateOrRecycleBufferTextureData(
    const RemoteTextureOwnerId aOwnerId, gfx::IntSize aSize,
    gfx::SurfaceFormat aFormat) {
  auto texture =
      RemoteTextureMap::Get()->RemoteTextureMap::GetRecycledBufferTextureData(
          aOwnerId, mForPid, aSize, aFormat);
  if (texture) {
    return texture;
  }

  auto* data = BufferTextureData::Create(
      aSize, aFormat, gfx::BackendType::SKIA, LayersBackend::LAYERS_WR,
      TextureFlags::DEALLOCATE_CLIENT, ALLOC_DEFAULT, nullptr);
  return UniquePtr<TextureData>(data);
}

/* static */
UniquePtr<RemoteTextureConsumerClient> RemoteTextureConsumerClient::Create(
    const RemoteTextureOwnerId aOwnerId, const CompositableHandle& aHandle,
    const base::ProcessId aForPid) {
  return MakeUnique<RemoteTextureConsumerClient>(aOwnerId, aHandle, aForPid);
}

RemoteTextureConsumerClient::RemoteTextureConsumerClient(
    const RemoteTextureOwnerId aOwnerId, const CompositableHandle& aHandle,
    const base::ProcessId aForPid)
    : mOwnerId(aOwnerId), mHandle(aHandle), mForPid(aForPid) {}

RemoteTextureConsumerClient::~RemoteTextureConsumerClient() {
  RemoteTextureMap::Get()->UnregisterTextureConsumer(mOwnerId, mHandle,
                                                     mForPid);
}

RefPtr<TextureHost> RemoteTextureConsumerClient::GetTextureHost(
    const RemoteTextureId aTextureId, const gfx::IntSize aSize,
    const TextureFlags aFlags, const RemoteTextureCallback& aCallback) {
  return RemoteTextureMap::Get()->GetTextureHost(
      aTextureId, mOwnerId, mHandle, mForPid, aSize, aFlags, aCallback);
}

std::shared_ptr<gl::SharedSurface>
RemoteTextureOwnerClient::GetRecycledSharedSurface(
    const RemoteTextureOwnerId aOwnerId) {
  return RemoteTextureMap::Get()->RemoteTextureMap::GetRecycledSharedSurface(
      aOwnerId, mForPid);
}

SharedSurfaceWrapper::SharedSurfaceWrapper(
    const std::shared_ptr<gl::SharedSurface>& aSharedSurface)
    : mSharedSurface(aSharedSurface) {}

SharedSurfaceWrapper::~SharedSurfaceWrapper() = default;

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

void RemoteTextureMap::PushTexure(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    const base::ProcessId aForPid, UniquePtr<TextureData>&& aTextureData,
    const std::shared_ptr<gl::SharedSurface>& aSharedSurface) {
  MOZ_RELEASE_ASSERT(aTextureData);

  std::vector<RemoteTextureCallback> callbacks;  // Release outside the mutex
  {
    MutexAutoLock lock(mMutex);

    auto* owner = GetTextureOwner(lock, aOwnerId, aForPid);
    if (!owner) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return;
    }

    ThreadSafeWeakPtr<SharedSurfaceWrapper> weak;
    if (aSharedSurface) {
      RefPtr<SharedSurfaceWrapper> wrapper =
          new SharedSurfaceWrapper(aSharedSurface);
      weak = wrapper;
      owner->mSharedSurfaces.push(wrapper);
    }
    auto textureData = MakeUnique<TextureDataWrapper>(
        aTextureId, std::move(aTextureData), weak);
    owner->mTextureDataWrappers.push(std::move(textureData));

    // Release unused surfaces outside the mutex.
    // SharedSurface needs to be released on creation thread.
    while (!owner->mSharedSurfaces.empty()) {
      auto& front = owner->mSharedSurfaces.front();
      if (front->IsUsed()) {
        break;
      }
      owner->mRecycledSharedSurfaces.push(front);
      owner->mSharedSurfaces.pop();
    }

    auto* consumer = GetTextureConsumer(lock, aOwnerId, aForPid);
    if (consumer) {
      while (!consumer->mCallbacks.empty()) {
        auto& front = consumer->mCallbacks.front();
        if (front.first > aTextureId) {
          break;
        }
        callbacks.push_back(front.second);
        consumer->mCallbacks.pop();
      }
    }
  }

  // Call callback when it is requested.
  if (!callbacks.empty()) {
    // Only last/latest callback needs to be called.
    auto& callback = callbacks.back();
    callback(aTextureId, aOwnerId, aForPid);
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

void RemoteTextureMap::UnregisterTextureOwner(
    const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid) {
  UniquePtr<TextureOwner> owner;  // Release outside the mutex
  {
    MutexAutoLock lock(mMutex);

    const auto key = std::pair(aForPid, aOwnerId);
    auto it = mTextureOwners.find(key);
    if (it == mTextureOwners.end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return;
    }
    owner = std::move(it->second);
    mTextureOwners.erase(it);
  }
}

void RemoteTextureMap::UnregisterTextureOwners(
    const std::unordered_set<RemoteTextureOwnerId,
                             RemoteTextureOwnerId::HashFn>& aOwnerIds,
    const base::ProcessId aForPid) {
  std::vector<UniquePtr<TextureOwner>> owners;  // Release outside the mutex
  {
    MutexAutoLock lock(mMutex);

    for (auto id : aOwnerIds) {
      const auto key = std::pair(aForPid, id);
      auto it = mTextureOwners.find(key);
      if (it == mTextureOwners.end()) {
        MOZ_ASSERT_UNREACHABLE("unexpected to be called");
        continue;
      }
      owners.push_back(std::move(it->second));
      mTextureOwners.erase(it);
    }
  }
}

UniquePtr<RemoteTextureConsumerClient>
RemoteTextureMap::RegisterTextureConsumer(const RemoteTextureOwnerId aOwnerId,
                                          const CompositableHandle& aHandle,
                                          const base::ProcessId aForPid) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  UniquePtr<TextureConsumer> obsoletedConsumer;  // Release outside the mutex
  auto client = RemoteTextureConsumerClient::Create(aOwnerId, aHandle, aForPid);

  {
    MutexAutoLock lock(mMutex);

    const auto key = std::pair(aForPid, aOwnerId);
    auto it = mTextureConsumers.find(key);
    // Remove obsoleted consumer.
    if (it != mTextureConsumers.end()) {
      obsoletedConsumer = std::move(it->second);
      mTextureConsumers.erase(it);
    }
    auto consumer = MakeUnique<TextureConsumer>(client->mHandle);
    mTextureConsumers.emplace(key, std::move(consumer));
  }
  return client;
}

void RemoteTextureMap::UnregisterTextureConsumer(
    const RemoteTextureOwnerId aOwnerId, const CompositableHandle& aHandle,
    const base::ProcessId aForPid) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  UniquePtr<TextureConsumer> consumer;  // Release outside the mutex
  {
    MutexAutoLock lock(mMutex);

    const auto key = std::pair(aForPid, aOwnerId);
    auto it = mTextureConsumers.find(key);
    if (it == mTextureConsumers.end()) {
      return;
    }
    if (aHandle != it->second->mHandle) {
      // Consumer is alredy obsoleted.
      return;
    }
    consumer = std::move(it->second);
    mTextureConsumers.erase(it);
  }
}

RefPtr<TextureHost> RemoteTextureMap::CreateRemoteTexture(
    const gfx::IntSize aSize, const TextureFlags aFlags,
    TextureDataWrapper* aTextureData) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  TextureData::Info info;
  aTextureData->mTextureData->FillInfo(info);
  if (aSize != info.size) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    gfxCriticalNoteOnce << "Invalid remote texture size " << aSize << " "
                        << info.size;
    return nullptr;
  }

  SurfaceDescriptor desc;
  DebugOnly<bool> ret = aTextureData->mTextureData->Serialize(desc);
  MOZ_ASSERT(ret);
  TextureFlags flags = aFlags | TextureFlags::REMOTE_TEXTURE;

  // The BufferTextureData owns its buffer data. It needs to be alive during
  // host side usage.
  if (aTextureData->mTextureData->AsBufferTextureData()) {
    flags |= TextureFlags::DEALLOCATE_CLIENT;
  }

  Maybe<wr::ExternalImageId> externalImageId =
      Some(AsyncImagePipelineManager::GetNextExternalImageId());
  RefPtr<TextureHost> textureHost =
      TextureHost::Create(desc, null_t(), nullptr, LayersBackend::LAYERS_WR,
                          flags, externalImageId);
  MOZ_ASSERT(textureHost);
  if (!textureHost) {
    gfxCriticalNoteOnce << "Failed to create remote texture";
    return nullptr;
  }

  textureHost =
      new WebRenderTextureHost(desc, flags, textureHost, *externalImageId);
  return textureHost;
}

RefPtr<TextureHost> RemoteTextureMap::GetTextureHost(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    const CompositableHandle& aHandle, const base::ProcessId aForPid,
    const gfx::IntSize aSize, const TextureFlags aFlags,
    const RemoteTextureCallback& aCallback) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  UniquePtr<TextureDataWrapper> textureData;
  {
    MutexAutoLock lock(mMutex);
    auto* consumer = GetTextureConsumer(lock, aOwnerId, aForPid);
    if (!consumer) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return nullptr;
    }
    if (aHandle != consumer->mHandle) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return nullptr;
    }

    auto* owner = GetTextureOwner(lock, aOwnerId, aForPid);
    if (!owner) {
      consumer->mCallbacks.emplace(std::pair(aTextureId, aCallback));
      return nullptr;
    }

    if (aTextureId == owner->mLatestTextureId) {
      return owner->mLatestTextureHost;
    }

    if (aTextureId < owner->mLatestTextureId) {
      // Remote texture id is incremental. New id should be larger than the
      // previous one.
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return nullptr;
    }

    MOZ_ASSERT(aTextureId > owner->mLatestTextureId);

    // TextureDataWrapper is not registered yet.
    if (owner->mTextureDataWrappers.empty()) {
      consumer->mCallbacks.emplace(std::pair(aTextureId, aCallback));
      return nullptr;
    }

    auto& front = owner->mTextureDataWrappers.front();
    if (aTextureId < front->mTextureId) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return nullptr;
    }

    textureData = std::move(front);
    owner->mTextureDataWrappers.pop();
  }

  // If texture data exist, create TextureHost.
  RefPtr<TextureHost> textureHost =
      CreateRemoteTexture(aSize, aFlags, textureData.get());
  if (!textureHost) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return nullptr;
  }

  RefPtr<TextureHost> releasingTexture;  // Release outside the mutex
  {
    MutexAutoLock lock(mMutex);
    auto* owner = GetTextureOwner(lock, aOwnerId, aForPid);
    if (!owner) {
      return nullptr;
    }
    bool keepAliveNeeded = false;
    // This BufferTextureData owns its buffer data. It needs to be alive during
    // host side usage.
    if (textureData->mTextureData->AsBufferTextureData()) {
      keepAliveNeeded = true;
    }
    // Some SharedSurfaces needs to be alive during their usages.
    if (!textureData->mSharedSurface.IsNull()) {
      keepAliveNeeded = true;
    }

    if (keepAliveNeeded) {
      auto destroyedCallback = [=]() {
        RemoteTextureMap::Get()->NotifyReleased(aTextureId, aOwnerId, aForPid);
      };
      const auto key = std::pair(aForPid, aTextureId);
      mKeepAlives.emplace(key, std::move(textureData));
      // Keep alive TextureData until TextureHost is destroyed.
      textureHost->SetDestroyedCallback(destroyedCallback);
    }

    owner->mLatestTextureId = aTextureId;
    releasingTexture = owner->mLatestTextureHost.forget();
    owner->mLatestTextureHost = textureHost;
  }

  return textureHost;
}

void RemoteTextureMap::NotifyReleased(const RemoteTextureId aTextureId,
                                      const RemoteTextureOwnerId aOwnerId,
                                      const base::ProcessId aForPid) {
  MutexAutoLock lock(mMutex);

  const auto key = std::pair(aForPid, aTextureId);
  auto it = mKeepAlives.find(key);
  if (it == mKeepAlives.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }
  UniquePtr<TextureDataWrapper> textureData = std::move(it->second);
  mKeepAlives.erase(it);

  auto* owner = GetTextureOwner(lock, aOwnerId, aForPid);
  if (!owner) {
    return;
  }

  if (!textureData->mSharedSurface.IsNull()) {
    auto sharedSurface =
        RefPtr<SharedSurfaceWrapper>(textureData->mSharedSurface);
    if (sharedSurface) {
      // SharedSurface could be recycled.
      sharedSurface->EndUsage();
    }
  }

  // Recycle BufferTextureData
  if (textureData->mTextureData &&
      textureData->mTextureData->AsBufferTextureData()) {
    owner->mRecycledTextures.push(std::move(textureData->mTextureData));
  }
}

UniquePtr<TextureData> RemoteTextureMap::GetRecycledBufferTextureData(
    const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid,
    gfx::IntSize aSize, gfx::SurfaceFormat aFormat) {
  std::stack<UniquePtr<TextureData>> textures;
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
        owner->mRecycledTextures.swap(textures);
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
      sharedSurface = owner->mRecycledSharedSurfaces.top()->mSharedSurface;
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

RemoteTextureMap::TextureConsumer* RemoteTextureMap::GetTextureConsumer(
    const MutexAutoLock& aProofOfLock, const RemoteTextureOwnerId aOwnerId,
    const base::ProcessId aForPid) {
  const auto key = std::pair(aForPid, aOwnerId);
  auto it = mTextureConsumers.find(key);
  if (it == mTextureConsumers.end()) {
    return nullptr;
  }
  return it->second.get();
}

RemoteTextureMap::TextureDataWrapper::TextureDataWrapper(
    const RemoteTextureId aTextureId, UniquePtr<TextureData>&& aTextureData,
    const ThreadSafeWeakPtr<SharedSurfaceWrapper>& aSharedSurface)
    : mTextureId(aTextureId),
      mTextureData(std::move(aTextureData)),
      mSharedSurface(aSharedSurface) {}

}  // namespace mozilla::layers
