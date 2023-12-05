/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/RemoteTextureMap.h"

#include <vector>

#include "CompositableHost.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/AsyncImagePipelineManager.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/RemoteTextureHostWrapper.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "mozilla/StaticPrefs_webgl.h"
#include "mozilla/webgpu/ExternalTexture.h"
#include "mozilla/webrender/RenderThread.h"
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
    const RemoteTextureOwnerId aOwnerId, bool aIsSyncMode) {
  MOZ_ASSERT(mOwnerIds.find(aOwnerId) == mOwnerIds.end());
  mOwnerIds.emplace(aOwnerId);
  RemoteTextureMap::Get()->RegisterTextureOwner(aOwnerId, mForPid, aIsSyncMode);
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
  if (mOwnerIds.empty()) {
    return;
  }
  RemoteTextureMap::Get()->UnregisterTextureOwners(mOwnerIds, mForPid);
  mOwnerIds.clear();
}

void RemoteTextureOwnerClient::NotifyContextLost() {
  if (mOwnerIds.empty()) {
    return;
  }
  RemoteTextureMap::Get()->NotifyContextLost(mOwnerIds, mForPid);
}

void RemoteTextureOwnerClient::PushTexture(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    UniquePtr<TextureData>&& aTextureData) {
  MOZ_ASSERT(IsRegistered(aOwnerId));

  RefPtr<TextureHost> textureHost = RemoteTextureMap::CreateRemoteTexture(
      aTextureData.get(), TextureFlags::DEFAULT);
  if (!textureHost) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  RemoteTextureMap::Get()->PushTexture(aTextureId, aOwnerId, mForPid,
                                       std::move(aTextureData), textureHost,
                                       /* aResourceWrapper */ nullptr);
}

void RemoteTextureOwnerClient::PushTexture(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    UniquePtr<TextureData>&& aTextureData,
    const std::shared_ptr<gl::SharedSurface>& aSharedSurface) {
  MOZ_ASSERT(IsRegistered(aOwnerId));

  RefPtr<TextureHost> textureHost = RemoteTextureMap::CreateRemoteTexture(
      aTextureData.get(), TextureFlags::DEFAULT);
  if (!textureHost) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  RemoteTextureMap::Get()->PushTexture(
      aTextureId, aOwnerId, mForPid, std::move(aTextureData), textureHost,
      SharedResourceWrapper::SharedSurface(aSharedSurface));
}

void RemoteTextureOwnerClient::PushTexture(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    UniquePtr<TextureData>&& aTextureData,
    const std::shared_ptr<webgpu::ExternalTexture>& aExternalTexture) {
  MOZ_ASSERT(IsRegistered(aOwnerId));

  RefPtr<TextureHost> textureHost = RemoteTextureMap::CreateRemoteTexture(
      aTextureData.get(), TextureFlags::DEFAULT);
  if (!textureHost) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  RemoteTextureMap::Get()->PushTexture(
      aTextureId, aOwnerId, mForPid, std::move(aTextureData), textureHost,
      SharedResourceWrapper::ExternalTexture(aExternalTexture));
}

void RemoteTextureOwnerClient::PushDummyTexture(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId) {
  MOZ_ASSERT(IsRegistered(aOwnerId));

  auto flags = TextureFlags::DEALLOCATE_CLIENT | TextureFlags::REMOTE_TEXTURE |
               TextureFlags::DUMMY_TEXTURE;
  auto* rawData = BufferTextureData::Create(
      gfx::IntSize(1, 1), gfx::SurfaceFormat::B8G8R8A8, gfx::BackendType::SKIA,
      LayersBackend::LAYERS_WR, flags, ALLOC_DEFAULT, nullptr);
  if (!rawData) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  auto textureData = UniquePtr<TextureData>(rawData);

  RefPtr<TextureHost> textureHost = RemoteTextureMap::CreateRemoteTexture(
      textureData.get(), TextureFlags::DUMMY_TEXTURE);
  if (!textureHost) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  RemoteTextureMap::Get()->PushTexture(aTextureId, aOwnerId, mForPid,
                                       std::move(textureData), textureHost,
                                       /* aResourceWrapper */ nullptr);
}

void RemoteTextureOwnerClient::GetLatestBufferSnapshot(
    const RemoteTextureOwnerId aOwnerId, const mozilla::ipc::Shmem& aDestShmem,
    const gfx::IntSize& aSize) {
  MOZ_ASSERT(IsRegistered(aOwnerId));
  RemoteTextureMap::Get()->GetLatestBufferSnapshot(aOwnerId, mForPid,
                                                   aDestShmem, aSize);
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
  UniquePtr<SharedResourceWrapper> wrapper =
      RemoteTextureMap::Get()->RemoteTextureMap::GetRecycledSharedTexture(
          aOwnerId, mForPid);
  if (!wrapper) {
    return nullptr;
  }
  MOZ_ASSERT(wrapper->mSharedSurface);
  return wrapper->mSharedSurface;
}

