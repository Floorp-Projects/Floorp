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
#include "TiledLayerBuffer.h"           // for TiledLayerComposer
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/ContentHost.h"  // for ContentHostBase
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/LayersTypes.h"  // for MOZ_LAYERS_LOG
#include "mozilla/layers/TextureHost.h"  // for TextureHost
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL
#include "mozilla/layers/ThebesLayerComposite.h"
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsDebug.h"                    // for NS_WARNING, NS_ASSERTION
#include "nsRegion.h"                   // for nsIntRegion

namespace mozilla {
namespace layers {

class ClientTiledLayerBuffer;
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
    case CompositableOperation::TOpPaintTextureRegion: {
      MOZ_LAYERS_LOG(("[ParentSide] Paint ThebesLayer"));

      const OpPaintTextureRegion& op = aEdit.get_OpPaintTextureRegion();
      CompositableParent* compositableParent = static_cast<CompositableParent*>(op.compositableParent());
      CompositableHost* compositable =
        compositableParent->GetCompositableHost();
      Layer* layer = compositable->GetLayer();
      if (!layer || layer->GetType() != Layer::TYPE_THEBES) {
        return false;
      }
      ThebesLayerComposite* thebes = static_cast<ThebesLayerComposite*>(layer);

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
        OpContentBufferSwap(compositableParent, nullptr, frontUpdatedRegion));

      RenderTraceInvalidateEnd(thebes, "FF00FF");
      // return texure data to client if necessary
      ReturnTextureDataIfNecessary(compositable, replyv, op.compositableParent());
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
    case CompositableOperation::TOpUseTiledLayerBuffer: {
      MOZ_LAYERS_LOG(("[ParentSide] Paint TiledLayerBuffer"));
      const OpUseTiledLayerBuffer& op = aEdit.get_OpUseTiledLayerBuffer();
      CompositableParent* compositableParent = static_cast<CompositableParent*>(op.compositableParent());
      CompositableHost* compositable =
        compositableParent->GetCompositableHost();

      TiledLayerComposer* tileComposer = compositable->AsTiledLayerComposer();
      NS_ASSERTION(tileComposer, "compositable is not a tile composer");

      const SurfaceDescriptorTiles& tileDesc = op.tileLayerDescriptor();
      tileComposer->UseTiledLayerBuffer(this, tileDesc);
      break;
    }
    case CompositableOperation::TOpRemoveTexture: {
      const OpRemoveTexture& op = aEdit.get_OpRemoveTexture();
      CompositableHost* compositable = AsCompositable(op);
      RefPtr<TextureHost> tex = TextureHost::AsTextureHost(op.textureParent());

      MOZ_ASSERT(tex.get());
      compositable->RemoveTextureHost(tex);
      // return texure data to client if necessary
      ReturnTextureDataIfNecessary(compositable, replyv, op.compositableParent());
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
      // return texure data to client if necessary
      ReturnTextureDataIfNecessary(compositable, replyv, op.compositableParent());
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
      // return texure data to client if necessary
      ReturnTextureDataIfNecessary(compositable, replyv, op.compositableParent());
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

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
void
CompositableParentManager::ReturnTextureDataIfNecessary(CompositableHost* aCompositable,
                                                        EditReplyVector& replyv,
                                                        PCompositableParent* aParent)
{
  if (!aCompositable || !aCompositable->GetCompositableBackendSpecificData()) {
    return;
  }

  const std::vector< RefPtr<TextureHost> > textureList =
        aCompositable->GetCompositableBackendSpecificData()->GetPendingReleaseFenceTextureList();
  // Return pending Texture data
  for (size_t i = 0; i < textureList.size(); i++) {
    // File descriptor number is limited to 4 per IPC message.
    // See Bug 986253
    if (mPrevFenceHandles.size() >= 4) {
      break;
    }
    TextureHostOGL* hostOGL = textureList[i]->AsHostOGL();
    PTextureParent* actor = textureList[i]->GetIPDLActor();
    if (!hostOGL || !actor) {
      continue;
    }
    android::sp<android::Fence> fence = hostOGL->GetAndResetReleaseFence();
    if (fence.get() && fence->isValid()) {
      FenceHandle handle = FenceHandle(fence);
      replyv.push_back(ReturnReleaseFence(aParent, nullptr, actor, nullptr, handle));
      // Hold fence handle to prevent fence's file descriptor is closed before IPC happens.
      mPrevFenceHandles.push_back(handle);
    }
  }
  aCompositable->GetCompositableBackendSpecificData()->ClearPendingReleaseFenceTextureList();
}
#else
void
CompositableParentManager::ReturnTextureDataIfNecessary(CompositableHost* aCompositable,
                                                       EditReplyVector& replyv,
                                                       PCompositableParent* aParent)
{
  if (!aCompositable || !aCompositable->GetCompositableBackendSpecificData()) {
    return;
  }
  aCompositable->GetCompositableBackendSpecificData()->ClearPendingReleaseFenceTextureList();
}
#endif

void
CompositableParentManager::ClearPrevFenceHandles()
{
  mPrevFenceHandles.clear();
}

} // namespace
} // namespace

