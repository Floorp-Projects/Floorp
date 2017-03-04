/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_WebRenderBridgeChild_h
#define mozilla_layers_WebRenderBridgeChild_h

#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/PWebRenderBridgeChild.h"

namespace mozilla {

namespace widget {
class CompositorWidget;
}

namespace layers {

class CompositableClient;
class CompositorBridgeChild;
class TextureForwarder;

class WebRenderBridgeChild final : public PWebRenderBridgeChild
                                 , public CompositableForwarder
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebRenderBridgeChild, override)

public:
  explicit WebRenderBridgeChild(const wr::PipelineId& aPipelineId);

  void AddWebRenderCommand(const WebRenderCommand& aCmd);
  void AddWebRenderCommands(const nsTArray<WebRenderCommand>& aCommands);
  void AddWebRenderParentCommand(const WebRenderParentCommand& aCmd);
  void AddWebRenderParentCommands(const nsTArray<WebRenderParentCommand>& aCommands);

  bool DPBegin(const  gfx::IntSize& aSize);
  void DPEnd(const gfx::IntSize& aSize, bool aIsSync, uint64_t aTransactionId);

  CompositorBridgeChild* GetCompositorBridgeChild();

  // KnowsCompositor
  TextureForwarder* GetTextureForwarder() override;
  LayersIPCActor* GetLayersIPCActor() override;

  uint64_t AllocExternalImageId(const CompositableHandle& aHandle);
  uint64_t AllocExternalImageIdForCompositable(CompositableClient* aCompositable);
  void DeallocExternalImageId(uint64_t aImageId);

  /**
   * Clean this up, finishing with SendShutDown() which will cause __delete__
   * to be sent from the parent side.
   */
  void Destroy();
  bool IPCOpen() const { return mIPCOpen && !mDestroyed; }
  bool IsDestroyed() const { return mDestroyed; }

  uint32_t GetNextResourceId() { return ++mResourceId; }
  uint32_t GetNamespace() { return mIdNamespace; }
  void SetNamespace(uint32_t aIdNamespace)
  {
    mIdNamespace = aIdNamespace;
  }

private:
  friend class CompositorBridgeChild;

  ~WebRenderBridgeChild() {}

  uint64_t GetNextExternalImageId();

  wr::BuiltDisplayList ProcessWebrenderCommands(const gfx::IntSize &aSize,
                                                InfallibleTArray<WebRenderCommand>& aCommands);

  // CompositableForwarder
  void Connect(CompositableClient* aCompositable,
               ImageContainer* aImageContainer = nullptr) override;
  void UseTiledLayerBuffer(CompositableClient* aCompositable,
                           const SurfaceDescriptorTiles& aTiledDescriptor) override;
  void UpdateTextureRegion(CompositableClient* aCompositable,
                           const ThebesBufferData& aThebesBufferData,
                           const nsIntRegion& aUpdatedRegion) override;
  void ReleaseCompositable(const CompositableHandle& aHandle) override;
  bool DestroyInTransaction(PTextureChild* aTexture) override;
  bool DestroyInTransaction(const CompositableHandle& aHandle);
  void RemoveTextureFromCompositable(CompositableClient* aCompositable,
                                     TextureClient* aTexture) override;
  void UseTextures(CompositableClient* aCompositable,
                   const nsTArray<TimedTextureClient>& aTextures) override;
  void UseComponentAlphaTextures(CompositableClient* aCompositable,
                                 TextureClient* aClientOnBlack,
                                 TextureClient* aClientOnWhite) override;
  void UpdateFwdTransactionId() override;
  uint64_t GetFwdTransactionId() override;
  bool InForwarderThread() override;

  void ActorDestroy(ActorDestroyReason why) override;

  void AddIPDLReference() {
    MOZ_ASSERT(mIPCOpen == false);
    mIPCOpen = true;
    AddRef();
  }
  void ReleaseIPDLReference() {
    MOZ_ASSERT(mIPCOpen == true);
    mIPCOpen = false;
    Release();
  }

  bool AddOpDestroy(const OpDestroy& aOp);

  nsTArray<WebRenderCommand> mCommands;
  nsTArray<WebRenderParentCommand> mParentCommands;
  nsTArray<OpDestroy> mDestroyedActors;
  nsDataHashtable<nsUint64HashKey, CompositableClient*> mCompositables;
  bool mIsInTransaction;
  uint32_t mIdNamespace;
  uint32_t mResourceId;
  wr::PipelineId mPipelineId;

  bool mIPCOpen;
  bool mDestroyed;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_WebRenderBridgeChild_h