std::shared_ptr<webgpu::ExternalTexture>
RemoteTextureOwnerClient::GetRecycledExternalTexture(
    const RemoteTextureOwnerId aOwnerId) {
  UniquePtr<SharedResourceWrapper> wrapper =
      RemoteTextureMap::Get()->RemoteTextureMap::GetRecycledSharedTexture(
          aOwnerId, mForPid);
  if (!wrapper) {
    return nullptr;
  }
  MOZ_ASSERT(wrapper->mExternalTexture);
  return wrapper->mExternalTexture;
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

RemoteTextureMap::RemoteTextureMap() : mMonitor("D3D11TextureMap::mMonitor") {}

RemoteTextureMap::~RemoteTextureMap() = default;

void RemoteTextureMap::PushTexture(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    const base::ProcessId aForPid, UniquePtr<TextureData>&& aTextureData,
    RefPtr<TextureHost>& aTextureHost,
    UniquePtr<SharedResourceWrapper>&& aResourceWrapper) {
  MOZ_RELEASE_ASSERT(aTextureHost);

  std::vector<RefPtr<TextureHost>>
      releasingTextures;  // Release outside the monitor
  std::vector<std::function<void(const RemoteTextureInfo&)>>
      renderingReadyCallbacks;  // Call outside the monitor
  {
    MonitorAutoLock lock(mMonitor);

    auto* owner = GetTextureOwner(lock, aOwnerId, aForPid);
    if (!owner) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return;
    }

    if (owner->mIsContextLost &&
        !(aTextureHost->GetFlags() & TextureFlags::DUMMY_TEXTURE)) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      gfxCriticalNoteOnce << "Texture pushed during context lost";
    }

    const auto key = std::pair(aForPid, aOwnerId);
    auto it = mRemoteTexturePushListeners.find(key);
    // Notify a new texture if callback is requested
    if (it != mRemoteTexturePushListeners.end()) {
      RefPtr<CompositableHost> compositableHost = it->second;
      RefPtr<Runnable> runnable = NS_NewRunnableFunction(
          "RemoteTextureMap::PushTexture::Runnable",
          [compositableHost, aTextureId, aOwnerId, aForPid]() {
            compositableHost->NotifyPushTexture(aTextureId, aOwnerId, aForPid);
          });
      CompositorThread()->Dispatch(runnable.forget());
    }

    auto textureData = MakeUnique<TextureDataHolder>(
        aTextureId, aTextureHost, std::move(aTextureData),
        std::move(aResourceWrapper));

    MOZ_ASSERT(owner->mLatestTextureId < aTextureId);

    owner->mWaitingTextureDataHolders.push_back(std::move(textureData));

    if (!owner->mIsSyncMode) {
      renderingReadyCallbacks =
          GetRenderingReadyCallbacks(lock, owner, aTextureId);
      // Update mAsyncRemoteTextureHost for async mode.
      // This happens when PushTexture() with RemoteTextureId is called after
      // GetRemoteTextureForDisplayList() with the RemoteTextureId.
      const auto key = std::pair(aForPid, aTextureId);
      auto it = mRemoteTextureHostWrapperHolders.find(key);
      if (it != mRemoteTextureHostWrapperHolders.end()) {
        MOZ_ASSERT(!it->second->mAsyncRemoteTextureHost);
        it->second->mAsyncRemoteTextureHost = aTextureHost;
      }
    }

    mMonitor.Notify();

    // Release owner->mReleasingRenderedTextureHosts before checking
    // NumCompositableRefs()
    if (!owner->mReleasingRenderedTextureHosts.empty()) {
      std::transform(
          owner->mReleasingRenderedTextureHosts.begin(),
          owner->mReleasingRenderedTextureHosts.end(),
          std::back_inserter(releasingTextures),
          [](CompositableTextureHostRef& aRef) { return aRef.get(); });
      owner->mReleasingRenderedTextureHosts.clear();
    }

    // Drop obsoleted remote textures.
    while (!owner->mUsingTextureDataHolders.empty()) {
      auto& front = owner->mUsingTextureDataHolders.front();
      // If mLatestRenderedTextureHost is last compositable ref of remote
      // texture's TextureHost, its RemoteTextureHostWrapper is already
      // unregistered. It happens when pushed remote textures that follow are
      // not rendered since last mLatestRenderedTextureHost update. In this
      // case, remove the TextureHost from mUsingTextureDataHolders. It is for
      // unblocking remote texture recyclieng.
      if (front->mTextureHost &&
          front->mTextureHost->NumCompositableRefs() == 1 &&
          front->mTextureHost == owner->mLatestRenderedTextureHost) {
        owner->mUsingTextureDataHolders.pop_front();
        continue;
      }
      // When compositable ref of TextureHost becomes 0, the TextureHost is not
      // used by WebRender anymore.
      if (front->mTextureHost &&
          front->mTextureHost->NumCompositableRefs() == 0) {
        // Recycle SharedTexture
        if (front->mResourceWrapper) {
          owner->mRecycledSharedTextures.push(
              std::move(front->mResourceWrapper));
        }
        // Recycle BufferTextureData
        if (!(front->mTextureHost->GetFlags() & TextureFlags::DUMMY_TEXTURE) &&
            (front->mTextureData &&
             front->mTextureData->AsBufferTextureData())) {
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

  const auto info = RemoteTextureInfo(aTextureId, aOwnerId, aForPid);
  for (auto& callback : renderingReadyCallbacks) {
    callback(info);
  }
}

void RemoteTextureMap::GetLatestBufferSnapshot(
    const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid,
    const mozilla::ipc::Shmem& aDestShmem, const gfx::IntSize& aSize) {
  // The compositable ref of remote texture should be updated in mMonitor lock.
  CompositableTextureHostRef textureHostRef;
  RefPtr<TextureHost> releasingTexture;  // Release outside the monitor
  std::shared_ptr<webgpu::ExternalTexture> externalTexture;
  {
    MonitorAutoLock lock(mMonitor);

    auto* owner = GetTextureOwner(lock, aOwnerId, aForPid);
    if (!owner) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return;
    }

    // Get latest TextureHost of remote Texture.
    if (owner->mWaitingTextureDataHolders.empty() &&
        owner->mUsingTextureDataHolders.empty()) {
      return;
    }
    const auto* holder = !owner->mWaitingTextureDataHolders.empty()
                             ? owner->mWaitingTextureDataHolders.back().get()
                             : owner->mUsingTextureDataHolders.back().get();
    TextureHost* textureHost = holder->mTextureHost;

    if (textureHost->GetSize() != aSize) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return;
    }
    if (textureHost->GetFormat() != gfx::SurfaceFormat::R8G8B8A8 &&
        textureHost->GetFormat() != gfx::SurfaceFormat::B8G8R8A8) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return;
    }
    if (holder->mResourceWrapper &&
        holder->mResourceWrapper->mExternalTexture) {
      // Increment compositable ref to prevent that TextureDataHolder is removed
      // during memcpy.
      textureHostRef = textureHost;
      externalTexture = holder->mResourceWrapper->mExternalTexture;
    } else if (textureHost->AsBufferTextureHost()) {
      // Increment compositable ref to prevent that TextureDataHolder is removed
      // during memcpy.
      textureHostRef = textureHost;
    } else {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return;
    }
  }

  if (!textureHostRef) {
    return;
  }

  if (externalTexture) {
    externalTexture->GetSnapshot(aDestShmem, aSize);
  } else if (auto* bufferTextureHost = textureHostRef->AsBufferTextureHost()) {
    uint32_t stride = ImageDataSerializer::ComputeRGBStride(
        bufferTextureHost->GetFormat(), aSize.width);
    uint32_t bufferSize = stride * aSize.height;
    uint8_t* dst = aDestShmem.get<uint8_t>();
    uint8_t* src = bufferTextureHost->GetBuffer();

    MOZ_ASSERT(bufferSize <= aDestShmem.Size<uint8_t>());
    memcpy(dst, src, bufferSize);
  }

  {
    MonitorAutoLock lock(mMonitor);
    // Release compositable ref in mMonitor lock, but release RefPtr outside the
    // monitor
    releasingTexture = textureHostRef;
    textureHostRef = nullptr;
  }
}

