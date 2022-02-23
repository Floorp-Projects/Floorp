/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderBridgeChild.h"

#include "gfxPlatform.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/PTextureChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "PDMFactory.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

WebRenderBridgeChild::WebRenderBridgeChild(const wr::PipelineId& aPipelineId)
    : mIsInTransaction(false),
      mIsInClearCachedResources(false),
      mIdNamespace{0},
      mResourceId(0),
      mPipelineId(aPipelineId),
      mManager(nullptr),
      mIPCOpen(false),
      mDestroyed(false),
      mSentDisplayList(false),
      mFontKeysDeleted(0),
      mFontInstanceKeysDeleted(0) {}

WebRenderBridgeChild::~WebRenderBridgeChild() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDestroyed);
}

void WebRenderBridgeChild::Destroy(bool aIsSync) {
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

void WebRenderBridgeChild::ActorDestroy(ActorDestroyReason why) { DoDestroy(); }

void WebRenderBridgeChild::DoDestroy() {
  if (RefCountedShm::IsValid(mResourceShm) &&
      RefCountedShm::Release(mResourceShm) == 0) {
    RefCountedShm::Dealloc(this, mResourceShm);
    mResourceShm = RefCountedShmem();
  }

  // mDestroyed is used to prevent calling Send__delete__() twice.
  // When this function is called from CompositorBridgeChild::Destroy().
  // mActiveResourceTracker is not cleared here, since it is
  // used by PersistentBufferProviderShared.
  mDestroyed = true;
  mManager = nullptr;
}

void WebRenderBridgeChild::AddWebRenderParentCommand(
    const WebRenderParentCommand& aCmd) {
  mParentCommands.AppendElement(aCmd);
}

void WebRenderBridgeChild::BeginTransaction() {
  MOZ_ASSERT(!mDestroyed);

  UpdateFwdTransactionId();
  mIsInTransaction = true;
}

void WebRenderBridgeChild::UpdateResources(
    wr::IpcResourceUpdateQueue& aResources) {
  if (!IPCOpen()) {
    aResources.Clear();
    return;
  }

  if (aResources.IsEmpty()) {
    return;
  }

  nsTArray<OpUpdateResource> resourceUpdates;
  nsTArray<RefCountedShmem> smallShmems;
  nsTArray<ipc::Shmem> largeShmems;
  aResources.Flush(resourceUpdates, smallShmems, largeShmems);

  this->SendUpdateResources(mIdNamespace, resourceUpdates, smallShmems,
                            std::move(largeShmems));
}

bool WebRenderBridgeChild::EndTransaction(
    DisplayListData&& aDisplayListData, TransactionId aTransactionId,
    bool aContainsSVGGroup, const mozilla::VsyncId& aVsyncId,
    const mozilla::TimeStamp& aVsyncStartTime,
    const mozilla::TimeStamp& aRefreshStartTime,
    const mozilla::TimeStamp& aTxnStartTime, const nsCString& aTxnURL) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(mIsInTransaction);

  TimeStamp fwdTime = TimeStamp::Now();

  aDisplayListData.mCommands = std::move(mParentCommands);
  aDisplayListData.mIdNamespace = mIdNamespace;

  nsTArray<CompositionPayload> payloads;
  if (mManager) {
    mManager->TakeCompositionPayloads(payloads);
  }

  mSentDisplayList = true;
  bool ret = this->SendSetDisplayList(
      std::move(aDisplayListData), mDestroyedActors, GetFwdTransactionId(),
      aTransactionId, aContainsSVGGroup, aVsyncId, aVsyncStartTime,
      aRefreshStartTime, aTxnStartTime, aTxnURL, fwdTime, payloads);

  // With multiple render roots, we may not have sent all of our
  // mParentCommands, so go ahead and go through our mParentCommands and ensure
  // they get sent.
  ProcessWebRenderParentCommands();
  mDestroyedActors.Clear();
  mIsInTransaction = false;

  return ret;
}

