/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderBridgeChild.h"

#include "gfxPlatform.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/PTextureChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/webrender/WebRenderAPI.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

WebRenderBridgeChild::WebRenderBridgeChild(const wr::PipelineId& aPipelineId)
  : mReadLockSequenceNumber(0)
  , mIsInTransaction(false)
  , mIsInClearCachedResources(false)
  , mIdNamespace{0}
  , mResourceId(0)
  , mPipelineId(aPipelineId)
  , mManager(nullptr)
  , mIPCOpen(false)
  , mDestroyed(false)
  , mFontKeysDeleted(0)
  , mFontInstanceKeysDeleted(0)
{
}

WebRenderBridgeChild::~WebRenderBridgeChild()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDestroyed);
}

void
WebRenderBridgeChild::Destroy(bool aIsSync)
{
  if (!IPCOpen()) {
    return;
  }

  DoDestroy();

  if (aIsSync) {
    SendShutdownSync();
  } else {
    SendShutdown();
  }
}

void
WebRenderBridgeChild::ActorDestroy(ActorDestroyReason why)
{
  DoDestroy();
}

void
WebRenderBridgeChild::DoDestroy()
{
  // mDestroyed is used to prevent calling Send__delete__() twice.
  // When this function is called from CompositorBridgeChild::Destroy().
  // mActiveResourceTracker is not cleared here, since it is
  // used by PersistentBufferProviderShared.
  mDestroyed = true;
  mManager = nullptr;
}

void
WebRenderBridgeChild::AddWebRenderParentCommand(const WebRenderParentCommand& aCmd)
{
  MOZ_ASSERT(mIsInTransaction || mIsInClearCachedResources);
  mParentCommands.AppendElement(aCmd);
}

void
WebRenderBridgeChild::AddWebRenderParentCommands(const nsTArray<WebRenderParentCommand>& aCommands)
{
  MOZ_ASSERT(mIsInTransaction);
  mParentCommands.AppendElements(aCommands);
}

void
WebRenderBridgeChild::BeginTransaction()
{
  MOZ_ASSERT(!mDestroyed);

  UpdateFwdTransactionId();
  mIsInTransaction = true;
  mReadLockSequenceNumber = 0;
  mReadLocks.AppendElement();
}

void
WebRenderBridgeChild::ClearReadLocks()
{
  for (nsTArray<ReadLockInit>& locks : mReadLocks) {
    if (locks.Length()) {
      if (!SendInitReadLocks(locks)) {
        NS_WARNING("WARNING: sending read locks failed!");
        return;
      }
    }
  }

  mReadLocks.Clear();
}

void
WebRenderBridgeChild::UpdateResources(wr::IpcResourceUpdateQueue& aResources)
{
  if (!IPCOpen()) {
    aResources.Clear();
    return;
  }

  nsTArray<OpUpdateResource> resourceUpdates;
  nsTArray<ipc::Shmem> smallShmems;
  nsTArray<ipc::Shmem> largeShmems;
  aResources.Flush(resourceUpdates, smallShmems, largeShmems);

  this->SendUpdateResources(resourceUpdates, Move(smallShmems), Move(largeShmems));
}

void
WebRenderBridgeChild::EndTransaction(const wr::LayoutSize& aContentSize,
                                     wr::BuiltDisplayList& aDL,
                                     wr::IpcResourceUpdateQueue& aResources,
                                     const gfx::IntSize& aSize,
                                     uint64_t aTransactionId,
                                     const WebRenderScrollData& aScrollData,
                                     const mozilla::TimeStamp& aTxnStartTime)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(mIsInTransaction);

  ByteBuf dlData(aDL.dl.inner.data, aDL.dl.inner.length, aDL.dl.inner.capacity);
  aDL.dl.inner.capacity = 0;
  aDL.dl.inner.data = nullptr;

  TimeStamp fwdTime;
#if defined(ENABLE_FRAME_LATENCY_LOG)
  fwdTime = TimeStamp::Now();