void RemoteTextureMap::RegisterTextureOwner(const RemoteTextureOwnerId aOwnerId,
                                            const base::ProcessId aForPid,
                                            bool aIsSyncMode) {
  MonitorAutoLock lock(mMonitor);

  const auto key = std::pair(aForPid, aOwnerId);
  auto it = mTextureOwners.find(key);
  if (it != mTextureOwners.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }
  auto owner = MakeUnique<TextureOwner>();
  owner->mIsSyncMode = aIsSyncMode;

  mTextureOwners.emplace(key, std::move(owner));
}

void RemoteTextureMap::KeepTextureDataAliveForTextureHostIfNecessary(
    const MonitorAutoLock& aProofOfLock,
    std::deque<UniquePtr<TextureDataHolder>>& aHolders) {
  for (auto& holder : aHolders) {
    // If remote texture of TextureHost still exist, keep
    // SharedResourceWrapper/TextureData alive while the TextureHost is alive.
    if (holder->mTextureHost &&
        holder->mTextureHost->NumCompositableRefs() >= 0) {
      RefPtr<nsISerialEventTarget> eventTarget =
          MessageLoop::current()->SerialEventTarget();
      RefPtr<Runnable> runnable = NS_NewRunnableFunction(
          "RemoteTextureMap::UnregisterTextureOwner::Runnable",
          [data = std::move(holder->mTextureData),
           wrapper = std::move(holder->mResourceWrapper)]() {});

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
  UniquePtr<TextureOwner> releasingOwner;  // Release outside the monitor
  std::vector<RefPtr<TextureHost>>
      releasingTextures;  // Release outside the monitor
  std::vector<std::function<void(const RemoteTextureInfo&)>>
      renderingReadyCallbacks;  // Call outside the monitor
  {
    MonitorAutoLock lock(mMonitor);

    const auto key = std::pair(aForPid, aOwnerId);
    auto it = mTextureOwners.find(key);
    if (it == mTextureOwners.end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return;
    }

    if (it->second->mLatestTextureHost) {
      // Release CompositableRef in mMonitor
      releasingTextures.emplace_back(it->second->mLatestTextureHost);
      it->second->mLatestTextureHost = nullptr;
    }

    // mReleasingRenderedTextureHosts and mLatestRenderedTextureHost could
    // simply be cleared. Since NumCompositableRefs() > 0 keeps TextureHosts in
    // mUsingTextureDataHolders alive. They need to be cleared before
    // KeepTextureDataAliveForTextureHostIfNecessary() call. The function uses
    // NumCompositableRefs().
    if (!it->second->mReleasingRenderedTextureHosts.empty()) {
      std::transform(
          it->second->mReleasingRenderedTextureHosts.begin(),
          it->second->mReleasingRenderedTextureHosts.end(),
          std::back_inserter(releasingTextures),
          [](CompositableTextureHostRef& aRef) { return aRef.get(); });
      it->second->mReleasingRenderedTextureHosts.clear();
    }
    if (it->second->mLatestRenderedTextureHost) {
      it->second->mLatestRenderedTextureHost = nullptr;
    }

    renderingReadyCallbacks =
        GetAllRenderingReadyCallbacks(lock, it->second.get());

    KeepTextureDataAliveForTextureHostIfNecessary(
        lock, it->second->mWaitingTextureDataHolders);

    KeepTextureDataAliveForTextureHostIfNecessary(
        lock, it->second->mUsingTextureDataHolders);

    releasingOwner = std::move(it->second);
    mTextureOwners.erase(it);

    mMonitor.Notify();
  }

  const auto info =
      RemoteTextureInfo(RemoteTextureId{0}, RemoteTextureOwnerId{0}, 0);
  for (auto& callback : renderingReadyCallbacks) {
    callback(info);
  }
}

void RemoteTextureMap::UnregisterTextureOwners(
    const std::unordered_set<RemoteTextureOwnerId,
                             RemoteTextureOwnerId::HashFn>& aOwnerIds,
    const base::ProcessId aForPid) {
  std::vector<UniquePtr<TextureOwner>>
      releasingOwners;  // Release outside the monitor
  std::vector<RefPtr<TextureHost>>
      releasingTextures;  // Release outside the monitor
  std::vector<std::function<void(const RemoteTextureInfo&)>>
      renderingReadyCallbacks;  // Call outside the monitor
  {
    MonitorAutoLock lock(mMonitor);

    for (auto id : aOwnerIds) {
      const auto key = std::pair(aForPid, id);
      auto it = mTextureOwners.find(key);
      if (it == mTextureOwners.end()) {
        MOZ_ASSERT_UNREACHABLE("unexpected to be called");
        continue;
      }

      if (it->second->mLatestTextureHost) {
        // Release CompositableRef in mMonitor
        releasingTextures.emplace_back(it->second->mLatestTextureHost);
        it->second->mLatestTextureHost = nullptr;
      }

      // mReleasingRenderedTextureHosts and mLatestRenderedTextureHost could
      // simply be cleared. Since NumCompositableRefs() > 0 keeps TextureHosts
      // in mUsingTextureDataHolders alive. They need to be cleared before
      // KeepTextureDataAliveForTextureHostIfNecessary() call. The function uses
      // NumCompositableRefs().
      if (!it->second->mReleasingRenderedTextureHosts.empty()) {
        std::transform(
            it->second->mReleasingRenderedTextureHosts.begin(),
            it->second->mReleasingRenderedTextureHosts.end(),
            std::back_inserter(releasingTextures),
            [](CompositableTextureHostRef& aRef) { return aRef.get(); });
        it->second->mReleasingRenderedTextureHosts.clear();
      }
      if (it->second->mLatestRenderedTextureHost) {
        it->second->mLatestRenderedTextureHost = nullptr;
      }

      renderingReadyCallbacks =
          GetAllRenderingReadyCallbacks(lock, it->second.get());

      KeepTextureDataAliveForTextureHostIfNecessary(
          lock, it->second->mWaitingTextureDataHolders);

      KeepTextureDataAliveForTextureHostIfNecessary(
          lock, it->second->mUsingTextureDataHolders);

      releasingOwners.push_back(std::move(it->second));
      mTextureOwners.erase(it);
    }

    mMonitor.Notify();
  }

  const auto info =
      RemoteTextureInfo(RemoteTextureId{0}, RemoteTextureOwnerId{0}, 0);
  for (auto& callback : renderingReadyCallbacks) {
    callback(info);
  }
}

void RemoteTextureMap::NotifyContextLost(
    const std::unordered_set<RemoteTextureOwnerId,
                             RemoteTextureOwnerId::HashFn>& aOwnerIds,
    const base::ProcessId aForPid) {
  MonitorAutoLock lock(mMonitor);

  for (auto id : aOwnerIds) {
    const auto key = std::pair(aForPid, id);
    auto it = mTextureOwners.find(key);
    if (it == mTextureOwners.end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      continue;
    }
    it->second->mIsContextLost = true;
  }
  mMonitor.Notify();
}

/* static */
RefPtr<TextureHost> RemoteTextureMap::CreateRemoteTexture(
    TextureData* aTextureData, TextureFlags aTextureFlags) {
  SurfaceDescriptor desc;
  DebugOnly<bool> ret = aTextureData->Serialize(desc);
  MOZ_ASSERT(ret);
  TextureFlags flags = aTextureFlags | TextureFlags::REMOTE_TEXTURE |
                       TextureFlags::DEALLOCATE_CLIENT;

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

void RemoteTextureMap::UpdateTexture(const MonitorAutoLock& aProofOfLock,
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
    aOwner->mWaitingTextureDataHolders.pop_front();
    aOwner->mUsingTextureDataHolders.push_back(std::move(holder));
  }
}

std::vector<std::function<void(const RemoteTextureInfo&)>>
RemoteTextureMap::GetRenderingReadyCallbacks(
    const MonitorAutoLock& aProofOfLock, RemoteTextureMap::TextureOwner* aOwner,
    const RemoteTextureId aTextureId) {
  MOZ_ASSERT(aOwner);

  std::vector<std::function<void(const RemoteTextureInfo&)>> functions;

  while (!aOwner->mRenderingReadyCallbackHolders.empty()) {
    auto& front = aOwner->mRenderingReadyCallbackHolders.front();
    if (aTextureId < front->mTextureId) {
      break;
    }
    if (front->mCallback) {
      functions.push_back(std::move(front->mCallback));
    }
    aOwner->mRenderingReadyCallbackHolders.pop_front();
  }

  return functions;
}

std::vector<std::function<void(const RemoteTextureInfo&)>>
RemoteTextureMap::GetAllRenderingReadyCallbacks(
    const MonitorAutoLock& aProofOfLock,
    RemoteTextureMap::TextureOwner* aOwner) {
  auto functions =
      GetRenderingReadyCallbacks(aProofOfLock, aOwner, RemoteTextureId::Max());
  MOZ_ASSERT(aOwner->mRenderingReadyCallbackHolders.empty());

  return functions;
}

bool RemoteTextureMap::GetRemoteTextureForDisplayList(
    RemoteTextureHostWrapper* aTextureHostWrapper,
    std::function<void(const RemoteTextureInfo&)>&& aReadyCallback) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aTextureHostWrapper);

  if (aTextureHostWrapper->IsReadyForRendering()) {
    return false;
  }

  const auto& textureId = aTextureHostWrapper->mTextureId;
  const auto& ownerId = aTextureHostWrapper->mOwnerId;
  const auto& forPid = aTextureHostWrapper->mForPid;
  const auto& size = aTextureHostWrapper->mSize;

  RefPtr<TextureHost> textureHost;
  {
    MonitorAutoLock lock(mMonitor);

    auto* owner = GetTextureOwner(lock, ownerId, forPid);
    if (!owner) {
      return false;
    }

    UpdateTexture(lock, owner, textureId);

    if (owner->mLatestTextureHost &&
        (owner->mLatestTextureHost->GetFlags() & TextureFlags::DUMMY_TEXTURE)) {
      // Remote texture allocation was failed.
      return false;
    }

    bool syncMode = owner->mIsSyncMode || bool(aReadyCallback);

    if (syncMode) {
      // remote texture sync ipc
      if (textureId == owner->mLatestTextureId) {
        MOZ_ASSERT(owner->mLatestTextureHost);
        MOZ_ASSERT(owner->mLatestTextureHost->GetSize() == size);
        if (owner->mLatestTextureHost->GetSize() != size) {
          gfxCriticalNoteOnce << "unexpected remote texture size: "
                              << owner->mLatestTextureHost->GetSize()
                              << " expected: " << size;
        }
        textureHost = owner->mLatestTextureHost;
      } else {
        if (aReadyCallback) {
          auto callbackHolder = MakeUnique<RenderingReadyCallbackHolder>(
              textureId, std::move(aReadyCallback));
          owner->mRenderingReadyCallbackHolders.push_back(
              std::move(callbackHolder));
          return true;
        } else {
          MOZ_ASSERT_UNREACHABLE("unexpected to be called");
        }
      }
    } else {
      // remote texture async ipc
      if (owner->mLatestTextureHost) {
        if (owner->mLatestTextureHost->GetSize() == size) {
          textureHost = owner->mLatestTextureHost;
        } else {
          gfxCriticalNoteOnce << "unexpected remote texture size: "
                              << owner->mLatestTextureHost->GetSize()
                              << " expected: " << size;
          if (!owner->mIsContextLost) {
            MOZ_ASSERT_UNREACHABLE("unexpected to be called");
          }
        }
      } else {
        gfxCriticalNoteOnce << "remote texture does not exist";
        MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      }

      // Update mAsyncRemoteTextureHost for async mode
      if (textureId == owner->mLatestTextureId) {
        const auto key = std::pair(forPid, textureId);
        auto it = mRemoteTextureHostWrapperHolders.find(key);
        if (it != mRemoteTextureHostWrapperHolders.end() &&
            !it->second->mAsyncRemoteTextureHost) {
          it->second->mAsyncRemoteTextureHost = owner->mLatestTextureHost;
        } else {
          MOZ_ASSERT(it->second->mAsyncRemoteTextureHost ==
                     owner->mLatestTextureHost);
        }
      }
    }

    if (textureHost) {
      aTextureHostWrapper->SetRemoteTextureHostForDisplayList(lock, textureHost,
                                                              syncMode);
      aTextureHostWrapper->ApplyTextureFlagsToRemoteTexture();
    }
  }

  return false;
}

