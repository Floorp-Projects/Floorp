/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositableTransactionParent.h"
#include "ShadowLayers.h"
#include "RenderTrace.h"
#include "ShadowLayersManager.h"
#include "CompositableHost.h"
#include "mozilla/layers/ContentHost.h"
#include "ShadowLayerParent.h"
#include "TiledLayerBuffer.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/ThebesLayerComposite.h"
#include "mozilla/layers/TextureHost.h"
#include "CompositorParent.h"

namespace mozilla {
namespace layers {

template<typename T>
CompositableHost* AsCompositable(const T& op)
{
  return static_cast<CompositableParent*>(op.compositableParent())->GetCompositableHost();
}

template<typename T>
bool ScheduleComposition(const T& op)
{
  CompositableParent* comp = static_cast<CompositableParent*>(op.compositableParent());
  if (!comp || !comp->GetCompositorID()) {
    return false;
  }
  CompositorParent* cp
    = CompositorParent::GetCompositor(comp->GetCompositorID());
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
    case CompositableOperation::TOpCreatedTexture: {
      MOZ_LAYERS_LOG(("[ParentSide] Created texture"));
      const OpCreatedTexture& op = aEdit.get_OpCreatedTexture();
      CompositableParent* compositableParent =
        static_cast<CompositableParent*>(op.compositableParent());
      CompositableHost* compositable = compositableParent->GetCompositableHost();

      compositable->EnsureDeprecatedTextureHost(op.textureId(), op.descriptor(),
                                      compositableParent->GetCompositableManager(),
                                      op.textureInfo());

      break;
    }
    case CompositableOperation::TOpCreatedIncrementalTexture: {
      MOZ_LAYERS_LOG(("[ParentSide] Created texture"));
      const OpCreatedIncrementalTexture& op = aEdit.get_OpCreatedIncrementalTexture();

      CompositableParent* compositableParent =
        static_cast<CompositableParent*>(op.compositableParent());
      CompositableHost* compositable = compositableParent->GetCompositableHost();

      compositable->EnsureDeprecatedTextureHostIncremental(compositableParent->GetCompositableManager(),
                                                 op.textureInfo(),
                                                 op.bufferRect());
      break;
    }
    case CompositableOperation::TOpDestroyThebesBuffer: {
      MOZ_LAYERS_LOG(("[ParentSide] Created double buffer"));
      const OpDestroyThebesBuffer& op = aEdit.get_OpDestroyThebesBuffer();
      CompositableParent* compositableParent = static_cast<CompositableParent*>(op.compositableParent());
      ContentHostBase* content = static_cast<ContentHostBase*>(compositableParent->GetCompositableHost());
      content->DestroyTextures();

      break;
    }
    case CompositableOperation::TOpPaintTexture: {
      MOZ_LAYERS_LOG(("[ParentSide] Paint Texture X"));
      const OpPaintTexture& op = aEdit.get_OpPaintTexture();

      CompositableParent* compositableParent =
        static_cast<CompositableParent*>(op.compositableParent());
      CompositableHost* compositable =
        compositableParent->GetCompositableHost();

      Layer* layer = compositable ? compositable->GetLayer() : nullptr;
      LayerComposite* shadowLayer = layer ? layer->AsLayerComposite() : nullptr;
      if (shadowLayer) {
        Compositor* compositor = static_cast<LayerManagerComposite*>(layer->Manager())->GetCompositor();
        compositable->SetCompositor(compositor);
        compositable->SetLayer(layer);
      } else {
        // if we reach this branch, it most likely means that async textures
        // are coming in before we had time to attach the compositable to a
        // layer. Don't panic, it is okay in this case. it should not be
        // happening continuously, though.
      }

      if (layer) {
        RenderTraceInvalidateStart(layer, "FF00FF", layer->GetVisibleRegion().GetBounds());
      }

      if (compositable) {
        const SurfaceDescriptor& descriptor = op.image();
        compositable->EnsureDeprecatedTextureHost(op.textureId(),
                                        descriptor,
                                        compositableParent->GetCompositableManager(),
                                        TextureInfo());
        MOZ_ASSERT(compositable->GetDeprecatedTextureHost());

        SurfaceDescriptor newBack;
        bool shouldRecomposite = compositable->Update(descriptor, &newBack);
        if (IsSurfaceDescriptorValid(newBack)) {
          replyv.push_back(OpTextureSwap(compositableParent, nullptr,
                                         op.textureId(), newBack));
        }

        if (shouldRecomposite) {
          ScheduleComposition(op);
        }
      }

      if (layer) {
        RenderTraceInvalidateEnd(layer, "FF00FF");
      }

      break;
    }
    case CompositableOperation::TOpPaintTextureRegion: {
      MOZ_LAYERS_LOG(("[ParentSide] Paint ThebesLayer"));

      const OpPaintTextureRegion& op = aEdit.get_OpPaintTextureRegion();
      CompositableParent* compositableParent = static_cast<CompositableParent*>(op.compositableParent());
      CompositableHost* compositable =
        compositableParent->GetCompositableHost();
      ThebesLayerComposite* thebes =
        static_cast<ThebesLayerComposite*>(compositable->GetLayer());

      const ThebesBufferData& bufferData = op.bufferData();

      RenderTraceInvalidateStart(thebes, "FF00FF", op.updatedRegion().GetBounds());

      nsIntRegion frontUpdatedRegion;
      compositable->UpdateThebes(bufferData,
                                 op.updatedRegion(),
                                 thebes->GetValidRegion(),
                                 &frontUpdatedRegion);
      replyv.push_back(
        OpContentBufferSwap(compositableParent, nullptr, frontUpdatedRegion));

      RenderTraceInvalidateEnd(thebes, "FF00FF");
      break;
    }
    case CompositableOperation::TOpPaintTextureIncremental: {
      MOZ_LAYERS_LOG(("[ParentSide] Paint ThebesLayer"));

      const OpPaintTextureIncremental& op = aEdit.get_OpPaintTextureIncremental();

      CompositableParent* compositableParent = static_cast<CompositableParent*>(op.compositableParent());
      CompositableHost* compositable =
        compositableParent->GetCompositableHost();

      SurfaceDescriptor desc = op.image();

      compositable->UpdateIncremental(op.textureId(),
                                      desc,
                                      op.updatedRegion(),
                                      op.bufferRect(),
                                      op.bufferRotation());
      break;
    }
    case CompositableOperation::TOpUpdatePictureRect: {
      const OpUpdatePictureRect& op = aEdit.get_OpUpdatePictureRect();
      CompositableHost* compositable
       = static_cast<CompositableParent*>(op.compositableParent())->GetCompositableHost();
      MOZ_ASSERT(compositable);
      compositable->SetPictureRect(op.picture());
      break;
    }
    case CompositableOperation::TOpPaintTiledLayerBuffer: {
      MOZ_LAYERS_LOG(("[ParentSide] Paint TiledLayerBuffer"));
      const OpPaintTiledLayerBuffer& op = aEdit.get_OpPaintTiledLayerBuffer();
      CompositableParent* compositableParent = static_cast<CompositableParent*>(op.compositableParent());
      CompositableHost* compositable =
        compositableParent->GetCompositableHost();

      TiledLayerComposer* tileComposer = compositable->AsTiledLayerComposer();
      NS_ASSERTION(tileComposer, "compositable is not a tile composer");

      const SurfaceDescriptorTiles& tileDesc = op.tileLayerDescriptor();
      tileComposer->PaintedTiledLayerBuffer(this, tileDesc);
      break;
    }
    case CompositableOperation::TOpUseTexture: {
      const OpUseTexture& op = aEdit.get_OpUseTexture();
      if (op.textureID() == 0) {
        NS_WARNING("Invalid texture ID");
        break;
      }
      CompositableHost* compositable = AsCompositable(op);
      RefPtr<TextureHost> tex = compositable->GetTextureHost(op.textureID());

      MOZ_ASSERT(tex.get());
      compositable->UseTextureHost(tex);

      if (!ScheduleComposition(op)) {
        NS_WARNING("could not find a compositor to schedule composition");
      }
      break;
    }
    case CompositableOperation::TOpAddTexture: {
      const OpAddTexture& op = aEdit.get_OpAddTexture();
      if (op.textureID() == 0) {
        NS_WARNING("Invalid texture ID");
        break;
      }
      CompositableHost* compositable = AsCompositable(op);
      RefPtr<TextureHost> tex = TextureHost::Create(op.textureID(),
                                                    op.data(),
                                                    this,
                                                    op.textureFlags());
      MOZ_ASSERT(tex.get());
      tex->SetCompositor(compositable->GetCompositor());
      compositable->AddTextureHost(tex);
      MOZ_ASSERT(compositable->GetTextureHost(op.textureID()) == tex.get());
      break;
    }
    case CompositableOperation::TOpRemoveTexture: {
      const OpRemoveTexture& op = aEdit.get_OpRemoveTexture();
      if (op.textureID() == 0) {
        NS_WARNING("Invalid texture ID");
        break;
      }
      CompositableHost* compositable = AsCompositable(op);

      RefPtr<TextureHost> texture = compositable->GetTextureHost(op.textureID());
      MOZ_ASSERT(texture);

      TextureFlags flags = texture->GetFlags();

      if (flags & TEXTURE_DEALLOCATE_HOST) {
        texture->DeallocateSharedData();
      }

      compositable->RemoveTextureHost(op.textureID());

      // if it is not the host that deallocates the shared data, then we need
      // to notfy the client side to tell when it is safe to deallocate or
      // reuse it.
      if (!(flags & TEXTURE_DEALLOCATE_HOST)) {
        replyv.push_back(ReplyTextureRemoved(op.compositableParent(), nullptr,
                                             op.textureID()));
      }

      break;
    }
    case CompositableOperation::TOpUpdateTexture: {
      const OpUpdateTexture& op = aEdit.get_OpUpdateTexture();
      if (op.textureID() == 0) {
        NS_WARNING("Invalid texture ID");
        break;
      }
      CompositableHost* compositable = AsCompositable(op);
      MOZ_ASSERT(compositable);
      RefPtr<TextureHost> texture = compositable->GetTextureHost(op.textureID());
      MOZ_ASSERT(texture);

      texture->Updated(op.region().type() == MaybeRegion::TnsIntRegion
                       ? &op.region().get_nsIntRegion()
                       : nullptr); // no region means invalidate the entire surface


      compositable->UseTextureHost(texture);

      break;
    }

    default: {
      MOZ_ASSERT(false, "bad type");
    }
  }

  return true;
}

} // namespace
} // namespace