void WebRenderBridgeChild::EndEmptyTransaction(
    const FocusTarget& aFocusTarget, Maybe<TransactionData>&& aTransactionData,
    TransactionId aTransactionId, const mozilla::VsyncId& aVsyncId,
    const mozilla::TimeStamp& aVsyncStartTime,
    const mozilla::TimeStamp& aRefreshStartTime,
    const mozilla::TimeStamp& aTxnStartTime, const nsCString& aTxnURL) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(mIsInTransaction);

  TimeStamp fwdTime = TimeStamp::Now();

  if (aTransactionData) {
    aTransactionData->mCommands = std::move(mParentCommands);
  }

  nsTArray<CompositionPayload> payloads;
  if (mManager) {
    mManager->TakeCompositionPayloads(payloads);
  }

  this->SendEmptyTransaction(
      aFocusTarget, std::move(aTransactionData), mDestroyedActors,
      GetFwdTransactionId(), aTransactionId, aVsyncId, aVsyncStartTime,
      aRefreshStartTime, aTxnStartTime, aTxnURL, fwdTime, payloads);

  // With multiple render roots, we may not have sent all of our
  // mParentCommands, so go ahead and go through our mParentCommands and ensure
  // they get sent.
  ProcessWebRenderParentCommands();
  mDestroyedActors.Clear();
  mIsInTransaction = false;
}

void WebRenderBridgeChild::ProcessWebRenderParentCommands() {
  MOZ_ASSERT(!mDestroyed);

  if (!mParentCommands.IsEmpty()) {
    this->SendParentCommands(mParentCommands);
    mParentCommands.Clear();
  }
}

void WebRenderBridgeChild::AddPipelineIdForAsyncCompositable(
    const wr::PipelineId& aPipelineId, const CompositableHandle& aHandle) {
  AddWebRenderParentCommand(
      OpAddPipelineIdForCompositable(aPipelineId, aHandle, /* isAsync */ true));
}

void WebRenderBridgeChild::AddPipelineIdForCompositable(
    const wr::PipelineId& aPipelineId, const CompositableHandle& aHandle) {
  AddWebRenderParentCommand(OpAddPipelineIdForCompositable(
      aPipelineId, aHandle, /* isAsync */ false));
}

void WebRenderBridgeChild::RemovePipelineIdForCompositable(
    const wr::PipelineId& aPipelineId) {
  AddWebRenderParentCommand(OpRemovePipelineIdForCompositable(aPipelineId));
}

wr::ExternalImageId WebRenderBridgeChild::GetNextExternalImageId() {
  wr::MaybeExternalImageId id =
      GetCompositorBridgeChild()->GetNextExternalImageId();
  MOZ_RELEASE_ASSERT(id.isSome());
  return id.value();
}

void WebRenderBridgeChild::ReleaseTextureOfImage(const wr::ImageKey& aKey) {
  AddWebRenderParentCommand(OpReleaseTextureOfImage(aKey));
}

struct FontFileDataSink {
  wr::FontKey* mFontKey;
  WebRenderBridgeChild* mWrBridge;
  wr::IpcResourceUpdateQueue* mResources;
};

static void WriteFontFileData(const uint8_t* aData, uint32_t aLength,
                              uint32_t aIndex, void* aBaton) {
  FontFileDataSink* sink = static_cast<FontFileDataSink*>(aBaton);

  *sink->mFontKey = sink->mWrBridge->GetNextFontKey();

  sink->mResources->AddRawFont(
      *sink->mFontKey, Range<uint8_t>(const_cast<uint8_t*>(aData), aLength),
      aIndex);
}

static void WriteFontDescriptor(const uint8_t* aData, uint32_t aLength,
                                uint32_t aIndex, void* aBaton) {
  FontFileDataSink* sink = static_cast<FontFileDataSink*>(aBaton);

  *sink->mFontKey = sink->mWrBridge->GetNextFontKey();

  sink->mResources->AddFontDescriptor(
      *sink->mFontKey, Range<uint8_t>(const_cast<uint8_t*>(aData), aLength),
      aIndex);
}

void WebRenderBridgeChild::PushGlyphs(
    wr::DisplayListBuilder& aBuilder, Range<const wr::GlyphInstance> aGlyphs,
    gfx::ScaledFont* aFont, const wr::ColorF& aColor,
    const StackingContextHelper& aSc, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, bool aBackfaceVisible,
    const wr::GlyphOptions* aGlyphOptions) {
  MOZ_ASSERT(aFont);

  Maybe<wr::WrFontInstanceKey> key = GetFontKeyForScaledFont(aFont);
  MOZ_ASSERT(key.isSome());

  if (key.isSome()) {
    aBuilder.PushText(aBounds, aClip, aBackfaceVisible, aColor, key.value(),
                      aGlyphs, aGlyphOptions);
  }
}

