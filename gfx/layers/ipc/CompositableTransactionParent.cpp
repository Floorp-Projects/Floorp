/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositableTransactionParent.h"
#include "CompositableHost.h"           // for CompositableParent, etc
#include "CompositorBridgeParent.h"     // for CompositorBridgeParent
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
static bool
ScheduleComposition(CompositableHost* aCompositable)
{
  uint64_t id = aCompositable->GetCompositorID();
  if (!id) {
    return false;
  }
  CompositorBridgeParent* cp = CompositorBridgeParent::GetCompositor(id);
  if (!cp) {
    return false;
  }
  cp->ScheduleComposition();
  return true;
}

#if defined(DEBUG) || defined(MOZ_WIDGET_GONK)
static bool ValidatePictureRect(const mozilla::gfx::IntSize& aSize,
                                const nsIntRect& aPictureRect)
{
  return nsIntRect(0, 0, aSize.width, aSize.height).Contains(aPictureRect) &&
      !aPictureRect.IsEmpty();
}
#endif

bool
CompositableParentManager::ReceiveCompositableUpdate(const CompositableOperation& aEdit,
                                                     EditReplyVector& replyv)
{
  // Ignore all operations on compositables created on stale compositors. We
  // return true because the child is unable to handle errors.
  CompositableHost* compositable = CompositableHost::FromIPDLActor(aEdit.compositableParent());
  if (compositable->GetCompositor() && compositable->GetCompositor()->IsValid()) {
    return true;
  }

  switch (aEdit.detail().type()) {
    case CompositableOperationDetail::TOpPaintTextureRegion: {
      MOZ_LAYERS_LOG(("[ParentSide] Paint PaintedLayer"));

      const OpPaintTextureRegion& op = aEdit.detail().get_OpPaintTextureRegion();
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
        OpContentBufferSwap(aEdit.compositableParent(), nullptr, frontUpdatedRegion));

      RenderTraceInvalidateEnd(thebes, "FF00FF");
      break;
    }
    case CompositableOperationDetail::TOpUseTiledLayerBuffer: {
      MOZ_LAYERS_LOG(("[ParentSide] Paint TiledLayerBuffer"));
      const OpUseTiledLayerBuffer& op = aEdit.detail().get_OpUseTiledLayerBuffer();
      TiledContentHost* tiledHost = compositable->AsTiledContentHost();

      NS_ASSERTION(tiledHost, "The compositable is not tiled");

      const SurfaceDescriptorTiles& tileDesc = op.tileLayerDescriptor();

      bool success = tiledHost->UseTiledLayerBuffer(this, tileDesc);

      const InfallibleTArray<TileDescriptor>& tileDescriptors = tileDesc.tiles();
      for (size_t i = 0; i < tileDescriptors.Length(); i++) {
        const TileDescriptor& tileDesc = tileDescriptors[i];
        if (tileDesc.type() != TileDescriptor::TTexturedTileDescriptor) {
          continue;
        }
        const TexturedTileDescriptor& texturedDesc = tileDesc.get_TexturedTileDescriptor();
        RefPtr<TextureHost> texture = TextureHost::AsTextureHost(texturedDesc.textureParent());
        if (texture) {
          texture->SetLastFwdTransactionId(mFwdTransactionId);
          // Make sure that each texture was handled by the compositable
          // because the recycling logic depends on it.
          MOZ_ASSERT(texture->NumCompositableRefs() > 0);
        }
        if (texturedDesc.textureOnWhite().type() == MaybeTexture::TPTextureParent) {
          texture = TextureHost::AsTextureHost(texturedDesc.textureOnWhite().get_PTextureParent());
          if (texture) {
            texture->SetLastFwdTransactionId(mFwdTransactionId);
            // Make sure that each texture was handled by the compositable
            // because the recycling logic depends on it.
            MOZ_ASSERT(texture->NumCompositableRefs() > 0);
          }
        }
      }
      if (!success) {
        return false;
      }
      break;
    }
    case CompositableOperationDetail::TOpRemoveTexture: {
      const OpRemoveTexture& op = aEdit.detail().get_OpRemoveTexture();

      RefPtr<TextureHost> tex = TextureHost::AsTextureHost(op.textureParent());

      MOZ_ASSERT(tex.get());
      compositable->RemoveTextureHost(tex);
      break;
    }
    case CompositableOperationDetail::TOpRemoveTextureAsync: {
      const OpRemoveTextureAsync& op = aEdit.detail().get_OpRemoveTextureAsync();
      RefPtr<TextureHost> tex = TextureHost::AsTextureHost(op.textureParent());

      MOZ_ASSERT(tex.get());
      compositable->RemoveTextureHost(tex);

      // Only ImageBridge child sends it.
      MOZ_ASSERT(UsesImageBridge());
      if (UsesImageBridge()) {
        ReplyRemoveTexture(OpReplyRemoveTexture(op.holderId(),
                                                op.transactionId()));
      }
      break;
    }
    case CompositableOperationDetail::TOpUseTexture: {
      const OpUseTexture& op = aEdit.detail().get_OpUseTexture();

      AutoTArray<CompositableHost::TimedTexture,4> textures;
      for (auto& timedTexture : op.textures()) {
        CompositableHost::TimedTexture* t = textures.AppendElement();
        t->mTexture =
            TextureHost::AsTextureHost(timedTexture.textureParent());
        MOZ_ASSERT(t->mTexture);
        t->mTimeStamp = timedTexture.timeStamp();
        t->mPictureRect = timedTexture.picture();
        t->mFrameID = timedTexture.frameID();
        t->mProducerID = timedTexture.producerID();
        t->mInputFrameID = timedTexture.inputFrameID();
        t->mTexture->DeserializeReadLock(timedTexture.sharedLock(), this);
        MOZ_ASSERT(ValidatePictureRect(t->mTexture->GetSize(), t->mPictureRect));

        MaybeFence maybeFence = timedTexture.fence();
        if (maybeFence.type() == MaybeFence::TFenceHandle) {
          FenceHandle fence = maybeFence.get_FenceHandle();
          if (fence.IsValid()) {
            t->mTexture->SetAcquireFenceHandle(fence);
          }
        }
      }
      if (textures.Length() > 0) {
        compositable->UseTextureHost(textures);

        for (auto& timedTexture : op.textures()) {
          RefPtr<TextureHost> texture = TextureHost::AsTextureHost(timedTexture.textureParent());
          if (texture) {
            texture->SetLastFwdTransactionId(mFwdTransactionId);
            // Make sure that each texture was handled by the compositable
            // because the recycling logic depends on it.
            MOZ_ASSERT(texture->NumCompositableRefs() > 0);
          }
        }
      }

      if (UsesImageBridge() && compositable->GetLayer()) {
        ScheduleComposition(compositable);
      }
      break;
    }
    case CompositableOperationDetail::TOpUseComponentAlphaTextures: {
      const OpUseComponentAlphaTextures& op = aEdit.detail().get_OpUseComponentAlphaTextures();
      RefPtr<TextureHost> texOnBlack = TextureHost::AsTextureHost(op.textureOnBlackParent());
      RefPtr<TextureHost> texOnWhite = TextureHost::AsTextureHost(op.textureOnWhiteParent());
      texOnBlack->DeserializeReadLock(op.sharedLockBlack(), this);
      texOnWhite->DeserializeReadLock(op.sharedLockWhite(), this);

      MOZ_ASSERT(texOnBlack && texOnWhite);
      compositable->UseComponentAlphaTextures(texOnBlack, texOnWhite);

      if (texOnBlack) {
        texOnBlack->SetLastFwdTransactionId(mFwdTransactionId);
        // Make sure that each texture was handled by the compositable
        // because the recycling logic depends on it.
        MOZ_ASSERT(texOnBlack->NumCompositableRefs() > 0);
      }

      if (texOnWhite) {
        texOnWhite->SetLastFwdTransactionId(mFwdTransactionId);
        // Make sure that each texture was handled by the compositable
        // because the recycling logic depends on it.
        MOZ_ASSERT(texOnWhite->NumCompositableRefs() > 0);
      }

      if (UsesImageBridge()) {
        ScheduleComposition(compositable);
      }
      break;
    }
#ifdef MOZ_WIDGET_GONK
    case CompositableOperationDetail::TOpUseOverlaySource: {
      const OpUseOverlaySource& op = aEdit.detail().get_OpUseOverlaySource();
      if (!ValidatePictureRect(op.overlay().size(), op.picture())) {
        return false;
      }
      compositable->UseOverlaySource(op.overlay(), op.picture());
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
CompositableParentManager::DestroyActor(const OpDestroy& aOp)
{
  switch (aOp.type()) {
    case OpDestroy::TPTextureParent: {
      auto actor = aOp.get_PTextureParent();
      TextureHost::ReceivedDestroy(actor);
      break;
    }
    case OpDestroy::TPCompositableParent: {
      auto actor = aOp.get_PCompositableParent();
      CompositableHost::ReceivedDestroy(actor);
      break;
    }
    default: {
      MOZ_ASSERT(false, "unsupported type");
    }
  }
}

} // namespace layers
} // namespace mozilla

