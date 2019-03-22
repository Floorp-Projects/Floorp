/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/RenderRootStateManager.h"

#include "mozilla/layers/WebRenderBridgeChild.h"

namespace mozilla {
namespace layers {

// RenderRootStateManager shares its ref count with the WebRenderLayerManager
// that created it. You can think of the two classes as being one unit, except
// there are multiple RenderRootStateManagers per WebRenderLayerManager. Since
// we need to reference the WebRenderLayerManager and it needs to reference us,
// this avoids us needing to involve the cycle collector.
void RenderRootStateManager::AddRef() { mLayerManager->AddRef(); }

void RenderRootStateManager::Release() { mLayerManager->Release(); }

WebRenderBridgeChild* RenderRootStateManager::WrBridge() const {
  return mLayerManager->WrBridge();
}

WebRenderCommandBuilder& RenderRootStateManager::CommandBuilder() {
  return mLayerManager->CommandBuilder();
}

RenderRootStateManager::WebRenderUserDataRefTable*
RenderRootStateManager::GetWebRenderUserDataTable() {
  return mLayerManager->GetWebRenderUserDataTable();
}

wr::IpcResourceUpdateQueue& RenderRootStateManager::AsyncResourceUpdates() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess() || mRenderRoot == wr::RenderRoot::Default);

  if (!mAsyncResourceUpdates) {
    mAsyncResourceUpdates.emplace(WrBridge(), mRenderRoot);

    RefPtr<Runnable> task = NewRunnableMethod(
        "RenderRootStateManager::FlushAsyncResourceUpdates", this,
        &RenderRootStateManager::FlushAsyncResourceUpdates);
    NS_DispatchToMainThread(task.forget());
  }

  return mAsyncResourceUpdates.ref();
}

void RenderRootStateManager::Destroy() {
  ClearAsyncAnimations();

  if (WrBridge()) {
    // Just clear ImageKeys, they are deleted during WebRenderAPI destruction.
    DiscardLocalImages();
    // CompositorAnimations are cleared by WebRenderBridgeParent.
    mDiscardedCompositorAnimationsIds.Clear();
  }

  mActiveCompositorAnimationIds.clear();

  mDestroyed = true;
}

void RenderRootStateManager::FlushAsyncResourceUpdates() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mAsyncResourceUpdates) {
    return;
  }

  if (!IsDestroyed() && WrBridge()) {
    WrBridge()->UpdateResources(mAsyncResourceUpdates.ref(), mRenderRoot);
  }

  mAsyncResourceUpdates.reset();
}

void RenderRootStateManager::AddImageKeyForDiscard(wr::ImageKey key) {
  mImageKeysToDelete.AppendElement(key);
}

void RenderRootStateManager::AddBlobImageKeyForDiscard(wr::BlobImageKey key) {
  mBlobImageKeysToDelete.AppendElement(key);
}

void RenderRootStateManager::DiscardImagesInTransaction(
    wr::IpcResourceUpdateQueue& aResources) {
  for (const auto& key : mImageKeysToDelete) {
    aResources.DeleteImage(key);
  }
  for (const auto& key : mBlobImageKeysToDelete) {
    aResources.DeleteBlobImage(key);
  }
  mImageKeysToDelete.Clear();
  mBlobImageKeysToDelete.Clear();
}

void RenderRootStateManager::DiscardLocalImages() {
  // Removes images but doesn't tell the parent side about them
  // This is useful in empty / failed transactions where we created
  // image keys but didn't tell the parent about them yet.
  mImageKeysToDelete.Clear();
  mBlobImageKeysToDelete.Clear();
}

void RenderRootStateManager::ClearCachedResources() {
  mActiveCompositorAnimationIds.clear();
  mDiscardedCompositorAnimationsIds.Clear();
}

void RenderRootStateManager::AddActiveCompositorAnimationId(uint64_t aId) {
  // In layers-free mode we track the active compositor animation ids on the
  // client side so that we don't try to discard the same animation id multiple
  // times. We could just ignore the multiple-discard on the parent side, but
  // checking on the content side reduces IPC traffic.
  mActiveCompositorAnimationIds.insert(aId);
}

void RenderRootStateManager::AddCompositorAnimationsIdForDiscard(uint64_t aId) {
  if (mActiveCompositorAnimationIds.erase(aId)) {
    // For layers-free ensure we don't try to discard an animation id that
    // wasn't active. We also remove it from mActiveCompositorAnimationIds so we
    // don't discard it again unless it gets re-activated.
    mDiscardedCompositorAnimationsIds.AppendElement(aId);
  }
}