wr::MaybeExternalImageId RemoteTextureMap::GetExternalImageIdOfRemoteTexture(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    const base::ProcessId aForPid) {
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());
  MonitorAutoLock lock(mMonitor);

  const auto key = std::pair(aForPid, aTextureId);
  auto it = mRemoteTextureHostWrapperHolders.find(key);
  if (it == mRemoteTextureHostWrapperHolders.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return Nothing();
  }

  TextureHost* remoteTexture = it->second->mAsyncRemoteTextureHost;

  auto* owner = GetTextureOwner(lock, aOwnerId, aForPid);
  if (!owner) {
    if (!remoteTexture) {
      // This could happen with IPC abnormal shutdown
      return Nothing();
    }
    return remoteTexture->GetMaybeExternalImageId();
  }

  if (remoteTexture &&
      remoteTexture->GetFlags() & TextureFlags::DUMMY_TEXTURE) {
    // Remote texture allocation was failed.
    return Nothing();
  }
  MOZ_ASSERT(!(remoteTexture &&
               remoteTexture->GetFlags() & TextureFlags::DUMMY_TEXTURE));

  MOZ_ASSERT(owner);

  if (!remoteTexture) {
    // Use mLatestRenderedTextureHost for rendering. Remote texture of
    // aTextureId does not exist.
    remoteTexture = owner->mLatestRenderedTextureHost;
    if (!it->second->mReadyCheckSuppressed && !owner->mIsContextLost) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      gfxCriticalNoteOnce << "remote texture for rendering does not exist id:"
                          << uint64_t(aTextureId);
    }
  } else {
    // Update mLatestRenderedTextureHost
    if (owner->mLatestRenderedTextureHost) {
      owner->mReleasingRenderedTextureHosts.push_back(
          owner->mLatestRenderedTextureHost);
      owner->mLatestRenderedTextureHost = nullptr;
    }
    owner->mLatestRenderedTextureHost = remoteTexture;
  }

  if (!remoteTexture) {
    return Nothing();
  }

  return remoteTexture->GetMaybeExternalImageId();
}