#endif

  nsTArray<OpUpdateResource> resourceUpdates;
  nsTArray<ipc::Shmem> smallShmems;
  nsTArray<ipc::Shmem> largeShmems;
  aResources.Flush(resourceUpdates, smallShmems, largeShmems);

  this->SendSetDisplayList(aSize, mParentCommands, mDestroyedActors,
                           GetFwdTransactionId(), aTransactionId,
                           aContentSize, dlData, aDL.dl_desc, aScrollData,
                           Move(resourceUpdates), Move(smallShmems), Move(largeShmems),
                           mIdNamespace, aTxnStartTime, fwdTime);

  mParentCommands.Clear();
  mDestroyedActors.Clear();
  mIsInTransaction = false;
}

void
WebRenderBridgeChild::EndEmptyTransaction(const FocusTarget& aFocusTarget,
                                          uint64_t aTransactionId,
                                          const mozilla::TimeStamp& aTxnStartTime)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(mIsInTransaction);

  TimeStamp fwdTime;
#if defined(ENABLE_FRAME_LATENCY_LOG)
  fwdTime = TimeStamp::Now();
#endif

  this->SendEmptyTransaction(aFocusTarget,
                             mParentCommands, mDestroyedActors,
                             GetFwdTransactionId(), aTransactionId,
                             mIdNamespace, aTxnStartTime, fwdTime);
  mParentCommands.Clear();
  mDestroyedActors.Clear();
  mIsInTransaction = false;
}

void
WebRenderBridgeChild::ProcessWebRenderParentCommands()
{
  MOZ_ASSERT(!mDestroyed);

  if (mParentCommands.IsEmpty()) {
    return;
  }
  this->SendParentCommands(mParentCommands);
  mParentCommands.Clear();
}

void
WebRenderBridgeChild::AddPipelineIdForAsyncCompositable(const wr::PipelineId& aPipelineId,
                                                        const CompositableHandle& aHandle)
{
  MOZ_ASSERT(!mDestroyed);
  SendAddPipelineIdForCompositable(aPipelineId, aHandle, true);
}

void
WebRenderBridgeChild::AddPipelineIdForCompositable(const wr::PipelineId& aPipelineId,
                                                   const CompositableHandle& aHandle)
{
  MOZ_ASSERT(!mDestroyed);
  SendAddPipelineIdForCompositable(aPipelineId, aHandle, false);
}

void
WebRenderBridgeChild::RemovePipelineIdForCompositable(const wr::PipelineId& aPipelineId)
{
  if (!IPCOpen()) {
    return;
  }
  SendRemovePipelineIdForCompositable(aPipelineId);
}

wr::ExternalImageId
WebRenderBridgeChild::GetNextExternalImageId()
{
  wr::MaybeExternalImageId id = GetCompositorBridgeChild()->GetNextExternalImageId();
  MOZ_RELEASE_ASSERT(id.isSome());
  return id.value();
}

wr::ExternalImageId
WebRenderBridgeChild::AllocExternalImageIdForCompositable(CompositableClient* aCompositable)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(aCompositable->IsConnected());

  wr::ExternalImageId imageId = GetNextExternalImageId();
  SendAddExternalImageIdForCompositable(imageId, aCompositable->GetIPCHandle());
  return imageId;
}

void
WebRenderBridgeChild::DeallocExternalImageId(const wr::ExternalImageId& aImageId)
{
  if (mDestroyed) {
    // This can happen if the IPC connection was torn down, because, e.g.
    // the GPU process died.
    return;
  }
  SendRemoveExternalImageId(aImageId);
}

struct FontFileDataSink
{
  wr::FontKey* mFontKey;
  WebRenderBridgeChild* mWrBridge;
  wr::IpcResourceUpdateQueue* mResources;
};

static void
WriteFontFileData(const uint8_t* aData, uint32_t aLength, uint32_t aIndex,
                  void* aBaton)
{
  FontFileDataSink* sink = static_cast<FontFileDataSink*>(aBaton);

  *sink->mFontKey = sink->mWrBridge->GetNextFontKey();

  sink->mResources->AddRawFont(*sink->mFontKey, Range<uint8_t>(const_cast<uint8_t*>(aData), aLength), aIndex);
}

