/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPageFrame_h___
#define nsPageFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsLeafFrame.h"

class nsFontMetrics;
class nsSharedPageData;

// Page frame class used by the simple page sequence frame
class nsPageFrame MOZ_FINAL : public nsContainerFrame {

public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsPageFrame* NS_NewPageFrame(nsIPresShell* aPresShell,
                                      nsStyleContext* aContext);

  virtual void Reflow(nsPresContext*      aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aMaxSize,
                      nsReflowStatus&      aStatus) MOZ_OVERRIDE;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::pageFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  
#ifdef DEBUG_FRAME_DUMP
  virtual nsresult  GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

  //////////////////
  // For Printing
  //////////////////

  // Tell the page which page number it is out of how many
  virtual void  SetPageNumInfo(int32_t aPageNumber, int32_t aTotalPages);

  virtual void SetSharedPageData(nsSharedPageData* aPD);

  // We must allow Print Preview UI to have a background, no matter what the
  // user's settings
  virtual bool HonorPrintBackgroundSettings() MOZ_OVERRIDE { return false; }

  void PaintHeaderFooter(nsRenderingContext& aRenderingContext,
                         nsPoint aPt);

protected:
  explicit nsPageFrame(nsStyleContext* aContext);
  virtual ~nsPageFrame();

  typedef enum {
    eHeader,
    eFooter
  } nsHeaderFooterEnum;

  nscoord GetXPosition(nsRenderingContext& aRenderingContext,
                       nsFontMetrics&       aFontMetrics,
                       const nsRect&        aRect, 
                       int32_t              aJust,
                       const nsString&      aStr);

  void DrawHeaderFooter(nsRenderingContext& aRenderingContext,
                        nsFontMetrics&       aFontMetrics,
                        nsHeaderFooterEnum   aHeaderFooter,
                        int32_t              aJust,
                        const nsString&      sStr,
                        const nsRect&        aRect,
                        nscoord              aHeight,
                        nscoord              aAscent,
                        nscoord              aWidth);

  void DrawHeaderFooter(nsRenderingContext& aRenderingContext,
                        nsFontMetrics&       aFontMetrics,
                        nsHeaderFooterEnum   aHeaderFooter,
                        const nsString&      aStrLeft,
                        const nsString&      aStrRight,
                        const nsString&      aStrCenter,
                        const nsRect&        aRect,
                        nscoord              aAscent,
                        nscoord              aHeight);

  void ProcessSpecialCodes(const nsString& aStr, nsString& aNewStr);

  int32_t     mPageNum;
  int32_t     mTotNumPages;

  nsSharedPageData* mPD;
};


class nsPageBreakFrame : public nsLeafFrame
{
  NS_DECL_FRAMEARENA_HELPERS

  explicit nsPageBreakFrame(nsStyleContext* aContext);
  ~nsPageBreakFrame();

  virtual void Reflow(nsPresContext*          aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult  GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

protected:

  virtual nscoord GetIntrinsicISize() MOZ_OVERRIDE;
  virtual nscoord GetIntrinsicBSize() MOZ_OVERRIDE;

    bool mHaveReflowed;

    friend nsIFrame* NS_NewPageBreakFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
};

#endif /* nsPageFrame_h___ */

