/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RemoteTextureMap_H
#define MOZILLA_GFX_RemoteTextureMap_H

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <stack>
#include <unordered_set>
#include <utility>

#include "mozilla/gfx/Point.h"  // for IntSize
#include "mozilla/gfx/Types.h"  // for SurfaceFormat
#include "mozilla/ipc/Shmem.h"
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags, etc
#include "mozilla/layers/LayersSurfaces.h"   // for SurfaceDescriptor
#include "mozilla/layers/TextureHost.h"
#include "mozilla/Monitor.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadSafeWeakPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/webrender/WebRenderTypes.h"

class nsISerialEventTarget;

namespace mozilla {

namespace gl {
class SharedSurface;
}

namespace webgpu {
class ExternalTexture;
}

namespace layers {

class CompositableHost;
class RemoteTextureHostWrapper;
class TextureData;
class TextureHost;

struct RemoteTextureInfo {
  RemoteTextureInfo(const RemoteTextureId aTextureId,
                    const RemoteTextureOwnerId aOwnerId,
                    const base::ProcessId aForPid)
      : mTextureId(aTextureId), mOwnerId(aOwnerId), mForPid(aForPid) {}

  const RemoteTextureId mTextureId;
  const RemoteTextureOwnerId mOwnerId;
  const base::ProcessId mForPid;
};

struct RemoteTextureInfoList {
  std::queue<RemoteTextureInfo> mList;
};

class SharedResourceWrapper {
 public:
  enum class Tag { SharedSurface, ExternalTexture };
  const Tag mTag;

  static UniquePtr<SharedResourceWrapper> SharedSurface(
      const std::shared_ptr<gl::SharedSurface>& aSharedSurface) {
    return MakeUnique<SharedResourceWrapper>(Tag::SharedSurface,
                                             aSharedSurface);
  }

  static UniquePtr<SharedResourceWrapper> ExternalTexture(
      const std::shared_ptr<webgpu::ExternalTexture>& aExternalTexture) {
    return MakeUnique<SharedResourceWrapper>(Tag::ExternalTexture,
                                             aExternalTexture);
  }

  SharedResourceWrapper(
      const Tag aTag, const std::shared_ptr<gl::SharedSurface>& aSharedSurface)
      : mTag(aTag), mSharedSurface(aSharedSurface) {
    MOZ_ASSERT(mTag == Tag::SharedSurface);
  }
  SharedResourceWrapper(
      const Tag aTag,
      const std::shared_ptr<webgpu::ExternalTexture>& aExternalTexture)
      : mTag(aTag), mExternalTexture(aExternalTexture) {
    MOZ_ASSERT(mTag == Tag::ExternalTexture);
  }

  const std::shared_ptr<gl::SharedSurface> mSharedSurface;
  const std::shared_ptr<webgpu::ExternalTexture> mExternalTexture;

  std::shared_ptr<gl::SharedSurface> SharedSurface() {
    if (mTag == Tag::SharedSurface) {
      return mSharedSurface;
    }
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return nullptr;
  }

  std::shared_ptr<webgpu::ExternalTexture> ExternalTexture() {
    if (mTag == Tag::ExternalTexture) {
      return mExternalTexture;
    }
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return nullptr;
  }
};

/**
 * A class provides API for remote texture owners.
 */
class RemoteTextureOwnerClient final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteTextureOwnerClient)

  explicit RemoteTextureOwnerClient(const base::ProcessId aForPid);

  bool IsRegistered(const RemoteTextureOwnerId aOwnerId);
  void RegisterTextureOwner(const RemoteTextureOwnerId aOwnerId,
                            bool aIsSyncMode);
  void UnregisterTextureOwner(const RemoteTextureOwnerId aOwnerId);
  void UnregisterAllTextureOwners();
  void NotifyContextLost();
  void PushTexture(const RemoteTextureId aTextureId,
                   const RemoteTextureOwnerId aOwnerId,
                   UniquePtr<TextureData>&& aTextureData);
  void PushTexture(const RemoteTextureId aTextureId,
                   const RemoteTextureOwnerId aOwnerId,
                   UniquePtr<TextureData>&& aTextureData,
                   const std::shared_ptr<gl::SharedSurface>& aSharedSurface);
  void PushTexture(
      const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
      UniquePtr<TextureData>&& aTextureData,
      const std::shared_ptr<webgpu::ExternalTexture>& aExternalTexture);
  void PushDummyTexture(const RemoteTextureId aTextureId,
                        const RemoteTextureOwnerId aOwnerId);
  void GetLatestBufferSnapshot(const RemoteTextureOwnerId aOwnerId,
                               const mozilla::ipc::Shmem& aDestShmem,
                               const gfx::IntSize& aSize);
  UniquePtr<TextureData> CreateOrRecycleBufferTextureData(
      const RemoteTextureOwnerId aOwnerId, gfx::IntSize aSize,
      gfx::SurfaceFormat aFormat);
  std::shared_ptr<gl::SharedSurface> GetRecycledSharedSurface(
      const RemoteTextureOwnerId aOwnerId);
  std::shared_ptr<webgpu::ExternalTexture> GetRecycledExternalTexture(
      const RemoteTextureOwnerId aOwnerId);

