/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GpuProcessD3D11TextureMap_H
#define MOZILLA_GFX_GpuProcessD3D11TextureMap_H

#include <d3d11.h>
#include <unordered_map>
#include <unordered_set>

#include "mozilla/gfx/2D.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace layers {

class IMFSampleUsageInfo;
class TextureWrapperD3D11Allocator;

/**
 * A class to manage ID3D11Texture2Ds that is shared without using shared handle
 * in GPU process. On some GPUs, ID3D11Texture2Ds of hardware decoded video
 * frames with zero video frame copy could not use shared handle.
 */
class GpuProcessD3D11TextureMap {
  struct UpdatingTextureHolder;

 public:
  static void Init();
  static void Shutdown();
  static GpuProcessD3D11TextureMap* Get() { return sInstance; }
  static GpuProcessTextureId GetNextTextureId();

  GpuProcessD3D11TextureMap();
  ~GpuProcessD3D11TextureMap();

  void Register(GpuProcessTextureId aTextureId, ID3D11Texture2D* aTexture,
                uint32_t aArrayIndex, const gfx::IntSize& aSize,
                RefPtr<IMFSampleUsageInfo> aUsageInfo);
  void Register(const MonitorAutoLock& aProofOfLock,
                GpuProcessTextureId aTextureId, ID3D11Texture2D* aTexture,
                uint32_t aArrayIndex, const gfx::IntSize& aSize,
                RefPtr<IMFSampleUsageInfo> aUsageInfo);
  void Unregister(GpuProcessTextureId aTextureId);

  RefPtr<ID3D11Texture2D> GetTexture(GpuProcessTextureId aTextureId);
  Maybe<HANDLE> GetSharedHandleOfCopiedTexture(GpuProcessTextureId aTextureId);

  size_t GetWaitingTextureCount() const;

  bool WaitTextureReady(const GpuProcessTextureId aTextureId);

  void PostUpdateTextureDataTask(const GpuProcessTextureId aTextureId,
                                 TextureHost* aTextureHost,
                                 TextureHost* aWrappedTextureHost,
                                 TextureWrapperD3D11Allocator* aAllocator);

  void HandleInTextureUpdateThread();

 private:
  struct TextureHolder {
    TextureHolder(ID3D11Texture2D* aTexture, uint32_t aArrayIndex,
                  const gfx::IntSize& aSize,
                  RefPtr<IMFSampleUsageInfo> aUsageInfo);
    TextureHolder() = default;

    RefPtr<ID3D11Texture2D> mTexture;
    uint32_t mArrayIndex = 0;
    gfx::IntSize mSize;
    RefPtr<IMFSampleUsageInfo> mIMFSampleUsageInfo;
    RefPtr<ID3D11Texture2D> mCopiedTexture;
    RefPtr<gfx::FileHandleWrapper> mCopiedTextureSharedHandle;
  };

  struct UpdatingTextureHolder {
    UpdatingTextureHolder(const GpuProcessTextureId aTextureId,
                          TextureHost* aTextureHost,
                          TextureHost* aWrappedTextureHost,
                          TextureWrapperD3D11Allocator* aAllocator);

    ~UpdatingTextureHolder();

    const GpuProcessTextureId mTextureId;
    RefPtr<TextureHost> mTextureHost;
    CompositableTextureHostRef mWrappedTextureHost;
    RefPtr<TextureWrapperD3D11Allocator> mAllocator;
  };

  enum class UpdatingStatus { Waiting, Updating, Error };

  RefPtr<ID3D11Texture2D> UpdateTextureData(UpdatingTextureHolder* aHolder);

  mutable Monitor mMonitor MOZ_UNANNOTATED;

  std::unordered_map<GpuProcessTextureId, TextureHolder,
                     GpuProcessTextureId::HashFn>
      mD3D11TexturesById;

  std::deque<UniquePtr<UpdatingTextureHolder>> mWaitingTextureQueue;

  std::unordered_set<GpuProcessTextureId, GpuProcessTextureId::HashFn>
      mWaitingTextures;

  static StaticAutoPtr<GpuProcessD3D11TextureMap> sInstance;
};

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_GpuProcessD3D11TextureMap_H */