void RenderRootStateManager::DiscardCompositorAnimations() {
  if (WrBridge()->IPCOpen() && !mDiscardedCompositorAnimationsIds.IsEmpty()) {
    WrBridge()->SendDeleteCompositorAnimations(
        mDiscardedCompositorAnimationsIds);
  }
  mDiscardedCompositorAnimationsIds.Clear();
}

void RenderRootStateManager::RegisterAsyncAnimation(
    const wr::ImageKey& aKey, SharedSurfacesAnimation* aAnimation) {
  mAsyncAnimations.insert(std::make_pair(wr::AsUint64(aKey), aAnimation));
}

void RenderRootStateManager::DeregisterAsyncAnimation(
    const wr::ImageKey& aKey) {
  mAsyncAnimations.erase(wr::AsUint64(aKey));
}

void RenderRootStateManager::ClearAsyncAnimations() {
  for (const auto& i : mAsyncAnimations) {
    i.second->Invalidate(this);
  }
  mAsyncAnimations.clear();
}

void RenderRootStateManager::WrReleasedImages(
    const nsTArray<wr::ExternalImageKeyPair>& aPairs) {
  // A SharedSurfaceAnimation object's lifetime is tied to its owning
  // ImageContainer. When the ImageContainer is released,
  // SharedSurfaceAnimation::Destroy is called which should ensure it is removed
  // from the layer manager. Whenever the namespace for the
  // WebRenderLayerManager itself is invalidated (e.g. we changed windows, or
  // were destroyed ourselves), we callback into the SharedSurfaceAnimation
  // object to remove its image key for us and any bound surfaces. If, for any
  // reason, we somehow missed an WrReleasedImages call before the animation
  // was bound to the layer manager, it will free those associated surfaces on
  // the next ReleasePreviousFrame call.
  for (const auto& pair : aPairs) {
    auto i = mAsyncAnimations.find(wr::AsUint64(pair.key));
    if (i != mAsyncAnimations.end()) {
      i->second->ReleasePreviousFrame(this, pair.id);
    }
  }
}

void RenderRootStateManager::AddWebRenderParentCommand(
    const WebRenderParentCommand& aCmd) {
  WrBridge()->AddWebRenderParentCommand(aCmd, mRenderRoot);
}
void RenderRootStateManager::UpdateResources(
    wr::IpcResourceUpdateQueue& aResources) {
  WrBridge()->UpdateResources(aResources, mRenderRoot);
}
void RenderRootStateManager::AddPipelineIdForAsyncCompositable(
    const wr::PipelineId& aPipelineId, const CompositableHandle& aHandle) {
  WrBridge()->AddPipelineIdForAsyncCompositable(aPipelineId, aHandle,
                                                mRenderRoot);
}
void RenderRootStateManager::AddPipelineIdForCompositable(
    const wr::PipelineId& aPipelineId, const CompositableHandle& aHandle) {
  WrBridge()->AddPipelineIdForCompositable(aPipelineId, aHandle, mRenderRoot);
}
void RenderRootStateManager::RemovePipelineIdForCompositable(
    const wr::PipelineId& aPipelineId) {
  WrBridge()->RemovePipelineIdForCompositable(aPipelineId, mRenderRoot);
}
/// Release TextureClient that is bounded to ImageKey.
/// It is used for recycling TextureClient.
void RenderRootStateManager::ReleaseTextureOfImage(const wr::ImageKey& aKey) {
  WrBridge()->ReleaseTextureOfImage(aKey, mRenderRoot);
}

Maybe<wr::FontInstanceKey> RenderRootStateManager::GetFontKeyForScaledFont(
    gfx::ScaledFont* aScaledFont, wr::IpcResourceUpdateQueue* aResources) {
  return WrBridge()->GetFontKeyForScaledFont(aScaledFont, mRenderRoot,
                                             aResources);
}

Maybe<wr::FontKey> RenderRootStateManager::GetFontKeyForUnscaledFont(
    gfx::UnscaledFont* aUnscaledFont, wr::IpcResourceUpdateQueue* aResources) {
  return WrBridge()->GetFontKeyForUnscaledFont(aUnscaledFont, mRenderRoot,
                                               aResources);
}

}  // namespace layers
}  // namespace mozilla
