/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositableTransactionParent.h"
#include "CompositableHost.h"           // for CompositableParent, etc
#include "CompositorParent.h"           // for CompositorParent
#include "Layers.h"                     // for Layer
#include "RenderTrace.h"                // for RenderTraceInvalidateEnd, etc
#include "TiledLayerBuffer.h"           // for TiledLayerComposer
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/ContentHost.h"  // for ContentHostBase
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/LayersTypes.h"  // for MOZ_LAYERS_LOG
#include "mozilla/layers/TextureHost.h"  // for TextureHost
#include "mozilla/layers/ThebesLayerComposite.h"
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsDebug.h"                    // for NS_WARNING, NS_ASSERTION
#include "nsRegion.h"                   // for nsIntRegion

namespace mozilla {
namespace layers {

class BasicTiledLayerBuffer;
class Compositor;

template<typename T>
CompositableHost* AsCompositable(const T& op)
{
  return static_cast<CompositableParent*>(op.compositableParent())->GetCompositableHost();
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
      DeprecatedContentHostBase* content = static_cast<DeprecatedContentHostBase*>(compositableParent->GetCompositableHost());
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

        if (IsAsync() && shouldRecomposite) {
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
    case CompositableOperation::TOpRemoveTexture: {
      const OpRemoveTexture& op = aEdit.get_OpRemoveTexture();
      CompositableHost* compositable = AsCompositable(op);
      RefPtr<TextureHost> tex = TextureHost::AsTextureHost(op.textureParent());

      MOZ_ASSERT(tex.get());
      compositable->RemoveTextureHost(tex);
      break;
    }
    case CompositableOperation::TOpUseTexture: {
      const OpUseTexture& op = aEdit.get_OpUseTexture();
      CompositableHost* compositable = AsCompositable(op);
      RefPtr<TextureHost> tex = TextureHost::AsTextureHost(op.textureParent());

      MOZ_ASSERT(tex.get());
      compositable->UseTextureHost(tex);

      if (IsAsync()) {
        ScheduleComposition(op);
        // Async layer updates don't trigger invalidation, manually tell the layer
        // that its content have changed.
        if (compositable->GetLayer()) {
          compositable->GetLayer()->SetInvalidRectToVisibleRegion();
        }
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
    case CompositableOperation::TOpUpdateTexture: {
      const OpUpdateTexture& op = aEdit.get_OpUpdateTexture();
      RefPtr<TextureHost> texture = TextureHost::AsTextureHost(op.textureParent());
      MOZ_ASSERT(texture);

      texture->Updated(op.region().type() == MaybeRegion::TnsIntRegion
                       ? &op.region().get_nsIntRegion()
                       : nullptr); // no region means invalidate the entire surface

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

