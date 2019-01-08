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

namespace layers {
struct TextureFactoryIdentifier;
}  // namespace layers

namespace layout {

class RenderFrame final {
  typedef mozilla::layers::CompositorOptions CompositorOptions;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::LayersId LayersId;
  typedef mozilla::layers::TextureFactoryIdentifier TextureFactoryIdentifier;

 public:
  RenderFrame();
  virtual ~RenderFrame();

  bool Initialize(nsFrameLoader* aFrameLoader);
  void Destroy();

  void EnsureLayersConnected(CompositorOptions* aCompositorOptions);
  LayerManager* AttachLayerManager();
  void OwnerContentChanged(nsIContent* aContent);

  nsFrameLoader* GetFrameLoader() const { return mFrameLoader; }
  LayersId GetLayersId() const { return mLayersId; }
  CompositorOptions GetCompositorOptions() const { return mCompositorOptions; }

  void GetTextureFactoryIdentifier(
      TextureFactoryIdentifier* aTextureFactoryIdentifier) const;

  bool IsInitialized() const { return mInitialized; }
  bool IsLayersConnected() const { return mLayersConnected; }

 private:
  base::ProcessId mTabProcessId;
  // When our child frame is pushing transactions directly to the
  // compositor, this is the ID of its layer tree in the compositor's
  // context.
  LayersId mLayersId;
  // The compositor options for this layers id. This is only meaningful if
  // the compositor actually knows about this layers id (i.e. when
  // mLayersConnected is true).
  CompositorOptions mCompositorOptions;

  RefPtr<nsFrameLoader> mFrameLoader;
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
 * A DisplayRemote exists solely to graft a child process's shadow
 * layer tree (for a given RenderFrame) into its parent
 * process's layer tree.
 */
class nsDisplayRemote final : public nsDisplayItem {
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

  bool HasDeletedFrame() const override;

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
  LayersId GetRemoteLayersId() const;
  RenderFrame* GetRenderFrame() const;

  TabId mTabId;
  LayoutDeviceIntPoint mOffset;
  EventRegionsOverride mEventRegionsOverride;
};

#endif  // mozilla_layout_RenderFrame_h
