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
#include "LayerManagerComposite.h"
#include "CompositorParent.h"

namespace mozilla {
namespace layers {

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

      compositable->EnsureTextureHost(op.textureId(), op.descriptor(),
                                      compositableParent->GetCompositableManager(),
                                      op.textureInfo());

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
      ShadowLayer* shadowLayer = layer ? layer->AsShadowLayer() : nullptr;
      if (shadowLayer) {
        Compositor* compositor = static_cast<LayerManagerComposite*>(layer->Manager())->GetCompositor();
        compositable->SetCompositor(compositor);
        compositable->SetLayer(layer);
      } else {
        // if we reach this branch, it most likely means that async textures
        // are coming in before we had time to attach the conmpositable to a
        // layer. Don't panic, it is okay in this case. it should not be
        // happening continuously, though.
      }

      if (layer) {
        RenderTraceInvalidateStart(layer, "FF00FF", layer->GetVisibleRegion().GetBounds());
      }

      const SurfaceDescriptor& descriptor = op.image();
      compositable->EnsureTextureHost(op.textureId(),
                                      descriptor,
                                      compositableParent->GetCompositableManager(),
                                      TextureInfo());
      MOZ_ASSERT(compositable->GetTextureHost());

      SurfaceDescriptor newBack;
      bool shouldRecomposite = compositable->Update(descriptor, &newBack);
      if (IsSurfaceDescriptorValid(newBack)) {
        replyv.push_back(OpTextureSwap(compositableParent, nullptr, op.textureId(), newBack));
      }

      if (shouldRecomposite && compositableParent->GetCompositorID()) {
        CompositorParent* cp
          = CompositorParent::GetCompositor(compositableParent->GetCompositorID());
        if (cp) {
          cp->ScheduleComposition();
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
      ShadowThebesLayer* thebes =
        static_cast<ShadowThebesLayer*>(compositable->GetLayer());

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
    case CompositableOperation::TOpUpdatePictureRect: {
      const OpUpdatePictureRect& op = aEdit.get_OpUpdatePictureRect();
      CompositableHost* compositable
       = static_cast<CompositableParent*>(op.compositableParent())->GetCompositableHost();
      MOZ_ASSERT(compositable);
      compositable->SetPictureRect(op.picture());
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

