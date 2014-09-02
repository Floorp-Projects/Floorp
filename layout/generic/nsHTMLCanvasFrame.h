/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for the HTML <canvas> element */

#ifndef nsHTMLCanvasFrame_h___
#define nsHTMLCanvasFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "FrameLayerBuilder.h"

namespace mozilla {
namespace layers {
class Layer;
class LayerManager;
}
}

class nsPresContext;
class nsDisplayItem;
class nsAString;

nsIFrame* NS_NewHTMLCanvasFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

class nsHTMLCanvasFrame : public nsContainerFrame
{
public:
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::ContainerLayerParameters ContainerLayerParameters;

  NS_DECL_QUERYFRAME_TARGET(nsHTMLCanvasFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  explicit nsHTMLCanvasFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext)
    , mBorderPadding(GetWritingMode()) {}

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) MOZ_OVERRIDE;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     nsDisplayItem* aItem,
                                     const ContainerLayerParameters& aContainerParameters);

  /* get the size of the canvas's image */
  nsIntSize GetCanvasSize();

  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nsSize GetIntrinsicRatio() MOZ_OVERRIDE;

  virtual mozilla::LogicalSize
  ComputeSize(nsRenderingContext *aRenderingContext,
              mozilla::WritingMode aWritingMode,
              const mozilla::LogicalSize& aCBSize,
              nscoord aAvailableISize,
              const mozilla::LogicalSize& aMargin,
              const mozilla::LogicalSize& aBorder,
              const mozilla::LogicalSize& aPadding,
              uint32_t aFlags) MOZ_OVERRIDE;

  virtual void Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  nsRect GetInnerArea() const;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsSplittableFrame::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced));
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

  // Inserted child content gets its frames parented by our child block
  virtual nsContainerFrame* GetContentInsertionFrame() MOZ_OVERRIDE {
    return GetFirstPrincipalChild()->GetContentInsertionFrame();
  }

protected:
  virtual ~nsHTMLCanvasFrame();

  nscoord GetContinuationOffset(nscoord* aWidth = 0) const;

  mozilla::LogicalMargin mBorderPadding;
};

#endif /* nsHTMLCanvasFrame_h___ */