  const base::ProcessId mForPid;

 protected:
  ~RemoteTextureOwnerClient();

  std::unordered_set<RemoteTextureOwnerId, RemoteTextureOwnerId::HashFn>
      mOwnerIds;
};

/**
 * A class to map RemoteTextureId to remote texture(TextureHost).
 * Remote textures are provided by texture owner.
 */
class RemoteTextureMap {
 public:
  static void Init();
  static void Shutdown();
  static RemoteTextureMap* Get() { return sInstance; }

  RemoteTextureMap();
  ~RemoteTextureMap();

  // Push remote texture data and gl::SharedSurface from texture owner.
  // The texture data is used for creating TextureHost.
  // gl::SharedSurface is pushed only when the surface needs to be kept alive
  // during TextureHost usage. The texture data and the surface might be
  // recycled when TextureHost is destroyed.
  void PushTexture(const RemoteTextureId aTextureId,
                   const RemoteTextureOwnerId aOwnerId,
                   const base::ProcessId aForPid,
                   UniquePtr<TextureData>&& aTextureData,
                   RefPtr<TextureHost>& aTextureHost,
                   UniquePtr<SharedResourceWrapper>&& aResourceWrapper);

  void GetLatestBufferSnapshot(const RemoteTextureOwnerId aOwnerId,
                               const base::ProcessId aForPid,
                               const mozilla::ipc::Shmem& aDestShmem,
                               const gfx::IntSize& aSize);

  // aIsSyncMode defines if RemoteTextureMap::GetRemoteTextureForDisplayList()
  // works synchronously.
  void RegisterTextureOwner(const RemoteTextureOwnerId aOwnerId,
                            const base::ProcessId aForPid, bool aIsSyncMode);
  void UnregisterTextureOwner(const RemoteTextureOwnerId aOwnerIds,
                              const base::ProcessId aForPid);
  void UnregisterTextureOwners(
      const std::unordered_set<RemoteTextureOwnerId,
                               RemoteTextureOwnerId::HashFn>& aOwnerIds,
      const base::ProcessId aForPid);

  void NotifyContextLost(
      const std::unordered_set<RemoteTextureOwnerId,
                               RemoteTextureOwnerId::HashFn>& aOwnerIds,
      const base::ProcessId aForPid);

  // Get TextureHost that is used for building wr display list.
  // In sync mode, mRemoteTextureForDisplayList holds TextureHost of mTextureId.
  // In async mode, it could be previous remote texture's TextureHost that is
  // compatible to the mTextureId's TextureHost.
  //
  // return true when aReadyCallback will be called.
  bool GetRemoteTextureForDisplayList(
      RemoteTextureHostWrapper* aTextureHostWrapper,
      std::function<void(const RemoteTextureInfo&)>&& aReadyCallback);

  // Get ExternalImageId of remote texture for WebRender rendering.
  wr::MaybeExternalImageId GetExternalImageIdOfRemoteTexture(
      const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
      const base::ProcessId aForPid);

  void ReleaseRemoteTextureHostForDisplayList(
      RemoteTextureHostWrapper* aTextureHostWrapper);

  RefPtr<TextureHost> GetOrCreateRemoteTextureHostWrapper(
      const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
      const base::ProcessId aForPid, const gfx::IntSize aSize,
      const TextureFlags aFlags);

  void UnregisterRemoteTextureHostWrapper(const RemoteTextureId aTextureId,
                                          const RemoteTextureOwnerId aOwnerId,
                                          const base::ProcessId aForPid);

  void RegisterRemoteTexturePushListener(const RemoteTextureOwnerId aOwnerId,
                                         const base::ProcessId aForPid,
                                         CompositableHost* aListener);

  void UnregisterRemoteTexturePushListener(const RemoteTextureOwnerId aOwnerId,
                                           const base::ProcessId aForPid,
                                           CompositableHost* aListener);

  bool CheckRemoteTextureReady(
      const RemoteTextureInfo& aInfo,
      std::function<void(const RemoteTextureInfo&)>&& aCallback);

  bool WaitRemoteTextureReady(const RemoteTextureInfo& aInfo);

  void SuppressRemoteTextureReadyCheck(const RemoteTextureId aTextureId,
                                       const base::ProcessId aForPid);

  UniquePtr<TextureData> GetRecycledBufferTextureData(
      const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid,
      gfx::IntSize aSize, gfx::SurfaceFormat aFormat);

  UniquePtr<SharedResourceWrapper> GetRecycledSharedTexture(
      const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid);

