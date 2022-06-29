/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RemoteTextureMap_H
#define MOZILLA_GFX_RemoteTextureMap_H

#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "mozilla/gfx/Point.h"               // for IntSize
#include "mozilla/gfx/Types.h"               // for SurfaceFormat
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags, etc
#include "mozilla/layers/LayersSurfaces.h"   // for SurfaceDescriptor
#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadSafeWeakPtr.h"
#include "mozilla/UniquePtr.h"

class nsISerialEventTarget;

namespace mozilla {

namespace gl {
class SharedSurface;
}

namespace layers {

class TextureData;
class TextureHost;

/**
 * A class provides API for remote texture owners.
 */
class RemoteTextureOwnerClient final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteTextureOwnerClient)

  explicit RemoteTextureOwnerClient(const base::ProcessId aForPid);

  bool IsRegistered(const RemoteTextureOwnerId aOwnerId);
  void RegisterTextureOwner(const RemoteTextureOwnerId aOwnerId);
  void UnregisterTextureOwner(const RemoteTextureOwnerId aOwnerId);
  void UnregisterAllTextureOwners();
  void PushTexure(const RemoteTextureId aTextureId,
                  const RemoteTextureOwnerId aOwnerId,
                  UniquePtr<TextureData>&& aTextureData,
                  const std::shared_ptr<gl::SharedSurface>& aSharedSurface);
  UniquePtr<TextureData> CreateOrRecycleBufferTextureData(
      const RemoteTextureOwnerId aOwnerId, gfx::IntSize aSize,
      gfx::SurfaceFormat aFormat);
  std::shared_ptr<gl::SharedSurface> GetRecycledSharedSurface(
      const RemoteTextureOwnerId aOwnerId);

  const base::ProcessId mForPid;

 protected:
  ~RemoteTextureOwnerClient();

  std::unordered_set<RemoteTextureOwnerId, RemoteTextureOwnerId::HashFn>
      mOwnerIds;
};

using RemoteTextureCallback = std::function<void(
    const RemoteTextureId aTextureId, const RemoteTextureOwnerId aOwnerId,
    const base::ProcessId aForPid)>;

/**
 * A class provides API for remote texture consumer.
 * This handle only one texture consumer.
 */
class RemoteTextureConsumerClient {
 public:
  static UniquePtr<RemoteTextureConsumerClient> Create(
      const RemoteTextureOwnerId aOwnerId, const CompositableHandle& aHandle,
      const base::ProcessId aForPid);

  RemoteTextureConsumerClient(const RemoteTextureOwnerId aOwnerId,
                              const CompositableHandle& aHandle,
                              const base::ProcessId aForPid);
  ~RemoteTextureConsumerClient();

  RefPtr<TextureHost> GetTextureHost(const RemoteTextureId aTextureId,
                                     const gfx::IntSize aSize,
                                     const TextureFlags aFlags,
                                     const RemoteTextureCallback& aCallback);

  const RemoteTextureOwnerId mOwnerId;
  const CompositableHandle mHandle;
  const base::ProcessId mForPid;
};

/**
 * A class wraps SharedSurface and provides thread safe weak pointer.
 * Since SharedSurface is not thread safe.
 */
class SharedSurfaceWrapper
    : public SupportsThreadSafeWeakPtr<SharedSurfaceWrapper> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(SharedSurfaceWrapper)

  explicit SharedSurfaceWrapper(
      const std::shared_ptr<gl::SharedSurface>& aSharedSurface);
  virtual ~SharedSurfaceWrapper();

  bool IsUsed() { return mIsUsed; }
  void EndUsage() { mIsUsed = false; }

  const std::shared_ptr<gl::SharedSurface> mSharedSurface;

 protected:
  Atomic<bool> mIsUsed{true};
};