void RemoteTextureMap::ReleaseRemoteTextureHostForDisplayList(
    RemoteTextureHostWrapper* aTextureHostWrapper) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aTextureHostWrapper);

  RefPtr<TextureHost> releasingTexture;  // Release outside the mutex
  {
    MonitorAutoLock lock(mMonitor);
    releasingTexture =
        aTextureHostWrapper->GetRemoteTextureHostForDisplayList(lock);
    aTextureHostWrapper->ClearRemoteTextureHostForDisplayList(lock);
  }
}

RefPtr<TextureHost> RemoteTextureMap::GetOrCreateRemoteTextureHostWrapper(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    const base::ProcessId aForPid, const gfx::IntSize aSize,
    const TextureFlags aFlags) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MonitorAutoLock lock(mMonitor);

  const auto key = std::pair(aForPid, aTextureId);
  auto it = mRemoteTextureHostWrapperHolders.find(key);
  if (it != mRemoteTextureHostWrapperHolders.end()) {
    return it->second->mRemoteTextureHostWrapper;
  }

  auto wrapper = RemoteTextureHostWrapper::Create(aTextureId, aOwnerId, aForPid,
                                                  aSize, aFlags);
  auto wrapperHolder = MakeUnique<RemoteTextureHostWrapperHolder>(wrapper);

  mRemoteTextureHostWrapperHolders.emplace(key, std::move(wrapperHolder));

  return wrapper;
}

