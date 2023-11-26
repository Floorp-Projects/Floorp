/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for the HTML <canvas> element */

#ifndef nsHTMLCanvasFrame_h___
#define nsHTMLCanvasFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsStringFwd.h"

namespace mozilla {
class PresShell;
namespace layers {
class CanvasRenderer;
class Layer;
class LayerManager;
class WebRenderCanvasData;
}  // namespace layers
}  // namespace mozilla

class nsPresContext;

nsIFrame* NS_NewHTMLCanvasFrame(mozilla::PresShell* aPresShell,
                                mozilla::ComputedStyle* aStyle);

class nsHTMLCanvasFrame final : public nsContainerFrame {
 public:
  typedef mozilla::layers::CanvasRenderer CanvasRenderer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::WebRenderCanvasData WebRenderCanvasData;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsHTMLCanvasFrame)

  nsHTMLCanvasFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsContainerFrame(aStyle, aPresContext, kClassID) {}

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  void Destroy(DestroyContext&) override;

  bool UpdateWebRenderCanvasData(nsDisplayListBuilder* aBuilder,
                                 WebRenderCanvasData* aCanvasData);

  /* get the size of the canvas's image */
  nsIntSize GetCanvasSize() const;

  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;
  virtual mozilla::IntrinsicSize GetIntrinsicSize() override;
  mozilla::AspectRatio GetIntrinsicRatio() const override;

  void UnionChildOverflow(mozilla::OverflowAreas& aOverflowAreas) override;

  SizeComputationResult ComputeSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) override;

  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  // Inserted child content gets its frames parented by our child block
  virtual nsContainerFrame* GetContentInsertionFrame() override {
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  }

  // Return the ::-moz-html-canvas-content anonymous box.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

 protected:
  virtual ~nsHTMLCanvasFrame();
};

#endif /* nsHTMLCanvasFrame_h___ */
