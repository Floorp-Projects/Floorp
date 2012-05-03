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

#ifndef mozilla_layout_RenderFrameParent_h
#define mozilla_layout_RenderFrameParent_h

#include "mozilla/layout/PRenderFrameParent.h"
#include "mozilla/layers/ShadowLayersManager.h"

#include <map>
#include "nsDisplayList.h"
#include "Layers.h"

class nsContentView;
class nsFrameLoader;
class nsSubDocumentFrame;

namespace mozilla {

namespace layers {
class ShadowLayersParent;
}

namespace layout {

class RenderFrameParent : public PRenderFrameParent,
                          public mozilla::layers::ShadowLayersManager
{
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ContainerLayer ContainerLayer;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::ShadowLayersParent ShadowLayersParent;
  typedef FrameMetrics::ViewID ViewID;

public:
  typedef std::map<ViewID, nsRefPtr<nsContentView> > ViewMap;

  RenderFrameParent(nsFrameLoader* aFrameLoader);
  virtual ~RenderFrameParent();

  void Destroy();

  /**
   * Helper function for getting a non-owning reference to a scrollable.
   * @param aId The ID of the frame.
   */
  nsContentView* GetContentView(ViewID aId = FrameMetrics::ROOT_SCROLL_ID);

  void ContentViewScaleChanged(nsContentView* aView);

  virtual void ShadowLayersUpdated(bool isFirstPaint) MOZ_OVERRIDE;

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder* aBuilder,
                              nsSubDocumentFrame* aFrame,
                              const nsRect& aDirtyRect,
                              const nsDisplayListSet& aLists);

  already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame,
                                     LayerManager* aManager,
                                     const nsIntRect& aVisibleRect);

  void OwnerContentChanged(nsIContent* aContent);

  void SetBackgroundColor(nscolor aColor) { mBackgroundColor = gfxRGBA(aColor); };

protected:
  NS_OVERRIDE void ActorDestroy(ActorDestroyReason why);

  NS_OVERRIDE virtual PLayersParent* AllocPLayers(LayerManager::LayersBackend* aBackendType);
  NS_OVERRIDE virtual bool DeallocPLayers(PLayersParent* aLayers);

private:
  void BuildViewMap();

  ShadowLayersParent* GetShadowLayers() const;
  ContainerLayer* GetRootLayer() const;

  nsRefPtr<nsFrameLoader> mFrameLoader;
  nsRefPtr<ContainerLayer> mContainer;

  // This contains the views for all the scrollable frames currently in the
  // painted region of our remote content.
  ViewMap mContentViews;

  // True after Destroy() has been called, which is triggered
  // originally by nsFrameLoader::Destroy().  After this point, we can
  // no longer safely ask the frame loader to find its nearest layer
  // manager, because it may have been disconnected from the DOM.
  // It's still OK to *tell* the frame loader that we've painted after
  // it's destroyed; it'll just ignore us, and we won't be able to
  // find an nsIFrame to invalidate.  See ShadowLayersUpdated().
  //
  // Prefer the extra bit of state to null'ing out mFrameLoader in
  // Destroy() so that less code needs to be special-cased for after
  // Destroy().
  // 
  // It's possible for mFrameLoader==null and
  // mFrameLoaderDestroyed==false.
  bool mFrameLoaderDestroyed;
  // this is gfxRGBA because that's what ColorLayer wants.
  gfxRGBA mBackgroundColor;
};

} // namespace layout
} // namespace mozilla

/**
 * A DisplayRemote exists solely to graft a child process's shadow
 * layer tree (for a given RenderFrameParent) into its parent
 * process's layer tree.
 */
class nsDisplayRemote : public nsDisplayItem
{
  typedef mozilla::layout::RenderFrameParent RenderFrameParent;

public:
  nsDisplayRemote(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                  RenderFrameParent* aRemoteFrame)
    : nsDisplayItem(aBuilder, aFrame)
    , mRemoteFrame(aRemoteFrame)
  {}

  NS_OVERRIDE
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters)
  { return mozilla::LAYER_ACTIVE; }  

  NS_OVERRIDE
  virtual already_AddRefed<Layer>
  BuildLayer(nsDisplayListBuilder* aBuilder, LayerManager* aManager,
             const ContainerParameters& aContainerParameters);

  NS_DISPLAY_DECL_NAME("Remote", TYPE_REMOTE)

private:
  RenderFrameParent* mRemoteFrame;
};

/**
 * nsDisplayRemoteShadow is a way of adding display items for frames in a
 * separate process, for hit testing only. After being processed, the hit
 * test state will contain IDs for any remote frames that were hit.
 *
 * The frame should be its respective render frame parent.
 */
class nsDisplayRemoteShadow : public nsDisplayItem
{
  typedef mozilla::layout::RenderFrameParent RenderFrameParent;
  typedef mozilla::layers::FrameMetrics::ViewID ViewID;

public:
  nsDisplayRemoteShadow(nsDisplayListBuilder* aBuilder,
                        nsIFrame* aFrame,
                        nsRect aRect,
                        ViewID aId)
    : nsDisplayItem(aBuilder, aFrame)
    , mRect(aRect)
    , mId(aId)
  {}

  NS_OVERRIDE nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
  {
    *aSnap = false;
    return mRect;
  }

  virtual PRUint32 GetPerFrameKey()
  {
    NS_ABORT();
    return 0;
  }

  NS_OVERRIDE void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                           HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames);

  NS_DISPLAY_DECL_NAME("Remote-Shadow", TYPE_REMOTE_SHADOW)

private:
  nsRect mRect;
  ViewID mId;
};

#endif  // mozilla_layout_RenderFrameParent_h