void RemoteTextureMap::UnregisterRemoteTextureHostWrapper(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    const base::ProcessId aForPid) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  std::vector<RefPtr<TextureHost>>
      releasingTextures;  // Release outside the monitor
  {
    MonitorAutoLock lock(mMonitor);

    const auto key = std::pair(aForPid, aTextureId);
    auto it = mRemoteTextureHostWrapperHolders.find(key);
    if (it == mRemoteTextureHostWrapperHolders.end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return;
    }
    releasingTextures.emplace_back(it->second->mRemoteTextureHostWrapper);
    if (it->second->mAsyncRemoteTextureHost) {
      releasingTextures.emplace_back(it->second->mAsyncRemoteTextureHost);
    }

    mRemoteTextureHostWrapperHolders.erase(it);
    mMonitor.Notify();
  }
}

void RemoteTextureMap::RegisterRemoteTexturePushListener(
    const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid,
    CompositableHost* aListener) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  RefPtr<CompositableHost>
      releasingCompositableHost;  // Release outside the monitor
  {
    MonitorAutoLock lock(mMonitor);

    const auto key = std::pair(aForPid, aOwnerId);
    auto it = mRemoteTexturePushListeners.find(key);
    // Remove obsoleted CompositableHost.
    if (it != mRemoteTexturePushListeners.end()) {
      releasingCompositableHost = std::move(it->second);
      mRemoteTexturePushListeners.erase(it);
    }
    mRemoteTexturePushListeners.emplace(key, aListener);

    auto* owner = GetTextureOwner(lock, aOwnerId, aForPid);
    if (!owner) {
      return;
    }
    if (owner->mWaitingTextureDataHolders.empty() &&
        !owner->mLatestTextureHost) {
      return;
    }

    // Get latest RemoteTextureId.
    auto textureId = !owner->mWaitingTextureDataHolders.empty()
                         ? owner->mWaitingTextureDataHolders.back()->mTextureId
                         : owner->mLatestTextureId;

    // Notify the RemoteTextureId to callback
    RefPtr<CompositableHost> compositableHost = aListener;
    RefPtr<Runnable> runnable = NS_NewRunnableFunction(
        "RemoteTextureMap::RegisterRemoteTexturePushListener::Runnable",
        [compositableHost, textureId, aOwnerId, aForPid]() {
          compositableHost->NotifyPushTexture(textureId, aOwnerId, aForPid);
        });
    CompositorThread()->Dispatch(runnable.forget());
  }
}

