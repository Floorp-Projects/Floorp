/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSUBDOCUMENTFRAME_H_
#define NSSUBDOCUMENTFRAME_H_

#include "LayerState.h"
#include "mozilla/Attributes.h"
#include "nsDisplayList.h"
#include "nsAtomicContainerFrame.h"
#include "nsIReflowCallback.h"
#include "nsFrameLoader.h"
#include "Units.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

namespace mozilla::layers {
class Layer;
class RefLayer;
class RenderRootStateManager;
class WebRenderLayerScrollData;
class WebRenderScrollData;
}  // namespace mozilla::layers

/******************************************************************************
 * nsSubDocumentFrame
 *****************************************************************************/
class nsSubDocumentFrame final : public nsAtomicContainerFrame,
                                 public nsIReflowCallback {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsSubDocumentFrame)

  explicit nsSubDocumentFrame(ComputedStyle* aStyle,
                              nsPresContext* aPresContext);

#ifdef DEBUG_FRAME_DUMP
  void List(FILE* out = stderr, const char* aPrefix = "",
            ListFlags aFlags = ListFlags()) const override;
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

  NS_DECL_QUERYFRAME

  bool IsFrameOfType(uint32_t aFlags) const override {
    return nsAtomicContainerFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eReplaced | nsIFrame::eReplacedSizing |
                   nsIFrame::eReplacedContainsBlock));
  }

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  void DestroyFrom(nsIFrame* aDestructRoot,
                   PostDestroyData& aPostDestroyData) override;

  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  mozilla::IntrinsicSize GetIntrinsicSize() override;
  mozilla::AspectRatio GetIntrinsicRatio() const override;

  mozilla::LogicalSize ComputeAutoSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWritingMode,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) override;

  SizeComputationResult ComputeSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) override;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  void DidSetComputedStyle(ComputedStyle* aOldComputedStyle) override;

  // if the content is "visibility:hidden", then just hide the view
  // and all our contents. We don't extend "visibility:hidden" to
  // the child content ourselves, since it belongs to a different
  // document and CSS doesn't inherit in there.
  bool SupportsVisibilityHidden() override { return false; }

#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() override;
#endif

  nsIDocShell* GetDocShell() const;
  nsresult BeginSwapDocShells(nsIFrame* aOther);
  void EndSwapDocShells(nsIFrame* aOther);
  nsView* EnsureInnerView();
  nsPoint GetExtraOffset() const;
  nsIFrame* GetSubdocumentRootFrame();
  enum { IGNORE_PAINT_SUPPRESSION = 0x1 };
  mozilla::PresShell* GetSubdocumentPresShellForPainting(uint32_t aFlags);
  mozilla::ScreenIntSize GetSubdocumentSize();

  // nsIReflowCallback
  bool ReflowFinished() override;
  void ReflowCallbackCanceled() override;

  /**
   * Return true if pointer event hit-testing should be allowed to target
   * content in the subdocument.
   */
  bool PassPointerEventsToChildren();

  void MaybeShowViewer() {
    if (!mDidCreateDoc && !mCallingShow) {
      ShowViewer();
    }
  }

  nsFrameLoader* FrameLoader() const;

  enum class RetainPaintData : bool { No, Yes };
  void ResetFrameLoader(RetainPaintData);
  void ClearRetainedPaintData();

  void PropagateIsUnderHiddenEmbedderElementToSubView(
      bool aIsUnderHiddenEmbedderElement);

  void ClearDisplayItems();

  void SubdocumentIntrinsicSizeOrRatioChanged();

  struct RemoteFramePaintData {
    mozilla::layers::LayersId mLayersId;
    mozilla::dom::TabId mTabId{0};
  };

  RemoteFramePaintData GetRemotePaintData() const;
  bool HasRetainedPaintData() const { return mRetainedRemoteFrame.isSome(); }

 protected:
  friend class AsyncFrameInit;

  bool IsInline() { return mIsInline; }

  nscoord GetIntrinsicBSize() {
    auto size = GetIntrinsicSize();
    Maybe<nscoord> bSize =
        GetWritingMode().IsVertical() ? size.width : size.height;
    return bSize.valueOr(0);
  }

  nscoord GetIntrinsicISize() {
    auto size = GetIntrinsicSize();
    Maybe<nscoord> iSize =
        GetWritingMode().IsVertical() ? size.height : size.width;
    return iSize.valueOr(0);
  }

  // Show our document viewer. The document viewer is hidden via a script
  // runner, so that we can save and restore the presentation if we're
  // being reframed.
  void ShowViewer();

  nsView* GetViewInternal() const override { return mOuterView; }
  void SetViewInternal(nsView* aView) override { mOuterView = aView; }

  mutable RefPtr<nsFrameLoader> mFrameLoader;

  nsView* mOuterView;
  nsView* mInnerView;

  // When process-switching a remote tab, we might temporarily paint the old
  // one.
  Maybe<RemoteFramePaintData> mRetainedRemoteFrame;

  bool mIsInline : 1;
  bool mPostedReflowCallback : 1;
  bool mDidCreateDoc : 1;
  bool mCallingShow : 1;
};

namespace mozilla {

/**
 * A nsDisplayRemote will graft a remote frame's shadow layer tree (for a given
 * nsFrameLoader) into its parent frame's layer tree.
 */
class nsDisplayRemote final : public nsPaintedDisplayItem {
  typedef mozilla::ContainerLayerParameters ContainerLayerParameters;
  typedef mozilla::dom::TabId TabId;
  typedef mozilla::gfx::Matrix4x4 Matrix4x4;
  typedef mozilla::layers::EventRegionsOverride EventRegionsOverride;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayersId LayersId;
  typedef mozilla::layers::RefLayer RefLayer;
  typedef mozilla::layers::StackingContextHelper StackingContextHelper;
  typedef mozilla::LayerState LayerState;
  typedef mozilla::LayoutDeviceRect LayoutDeviceRect;
  typedef mozilla::LayoutDevicePoint LayoutDevicePoint;

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
  friend class nsDisplayItem;
  using RemoteFramePaintData = nsSubDocumentFrame::RemoteFramePaintData;

  nsFrameLoader* GetFrameLoader() const;

  RemoteFramePaintData mPaintData;
  LayoutDevicePoint mOffset;
  EventRegionsOverride mEventRegionsOverride;
};

}  // namespace mozilla

#endif /* NSSUBDOCUMENTFRAME_H_ */