static void
WriteFontDescriptor(const uint8_t* aData, uint32_t aLength, uint32_t aIndex,
                  void* aBaton)
{
  FontFileDataSink* sink = static_cast<FontFileDataSink*>(aBaton);

  *sink->mFontKey = sink->mWrBridge->GetNextFontKey();

  sink->mResources->AddFontDescriptor(*sink->mFontKey, Range<uint8_t>(const_cast<uint8_t*>(aData), aLength), aIndex);
}

void
WebRenderBridgeChild::PushGlyphs(wr::DisplayListBuilder& aBuilder, Range<const wr::GlyphInstance> aGlyphs,
                                 gfx::ScaledFont* aFont, const wr::ColorF& aColor, const StackingContextHelper& aSc,
                                 const wr::LayerRect& aBounds, const wr::LayerRect& aClip, bool aBackfaceVisible,
                                 const wr::GlyphOptions* aGlyphOptions)
{
  MOZ_ASSERT(aFont);

  wr::WrFontInstanceKey key = GetFontKeyForScaledFont(aFont);
  MOZ_ASSERT(key.mNamespace.mHandle && key.mHandle);

  aBuilder.PushText(aBounds,
                    aClip,
                    aBackfaceVisible,
                    aColor,
                    key,
                    aGlyphs,
                    aGlyphOptions);
}

wr::FontInstanceKey
WebRenderBridgeChild::GetFontKeyForScaledFont(gfx::ScaledFont* aScaledFont)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(aScaledFont);
  MOZ_ASSERT((aScaledFont->GetType() == gfx::FontType::DWRITE) ||
             (aScaledFont->GetType() == gfx::FontType::MAC) ||
             (aScaledFont->GetType() == gfx::FontType::FONTCONFIG));

  wr::FontInstanceKey instanceKey = { wr::IdNamespace { 0 }, 0 };
  if (mFontInstanceKeys.Get(aScaledFont, &instanceKey)) {
    return instanceKey;
  }

  wr::IpcResourceUpdateQueue resources(GetShmemAllocator());

  wr::FontKey fontKey = GetFontKeyForUnscaledFont(aScaledFont->GetUnscaledFont());
  wr::FontKey nullKey = { wr::IdNamespace { 0 }, 0};
  if (fontKey == nullKey) {
    return instanceKey;
  }

  instanceKey = GetNextFontInstanceKey();

  Maybe<wr::FontInstanceOptions> options;
  Maybe<wr::FontInstancePlatformOptions> platformOptions;
  std::vector<FontVariation> variations;
  aScaledFont->GetWRFontInstanceOptions(&options, &platformOptions, &variations);

  resources.AddFontInstance(instanceKey, fontKey, aScaledFont->GetSize(),
                            options.ptrOr(nullptr), platformOptions.ptrOr(nullptr),
                            Range<const FontVariation>(variations.data(), variations.size()));
  UpdateResources(resources);

  mFontInstanceKeys.Put(aScaledFont, instanceKey);

  return instanceKey;

}

wr::FontKey
WebRenderBridgeChild::GetFontKeyForUnscaledFont(gfx::UnscaledFont* aUnscaled)
{
  MOZ_ASSERT(!mDestroyed);

  wr::FontKey fontKey = { wr::IdNamespace { 0 }, 0};
  if (!mFontKeys.Get(aUnscaled, &fontKey)) {
    wr::IpcResourceUpdateQueue resources(GetShmemAllocator());
    FontFileDataSink sink = { &fontKey, this, &resources };
    // First try to retrieve a descriptor for the font, as this is much cheaper
    // to send over IPC than the full raw font data. If this is not possible, then
    // and only then fall back to getting the raw font file data. If that fails,
    // then the only thing left to do is signal failure by returning a null font key.
    if (!aUnscaled->GetWRFontDescriptor(WriteFontDescriptor, &sink) &&
        !aUnscaled->GetFontFileData(WriteFontFileData, &sink)) {
      return fontKey;
    }
    UpdateResources(resources);

    mFontKeys.Put(aUnscaled, fontKey);
  }

  return fontKey;
}