void RemoteTextureMap::UnregisterRemoteTexturePushListener(
    const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid,
    CompositableHost* aListener) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  RefPtr<CompositableHost>
      releasingCompositableHost;  // Release outside the monitor
  {
    MonitorAutoLock lock(mMonitor);

    const auto key = std::pair(aForPid, aOwnerId);
    auto it = mRemoteTexturePushListeners.find(key);
    if (it == mRemoteTexturePushListeners.end()) {
      return;
    }
    if (aListener != it->second) {
      // aListener was alredy obsoleted.
      return;
    }
    releasingCompositableHost = std::move(it->second);
    mRemoteTexturePushListeners.erase(it);
  }
}

bool RemoteTextureMap::CheckRemoteTextureReady(
    const RemoteTextureInfo& aInfo,
    std::function<void(const RemoteTextureInfo&)>&& aCallback) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  MonitorAutoLock lock(mMonitor);

  auto* owner = GetTextureOwner(lock, aInfo.mOwnerId, aInfo.mForPid);
  if (!owner || owner->mIsContextLost) {
    // Owner is already removed or context lost. No need to wait texture ready.
    return true;
  }

  const auto key = std::pair(aInfo.mForPid, aInfo.mTextureId);
  auto it = mRemoteTextureHostWrapperHolders.find(key);
  if (it == mRemoteTextureHostWrapperHolders.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    gfxCriticalNoteOnce << "Remote texture does not exist id:"
                        << uint64_t(aInfo.mTextureId);
    return true;
  }

  if (it->second->mAsyncRemoteTextureHost) {
    return true;
  }
  MOZ_ASSERT(!it->second->mAsyncRemoteTextureHost);

  // Check if RemoteTextureId is as expected.
  if (!owner->mRenderingReadyCallbackHolders.empty()) {
    auto& front = owner->mRenderingReadyCallbackHolders.front();
    MOZ_RELEASE_ASSERT(aInfo.mTextureId >= front->mTextureId);
  }

  auto callbackHolder = MakeUnique<RenderingReadyCallbackHolder>(
      aInfo.mTextureId, std::move(aCallback));
  owner->mRenderingReadyCallbackHolders.push_back(std::move(callbackHolder));

  return false;
}

