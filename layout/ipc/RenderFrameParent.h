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
#include "nsDisplayList.h"
#include "RenderFrameUtils.h"

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
struct ScrollableLayerGuid;
}

namespace layout {

class RemoteContentController;

class RenderFrameParent : public PRenderFrameParent
{
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ContainerLayer ContainerLayer;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::TargetConfig TargetConfig;
  typedef mozilla::layers::LayerTransactionParent LayerTransactionParent;
  typedef mozilla::ContainerLayerParameters ContainerLayerParameters;
  typedef mozilla::layers::TextureFactoryIdentifier TextureFactoryIdentifier;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;
  typedef mozilla::layers::ZoomConstraints ZoomConstraints;
  typedef FrameMetrics::ViewID ViewID;

public:


  /**
   * Select the desired scrolling behavior.  If ASYNC_PAN_ZOOM is
   * chosen, then RenderFrameParent will watch input events and use
   * them to asynchronously pan and zoom.
   */
  RenderFrameParent(nsFrameLoader* aFrameLoader,
                    ScrollingBehavior aScrollingBehavior,
                    TextureFactoryIdentifier* aTextureFactoryIdentifier,
                    uint64_t* aId, bool* aSuccess);
  virtual ~RenderFrameParent();

  void Destroy();

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        nsSubDocumentFrame* aFrame,
                        const nsRect& aDirtyRect,
                        const nsDisplayListSet& aLists);

  already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame,
                                     LayerManager* aManager,
                                     const nsIntRect& aVisibleRect,
                                     nsDisplayItem* aItem,
                                     const ContainerLayerParameters& aContainerParameters);

  void OwnerContentChanged(nsIContent* aContent);

  void SetBackgroundColor(nscolor aColor) { mBackgroundColor = gfxRGBA(aColor); };

  /**
   * Notify the APZ code of an input event, and get back the untransformed event.
   * @param aEvent the input event; this is modified in-place so that the async
   *        transforms are unapplied. This can be passed to Gecko for hit testing
   *        and normal event dispatch.
   * @param aOutTargetGuid An out-parameter that will contain the identifier
   *        of the APZC instance that handled the event, if one was found. This
   *        argument may be null.
   */
  nsEventStatus NotifyInputEvent(WidgetInputEvent& aEvent,
                                 ScrollableLayerGuid* aOutTargetGuid);

  void ZoomToRect(uint32_t aPresShellId, ViewID aViewId, const CSSRect& aRect);

  void ContentReceivedTouch(const ScrollableLayerGuid& aGuid,
                            bool aPreventDefault);

  void UpdateZoomConstraints(uint32_t aPresShellId,
                             ViewID aViewId,
                             bool aIsRoot,
                             const ZoomConstraints& aConstraints);

  bool HitTest(const nsRect& aRect);

protected:
  void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  virtual bool RecvNotifyCompositorTransaction() MOZ_OVERRIDE;

  virtual bool RecvUpdateHitRegion(const nsRegion& aRegion) MOZ_OVERRIDE;

private:
  void TriggerRepaint();
  void DispatchEventForPanZoomController(const InputEvent& aEvent);

  uint64_t GetLayerTreeId() const;

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
                                   const ContainerLayerParameters& aParameters) MOZ_OVERRIDE
  { return mozilla::LAYER_ACTIVE_FORCE; }

  virtual already_AddRefed<Layer>
  BuildLayer(nsDisplayListBuilder* aBuilder, LayerManager* aManager,
             const ContainerLayerParameters& aContainerParameters) MOZ_OVERRIDE;

  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE;

  NS_DISPLAY_DECL_NAME("Remote", TYPE_REMOTE)

private:
  RenderFrameParent* mRemoteFrame;
};


#endif  // mozilla_layout_RenderFrameParent_h