/**
 * A class to map RemoteTextureId to remote texture(TextureHost).
 * Remote textures are provided by texture owner. The textures are consumed by
 * texture consumer. Textures of one texture owner are consumed by one texture
 * consumer at a time. Lifetime of the consumer is different from the owner. A
 * new consumer could replace old consumer.
 *
 * There could be a case that remote texture is not provided yet by texture
 * owner, when textur consumer requests the texture. In this case, callback
 * function is called when the texture is provided by the owner.
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
  void PushTexure(const RemoteTextureId aTextureId,
                  const RemoteTextureOwnerId aOwnerId,
                  const base::ProcessId aForPid,
                  UniquePtr<TextureData>&& aTextureData,
                  const std::shared_ptr<gl::SharedSurface>& aSharedSurface);
  void RegisterTextureOwner(const RemoteTextureOwnerId aOwnerId,
                            const base::ProcessId aForPid);
  void UnregisterTextureOwner(const RemoteTextureOwnerId aOwnerIds,
                              const base::ProcessId aForPid);
  void UnregisterTextureOwners(
      const std::unordered_set<RemoteTextureOwnerId,
                               RemoteTextureOwnerId::HashFn>& aOwnerIds,
      const base::ProcessId aForPid);

  UniquePtr<RemoteTextureConsumerClient> RegisterTextureConsumer(
      const RemoteTextureOwnerId aOwnerId, const CompositableHandle& aHandle,
      const base::ProcessId aForPid);

  void UnregisterTextureConsumer(const RemoteTextureOwnerId aOwnerId,
                                 const CompositableHandle& aHandle,
                                 const base::ProcessId aForPid);

  RefPtr<TextureHost> GetTextureHost(const RemoteTextureId aTextureId,
                                     const RemoteTextureOwnerId aOwnerId,
                                     const CompositableHandle& aHandle,
                                     const base::ProcessId aForPid,
                                     const gfx::IntSize aSize,
                                     const TextureFlags aFlags,
                                     const RemoteTextureCallback& aCallback);

  void NotifyReleased(const RemoteTextureId aTextureId,
                      const RemoteTextureOwnerId aOwnerId,
                      const base::ProcessId aForPid);

  UniquePtr<TextureData> GetRecycledBufferTextureData(
      const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid,
      gfx::IntSize aSize, gfx::SurfaceFormat aFormat);

  std::shared_ptr<gl::SharedSurface> GetRecycledSharedSurface(
      const RemoteTextureOwnerId aOwnerId, const base::ProcessId aForPid);

 protected:
  // Holds data necessary for creating TextureHost.
  struct TextureDataWrapper {
    TextureDataWrapper(
        const RemoteTextureId aTextureId, UniquePtr<TextureData>&& aTextureData,
        const ThreadSafeWeakPtr<SharedSurfaceWrapper>& aSharedSurface);

    const RemoteTextureId mTextureId;
    UniquePtr<TextureData> mTextureData;
    ThreadSafeWeakPtr<SharedSurfaceWrapper> mSharedSurface;
  };

  struct TextureOwner {
    std::queue<UniquePtr<TextureDataWrapper>> mTextureDataWrappers;
    RemoteTextureId mLatestTextureId = {0};
    RefPtr<TextureHost> mLatestTextureHost;
    std::queue<RefPtr<SharedSurfaceWrapper>> mSharedSurfaces;
    std::stack<UniquePtr<TextureData>> mRecycledTextures;
    std::stack<RefPtr<SharedSurfaceWrapper>> mRecycledSharedSurfaces;
  };

  // Holds callbacks of texture consumer.
  // Callback is used only when remote texture is not provided yet by texture
  // owner during GetTextureHost() call.
  struct TextureConsumer {
    explicit TextureConsumer(const CompositableHandle& aHandle)
        : mHandle(aHandle) {}
    CompositableHandle mHandle;
    std::queue<std::pair<RemoteTextureId, RemoteTextureCallback>> mCallbacks;
  };

  RemoteTextureMap::TextureOwner* GetTextureOwner(
      const MutexAutoLock& aProofOfLock, const RemoteTextureOwnerId aOwnerId,
      const base::ProcessId aForPid);

  RemoteTextureMap::TextureConsumer* GetTextureConsumer(
      const MutexAutoLock& aProofOfLock, const RemoteTextureOwnerId aOwnerId,
      const base::ProcessId aForPid);

  RefPtr<TextureHost> CreateRemoteTexture(const gfx::IntSize aSize,
                                          const TextureFlags aFlags,
                                          TextureDataWrapper* aTextureData);

  Mutex mMutex MOZ_UNANNOTATED;

  std::map<std::pair<base::ProcessId, RemoteTextureOwnerId>,
           UniquePtr<TextureOwner>>
      mTextureOwners;

  // Holds TextureConsumers that consume related TextureOwner.
  std::map<std::pair<base::ProcessId, RemoteTextureOwnerId>,
           UniquePtr<TextureConsumer>>
      mTextureConsumers;

  // Keeps TextureDataWrappers alive during their usage.
  std::map<std::pair<base::ProcessId, RemoteTextureId>,
           UniquePtr<TextureDataWrapper>>
      mKeepAlives;

  static StaticAutoPtr<RemoteTextureMap> sInstance;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_RemoteTextureMap_H