bool RemoteTextureMap::WaitRemoteTextureReady(const RemoteTextureInfo& aInfo) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  MonitorAutoLock lock(mMonitor);

  auto* owner = GetTextureOwner(lock, aInfo.mOwnerId, aInfo.mForPid);
  if (!owner || owner->mIsContextLost) {
    // Owner is already removed or context lost.
    return false;
  }

  const auto key = std::pair(aInfo.mForPid, aInfo.mTextureId);
  auto it = mRemoteTextureHostWrapperHolders.find(key);
  if (it == mRemoteTextureHostWrapperHolders.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    gfxCriticalNoteOnce << "Remote texture does not exist id:"
                        << uint64_t(aInfo.mTextureId);
    return false;
  }

  const TimeDuration timeout = TimeDuration::FromMilliseconds(1000);
  TextureHost* remoteTexture = it->second->mAsyncRemoteTextureHost;

  while (!remoteTexture) {
    CVStatus status = mMonitor.Wait(timeout);
    if (status == CVStatus::Timeout) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      gfxCriticalNoteOnce << "Remote texture wait time out id:"
                          << uint64_t(aInfo.mTextureId);
      return false;
    }

    auto it = mRemoteTextureHostWrapperHolders.find(key);
    if (it == mRemoteTextureHostWrapperHolders.end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return false;
    }

    remoteTexture = it->second->mAsyncRemoteTextureHost;
    if (!remoteTexture) {
      auto* owner = GetTextureOwner(lock, aInfo.mOwnerId, aInfo.mForPid);
      // When owner is alreay unregistered, remote texture will not be pushed.
      if (!owner || owner->mIsContextLost) {
        // This could happen with IPC abnormal shutdown
        return false;
      }
    }
  }

  return true;
}

void RemoteTextureMap::SuppressRemoteTextureReadyCheck(
    const RemoteTextureId aTextureId, const base::ProcessId aForPid) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MonitorAutoLock lock(mMonitor);

  const auto key = std::pair(aForPid, aTextureId);
  auto it = mRemoteTextureHostWrapperHolders.find(key);
  if (it == mRemoteTextureHostWrapperHolders.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }
  it->second->mReadyCheckSuppressed = true;
}

UniquePtr<TextureData> RemoteTextureMap::GetRecycledBufferTextureData(
    const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid,
    gfx::IntSize aSize, gfx::SurfaceFormat aFormat) {
  std::stack<UniquePtr<TextureData>>
      releasingTextures;  // Release outside the monitor
  UniquePtr<TextureData> texture;
  {
    MonitorAutoLock lock(mMonitor);

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

UniquePtr<SharedResourceWrapper> RemoteTextureMap::GetRecycledSharedTexture(
    const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid) {
  UniquePtr<SharedResourceWrapper> wrapper;
  {
    MonitorAutoLock lock(mMonitor);

    auto* owner = GetTextureOwner(lock, aOwnerId, aForPid);
    if (!owner) {
      return nullptr;
    }

    if (owner->mRecycledSharedTextures.empty()) {
      return nullptr;
    }

    auto& front = owner->mRecycledSharedTextures.front();
    wrapper = std::move(front);
    owner->mRecycledSharedTextures.pop();
  }
  return wrapper;
}

RemoteTextureMap::TextureOwner* RemoteTextureMap::GetTextureOwner(
    const MonitorAutoLock& aProofOfLock, const RemoteTextureOwnerId aOwnerId,
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
    UniquePtr<SharedResourceWrapper>&& aResourceWrapper)
    : mTextureId(aTextureId),
      mTextureHost(aTextureHost),
      mTextureData(std::move(aTextureData)),
      mResourceWrapper(std::move(aResourceWrapper)) {}

RemoteTextureMap::RenderingReadyCallbackHolder::RenderingReadyCallbackHolder(
    const RemoteTextureId aTextureId,
    std::function<void(const RemoteTextureInfo&)>&& aCallback)
    : mTextureId(aTextureId), mCallback(aCallback) {}

RemoteTextureMap::RemoteTextureHostWrapperHolder::
    RemoteTextureHostWrapperHolder(
        RefPtr<TextureHost> aRemoteTextureHostWrapper)
    : mRemoteTextureHostWrapper(aRemoteTextureHostWrapper) {}

}  // namespace mozilla::layers
