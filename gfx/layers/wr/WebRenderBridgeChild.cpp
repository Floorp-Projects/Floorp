/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderBridgeChild.h"

#include "gfxPlatform.h"
#include "mozilla/layers/CompositableChild.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/PTextureChild.h"

namespace mozilla {
namespace layers {

WebRenderBridgeChild::WebRenderBridgeChild(const uint64_t& aPipelineId)
  : mIsInTransaction(false)
  , mSyncTransaction(false)
  , mIPCOpen(false)
  , mDestroyed(false)
{
}

void
WebRenderBridgeChild::Destroy()
{
  if (!IPCOpen()) {
    return;
  }
  // mDestroyed is used to prevent calling Send__delete__() twice.
  // When this function is called from CompositorBridgeChild::Destroy().
  mDestroyed = true;

  SendShutdown();
}

void
WebRenderBridgeChild::ActorDestroy(ActorDestroyReason why)
{
  mDestroyed = true;
}

void
WebRenderBridgeChild::AddWebRenderCommand(const WebRenderCommand& aCmd)
{
  MOZ_ASSERT(mIsInTransaction);
  mCommands.AppendElement(aCmd);
}

bool
WebRenderBridgeChild::DPBegin(uint32_t aWidth, uint32_t aHeight)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(!mIsInTransaction);
  bool success = false;
  UpdateFwdTransactionId();
  this->SendDPBegin(aWidth, aHeight, &success);
  if (!success) {
    return false;
  }

  mIsInTransaction = true;
  return true;
}

void
WebRenderBridgeChild::DPEnd(bool aIsSync, uint64_t aTransactionId)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(mIsInTransaction);
  if (aIsSync) {
    this->SendDPSyncEnd(mCommands, mDestroyedActors, GetFwdTransactionId(), aTransactionId);
  } else {
    this->SendDPEnd(mCommands, mDestroyedActors, GetFwdTransactionId(), aTransactionId);
  }

  mCommands.Clear();
  mDestroyedActors.Clear();
  mIsInTransaction = false;
  mSyncTransaction = false;
}

uint64_t
WebRenderBridgeChild::GetNextExternalImageId()
{
  static uint32_t sNextID = 1;
  ++sNextID;
  MOZ_RELEASE_ASSERT(sNextID != UINT32_MAX);

  // XXX replace external image id allocation with webrender's id allocation.
  // Use proc id as IdNamespace for now.
  uint32_t procId = static_cast<uint32_t>(base::GetCurrentProcId());
  uint64_t imageId = procId;
  imageId = imageId << 32 | sNextID;
  return imageId;
}

uint64_t
WebRenderBridgeChild::AllocExternalImageId(uint64_t aAsyncContainerID)
{
  MOZ_ASSERT(!mDestroyed);

  uint64_t imageId = GetNextExternalImageId();
  SendAddExternalImageId(imageId, aAsyncContainerID);
  return imageId;
}

uint64_t
WebRenderBridgeChild::AllocExternalImageIdForCompositable(CompositableClient* aCompositable)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(aCompositable->GetIPDLActor());

  if (!aCompositable->GetIPDLActor()) {
    return 0;
  }

  uint64_t imageId = GetNextExternalImageId();
  SendAddExternalImageIdForCompositable(imageId, aCompositable->GetIPDLActor());
  return imageId;
}

void
WebRenderBridgeChild::DeallocExternalImageId(uint64_t aImageId)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(aImageId);
  SendRemoveExternalImageId(aImageId);
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

PCompositableChild*
WebRenderBridgeChild::AllocPCompositableChild(const TextureInfo& aInfo)
{
  MOZ_ASSERT(!mDestroyed);
  return CompositableChild::CreateActor();
}

bool
WebRenderBridgeChild::DeallocPCompositableChild(PCompositableChild* aActor)
{
  CompositableChild::DestroyActor(aActor);
  return true;
}

void
WebRenderBridgeChild::Connect(CompositableClient* aCompositable,
                              ImageContainer* aImageContainer)
{
  MOZ_ASSERT(aCompositable);

  PCompositableChild* actor =
    SendPCompositableConstructor(aCompositable->GetTextureInfo());
  if (!actor) {
    return;
  }
  aCompositable->InitIPDLActor(actor);
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
WebRenderBridgeChild::AddOpDestroy(const OpDestroy& aOp, bool aSynchronously)
{
  if (!mIsInTransaction) {
    return false;
  }

  mDestroyedActors.AppendElement(aOp);
  if (aSynchronously) {
    MarkSyncTransaction();
  }
  return true;
}

bool
WebRenderBridgeChild::DestroyInTransaction(PTextureChild* aTexture, bool aSynchronously)
{
  return AddOpDestroy(OpDestroy(aTexture), aSynchronously);
}

bool
WebRenderBridgeChild::DestroyInTransaction(PCompositableChild* aCompositable, bool aSynchronously)
{
  return AddOpDestroy(OpDestroy(aCompositable), aSynchronously);
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

  AddWebRenderCommand(
    CompositableOperation(
      nullptr, aCompositable->GetIPDLActor(),
      OpRemoveTexture(nullptr, aTexture->GetIPDLActor())));
  if (aTexture->GetFlags() & TextureFlags::DEALLOCATE_CLIENT) {
    MarkSyncTransaction();
  }
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
    t.mTextureClient->SerializeReadLock(readLock);
    textures.AppendElement(TimedTexture(nullptr, t.mTextureClient->GetIPDLActor(),
                                        readLock,
                                        t.mTimeStamp, t.mPictureRect,
                                        t.mFrameID, t.mProducerID));
    if ((t.mTextureClient->GetFlags() & TextureFlags::IMMEDIATE_UPLOAD)
        && t.mTextureClient->HasIntermediateBuffer()) {

      // We use IMMEDIATE_UPLOAD when we want to be sure that the upload cannot
      // race with updates on the main thread. In this case we want the transaction
      // to be synchronous.

      MarkSyncTransaction();
    }
    GetCompositorBridgeChild()->HoldUntilCompositableRefReleasedIfNecessary(t.mTextureClient);
  }
  AddWebRenderCommand(CompositableOperation(nullptr, aCompositable->GetIPDLActor(),
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

} // namespace layers
} // namespace mozilla
