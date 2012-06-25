/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPageFrame_h___
#define nsPageFrame_h___

#include "nsContainerFrame.h"
#include "nsLeafFrame.h"

class nsSharedPageData;

// Page frame class used by the simple page sequence frame
class nsPageFrame : public nsContainerFrame {

public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewPageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD  Reflow(nsPresContext*      aPresContext,
                     nsHTMLReflowMetrics& aDesiredSize,
                     const nsHTMLReflowState& aMaxSize,
                     nsReflowStatus&      aStatus);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::pageFrame
   */
  virtual nsIAtom* GetType() const;
  
#ifdef DEBUG
  NS_IMETHOD  GetFrameName(nsAString& aResult) const;
#endif

  //////////////////
  // For Printing
  //////////////////

  // Tell the page which page number it is out of how many
  virtual void  SetPageNumInfo(PRInt32 aPageNumber, PRInt32 aTotalPages);

  virtual void SetSharedPageData(nsSharedPageData* aPD);

  // We must allow Print Preview UI to have a background, no matter what the
  // user's settings
  virtual bool HonorPrintBackgroundSettings() { return false; }

  void PaintHeaderFooter(nsRenderingContext& aRenderingContext,
                         nsPoint aPt);
  void PaintPageContent(nsRenderingContext& aRenderingContext,
                        const nsRect&        aDirtyRect,
                        nsPoint              aPt);

protected:
  nsPageFrame(nsStyleContext* aContext);
  virtual ~nsPageFrame();

  typedef enum {
    eHeader,
    eFooter
  } nsHeaderFooterEnum;

  nscoord GetXPosition(nsRenderingContext& aRenderingContext, 
                       const nsRect&        aRect, 
                       PRInt32              aJust,
                       const nsString&      aStr);

  void DrawHeaderFooter(nsRenderingContext& aRenderingContext,
                        nsHeaderFooterEnum   aHeaderFooter,
                        PRInt32              aJust,
                        const nsString&      sStr,
                        const nsRect&        aRect,
                        nscoord              aHeight,
                        nscoord              aAscent,
                        nscoord              aWidth);

  void DrawHeaderFooter(nsRenderingContext& aRenderingContext,
                        nsHeaderFooterEnum   aHeaderFooter,
                        const nsString&      aStrLeft,
                        const nsString&      aStrRight,
                        const nsString&      aStrCenter,
                        const nsRect&        aRect,
                        nscoord              aAscent,
                        nscoord              aHeight);

  void ProcessSpecialCodes(const nsString& aStr, nsString& aNewStr);

  PRInt32     mPageNum;
  PRInt32     mTotNumPages;

  nsSharedPageData* mPD;
};


class nsPageBreakFrame : public nsLeafFrame
{
  NS_DECL_FRAMEARENA_HELPERS

  nsPageBreakFrame(nsStyleContext* aContext);
  ~nsPageBreakFrame();

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD  GetFrameName(nsAString& aResult) const;
#endif

protected:

  virtual nscoord GetIntrinsicWidth();
  virtual nscoord GetIntrinsicHeight();

    bool mHaveReflowed;

    friend nsIFrame* NS_NewPageBreakFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
};

#endif /* nsPageFrame_h___ */

