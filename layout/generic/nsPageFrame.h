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
class nsPageFrame final : public nsContainerFrame {

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsPageFrame)

  friend nsPageFrame* NS_NewPageFrame(nsIPresShell* aPresShell,
                                      nsStyleContext* aContext);

  virtual void Reflow(nsPresContext*      aPresContext,
                      ReflowOutput& aDesiredSize,
                      const ReflowInput& aMaxSize,
                      nsReflowStatus&      aStatus) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult  GetFrameName(nsAString& aResult) const override;
#endif

  //////////////////
  // For Printing
  //////////////////

  // Tell the page which page number it is out of how many
  virtual void  SetPageNumInfo(int32_t aPageNumber, int32_t aTotalPages);

  virtual void SetSharedPageData(nsSharedPageData* aPD);

  // We must allow Print Preview UI to have a background, no matter what the
  // user's settings
  virtual bool HonorPrintBackgroundSettings() override { return false; }

  void PaintHeaderFooter(gfxContext& aRenderingContext,
                         nsPoint aPt, bool aSubpixelAA);

protected:
  explicit nsPageFrame(nsStyleContext* aContext);
  virtual ~nsPageFrame();

  typedef enum {
    eHeader,
    eFooter
  } nsHeaderFooterEnum;

  nscoord GetXPosition(gfxContext&          aRenderingContext,
                       nsFontMetrics&       aFontMetrics,
                       const nsRect&        aRect, 
                       int32_t              aJust,
                       const nsString&      aStr);

  void DrawHeaderFooter(gfxContext&          aRenderingContext,
                        nsFontMetrics&       aFontMetrics,
                        nsHeaderFooterEnum   aHeaderFooter,
                        int32_t              aJust,
                        const nsString&      sStr,
                        const nsRect&        aRect,
                        nscoord              aHeight,
                        nscoord              aAscent,
                        nscoord              aWidth);

  void DrawHeaderFooter(gfxContext&          aRenderingContext,
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
  nsMargin mPageContentMargin;
};


class nsPageBreakFrame : public nsLeafFrame
{
  NS_DECL_FRAMEARENA_HELPERS(nsPageBreakFrame)

  explicit nsPageBreakFrame(nsStyleContext* aContext);
  ~nsPageBreakFrame();

  virtual void Reflow(nsPresContext* aPresContext,
                      ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult  GetFrameName(nsAString& aResult) const override;
#endif

protected:

  virtual nscoord GetIntrinsicISize() override;
  virtual nscoord GetIntrinsicBSize() override;

  bool mHaveReflowed;

  friend nsIFrame* NS_NewPageBreakFrame(nsIPresShell* aPresShell,
                                        nsStyleContext* aContext);
};

#endif /* nsPageFrame_h___ */

