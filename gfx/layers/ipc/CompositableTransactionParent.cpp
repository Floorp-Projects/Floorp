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
#include "mozilla/layers/TextureParent.h"
#include "LayerManagerComposite.h"
#include "CompositorParent.h"

namespace mozilla {
namespace layers {

//--------------------------------------------------
// Convenience accessors
template<class OpPaintT>
Layer* GetLayerFromOpPaint(const OpPaintT& op)
{
  PTextureParent* textureParent = op.textureParent();
  CompositableHost* compoHost
    = static_cast<CompositableParent*>(textureParent->Manager())->GetCompositableHost();
  return compoHost ? compoHost->GetLayer() : nullptr;
}

bool
CompositableParentManager::ReceiveCompositableUpdate(const CompositableOperation& aEdit,
                                                     EditReplyVector& replyv)
{
  switch (aEdit.type()) {
    case CompositableOperation::TOpCreatedSingleBuffer: {
      MOZ_LAYERS_LOG(("[ParentSide] Created single buffer"));
      const OpCreatedSingleBuffer& op = aEdit.get_OpCreatedSingleBuffer();
      CompositableParent* compositableParent = static_cast<CompositableParent*>(op.compositableParent());
      TextureParent* textureParent = static_cast<TextureParent*>(op.bufferParent());

      textureParent->EnsureTextureHost(op.descriptor().type());
      textureParent->GetTextureHost()->SetBuffer(new SurfaceDescriptor(op.descriptor()),
                                                 compositableParent->GetCompositableManager());

      ContentHostBase* content = static_cast<ContentHostBase*>(compositableParent->GetCompositableHost());
      content->SetTextureHosts(textureParent->GetTextureHost());

      break;
    }
    case CompositableOperation::TOpCreatedDoubleBuffer: {
      MOZ_LAYERS_LOG(("[ParentSide] Created double buffer"));
      const OpCreatedDoubleBuffer& op = aEdit.get_OpCreatedDoubleBuffer();
      CompositableParent* compositableParent = static_cast<CompositableParent*>(op.compositableParent());
      TextureParent* frontParent = static_cast<TextureParent*>(op.frontParent());
      TextureParent* backParent = static_cast<TextureParent*>(op.backParent());


      frontParent->EnsureTextureHost(op.frontDescriptor().type());
      backParent->EnsureTextureHost(op.backDescriptor().type());
      frontParent->GetTextureHost()->SetBuffer(new SurfaceDescriptor(op.frontDescriptor()),
                                               compositableParent->GetCompositableManager());
      backParent->GetTextureHost()->SetBuffer(new SurfaceDescriptor(op.backDescriptor()),
                                              compositableParent->GetCompositableManager());

      ContentHostBase* content = static_cast<ContentHostBase*>(compositableParent->GetCompositableHost());
      content->SetTextureHosts(frontParent->GetTextureHost(),
                               backParent->GetTextureHost());

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

      TextureParent* textureParent = static_cast<TextureParent*>(op.textureParent());
      CompositableHost* compositable = textureParent->GetCompositableHost();
      Layer* layer = GetLayerFromOpPaint(op);
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
      textureParent->EnsureTextureHost(descriptor.type());
      MOZ_ASSERT(textureParent->GetTextureHost());

      SurfaceDescriptor newBack;
      bool shouldRecomposite = compositable->Update(op.image(), &newBack);
      if (IsSurfaceDescriptorValid(newBack)) {
        replyv.push_back(OpTextureSwap(op.textureParent(), nullptr, newBack));
      }

      if (shouldRecomposite && textureParent->GetCompositorID()) {
        CompositorParent* cp
          = CompositorParent::GetCompositor(textureParent->GetCompositorID());
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

