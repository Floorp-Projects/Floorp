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

namespace wr {
class DisplayListBuilder;
class ResourceUpdateQueue;
class IpcResourceUpdateQueue;
}

namespace layers {

class CompositableClient;
class CompositorBridgeChild;
class StackingContextHelper;
class TextureForwarder;

template<class T>
class WeakPtrHashKey : public PLDHashEntryHdr
{
public:
  typedef T* KeyType;
  typedef const T* KeyTypePointer;

  explicit WeakPtrHashKey(KeyTypePointer aKey) : mKey(const_cast<KeyType>(aKey)) {}

  KeyType GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const { return aKey == mKey; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return NS_PTR_TO_UINT32(aKey) >> 2;
  }
  enum { ALLOW_MEMMOVE = true };

private:
  WeakPtr<T> mKey;
};

typedef WeakPtrHashKey<gfx::UnscaledFont> UnscaledFontHashKey;
typedef WeakPtrHashKey<gfx::ScaledFont> ScaledFontHashKey;

class WebRenderBridgeChild final : public PWebRenderBridgeChild
                                 , public CompositableForwarder
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebRenderBridgeChild, override)

public:
  explicit WebRenderBridgeChild(const wr::PipelineId& aPipelineId);

  void AddWebRenderParentCommand(const WebRenderParentCommand& aCmd);
  void AddWebRenderParentCommands(const nsTArray<WebRenderParentCommand>& aCommands);

  void UpdateResources(wr::IpcResourceUpdateQueue& aResources);
  bool BeginTransaction(const  gfx::IntSize& aSize);
  void EndTransaction(wr::DisplayListBuilder &aBuilder,
                      wr::IpcResourceUpdateQueue& aResources,
                      const gfx::IntSize& aSize,
                      bool aIsSync, uint64_t aTransactionId,
                      const WebRenderScrollData& aScrollData,
                      const mozilla::TimeStamp& aTxnStartTime);
  void ProcessWebRenderParentCommands();

  CompositorBridgeChild* GetCompositorBridgeChild();

  wr::PipelineId GetPipeline() { return mPipelineId; }

  // KnowsCompositor
  TextureForwarder* GetTextureForwarder() override;
  LayersIPCActor* GetLayersIPCActor() override;

  void AddPipelineIdForAsyncCompositable(const wr::PipelineId& aPipelineId,
                                         const CompositableHandle& aHandlee);
  void AddPipelineIdForCompositable(const wr::PipelineId& aPipelineId,
                                    const CompositableHandle& aHandlee);
  void RemovePipelineIdForCompositable(const wr::PipelineId& aPipelineId);

  wr::ExternalImageId AllocExternalImageIdForCompositable(CompositableClient* aCompositable);
  void DeallocExternalImageId(wr::ExternalImageId& aImageId);

  /**
   * Clean this up, finishing with SendShutDown() which will cause __delete__
   * to be sent from the parent side.
   */
  void Destroy(bool aIsSync);
  bool IPCOpen() const { return mIPCOpen && !mDestroyed; }
  bool IsDestroyed() const { return mDestroyed; }

  uint32_t GetNextResourceId() { return ++mResourceId; }
  wr::IdNamespace GetNamespace() { return mIdNamespace; }
  void SetNamespace(wr::IdNamespace aIdNamespace)
  {
    mIdNamespace = aIdNamespace;
  }

  wr::WrImageKey GetNextImageKey()
  {
    return wr::WrImageKey{ GetNamespace(), GetNextResourceId() };
  }

  void PushGlyphs(wr::DisplayListBuilder& aBuilder, const nsTArray<gfx::Glyph>& aGlyphs,
                  gfx::ScaledFont* aFont, const gfx::Color& aColor,
                  const StackingContextHelper& aSc,
                  const LayerRect& aBounds, const LayerRect& aClip);

  wr::FontInstanceKey GetFontKeyForScaledFont(gfx::ScaledFont* aScaledFont);

  void RemoveExpiredFontKeys();
  void ClearReadLocks();

  void BeginClearCachedResources();
  void EndClearCachedResources();

  ipc::IShmemAllocator* GetShmemAllocator();

private:
  friend class CompositorBridgeChild;

  ~WebRenderBridgeChild() {}

  wr::ExternalImageId GetNextExternalImageId();

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

  virtual mozilla::ipc::IPCResult RecvWrUpdated(const wr::IdNamespace& aNewIdNamespace) override;

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

  nsTArray<WebRenderParentCommand> mParentCommands;
  nsTArray<OpDestroy> mDestroyedActors;
  nsDataHashtable<nsUint64HashKey, CompositableClient*> mCompositables;
  nsTArray<nsTArray<ReadLockInit>> mReadLocks;
  uint64_t mReadLockSequenceNumber;
  bool mIsInTransaction;
  bool mIsInClearCachedResources;
  wr::IdNamespace mIdNamespace;
  uint32_t mResourceId;
  wr::PipelineId mPipelineId;

  bool mIPCOpen;
  bool mDestroyed;

  uint32_t mFontKeysDeleted;
  nsDataHashtable<UnscaledFontHashKey, wr::FontKey> mFontKeys;

  uint32_t mFontInstanceKeysDeleted;
  nsDataHashtable<ScaledFontHashKey, wr::FontInstanceKey> mFontInstanceKeys;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_WebRenderBridgeChild_h
