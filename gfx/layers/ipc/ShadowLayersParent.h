/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_layers_ShadowLayersParent_h
#define mozilla_layers_ShadowLayersParent_h

#include "mozilla/layers/PLayersParent.h"
#include "ShadowLayers.h"
#include "ShadowLayersManager.h"

namespace mozilla {

namespace layout {
class RenderFrameParent;
}

namespace layers {

class Layer;
class ShadowLayerManager;

class ShadowLayersParent : public PLayersParent,
                           public ISurfaceDeAllocator
{
  typedef mozilla::layout::RenderFrameParent RenderFrameParent;
  typedef InfallibleTArray<Edit> EditArray;
  typedef InfallibleTArray<EditReply> EditReplyArray;

public:
  ShadowLayersParent(ShadowLayerManager* aManager, ShadowLayersManager* aLayersManager);
  ~ShadowLayersParent();

  void Destroy();

  ShadowLayerManager* layer_manager() const { return mLayerManager; }

  ContainerLayer* GetRoot() const { return mRoot; }

  virtual void DestroySharedSurface(gfxSharedImageSurface* aSurface);
  virtual void DestroySharedSurface(SurfaceDescriptor* aSurface);

protected:
  NS_OVERRIDE virtual bool RecvUpdate(const EditArray& cset,
                                      const bool& isFirstPaint,
                                      EditReplyArray* reply);

  NS_OVERRIDE virtual PLayerParent* AllocPLayer();
  NS_OVERRIDE virtual bool DeallocPLayer(PLayerParent* actor);

private:
  nsRefPtr<ShadowLayerManager> mLayerManager;
  ShadowLayersManager* mShadowLayersManager;
  // Hold the root because it might be grafted under various
  // containers in the "real" layer tree
  nsRefPtr<ContainerLayer> mRoot;
  // When the widget/frame/browser stuff in this process begins its
  // destruction process, we need to Disconnect() all the currently
  // live shadow layers, because some of them might be orphaned from
  // the layer tree.  This happens in Destroy() above.  After we
  // Destroy() ourself, there's a window in which that information
  // hasn't yet propagated back to the child side and it might still
  // send us layer transactions.  We want to ignore those transactions
  // because they refer to "zombie layers" on this side.  So, we track
  // that state with |mDestroyed|.  This is similar to, but separate
  // from, |mLayerManager->IsDestroyed()|; we might have had Destroy()
  // called on us but the mLayerManager might not be destroyed, or
  // vice versa.  In both cases though, we want to ignore shadow-layer
  // transactions posted by the child.
  bool mDestroyed;
};

} // namespace layers
} // namespace mozilla

#endif // ifndef mozilla_layers_ShadowLayersParent_h
