/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderBridgeChild.h"

#include "gfxPlatform.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/PTextureChild.h"
#include "mozilla/webrender/WebRenderAPI.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

WebRenderBridgeChild::WebRenderBridgeChild(const wr::PipelineId& aPipelineId)
  : mReadLockSequenceNumber(0)
  , mIsInTransaction(false)
  , mIsInClearCachedResources(false)
  , mIdNamespace(0)
  , mResourceId(0)
  , mPipelineId(aPipelineId)
  , mIPCOpen(false)
  , mDestroyed(false)
  , mFontKeysDeleted(0)
{
}

void
WebRenderBridgeChild::Destroy(bool aIsSync)
{
  if (!IPCOpen()) {
    return;
  }
  // mDestroyed is used to prevent calling Send__delete__() twice.
  // When this function is called from CompositorBridgeChild::Destroy().
  mDestroyed = true;

  if (aIsSync) {
    SendShutdownSync();
  } else {
    SendShutdown();
  }
}

void
WebRenderBridgeChild::ActorDestroy(ActorDestroyReason why)
{
  mDestroyed = true;
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

bool
WebRenderBridgeChild::DPBegin(const gfx::IntSize& aSize)
{
  MOZ_ASSERT(!mDestroyed);

  UpdateFwdTransactionId();
  this->SendDPBegin(aSize);
  mIsInTransaction = true;
  mReadLockSequenceNumber = 0;
  mReadLocks.AppendElement();
  return true;
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
WebRenderBridgeChild::DPEnd(wr::DisplayListBuilder &aBuilder,
                            const gfx::IntSize& aSize,
                            bool aIsSync,
                            uint64_t aTransactionId,
                            const WebRenderScrollData& aScrollData)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(mIsInTransaction);

  wr::BuiltDisplayList dl;
  wr::LayoutSize contentSize;
  aBuilder.Finalize(contentSize, dl);
  ByteBuffer dlData(Move(dl.dl));

  TimeStamp fwdTime;
#if defined(ENABLE_FRAME_LATENCY_LOG)
  fwdTime = TimeStamp::Now();
#endif

  if (aIsSync) {
    this->SendDPSyncEnd(aSize, mParentCommands, mDestroyedActors, GetFwdTransactionId(), aTransactionId,
                        contentSize, dlData, dl.dl_desc, aScrollData, mIdNamespace,fwdTime);
  } else {
    this->SendDPEnd(aSize, mParentCommands, mDestroyedActors, GetFwdTransactionId(), aTransactionId,
                    contentSize, dlData, dl.dl_desc, aScrollData, mIdNamespace, fwdTime);
  }

  mParentCommands.Clear();
  mDestroyedActors.Clear();
  mIsInTransaction = false;
}

void
WebRenderBridgeChild::ProcessWebRenderParentCommands()
{
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
  SendAddPipelineIdForAsyncCompositable(aPipelineId, aHandle);
}

void
WebRenderBridgeChild::RemovePipelineIdForAsyncCompositable(const wr::PipelineId& aPipelineId)
{
  SendRemovePipelineIdForAsyncCompositable(aPipelineId);
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
WebRenderBridgeChild::DeallocExternalImageId(wr::ExternalImageId& aImageId)
{
  if (mDestroyed) {
    // This can happen if the IPC connection was torn down, because, e.g.
    // the GPU process died.
    return;
  }
  SendRemoveExternalImageId(aImageId);
}

struct FontFileData
{
  wr::ByteBuffer mFontBuffer;
  uint32_t mFontIndex;
};

static void
WriteFontFileData(const uint8_t* aData, uint32_t aLength, uint32_t aIndex,
                  void* aBaton)
{
  FontFileData* data = static_cast<FontFileData*>(aBaton);

  if (!data->mFontBuffer.Allocate(aLength)) {
    return;
  }
  memcpy(data->mFontBuffer.mData, aData, aLength);

  data->mFontIndex = aIndex;
}

void
WebRenderBridgeChild::PushGlyphs(wr::DisplayListBuilder& aBuilder, const nsTArray<GlyphArray>& aGlyphs,
                                 gfx::ScaledFont* aFont, const StackingContextHelper& aSc,
                                 const LayerRect& aBounds, const LayerRect& aClip)
{
  MOZ_ASSERT(aFont);
  MOZ_ASSERT(!aGlyphs.IsEmpty());

  wr::WrFontKey key = GetFontKeyForScaledFont(aFont);
  MOZ_ASSERT(key.mNamespace.mHandle && key.mHandle);

  for (size_t i = 0; i < aGlyphs.Length(); i++) {
    GlyphArray glyph_array = aGlyphs[i];
    nsTArray<gfx::Glyph>& glyphs = glyph_array.glyphs();

    nsTArray<wr::GlyphInstance> wr_glyph_instances;
    wr_glyph_instances.SetLength(glyphs.Length());

    for (size_t j = 0; j < glyphs.Length(); j++) {
      wr_glyph_instances[j].index = glyphs[j].mIndex;
      wr_glyph_instances[j].point = aSc.ToRelativeLayoutPoint(
              LayerPoint::FromUnknownPoint(glyphs[j].mPosition));
    }

    aBuilder.PushText(aSc.ToRelativeLayoutRect(aBounds),
                      aSc.ToRelativeLayoutRect(aClip),
                      glyph_array.color().value(),
                      key,
                      Range<const wr::GlyphInstance>(wr_glyph_instances.Elements(), wr_glyph_instances.Length()),
                      aFont->GetSize());

  }
}

wr::FontKey
WebRenderBridgeChild::GetFontKeyForScaledFont(gfx::ScaledFont* aScaledFont)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(aScaledFont);
  MOZ_ASSERT((aScaledFont->GetType() == gfx::FontType::DWRITE) ||
             (aScaledFont->GetType() == gfx::FontType::MAC) ||
             (aScaledFont->GetType() == gfx::FontType::FONTCONFIG));

  RefPtr<gfx::UnscaledFont> unscaled = aScaledFont->GetUnscaledFont();
  MOZ_ASSERT(unscaled);

  wr::FontKey key = { wr::IdNamespace { 0 }, 0};
  if (mFontKeys.Get(unscaled, &key)) {
    return key;
  }

  FontFileData data;
  if (!unscaled->GetFontFileData(WriteFontFileData, &data) ||
      !data.mFontBuffer.mData) {
    return key;
  }

  key.mNamespace.mHandle = GetNamespace();
  key.mHandle = GetNextResourceId();

  SendAddRawFont(key, data.mFontBuffer, data.mFontIndex);

  mFontKeys.Put(unscaled, key);

  return key;
}

void
WebRenderBridgeChild::RemoveExpiredFontKeys()
{
  uint32_t counter = gfx::UnscaledFont::DeletionCounter();
  if (mFontKeysDeleted != counter) {
    mFontKeysDeleted = counter;
    for (auto iter = mFontKeys.Iter(); !iter.Done(); iter.Next()) {
      if (!iter.Key()) {
        SendDeleteFont(iter.Data());
        iter.Remove();
      }
    }
  }
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
WebRenderBridgeChild::Connect(CompositableClient* aCompositable,
                              ImageContainer* aImageContainer)
{
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
WebRenderBridgeChild::RecvWrUpdated(const uint32_t& aNewIdNameSpace)
{
  // Update mIdNamespace to identify obsolete keys and messages by WebRenderBridgeParent.
  // Since usage of invalid keys could cause crash in webrender.
  mIdNamespace = aNewIdNameSpace;
  // Remove all FontKeys since they are removed by WebRenderBridgeParent
  for (auto iter = mFontKeys.Iter(); !iter.Done(); iter.Next()) {
    SendDeleteFont(iter.Data());
  }
  mFontKeys.Clear();
  GetCompositorBridgeChild()->RecvInvalidateLayers(wr::AsUint64(mPipelineId));
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
  ProcessWebRenderParentCommands();
  SendClearCachedResources();
  mIsInClearCachedResources = false;
}

} // namespace layers
} // namespace mozilla
