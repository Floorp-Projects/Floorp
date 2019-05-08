/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_RenderFrame_h
#define mozilla_layout_RenderFrame_h

#include "base/process.h"

#include "mozilla/Attributes.h"

#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsDisplayList.h"

class nsFrameLoader;
class nsSubDocumentFrame;

namespace mozilla {

namespace dom {
class BrowserParent;
}  // namespace dom

namespace layers {
struct TextureFactoryIdentifier;
}  // namespace layers

namespace layout {

/**
 * RenderFrame connects and manages layer trees for remote frames. It is
 * directly owned by a BrowserParent and always lives in the parent process.
 */
class RenderFrame final {
  typedef mozilla::layers::CompositorOptions CompositorOptions;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::LayersId LayersId;
  typedef mozilla::layers::TextureFactoryIdentifier TextureFactoryIdentifier;

 public:
  RenderFrame();
  virtual ~RenderFrame();

  bool Initialize(dom::BrowserParent* aBrowserParent);
  void Destroy();

  void EnsureLayersConnected(CompositorOptions* aCompositorOptions);
  LayerManager* AttachLayerManager();
  void OwnerContentChanged();

  LayersId GetLayersId() const { return mLayersId; }
  CompositorOptions GetCompositorOptions() const { return mCompositorOptions; }

  void GetTextureFactoryIdentifier(
      TextureFactoryIdentifier* aTextureFactoryIdentifier) const;

  bool IsInitialized() const { return mInitialized; }
  bool IsLayersConnected() const { return mLayersConnected; }

 private:
  // The process id of the remote frame. This is used by the compositor to
  // do security checks on incoming layer transactions.
  base::ProcessId mTabProcessId;
  // The layers id of the remote frame.
  LayersId mLayersId;
  // The compositor options for this layers id. This is only meaningful if
  // the compositor actually knows about this layers id (i.e. when
  // mLayersConnected is true).
  CompositorOptions mCompositorOptions;

  dom::BrowserParent* mBrowserParent;
  RefPtr<LayerManager> mLayerManager;

  bool mInitialized;
  // A flag that indicates whether or not the compositor knows about the
  // layers id. In some cases this RenderFrame is not connected to the
  // compositor and so this flag is false.
  bool mLayersConnected;
};

}  // namespace layout
}  // namespace mozilla

/**
 * A nsDisplayRemote will graft a remote frame's shadow layer tree (for a given
 * nsFrameLoader) into its parent frame's layer tree.
 */
class nsDisplayRemote final : public nsPaintedDisplayItem {
  typedef mozilla::dom::TabId TabId;
  typedef mozilla::gfx::Matrix4x4 Matrix4x4;
  typedef mozilla::layers::EventRegionsOverride EventRegionsOverride;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayersId LayersId;
  typedef mozilla::layers::RefLayer RefLayer;
  typedef mozilla::layout::RenderFrame RenderFrame;
  typedef mozilla::LayoutDeviceRect LayoutDeviceRect;
  typedef mozilla::LayoutDeviceIntPoint LayoutDeviceIntPoint;

 public:
  nsDisplayRemote(nsDisplayListBuilder* aBuilder, nsSubDocumentFrame* aFrame);

  LayerState GetLayerState(
      nsDisplayListBuilder* aBuilder, LayerManager* aManager,
      const ContainerLayerParameters& aParameters) override;

  already_AddRefed<Layer> BuildLayer(
      nsDisplayListBuilder* aBuilder, LayerManager* aManager,
      const ContainerLayerParameters& aContainerParameters) override;

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  bool UpdateScrollData(
      mozilla::layers::WebRenderScrollData* aData,
      mozilla::layers::WebRenderLayerScrollData* aLayerData) override;

  NS_DISPLAY_DECL_NAME("Remote", TYPE_REMOTE)

 private:
  friend class nsDisplayItemBase;
  nsFrameLoader* GetFrameLoader() const;

  TabId mTabId;
  LayersId mLayersId;
  LayoutDeviceIntPoint mOffset;
  EventRegionsOverride mEventRegionsOverride;
};

#endif  // mozilla_layout_RenderFrame_h
