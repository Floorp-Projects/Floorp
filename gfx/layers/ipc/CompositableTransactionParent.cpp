/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositableTransactionParent.h"
#include "CompositableHost.h"           // for CompositableParent, etc
#include "CompositorParent.h"           // for CompositorParent
#include "GLContext.h"                  // for GLContext
#include "Layers.h"                     // for Layer
#include "RenderTrace.h"                // for RenderTraceInvalidateEnd, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/ContentHost.h"  // for ContentHostBase
#include "mozilla/layers/ImageBridgeParent.h" // for ImageBridgeParent
#include "mozilla/layers/SharedBufferManagerParent.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/LayersTypes.h"  // for MOZ_LAYERS_LOG
#include "mozilla/layers/TextureHost.h"  // for TextureHost
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL
#include "mozilla/layers/TiledContentHost.h"
#include "mozilla/layers/PaintedLayerComposite.h"
#include "mozilla/mozalloc.h"           // for operator delete
#include "mozilla/unused.h"
#include "nsDebug.h"                    // for NS_WARNING, NS_ASSERTION
#include "nsRegion.h"                   // for nsIntRegion

namespace mozilla {
namespace layers {

class ClientTiledLayerBuffer;
class Compositor;

template<typename Op>
CompositableHost* AsCompositable(const Op& op)
{
  return CompositableHost::FromIPDLActor(op.compositableParent());
}

// This function can in some cases fail and return false without it being a bug.
// This can theoretically happen if the ImageBridge sends frames before
// we created the layer tree. Since we can't enforce that the layer
// tree is already created before ImageBridge operates, there isn't much
// we can do about it, but in practice it is very rare.
// Typically when a tab with a video is dragged from a window to another,
// there can be a short time when the video is still sending frames
// asynchonously while the layer tree is not reconstructed. It's not a
// big deal.
// Note that Layers transactions do not need to call this because they always
// schedule the composition, in LayerManagerComposite::EndTransaction.
template<typename T>
bool ScheduleComposition(const T& op)
{
  CompositableHost* comp = AsCompositable(op);
  uint64_t id = comp->GetCompositorID();
  if (!comp || !id) {
    return false;
  }
  CompositorParent* cp = CompositorParent::GetCompositor(id);
  if (!cp) {
    return false;
  }
  cp->ScheduleComposition();
  return true;
}

bool
CompositableParentManager::ReceiveCompositableUpdate(const CompositableOperation& aEdit,
                                                     EditReplyVector& replyv)
{
  switch (aEdit.type()) {
    case CompositableOperation::TOpPaintTextureRegion: {
      MOZ_LAYERS_LOG(("[ParentSide] Paint PaintedLayer"));

      const OpPaintTextureRegion& op = aEdit.get_OpPaintTextureRegion();
      CompositableHost* compositable = AsCompositable(op);
      Layer* layer = compositable->GetLayer();
      if (!layer || layer->GetType() != Layer::TYPE_PAINTED) {
        return false;
      }
      PaintedLayerComposite* thebes = static_cast<PaintedLayerComposite*>(layer);

      const ThebesBufferData& bufferData = op.bufferData();

      RenderTraceInvalidateStart(thebes, "FF00FF", op.updatedRegion().GetBounds());

      nsIntRegion frontUpdatedRegion;
      if (!compositable->UpdateThebes(bufferData,
                                      op.updatedRegion(),
                                      thebes->GetValidRegion(),
                                      &frontUpdatedRegion))
      {
        return false;
      }
      replyv.push_back(
        OpContentBufferSwap(op.compositableParent(), nullptr, frontUpdatedRegion));

      RenderTraceInvalidateEnd(thebes, "FF00FF");
      break;
    }
    case CompositableOperation::TOpUpdatePictureRect: {
      const OpUpdatePictureRect& op = aEdit.get_OpUpdatePictureRect();
      CompositableHost* compositable = AsCompositable(op);
      MOZ_ASSERT(compositable);
      compositable->SetPictureRect(op.picture());
      break;
    }
    case CompositableOperation::TOpUseTiledLayerBuffer: {
      MOZ_LAYERS_LOG(("[ParentSide] Paint TiledLayerBuffer"));
      const OpUseTiledLayerBuffer& op = aEdit.get_OpUseTiledLayerBuffer();
      TiledContentHost* compositable = AsCompositable(op)->AsTiledContentHost();

      NS_ASSERTION(compositable, "The compositable is not tiled");

      const SurfaceDescriptorTiles& tileDesc = op.tileLayerDescriptor();
      bool success = compositable->UseTiledLayerBuffer(this, tileDesc);
      if (!success) {
        return false;
      }
      break;
    }
    case CompositableOperation::TOpRemoveTexture: {
      const OpRemoveTexture& op = aEdit.get_OpRemoveTexture();
      CompositableHost* compositable = AsCompositable(op);
      RefPtr<TextureHost> tex = TextureHost::AsTextureHost(op.textureParent());

      MOZ_ASSERT(tex.get());
      compositable->RemoveTextureHost(tex);
      // send FenceHandle if present.
      SendFenceHandleIfPresent(op.textureParent(), compositable);
      break;
    }
    case CompositableOperation::TOpRemoveTextureAsync: {
      const OpRemoveTextureAsync& op = aEdit.get_OpRemoveTextureAsync();
      CompositableHost* compositable = AsCompositable(op);
      RefPtr<TextureHost> tex = TextureHost::AsTextureHost(op.textureParent());

      MOZ_ASSERT(tex.get());
      compositable->RemoveTextureHost(tex);

      if (!IsAsync() && ImageBridgeParent::GetInstance(GetChildProcessId())) {
        // send FenceHandle if present via ImageBridge.
        ImageBridgeParent::AppendDeliverFenceMessage(
                             GetChildProcessId(),
                             op.holderId(),
                             op.transactionId(),
                             op.textureParent(),
                             compositable);

        // If the message is recievied via PLayerTransaction,
        // Send message back via PImageBridge.
        ImageBridgeParent::ReplyRemoveTexture(
                             GetChildProcessId(),
                             OpReplyRemoveTexture(op.holderId(),
                                                  op.transactionId()));
      } else {
        // send FenceHandle if present.
        SendFenceHandleIfPresent(op.textureParent(), compositable);

        ReplyRemoveTexture(OpReplyRemoveTexture(op.holderId(),
                                                op.transactionId()));
      }
      break;
    }
    case CompositableOperation::TOpUseTexture: {
      const OpUseTexture& op = aEdit.get_OpUseTexture();
      CompositableHost* compositable = AsCompositable(op);
      RefPtr<TextureHost> tex = TextureHost::AsTextureHost(op.textureParent());

      MOZ_ASSERT(tex.get());
      compositable->UseTextureHost(tex);

      MaybeFence maybeFence = op.fence();
      if (maybeFence.type() == MaybeFence::TFenceHandle) {
        FenceHandle fence = maybeFence.get_FenceHandle();
        if (fence.IsValid() && tex) {
          tex->SetAcquireFenceHandle(fence);
        }
      }

      if (IsAsync() && compositable->GetLayer()) {
        ScheduleComposition(op);
        // Async layer updates don't trigger invalidation, manually tell the layer
        // that its content have changed.
        compositable->GetLayer()->SetInvalidRectToVisibleRegion();
      }
      break;
    }
    case CompositableOperation::TOpUseComponentAlphaTextures: {
      const OpUseComponentAlphaTextures& op = aEdit.get_OpUseComponentAlphaTextures();
      CompositableHost* compositable = AsCompositable(op);
      RefPtr<TextureHost> texOnBlack = TextureHost::AsTextureHost(op.textureOnBlackParent());
      RefPtr<TextureHost> texOnWhite = TextureHost::AsTextureHost(op.textureOnWhiteParent());

      MOZ_ASSERT(texOnBlack && texOnWhite);
      compositable->UseComponentAlphaTextures(texOnBlack, texOnWhite);

      if (IsAsync()) {
        ScheduleComposition(op);
      }
      break;
    }
#ifdef MOZ_WIDGET_GONK
    case CompositableOperation::TOpUseOverlaySource: {
      const OpUseOverlaySource& op = aEdit.get_OpUseOverlaySource();
      CompositableHost* compositable = AsCompositable(op);
      MOZ_ASSERT(compositable->GetType() == CompositableType::IMAGE_OVERLAY, "Invalid operation!");
      compositable->UseOverlaySource(op.overlay());
      break;
    }
#endif
    default: {
      MOZ_ASSERT(false, "bad type");
    }
  }

  return true;
}

void
CompositableParentManager::SendPendingAsyncMessages()
{
  if (mPendingAsyncMessage.empty()) {
    return;
  }

  // Some type of AsyncParentMessageData message could have
  // one file descriptor (e.g. OpDeliverFence).
  // A number of file descriptors per gecko ipc message have a limitation
  // on OS_POSIX (MACOSX or LINUX).
#if defined(OS_POSIX)
  static const uint32_t kMaxMessageNumber = FileDescriptorSet::MAX_DESCRIPTORS_PER_MESSAGE;
#else
  // default number that works everywhere else
  static const uint32_t kMaxMessageNumber = 250;
#endif

  InfallibleTArray<AsyncParentMessageData> messages;
  messages.SetCapacity(mPendingAsyncMessage.size());
  for (size_t i = 0; i < mPendingAsyncMessage.size(); i++) {
    messages.AppendElement(mPendingAsyncMessage[i]);
    // Limit maximum number of messages.
    if (messages.Length() >= kMaxMessageNumber) {
      SendAsyncMessage(messages);
      // Initialize Messages.
      messages.Clear();
    }
  }

  if (messages.Length() > 0) {
    SendAsyncMessage(messages);
  }
  mPendingAsyncMessage.clear();
}

} // namespace
} // namespace

