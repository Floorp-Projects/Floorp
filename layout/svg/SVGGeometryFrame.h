/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGGEOMETRYFRAME_H_
#define LAYOUT_SVG_SVGGEOMETRYFRAME_H_

#include "mozilla/Attributes.h"
#include "mozilla/DisplaySVGItem.h"
#include "mozilla/ISVGDisplayableFrame.h"
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "nsIFrame.h"

namespace mozilla {

class DisplaySVGGeometry;
class PresShell;
class SVGGeometryFrame;
class SVGMarkerObserver;

namespace gfx {
class DrawTarget;
}  // namespace gfx

namespace image {
struct imgDrawingParams;
}  // namespace image

}  // namespace mozilla

class gfxContext;
class nsAtom;
class nsIFrame;

struct nsRect;

nsIFrame* NS_NewSVGGeometryFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGGeometryFrame final : public nsIFrame, public ISVGDisplayableFrame {
  using DrawTarget = gfx::DrawTarget;

  friend nsIFrame* ::NS_NewSVGGeometryFrame(mozilla::PresShell* aPresShell,
                                            ComputedStyle* aStyle);

  friend class DisplaySVGGeometry;

 protected:
  SVGGeometryFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                   nsIFrame::ClassID aID = kClassID)
      : nsIFrame(aStyle, aPresContext, aID) {
    AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_MAY_BE_TRANSFORMED);
  }

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(SVGGeometryFrame)

  // nsIFrame interface:
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  void DidSetComputedStyle(ComputedStyle* aOldComputedStyle) override;

  bool IsSVGTransformed(Matrix* aOwnTransforms = nullptr,
                        Matrix* aFromParentTransforms = nullptr) const override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGGeometry"_ns, aResult);
  }
#endif

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  // SVGGeometryFrame methods
  gfxMatrix GetCanvasTM();

  bool IsInvisible() const;

 private:
  // ISVGDisplayableFrame interface:
  void PaintSVG(gfxContext& aContext, const gfxMatrix& aTransform,
                imgDrawingParams& aImgParams) override;
  nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;
  void ReflowSVG() override;
  void NotifySVGChanged(uint32_t aFlags) override;
  SVGBBox GetBBoxContribution(const Matrix& aToBBoxUserspace,
                              uint32_t aFlags) override;
  bool IsDisplayContainer() override { return false; }

  enum { eRenderFill = 1, eRenderStroke = 2 };
  void Render(gfxContext* aContext, uint32_t aRenderComponents,
              const gfxMatrix& aTransform, imgDrawingParams& aImgParams);

  bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const mozilla::layers::StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder, DisplaySVGGeometry* aItem,
      bool aDryRun);
  /**
   * @param aMatrix The transform that must be multiplied onto aContext to
   *   establish this frame's SVG user space.
   */
  void PaintMarkers(gfxContext& aContext, const gfxMatrix& aTransform,
                    imgDrawingParams& aImgParams);

  /*
   * Get the stroke width that markers should use, accounting for
   * non-scaling stroke.
   */
  float GetStrokeWidthForMarkers();
};

//----------------------------------------------------------------------
// Display list item:

class DisplaySVGGeometry final : public DisplaySVGItem {
 public:
  DisplaySVGGeometry(nsDisplayListBuilder* aBuilder, SVGGeometryFrame* aFrame)
      : DisplaySVGItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(DisplaySVGGeometry);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(DisplaySVGGeometry)

  NS_DISPLAY_DECL_NAME("DisplaySVGGeometry", TYPE_SVG_GEOMETRY)

  // Whether this part of the SVG should be natively handled by webrender,
  // potentially becoming an "active layer" inside a blob image.
  bool ShouldBeActive(mozilla::wr::DisplayListBuilder& aBuilder,
                      mozilla::wr::IpcResourceUpdateQueue& aResources,
                      const mozilla::layers::StackingContextHelper& aSc,
                      mozilla::layers::RenderRootStateManager* aManager,
                      nsDisplayListBuilder* aDisplayListBuilder) {
    // We delegate this question to the parent frame to take advantage of
    // the SVGGeometryFrame inheritance hierarchy which provides actual
    // implementation details. The dryRun flag prevents serious side-effects.
    auto* frame = static_cast<SVGGeometryFrame*>(mFrame);
    return frame->CreateWebRenderCommands(aBuilder, aResources, aSc, aManager,
                                          aDisplayListBuilder, this,
                                          /*aDryRun=*/true);
  }

  bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const mozilla::layers::StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override {
    // We delegate this question to the parent frame to take advantage of
    // the SVGGeometryFrame inheritance hierarchy which provides actual
    // implementation details.
    auto* frame = static_cast<SVGGeometryFrame*>(mFrame);
    bool result = frame->CreateWebRenderCommands(aBuilder, aResources, aSc,
                                                 aManager, aDisplayListBuilder,
                                                 this, /*aDryRun=*/false);
    MOZ_ASSERT(result, "ShouldBeActive inconsistent with CreateWRCommands?");
    return result;
  }

  bool IsInvisible() const override {
    auto* frame = static_cast<SVGGeometryFrame*>(mFrame);
    return frame->IsInvisible();
  }
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGGEOMETRYFRAME_H_