  static RefPtr<TextureHost> CreateRemoteTexture(TextureData* aTextureData,
                                                 TextureFlags aTextureFlags);

 protected:
  // Holds data related to remote texture
  struct TextureDataHolder {
    TextureDataHolder(const RemoteTextureId aTextureId,
                      RefPtr<TextureHost> aTextureHost,
                      UniquePtr<TextureData>&& aTextureData,
                      UniquePtr<SharedResourceWrapper>&& aResourceWrapper);

    const RemoteTextureId mTextureId;
    // TextureHost of remote texture
    // Compositable ref of the mTextureHost should be updated within mMonitor.
    // The compositable ref is used to check if TextureHost(remote texture) is
    // still in use by WebRender.
    RefPtr<TextureHost> mTextureHost;
    // Holds BufferTextureData of TextureHost
    UniquePtr<TextureData> mTextureData;
    // Holds gl::SharedSurface of TextureHost
    UniquePtr<SharedResourceWrapper> mResourceWrapper;
  };

  struct RenderingReadyCallbackHolder {
    RenderingReadyCallbackHolder(
        const RemoteTextureId aTextureId,
        std::function<void(const RemoteTextureInfo&)>&& aCallback);

    const RemoteTextureId mTextureId;
    // callback of async RemoteTexture ready
    std::function<void(const RemoteTextureInfo&)> mCallback;
  };

  struct TextureOwner {
    bool mIsSyncMode = true;
    bool mIsContextLost = false;
    // Holds TextureDataHolders that wait to be used for building wr display
    // list.
    std::deque<UniquePtr<TextureDataHolder>> mWaitingTextureDataHolders;
    // Holds TextureDataHolders that are used for building wr display list.
    std::deque<UniquePtr<TextureDataHolder>> mUsingTextureDataHolders;
    // Holds async RemoteTexture ready callbacks.
    std::deque<UniquePtr<RenderingReadyCallbackHolder>>
        mRenderingReadyCallbackHolders;

    RemoteTextureId mLatestTextureId = {0};
    CompositableTextureHostRef mLatestTextureHost;
    CompositableTextureHostRef mLatestRenderedTextureHost;
    // Holds compositable refs to TextureHosts of RenderTextureHosts that are
    // waiting to be released in non-RenderThread.
    std::deque<CompositableTextureHostRef> mReleasingRenderedTextureHosts;
    std::stack<UniquePtr<TextureData>> mRecycledTextures;
    std::queue<UniquePtr<SharedResourceWrapper>> mRecycledSharedTextures;
  };

  // Holds data related to remote texture wrapper
  struct RemoteTextureHostWrapperHolder {
    explicit RemoteTextureHostWrapperHolder(
        RefPtr<TextureHost> aRemoteTextureHostWrapper);

    const RefPtr<TextureHost> mRemoteTextureHostWrapper;
    // Hold compositable ref of remote texture of the RemoteTextureId in async
    // mode. It is for keeping the texture alive during its rendering by
    // WebRender.
    CompositableTextureHostRef mAsyncRemoteTextureHost;
    bool mReadyCheckSuppressed = false;
  };

  void UpdateTexture(const MonitorAutoLock& aProofOfLock,
                     RemoteTextureMap::TextureOwner* aOwner,
                     const RemoteTextureId aTextureId);

  std::vector<std::function<void(const RemoteTextureInfo&)>>
  GetRenderingReadyCallbacks(const MonitorAutoLock& aProofOfLock,
                             RemoteTextureMap::TextureOwner* aOwner,
                             const RemoteTextureId aTextureId);

  std::vector<std::function<void(const RemoteTextureInfo&)>>
  GetAllRenderingReadyCallbacks(const MonitorAutoLock& aProofOfLock,
                                RemoteTextureMap::TextureOwner* aOwner);

  void KeepTextureDataAliveForTextureHostIfNecessary(
      const MonitorAutoLock& aProofOfLock,
      std::deque<UniquePtr<TextureDataHolder>>& aHolders);

  RemoteTextureMap::TextureOwner* GetTextureOwner(
      const MonitorAutoLock& aProofOfLock, const RemoteTextureOwnerId aOwnerId,
      const base::ProcessId aForPid);

  Monitor mMonitor MOZ_UNANNOTATED;

  std::map<std::pair<base::ProcessId, RemoteTextureOwnerId>,
           UniquePtr<TextureOwner>>
      mTextureOwners;

  std::map<std::pair<base::ProcessId, RemoteTextureId>,
           UniquePtr<RemoteTextureHostWrapperHolder>>
      mRemoteTextureHostWrapperHolders;

  std::map<std::pair<base::ProcessId, RemoteTextureOwnerId>,
           RefPtr<CompositableHost>>
      mRemoteTexturePushListeners;

  static StaticAutoPtr<RemoteTextureMap> sInstance;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_RemoteTextureMap_H
