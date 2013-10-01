/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_RenderFrameParent_h
#define mozilla_layout_RenderFrameParent_h

#include "mozilla/Attributes.h"
#include <map>

#include "mozilla/layout/PRenderFrameParent.h"
#include "mozilla/layers/ShadowLayersManager.h"
#include "nsDisplayList.h"
#include "RenderFrameUtils.h"

class nsContentView;
class nsFrameLoader;
class nsSubDocumentFrame;

namespace mozilla {

class InputEvent;

namespace layers {
class APZCTreeManager;
class GestureEventListener;
class TargetConfig;
class LayerTransactionParent;
struct TextureFactoryIdentifier;
}

namespace layout {

class RemoteContentController;

class RenderFrameParent : public PRenderFrameParent,
                          public mozilla::layers::ShadowLayersManager
{
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ContainerLayer ContainerLayer;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::TargetConfig TargetConfig;
  typedef mozilla::layers::LayerTransactionParent LayerTransactionParent;
  typedef mozilla::FrameLayerBuilder::ContainerParameters ContainerParameters;
  typedef mozilla::layers::TextureFactoryIdentifier TextureFactoryIdentifier;
  typedef FrameMetrics::ViewID ViewID;

public:
  typedef std::map<ViewID, nsRefPtr<nsContentView> > ViewMap;

  /**
   * Select the desired scrolling behavior.  If ASYNC_PAN_ZOOM is
   * chosen, then RenderFrameParent will watch input events and use
   * them to asynchronously pan and zoom.
   */
  RenderFrameParent(nsFrameLoader* aFrameLoader,
                    ScrollingBehavior aScrollingBehavior,
                    TextureFactoryIdentifier* aTextureFactoryIdentifier,
                    uint64_t* aId);
  virtual ~RenderFrameParent();

  void Destroy();

  /**
   * Helper function for getting a non-owning reference to a scrollable.
   * @param aId The ID of the frame.
   */
  nsContentView* GetContentView(ViewID aId = FrameMetrics::ROOT_SCROLL_ID);

  void ContentViewScaleChanged(nsContentView* aView);

  virtual void ShadowLayersUpdated(LayerTransactionParent* aLayerTree,
                                   const TargetConfig& aTargetConfig,
                                   bool isFirstPaint) MOZ_OVERRIDE;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        nsSubDocumentFrame* aFrame,
                        const nsRect& aDirtyRect,
                        const nsDisplayListSet& aLists);

  already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame,
                                     LayerManager* aManager,
                                     const nsIntRect& aVisibleRect,
                                     nsDisplayItem* aItem,
                                     const ContainerParameters& aContainerParameters);

  void OwnerContentChanged(nsIContent* aContent);

  void SetBackgroundColor(nscolor aColor) { mBackgroundColor = gfxRGBA(aColor); };

  void NotifyInputEvent(const WidgetInputEvent& aEvent,
                        WidgetInputEvent* aOutEvent);

  void NotifyDimensionsChanged(ScreenIntSize size);

  void ZoomToRect(const CSSRect& aRect);

  void ContentReceivedTouch(bool aPreventDefault);

  void UpdateZoomConstraints(bool aAllowZoom,
                             const CSSToScreenScale& aMinZoom,
                             const CSSToScreenScale& aMaxZoom);

  void UpdateScrollOffset(uint32_t aPresShellId,
                          ViewID aViewId,
                          const CSSIntPoint& aScrollOffset);

  bool HitTest(const nsRect& aRect);

protected:
  void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  virtual bool RecvNotifyCompositorTransaction() MOZ_OVERRIDE;

  virtual bool RecvCancelDefaultPanZoom() MOZ_OVERRIDE;
  virtual bool RecvDetectScrollableSubframe() MOZ_OVERRIDE;

  virtual bool RecvUpdateHitRegion(const nsRegion& aRegion) MOZ_OVERRIDE;

  virtual PLayerTransactionParent* AllocPLayerTransactionParent() MOZ_OVERRIDE;
  virtual bool DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers) MOZ_OVERRIDE;

private:
  void BuildViewMap();
  void TriggerRepaint();
  void DispatchEventForPanZoomController(const InputEvent& aEvent);

  LayerTransactionParent* GetShadowLayers() const;
  uint64_t GetLayerTreeId() const;
  Layer* GetRootLayer() const;

  // When our child frame is pushing transactions directly to the
  // compositor, this is the ID of its layer tree in the compositor's
  // context.
  uint64_t mLayersId;

  nsRefPtr<nsFrameLoader> mFrameLoader;
  nsRefPtr<ContainerLayer> mContainer;
  // When our scrolling behavior is ASYNC_PAN_ZOOM, we have a nonnull
  // APZCTreeManager. It's used to manipulate the shadow layer tree
  // on the compositor thread.
  nsRefPtr<layers::APZCTreeManager> mApzcTreeManager;
  nsRefPtr<RemoteContentController> mContentController;

  layers::APZCTreeManager* GetApzcTreeManager();

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

  nsRegion mTouchRegion;
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

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters) MOZ_OVERRIDE
  { return mozilla::LAYER_ACTIVE_FORCE; }

  virtual already_AddRefed<Layer>
  BuildLayer(nsDisplayListBuilder* aBuilder, LayerManager* aManager,
             const ContainerParameters& aContainerParameters) MOZ_OVERRIDE;

  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE;

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

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE
  {
    *aSnap = false;
    return mRect;
  }

  virtual uint32_t GetPerFrameKey() MOZ_OVERRIDE
  {
    NS_ABORT();
    return 0;
  }

  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE;

  NS_DISPLAY_DECL_NAME("Remote-Shadow", TYPE_REMOTE_SHADOW)

private:
  nsRect mRect;
  ViewID mId;
};

#endif  // mozilla_layout_RenderFrameParent_h
