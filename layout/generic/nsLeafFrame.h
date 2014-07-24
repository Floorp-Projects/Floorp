/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for rendering objects that do not have child lists */

#ifndef nsLeafFrame_h___
#define nsLeafFrame_h___

#include "mozilla/Attributes.h"
#include "nsFrame.h"
#include "nsDisplayList.h"

/**
 * Abstract class that provides simple fixed-size layout for leaf objects
 * (e.g. images, form elements, etc.). Deriviations provide the implementation
 * of the GetDesiredSize method. The rendering method knows how to render
 * borders and backgrounds.
 */
class nsLeafFrame : public nsFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame replacements
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE {
    DO_GLOBAL_REFLOW_COUNT_DSP("nsLeafFrame");
    DisplayBorderBackgroundOutline(aBuilder, aLists);
  }

  /**
   * Both GetMinISize and GetPrefISize will return whatever GetIntrinsicISize
   * returns.
   */
  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  /**
   * Our auto size is just intrinsic width and intrinsic height.
   */
  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap) MOZ_OVERRIDE;

  /**
   * Reflow our frame.  This will use the computed width plus borderpadding for
   * the desired width, and use the return value of GetIntrinsicBSize plus
   * borderpadding for the desired height.  Ascent will be set to the height,
   * and descent will be set to 0.
   */
  virtual void Reflow(nsPresContext*      aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&      aStatus) MOZ_OVERRIDE;
  
  /**
   * This method does most of the work that Reflow() above need done.
   */
  virtual void DoReflow(nsPresContext*      aPresContext,
                        nsHTMLReflowMetrics& aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&      aStatus);

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    // We don't actually contain a block, but we do always want a
    // computed width, so tell a little white lie here.
    return nsFrame::IsFrameOfType(aFlags & ~(nsIFrame::eReplacedContainsBlock));
  }

protected:
  nsLeafFrame(nsStyleContext* aContext) : nsFrame(aContext) {}
  virtual ~nsLeafFrame();

  /**
   * Return the intrinsic width of the frame's content area. Note that this
   * should not include borders or padding and should not depend on the applied
   * styles.
   */
  virtual nscoord GetIntrinsicISize() = 0;

  /**
   * Return the intrinsic height of the frame's content area.  This should not
   * include border or padding.  This will only matter if the specified height
   * is auto.  Note that subclasses must either implement this or override
   * Reflow and ComputeAutoSize; the default Reflow and ComputeAutoSize impls
   * call this method.
   */
  virtual nscoord GetIntrinsicBSize();

  /**
   * Subroutine to add in borders and padding
   */
  void AddBordersAndPadding(const nsHTMLReflowState& aReflowState,
                            mozilla::LogicalSize& aDesiredSize);

  /**
   * Set aDesiredSize to be the available size
   */
  void SizeToAvailSize(const nsHTMLReflowState& aReflowState,
                       nsHTMLReflowMetrics& aDesiredSize);
};

#endif /* nsLeafFrame_h___ */