Maybe<wr::FontInstanceKey> WebRenderBridgeChild::GetFontKeyForScaledFont(
    gfx::ScaledFont* aScaledFont, wr::IpcResourceUpdateQueue* aResources) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(aScaledFont);
  MOZ_ASSERT(aScaledFont->CanSerialize());

  return mFontInstanceKeys.WithEntryHandle(
      aScaledFont, [&](auto&& entry) -> Maybe<wr::FontInstanceKey> {
        if (!entry) {
          Maybe<wr::IpcResourceUpdateQueue> resources =
              aResources ? Nothing() : Some(wr::IpcResourceUpdateQueue(this));
          aResources = resources.ptrOr(aResources);

          Maybe<wr::FontKey> fontKey = GetFontKeyForUnscaledFont(
              aScaledFont->GetUnscaledFont(), aResources);
          if (fontKey.isNothing()) {
            return Nothing();
          }

          wr::FontInstanceKey instanceKey = GetNextFontInstanceKey();

          Maybe<wr::FontInstanceOptions> options;
          Maybe<wr::FontInstancePlatformOptions> platformOptions;
          std::vector<FontVariation> variations;
          aScaledFont->GetWRFontInstanceOptions(&options, &platformOptions,
                                                &variations);

          aResources->AddFontInstance(
              instanceKey, fontKey.value(), aScaledFont->GetSize(),
              options.ptrOr(nullptr), platformOptions.ptrOr(nullptr),
              Range<const FontVariation>(variations.data(), variations.size()));
          if (resources.isSome()) {
            UpdateResources(resources.ref());
          }

          entry.Insert(instanceKey);
        }

        return Some(*entry);
      });
}

Maybe<wr::FontKey> WebRenderBridgeChild::GetFontKeyForUnscaledFont(
    gfx::UnscaledFont* aUnscaled, wr::IpcResourceUpdateQueue* aResources) {
  MOZ_ASSERT(!mDestroyed);

  return mFontKeys.WithEntryHandle(
      aUnscaled, [&](auto&& entry) -> Maybe<wr::FontKey> {
        if (!entry) {
          Maybe<wr::IpcResourceUpdateQueue> resources =
              aResources ? Nothing() : Some(wr::IpcResourceUpdateQueue(this));

          wr::FontKey fontKey = {wr::IdNamespace{0}, 0};
          FontFileDataSink sink = {&fontKey, this, resources.ptrOr(aResources)};
          // First try to retrieve a descriptor for the font, as this is much
          // cheaper to send over IPC than the full raw font data. If this is
          // not possible, then and only then fall back to getting the raw font
          // file data. If that fails, then the only thing left to do is signal
          // failure by returning a null font key.
          if (!aUnscaled->GetFontDescriptor(WriteFontDescriptor, &sink) &&
              !aUnscaled->GetFontFileData(WriteFontFileData, &sink)) {
            return Nothing();
          }

          if (resources.isSome()) {
            UpdateResources(resources.ref());
          }

          entry.Insert(fontKey);
        }

        return Some(*entry);
      });
}

void WebRenderBridgeChild::RemoveExpiredFontKeys(
    wr::IpcResourceUpdateQueue& aResourceUpdates) {
  uint32_t counter = gfx::ScaledFont::DeletionCounter();
  if (mFontInstanceKeysDeleted != counter) {
    mFontInstanceKeysDeleted = counter;
    for (auto iter = mFontInstanceKeys.Iter(); !iter.Done(); iter.Next()) {
      if (!iter.Key()) {
        aResourceUpdates.DeleteFontInstance(iter.Data());
        iter.Remove();
      }
    }
  }
  counter = gfx::UnscaledFont::DeletionCounter();
  if (mFontKeysDeleted != counter) {
    mFontKeysDeleted = counter;
    for (auto iter = mFontKeys.Iter(); !iter.Done(); iter.Next()) {
      if (!iter.Key()) {
        aResourceUpdates.DeleteFont(iter.Data());
        iter.Remove();
      }
    }
  }
}

CompositorBridgeChild* WebRenderBridgeChild::GetCompositorBridgeChild() {
  if (!IPCOpen()) {
    return nullptr;
  }
  return static_cast<CompositorBridgeChild*>(Manager());
}

TextureForwarder* WebRenderBridgeChild::GetTextureForwarder() {
  return static_cast<TextureForwarder*>(GetCompositorBridgeChild());
}