void
WebRenderBridgeChild::RemoveExpiredFontKeys()
{
  uint32_t counter = gfx::ScaledFont::DeletionCounter();
  wr::IpcResourceUpdateQueue resources(GetShmemAllocator());
  if (mFontInstanceKeysDeleted != counter) {
    mFontInstanceKeysDeleted = counter;
    for (auto iter = mFontInstanceKeys.Iter(); !iter.Done(); iter.Next()) {
      if (!iter.Key()) {
        resources.DeleteFontInstance(iter.Data());
        iter.Remove();
      }
    }
  }
  counter = gfx::UnscaledFont::DeletionCounter();
  if (mFontKeysDeleted != counter) {
    mFontKeysDeleted = counter;
    for (auto iter = mFontKeys.Iter(); !iter.Done(); iter.Next()) {
      if (!iter.Key()) {
        resources.DeleteFont(iter.Data());
        iter.Remove();
      }
    }
  }
  UpdateResources(resources);
}

CompositorBridgeChild*
WebRenderBridgeChild::GetCompositorBridgeChild()
{
  return static_cast<CompositorBridgeChild*>(Manager());
}

TextureForwarder*
WebRenderBridgeChild::GetTextureForwarder()
{
  return static_cast<TextureForwarder*>(GetCompositorBridgeChild());
}

LayersIPCActor*
WebRenderBridgeChild::GetLayersIPCActor()
{
  return static_cast<LayersIPCActor*>(GetCompositorBridgeChild());
}

void
WebRenderBridgeChild::SyncWithCompositor()
{
  auto compositorBridge = GetCompositorBridgeChild();
  if (compositorBridge && compositorBridge->IPCOpen()) {
    compositorBridge->SendSyncWithCompositor();
  }
}

void
WebRenderBridgeChild::Connect(CompositableClient* aCompositable,
                              ImageContainer* aImageContainer)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(aCompositable);

  static uint64_t sNextID = 1;
  uint64_t id = sNextID++;

  mCompositables.Put(id, aCompositable);

  CompositableHandle handle(id);
  aCompositable->InitIPDL(handle);
  SendNewCompositable(handle, aCompositable->GetTextureInfo());
}

void
WebRenderBridgeChild::UseTiledLayerBuffer(CompositableClient* aCompositable,
                                          const SurfaceDescriptorTiles& aTiledDescriptor)
{

}

void
WebRenderBridgeChild::UpdateTextureRegion(CompositableClient* aCompositable,
                                          const ThebesBufferData& aThebesBufferData,
                                          const nsIntRegion& aUpdatedRegion)
{

}

bool
WebRenderBridgeChild::AddOpDestroy(const OpDestroy& aOp)
{
  if (!mIsInTransaction) {
    return false;
  }

  mDestroyedActors.AppendElement(aOp);
  return true;
}

void
WebRenderBridgeChild::ReleaseCompositable(const CompositableHandle& aHandle)
{
  if (!IPCOpen()) {
    // This can happen if the IPC connection was torn down, because, e.g.
    // the GPU process died.
    return;
  }
  if (!DestroyInTransaction(aHandle)) {
    SendReleaseCompositable(aHandle);
  }
  mCompositables.Remove(aHandle.Value());
}

bool
WebRenderBridgeChild::DestroyInTransaction(PTextureChild* aTexture)
{
  return AddOpDestroy(OpDestroy(aTexture));
}

bool
WebRenderBridgeChild::DestroyInTransaction(const CompositableHandle& aHandle)
{
  return AddOpDestroy(OpDestroy(aHandle));
}

void
WebRenderBridgeChild::RemoveTextureFromCompositable(CompositableClient* aCompositable,
                                                    TextureClient* aTexture)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aTexture->GetIPDLActor());
  MOZ_RELEASE_ASSERT(aTexture->GetIPDLActor()->GetIPCChannel() == GetIPCChannel());
  if (!aCompositable->IsConnected() || !aTexture->GetIPDLActor()) {
    // We don't have an actor anymore, don't try to use it!
    return;
  }

  AddWebRenderParentCommand(
    CompositableOperation(
      aCompositable->GetIPCHandle(),
      OpRemoveTexture(nullptr, aTexture->GetIPDLActor())));
}

