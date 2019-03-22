/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_RENDERROOTSTATEMANAGER_H
#define GFX_RENDERROOTSTATEMANAGER_H

#include "mozilla/webrender/WebRenderAPI.h"

#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/SharedSurfacesChild.h"
#include "mozilla/layers/WebRenderCommandBuilder.h"

namespace mozilla {

namespace layers {

class RenderRootStateManager {
  typedef nsTHashtable<nsRefPtrHashKey<WebRenderUserData>>
      WebRenderUserDataRefTable;

 public:
  void AddRef();
  void Release();

  RenderRootStateManager()
      : mLayerManager(nullptr),
        mRenderRoot(wr::RenderRoot::Default),
        mDestroyed(false) {}

  void Destroy();
  bool IsDestroyed() { return mDestroyed; }
  wr::RenderRoot GetRenderRoot() { return mRenderRoot; }
  wr::IpcResourceUpdateQueue& AsyncResourceUpdates();
  WebRenderBridgeChild* WrBridge() const;
  WebRenderCommandBuilder& CommandBuilder();
  WebRenderUserDataRefTable* GetWebRenderUserDataTable();
  WebRenderLayerManager* LayerManager() { return mLayerManager; }

  void AddImageKeyForDiscard(wr::ImageKey key);
  void AddBlobImageKeyForDiscard(wr::BlobImageKey key);
  void DiscardImagesInTransaction(wr::IpcResourceUpdateQueue& aResources);
  void DiscardLocalImages();

  void ClearCachedResources();

  // Methods to manage the compositor animation ids. Active animations are still
  // going, and when they end we discard them and remove them from the active
  // list.
  void AddActiveCompositorAnimationId(uint64_t aId);
  void AddCompositorAnimationsIdForDiscard(uint64_t aId);
  void DiscardCompositorAnimations();

  void RegisterAsyncAnimation(const wr::ImageKey& aKey,
                              SharedSurfacesAnimation* aAnimation);
  void DeregisterAsyncAnimation(const wr::ImageKey& aKey);
  void ClearAsyncAnimations();
  void WrReleasedImages(const nsTArray<wr::ExternalImageKeyPair>& aPairs);

  void AddWebRenderParentCommand(const WebRenderParentCommand& aCmd);
  void UpdateResources(wr::IpcResourceUpdateQueue& aResources);
  void AddPipelineIdForAsyncCompositable(const wr::PipelineId& aPipelineId,
                                         const CompositableHandle& aHandlee);
  void AddPipelineIdForCompositable(const wr::PipelineId& aPipelineId,
                                    const CompositableHandle& aHandlee);
  void RemovePipelineIdForCompositable(const wr::PipelineId& aPipelineId);
  /// Release TextureClient that is bounded to ImageKey.
  /// It is used for recycling TextureClient.
  void ReleaseTextureOfImage(const wr::ImageKey& aKey);
  Maybe<wr::FontInstanceKey> GetFontKeyForScaledFont(
      gfx::ScaledFont* aScaledFont,
      wr::IpcResourceUpdateQueue* aResources = nullptr);
  Maybe<wr::FontKey> GetFontKeyForUnscaledFont(
      gfx::UnscaledFont* aUnscaledFont,
      wr::IpcResourceUpdateQueue* aResources = nullptr);

  void FlushAsyncResourceUpdates();

 private:
  WebRenderLayerManager* mLayerManager;
  Maybe<wr::IpcResourceUpdateQueue> mAsyncResourceUpdates;
  nsTArray<wr::ImageKey> mImageKeysToDelete;
  nsTArray<wr::BlobImageKey> mBlobImageKeysToDelete;
  std::unordered_map<uint64_t, RefPtr<SharedSurfacesAnimation>>
      mAsyncAnimations;

  // Set of compositor animation ids for which there are active animations (as
  // of the last transaction) on the compositor side.
  std::unordered_set<uint64_t> mActiveCompositorAnimationIds;
  // Compositor animation ids for animations that are done now and that we want
  // the compositor to discard information for.
  nsTArray<uint64_t> mDiscardedCompositorAnimationsIds;

  wr::RenderRoot mRenderRoot;
  bool mDestroyed;

  friend class WebRenderLayerManager;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_RENDERROOTSTATEMANAGER_H */