LayersIPCActor* WebRenderBridgeChild::GetLayersIPCActor() {
  return static_cast<LayersIPCActor*>(GetCompositorBridgeChild());
}

void WebRenderBridgeChild::SyncWithCompositor() {
  if (!IPCOpen()) {
    return;
  }
  SendSyncWithCompositor();
}

void WebRenderBridgeChild::Connect(CompositableClient* aCompositable,
                                   ImageContainer* aImageContainer) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(aCompositable);

  static uint64_t sNextID = 1;
  uint64_t id = sNextID++;

  mCompositables.InsertOrUpdate(id, aCompositable);

  CompositableHandle handle(id);
  aCompositable->InitIPDL(handle);
  SendNewCompositable(handle, aCompositable->GetTextureInfo());
}

bool WebRenderBridgeChild::AddOpDestroy(const OpDestroy& aOp) {
  if (!mIsInTransaction) {
    return false;
  }

  mDestroyedActors.AppendElement(aOp);
  return true;
}

void WebRenderBridgeChild::ReleaseCompositable(
    const CompositableHandle& aHandle) {
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

bool WebRenderBridgeChild::DestroyInTransaction(PTextureChild* aTexture) {
  return AddOpDestroy(OpDestroy(aTexture));
}

bool WebRenderBridgeChild::DestroyInTransaction(
    const CompositableHandle& aHandle) {
  return AddOpDestroy(OpDestroy(aHandle));
}

void WebRenderBridgeChild::RemoveTextureFromCompositable(
    CompositableClient* aCompositable, TextureClient* aTexture) {
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aTexture->GetIPDLActor());
  MOZ_RELEASE_ASSERT(aTexture->GetIPDLActor()->GetIPCChannel() ==
                     GetIPCChannel());
  if (!aCompositable->IsConnected() || !aTexture->GetIPDLActor()) {
    // We don't have an actor anymore, don't try to use it!
    return;
  }

  AddWebRenderParentCommand(CompositableOperation(
      aCompositable->GetIPCHandle(),
      OpRemoveTexture(nullptr, aTexture->GetIPDLActor())));
}

void WebRenderBridgeChild::UseTextures(
    CompositableClient* aCompositable,
    const nsTArray<TimedTextureClient>& aTextures) {
  MOZ_ASSERT(aCompositable);

  if (!aCompositable->IsConnected()) {
    return;
  }

  AutoTArray<TimedTexture, 4> textures;

  for (auto& t : aTextures) {
    MOZ_ASSERT(t.mTextureClient);
    MOZ_ASSERT(t.mTextureClient->GetIPDLActor());
    MOZ_RELEASE_ASSERT(t.mTextureClient->GetIPDLActor()->GetIPCChannel() ==
                       GetIPCChannel());
    bool readLocked = t.mTextureClient->OnForwardedToHost();

    textures.AppendElement(
        TimedTexture(nullptr, t.mTextureClient->GetIPDLActor(), t.mTimeStamp,
                     t.mPictureRect, t.mFrameID, t.mProducerID, readLocked));
    GetCompositorBridgeChild()->HoldUntilCompositableRefReleasedIfNecessary(
        t.mTextureClient);

    auto fenceFd = t.mTextureClient->GetInternalData()->GetAcquireFence();
    if (fenceFd.IsValid()) {
      AddWebRenderParentCommand(CompositableOperation(
          aCompositable->GetIPCHandle(),
          OpDeliverAcquireFence(nullptr, t.mTextureClient->GetIPDLActor(),
                                fenceFd)));
    }
  }
  AddWebRenderParentCommand(CompositableOperation(aCompositable->GetIPCHandle(),
                                                  OpUseTexture(textures)));
}

void WebRenderBridgeChild::UpdateFwdTransactionId() {
  GetCompositorBridgeChild()->UpdateFwdTransactionId();
}

uint64_t WebRenderBridgeChild::GetFwdTransactionId() {
  return GetCompositorBridgeChild()->GetFwdTransactionId();
}

bool WebRenderBridgeChild::InForwarderThread() { return NS_IsMainThread(); }

