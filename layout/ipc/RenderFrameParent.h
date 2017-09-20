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

#include "mozilla/layers/APZUtils.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layout/PRenderFrameParent.h"
#include "nsDisplayList.h"

class nsFrameLoader;
class nsSubDocumentFrame;

namespace mozilla {

class InputEvent;

namespace layers {
class AsyncDragMetrics;
class TargetConfig;
struct TextureFactoryIdentifier;
struct ScrollableLayerGuid;
} // namespace layers

namespace layout {

class RenderFrameParent : public PRenderFrameParent
{
  typedef mozilla::layers::AsyncDragMetrics AsyncDragMetrics;
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::CompositorOptions CompositorOptions;
  typedef mozilla::layers::ContainerLayer ContainerLayer;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::TargetConfig TargetConfig;
  typedef mozilla::ContainerLayerParameters ContainerLayerParameters;
  typedef mozilla::layers::TextureFactoryIdentifier TextureFactoryIdentifier;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;
  typedef mozilla::layers::TouchBehaviorFlags TouchBehaviorFlags;
  typedef mozilla::layers::ZoomConstraints ZoomConstraints;
  typedef FrameMetrics::ViewID ViewID;

public:


  /**
   * Select the desired scrolling behavior.  If ASYNC_PAN_ZOOM is
   * chosen, then RenderFrameParent will watch input events and use
   * them to asynchronously pan and zoom.
   */
  explicit RenderFrameParent(nsFrameLoader* aFrameLoader);
  virtual ~RenderFrameParent();

  bool Init(nsFrameLoader* aFrameLoader);
  bool IsInitted();
  void Destroy();

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        nsSubDocumentFrame* aFrame,
                        const nsDisplayListSet& aLists);

  already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame,
                                     LayerManager* aManager,
                                     nsDisplayItem* aItem,
                                     const ContainerLayerParameters& aContainerParameters);

  void OwnerContentChanged(nsIContent* aContent);

  bool HitTest(const nsRect& aRect);

  void GetTextureFactoryIdentifier(TextureFactoryIdentifier* aTextureFactoryIdentifier);

  inline uint64_t GetLayersId() const { return mLayersId; }
  inline bool IsLayersConnected() const { return mLayersConnected; }
  inline CompositorOptions GetCompositorOptions() const { return mCompositorOptions; }

  void TakeFocusForClickFromTap();

  void EnsureLayersConnected(CompositorOptions* aCompositorOptions);

  LayerManager* AttachLayerManager();

protected:
  void ActorDestroy(ActorDestroyReason why) override;

  virtual mozilla::ipc::IPCResult RecvNotifyCompositorTransaction() override;

private:
  void TriggerRepaint();
  void DispatchEventForPanZoomController(const InputEvent& aEvent);

  // When our child frame is pushing transactions directly to the
  // compositor, this is the ID of its layer tree in the compositor's
  // context.
  uint64_t mLayersId;
  // A flag that indicates whether or not the compositor knows about the
  // layers id. In some cases this RenderFrameParent is not connected to the
  // compositor and so this flag is false.
  bool mLayersConnected;
  // The compositor options for this layers id. This is only meaningful if
  // the compositor actually knows about this layers id (i.e. when mLayersConnected
  // is true).
  CompositorOptions mCompositorOptions;

  RefPtr<nsFrameLoader> mFrameLoader;
  RefPtr<ContainerLayer> mContainer;
  RefPtr<LayerManager> mLayerManager;

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

  bool mAsyncPanZoomEnabled;
  bool mInitted;
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
  nsDisplayRemote(nsDisplayListBuilder* aBuilder, nsSubDocumentFrame* aFrame,
                  RenderFrameParent* aRemoteFrame);

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override
  { return mozilla::LAYER_ACTIVE_FORCE; }

  virtual already_AddRefed<Layer>
  BuildLayer(nsDisplayListBuilder* aBuilder, LayerManager* aManager,
             const ContainerLayerParameters& aContainerParameters) override;

  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;
  virtual bool UpdateScrollData(mozilla::layers::WebRenderScrollData* aData,
                                mozilla::layers::WebRenderLayerScrollData* aLayerData) override;

  uint64_t GetRemoteLayersId() const;

  NS_DISPLAY_DECL_NAME("Remote", TYPE_REMOTE)

private:
  RenderFrameParent* mRemoteFrame;
  mozilla::LayoutDeviceIntPoint mOffset;
  mozilla::layers::EventRegionsOverride mEventRegionsOverride;
};


#endif  // mozilla_layout_RenderFrameParent_h
