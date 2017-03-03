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
#include "mozilla/layers/PTextureChild.h"
#include "mozilla/webrender/WebRenderAPI.h"

namespace mozilla {
namespace layers {

WebRenderBridgeChild::WebRenderBridgeChild(const wr::PipelineId& aPipelineId)
  : mIsInTransaction(false)
  , mIdNamespace(0)
  , mResourceId(0)
  , mPipelineId(aPipelineId)
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

void
WebRenderBridgeChild::AddWebRenderCommands(const nsTArray<WebRenderCommand>& aCommands)
{
  MOZ_ASSERT(mIsInTransaction);
  mCommands.AppendElements(aCommands);
}

void
WebRenderBridgeChild::AddWebRenderParentCommand(const WebRenderParentCommand& aCmd)
{
  MOZ_ASSERT(mIsInTransaction);
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
  MOZ_ASSERT(!mIsInTransaction);

  UpdateFwdTransactionId();
  this->SendDPBegin(aSize);
  mIsInTransaction = true;
  return true;
}

wr::BuiltDisplayList
WebRenderBridgeChild::ProcessWebrenderCommands(const gfx::IntSize &aSize,
                                               InfallibleTArray<WebRenderCommand>& aCommands)
{
  wr::DisplayListBuilder builder(mPipelineId);
  builder.Begin(ViewAs<LayerPixel>(aSize));

  for (InfallibleTArray<WebRenderCommand>::index_type i = 0; i < aCommands.Length(); ++i) {
    const WebRenderCommand& cmd = aCommands[i];

    switch (cmd.type()) {
      case WebRenderCommand::TOpDPPushStackingContext: {
        const OpDPPushStackingContext& op = cmd.get_OpDPPushStackingContext();
        builder.PushStackingContext(op.bounds(), op.overflow(), op.mask().ptrOr(nullptr), op.opacity(), op.matrix(), op.mixBlendMode());
        break;
      }
      case WebRenderCommand::TOpDPPopStackingContext: {
        builder.PopStackingContext();
        break;
      }
      case WebRenderCommand::TOpDPPushScrollLayer: {
        const OpDPPushScrollLayer& op = cmd.get_OpDPPushScrollLayer();
        builder.PushScrollLayer(op.bounds(), op.overflow(), op.mask().ptrOr(nullptr));
        break;
      }
      case WebRenderCommand::TOpDPPopScrollLayer: {
        builder.PopScrollLayer();
        break;
      }
      case WebRenderCommand::TOpDPPushRect: {
        const OpDPPushRect& op = cmd.get_OpDPPushRect();
        builder.PushRect(op.bounds(), op.clip(), op.color());
        break;
      }
      case WebRenderCommand::TOpDPPushBorder: {
        const OpDPPushBorder& op = cmd.get_OpDPPushBorder();
        builder.PushBorder(op.bounds(), op.clip(),
                           op.top(), op.right(), op.bottom(), op.left(),
                           op.radius());
        break;
      }
      case WebRenderCommand::TOpDPPushLinearGradient: {
        const OpDPPushLinearGradient& op = cmd.get_OpDPPushLinearGradient();
        builder.PushLinearGradient(op.bounds(), op.clip(),
                                   op.startPoint(), op.endPoint(),
                                   op.stops(), op.extendMode());
        break;
      }
      case WebRenderCommand::TOpDPPushRadialGradient: {
        const OpDPPushRadialGradient& op = cmd.get_OpDPPushRadialGradient();
        builder.PushRadialGradient(op.bounds(), op.clip(),
                                   op.startCenter(), op.endCenter(),
                                   op.startRadius(), op.endRadius(),
                                   op.stops(), op.extendMode());
        break;
      }
      case WebRenderCommand::TOpDPPushImage: {
        const OpDPPushImage& op = cmd.get_OpDPPushImage();
        builder.PushImage(op.bounds(), op.clip(),
                          op.mask().ptrOr(nullptr), op.filter(), wr::ImageKey(op.key()));
        break;
      }
      case WebRenderCommand::TOpDPPushIframe: {
        const OpDPPushIframe& op = cmd.get_OpDPPushIframe();
        builder.PushIFrame(op.bounds(), op.clip(), op.pipelineId());
        break;
      }
      case WebRenderCommand::TOpDPPushText: {
        const OpDPPushText& op = cmd.get_OpDPPushText();
        const nsTArray<WrGlyphArray>& glyph_array = op.glyph_array();

        for (size_t i = 0; i < glyph_array.Length(); i++) {
          const nsTArray<WrGlyphInstance>& glyphs = glyph_array[i].glyphs;
          builder.PushText(op.bounds(),
                           op.clip(),
                           glyph_array[i].color,
                           op.key(),
                           Range<const WrGlyphInstance>(glyphs.Elements(), glyphs.Length()),
                           op.glyph_size());
        }

        break;
      }
      case WebRenderCommand::TOpDPPushBoxShadow: {
        const OpDPPushBoxShadow& op = cmd.get_OpDPPushBoxShadow();
        builder.PushBoxShadow(op.rect(),
                              op.clip(),
                              op.box_bounds(),
                              op.offset(),
                              op.color(),
                              op.blur_radius(),
                              op.spread_radius(),
                              op.border_radius(),
                              op.clip_mode());
        break;
      }
      default:
        NS_RUNTIMEABORT("not reached");
    }
  }
  builder.End();
  wr::BuiltDisplayList dl;
  builder.Finalize(dl.dl_desc, dl.dl, dl.aux_desc, dl.aux);
  return dl;
}

void
WebRenderBridgeChild::DPEnd(const gfx::IntSize& aSize, bool aIsSync, uint64_t aTransactionId)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(mIsInTransaction);

  wr::BuiltDisplayList dl = ProcessWebrenderCommands(aSize, mCommands);
  ByteBuffer dlData(Move(dl.dl));
  ByteBuffer auxData(Move(dl.aux));

  if (aIsSync) {
    this->SendDPSyncEnd(aSize, mParentCommands, mDestroyedActors, GetFwdTransactionId(), aTransactionId,
                        dlData, dl.dl_desc, auxData, dl.aux_desc);
  } else {
    this->SendDPEnd(aSize, mParentCommands, mDestroyedActors, GetFwdTransactionId(), aTransactionId,
                    dlData, dl.dl_desc, auxData, dl.aux_desc);
  }

  mCommands.Clear();
  mParentCommands.Clear();
  mDestroyedActors.Clear();
  mIsInTransaction = false;
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
WebRenderBridgeChild::AllocExternalImageId(const CompositableHandle& aHandle)
{
  MOZ_ASSERT(!mDestroyed);

  uint64_t imageId = GetNextExternalImageId();
  SendAddExternalImageId(imageId, aHandle);
  return imageId;
}

uint64_t
WebRenderBridgeChild::AllocExternalImageIdForCompositable(CompositableClient* aCompositable)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(aCompositable->IsConnected());

  uint64_t imageId = GetNextExternalImageId();
  SendAddExternalImageIdForCompositable(imageId, aCompositable->GetIPCHandle());
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
    t.mTextureClient->SerializeReadLock(readLock);
    textures.AppendElement(TimedTexture(nullptr, t.mTextureClient->GetIPDLActor(),
                                        readLock,
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

} // namespace layers
} // namespace mozilla