void
WebRenderBridgeChild::UseTextures(CompositableClient* aCompositable,
                                  const nsTArray<TimedTextureClient>& aTextures)
{
  MOZ_ASSERT(aCompositable);

  if (!aCompositable->IsConnected()) {
    return;
  }

  AutoTArray<TimedTexture,4> textures;

  for (auto& t : aTextures) {
    MOZ_ASSERT(t.mTextureClient);
    MOZ_ASSERT(t.mTextureClient->GetIPDLActor());
    MOZ_RELEASE_ASSERT(t.mTextureClient->GetIPDLActor()->GetIPCChannel() == GetIPCChannel());
    ReadLockDescriptor readLock;
    ReadLockHandle readLockHandle;
    if (t.mTextureClient->SerializeReadLock(readLock)) {
      readLockHandle = ReadLockHandle(++mReadLockSequenceNumber);
      if (mReadLocks.LastElement().Length() >= GetMaxFileDescriptorsPerMessage()) {
        mReadLocks.AppendElement();
      }
      mReadLocks.LastElement().AppendElement(ReadLockInit(readLock, readLockHandle));
    }
    textures.AppendElement(TimedTexture(nullptr, t.mTextureClient->GetIPDLActor(),
                                        readLockHandle,
                                        t.mTimeStamp, t.mPictureRect,
                                        t.mFrameID, t.mProducerID));
    GetCompositorBridgeChild()->HoldUntilCompositableRefReleasedIfNecessary(t.mTextureClient);
  }
  AddWebRenderParentCommand(CompositableOperation(aCompositable->GetIPCHandle(),
                                            OpUseTexture(textures)));
}

void
WebRenderBridgeChild::UseComponentAlphaTextures(CompositableClient* aCompositable,
                                                TextureClient* aClientOnBlack,
                                                TextureClient* aClientOnWhite)
{

}

void
WebRenderBridgeChild::UpdateFwdTransactionId()
{
  GetCompositorBridgeChild()->UpdateFwdTransactionId();
}

uint64_t
WebRenderBridgeChild::GetFwdTransactionId()
{
  return GetCompositorBridgeChild()->GetFwdTransactionId();
}

bool
WebRenderBridgeChild::InForwarderThread()
{
  return NS_IsMainThread();
}

mozilla::ipc::IPCResult
WebRenderBridgeChild::RecvWrUpdated(const wr::IdNamespace& aNewIdNamespace)
{
  if (mManager) {
    mManager->WrUpdated();
  }
  // Update mIdNamespace to identify obsolete keys and messages by WebRenderBridgeParent.
  // Since usage of invalid keys could cause crash in webrender.
  mIdNamespace = aNewIdNamespace;
  // Just clear FontInstaceKeys/FontKeys, they are removed during WebRenderAPI destruction.
  mFontInstanceKeys.Clear();
  mFontKeys.Clear();
  return IPC_OK();
}

void
WebRenderBridgeChild::BeginClearCachedResources()
{
  mIsInClearCachedResources = true;
}

void
WebRenderBridgeChild::EndClearCachedResources()
{
  if (!IPCOpen()) {
    mIsInClearCachedResources = false;
    return;
  }
  ProcessWebRenderParentCommands();
  SendClearCachedResources();
  mIsInClearCachedResources = false;
}

void
WebRenderBridgeChild::SetWebRenderLayerManager(WebRenderLayerManager* aManager)
{
  MOZ_ASSERT(aManager && !mManager);
  mManager = aManager;

  nsCOMPtr<nsIEventTarget> eventTarget = nullptr;
  if (dom::TabGroup* tabGroup = mManager->GetTabGroup()) {
    eventTarget = tabGroup->EventTargetFor(TaskCategory::Other);
  }
  MOZ_ASSERT(eventTarget || !XRE_IsContentProcess());
  mActiveResourceTracker = MakeUnique<ActiveResourceTracker>(
    1000, "CompositableForwarder", eventTarget);
}

ipc::IShmemAllocator*
WebRenderBridgeChild::GetShmemAllocator()
{
  return static_cast<CompositorBridgeChild*>(Manager());
}

} // namespace layers
} // namespace mozilla