mozilla::ipc::IPCResult WebRenderBridgeChild::RecvWrUpdated(
    const wr::IdNamespace& aNewIdNamespace,
    const TextureFactoryIdentifier& textureFactoryIdentifier) {
  if (mManager) {
    mManager->WrUpdated();
  }
  IdentifyTextureHost(textureFactoryIdentifier);
  // Update mIdNamespace to identify obsolete keys and messages by
  // WebRenderBridgeParent. Since usage of invalid keys could cause crash in
  // webrender.
  mIdNamespace = aNewIdNamespace;
  // Just clear FontInstaceKeys/FontKeys, they are removed during WebRenderAPI
  // destruction.
  mFontInstanceKeys.Clear();
  mFontKeys.Clear();
  return IPC_OK();
}

mozilla::ipc::IPCResult WebRenderBridgeChild::RecvWrReleasedImages(
    nsTArray<wr::ExternalImageKeyPair>&& aPairs) {
  if (mManager) {
    mManager->WrReleasedImages(aPairs);
  }
  return IPC_OK();
}

void WebRenderBridgeChild::BeginClearCachedResources() {
  mSentDisplayList = false;
  mIsInClearCachedResources = true;
  // Clear display list and animtaions at parent side before clearing cached
  // resources on client side. It prevents to clear resources before clearing
  // display list at parent side.
  SendClearCachedResources();
}

void WebRenderBridgeChild::EndClearCachedResources() {
  if (!IPCOpen()) {
    mIsInClearCachedResources = false;
    return;
  }
  ProcessWebRenderParentCommands();
  mIsInClearCachedResources = false;
}

void WebRenderBridgeChild::SetWebRenderLayerManager(
    WebRenderLayerManager* aManager) {
  MOZ_ASSERT(aManager && !mManager);
  mManager = aManager;

  MOZ_ASSERT(NS_IsMainThread() || !XRE_IsContentProcess());
  mActiveResourceTracker =
      MakeUnique<ActiveResourceTracker>(1000, "CompositableForwarder", nullptr);
}

ipc::IShmemAllocator* WebRenderBridgeChild::GetShmemAllocator() {
  if (!IPCOpen()) {
    return nullptr;
  }
  return static_cast<CompositorBridgeChild*>(Manager());
}

RefPtr<KnowsCompositor> WebRenderBridgeChild::GetForMedia() {
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure device initialization for video playback unless they are all remote.
  // The devices are lazily initialized with WebRender to reduce memory usage.
  if (!PDMFactory::AllDecodersAreRemote()) {
    gfxPlatform::GetPlatform()->EnsureDevicesInitialized();
  }

  return MakeAndAddRef<KnowsCompositorMediaProxy>(
      GetTextureFactoryIdentifier());
}

bool WebRenderBridgeChild::AllocResourceShmem(size_t aSize,
                                              RefCountedShmem& aShm) {
  // We keep a single shmem around to reuse later if it is reference count has
  // dropped back to 1 (the reference held by the WebRenderBridgeChild).

  // If the cached shmem exists, has the correct size and isn't held by anything
  // other than us, recycle it.
  bool alreadyAllocated = RefCountedShm::IsValid(mResourceShm);
  if (alreadyAllocated) {
    if (RefCountedShm::GetSize(mResourceShm) == aSize &&
        RefCountedShm::GetReferenceCount(mResourceShm) <= 1) {
      MOZ_ASSERT(RefCountedShm::GetReferenceCount(mResourceShm) == 1);
      aShm = mResourceShm;
      return true;
    }
  }

  // If there was no cached shmem or we couldn't recycle it, alloc a new one.
  if (!RefCountedShm::Alloc(this, aSize, aShm)) {
    return false;
  }

  // Now that we have a valid shmem, put it in the cache if we don't have one
  // yet.
  if (!alreadyAllocated) {
    mResourceShm = aShm;
    RefCountedShm::AddRef(aShm);
  }

  return true;
}

void WebRenderBridgeChild::DeallocResourceShmem(RefCountedShmem& aShm) {
  if (!RefCountedShm::IsValid(aShm)) {
    return;
  }
  MOZ_ASSERT(RefCountedShm::GetReferenceCount(aShm) == 0);

  RefCountedShm::Dealloc(this, aShm);
}

void WebRenderBridgeChild::Capture() { this->SendCapture(); }

void WebRenderBridgeChild::StartCaptureSequence(const nsCString& aPath,
                                                uint32_t aFlags) {
  this->SendStartCaptureSequence(aPath, aFlags);
}

void WebRenderBridgeChild::StopCaptureSequence() {
  this->SendStopCaptureSequence();
}

}  // namespace layers
}  // namespace mozilla
