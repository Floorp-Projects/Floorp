/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsSimplePageSequenceFrame_h___
#define nsSimplePageSequenceFrame_h___

#include "mozilla/Attributes.h"
#include "nsIPageSequenceFrame.h"
#include "nsContainerFrame.h"
#include "nsIPrintSettings.h"

namespace mozilla {
namespace dom {

class HTMLCanvasElement;

} // namespace dom
} // namespace mozilla

//-----------------------------------------------
// This class maintains all the data that 
// is used by all the page frame
// It lives while the nsSimplePageSequenceFrame lives
class nsSharedPageData {
public:
  // This object a shared by all the nsPageFrames
  // parented to a SimplePageSequenceFrame
  nsSharedPageData() : mShrinkToFitRatio(1.0f) {}

  nsString    mDateTimeStr;
  nsString    mPageNumFormat;
  nsString    mPageNumAndTotalsFormat;
  nsString    mDocTitle;
  nsString    mDocURL;
  nsFont      mHeadFootFont;

  nsSize      mReflowSize;
  nsMargin    mReflowMargin;
  // Margin for headers and footers; it defaults to 4/100 of an inch on UNIX 
  // and 0 elsewhere; I think it has to do with some inconsistency in page size
  // computations
  nsMargin    mEdgePaperMargin;

  nsCOMPtr<nsIPrintSettings> mPrintSettings;

  // The scaling ratio we need to apply to make all pages fit horizontally.  It's
  // the minimum "ComputedWidth / OverflowWidth" ratio of all page content frames
  // that overflowed.  It's 1.0 if none overflowed horizontally.
  float mShrinkToFitRatio;
};

// Simple page sequence frame class. Used when we're in paginated mode
class nsSimplePageSequenceFrame : public nsContainerFrame,
                                  public nsIPageSequenceFrame {
public:
  friend nsSimplePageSequenceFrame* NS_NewSimplePageSequenceFrame(nsIPresShell* aPresShell,
                                                                  nsStyleContext* aContext);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame
  virtual void Reflow(nsPresContext*      aPresContext,
                      ReflowOutput& aDesiredSize,
                      const ReflowInput& aMaxSize,
                      nsReflowStatus&      aStatus) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  // nsIPageSequenceFrame
  NS_IMETHOD SetPageNo(int32_t aPageNo) { return NS_OK;}
  NS_IMETHOD SetSelectionHeight(nscoord aYOffset, nscoord aHeight) override { mYSelOffset = aYOffset; mSelectionHeight = aHeight; return NS_OK; }
  NS_IMETHOD SetTotalNumPages(int32_t aTotal) override { mTotalPages = aTotal; return NS_OK; }
  
  // For Shrink To Fit
  NS_IMETHOD GetSTFPercent(float& aSTFPercent) override;

  // Async Printing
  NS_IMETHOD StartPrint(nsPresContext*    aPresContext,
                        nsIPrintSettings* aPrintSettings,
                        const nsAString&  aDocTitle,
                        const nsAString&  aDocURL) override;
  NS_IMETHOD PrePrintNextPage(nsITimerCallback* aCallback, bool* aDone) override;
  NS_IMETHOD PrintNextPage() override;
  NS_IMETHOD ResetPrintCanvasList() override;
  NS_IMETHOD GetCurrentPageNum(int32_t* aPageNum) override;
  NS_IMETHOD GetNumPages(int32_t* aNumPages) override;
  NS_IMETHOD IsDoingPrintRange(bool* aDoing) override;
  NS_IMETHOD GetPrintRange(int32_t* aFromPage, int32_t* aToPage) override;
  NS_IMETHOD DoPageEnd() override;

  // We must allow Print Preview UI to have a background, no matter what the
  // user's settings
  virtual bool HonorPrintBackgroundSettings() override { return false; }

  virtual bool HasTransformGetter() const override { return true; }

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::sequenceFrame
   */
  virtual nsIAtom* GetType() const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult  GetFrameName(nsAString& aResult) const override;
#endif

protected:
  explicit nsSimplePageSequenceFrame(nsStyleContext* aContext);
  virtual ~nsSimplePageSequenceFrame();

  void SetPageNumberFormat(const char* aPropName, const char* aDefPropVal, bool aPageNumOnly);

  // SharedPageData Helper methods
  void SetDateTimeStr(const nsAString& aDateTimeStr);
  void SetPageNumberFormat(const nsAString& aFormatStr, bool aForPageNumOnly);

  // Sets the frame desired size to the size of the viewport, or the given
  // nscoords, whichever is larger. Print scaling is applied in this function.
  void SetDesiredSize(ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nscoord aWidth, nscoord aHeight);

  // Helper function to compute the offset needed to center a child
  // page-frame's margin-box inside our content-box.
  nscoord ComputeCenteringMargin(nscoord aContainerContentBoxWidth,
                                 nscoord aChildPaddingBoxWidth,
                                 const nsMargin& aChildPhysicalMargin);


  void DetermineWhetherToPrintPage();
  nsIFrame* GetCurrentPageFrame();

  nsMargin mMargin;

  nsSize       mSize;
  nsSharedPageData* mPageData; // data shared by all the nsPageFrames

  // Asynch Printing
  int32_t      mPageNum;
  int32_t      mTotalPages;
  int32_t      mPrintRangeType;
  int32_t      mFromPageNum;
  int32_t      mToPageNum;
  nsTArray<int32_t> mPageRanges;
  nsTArray<RefPtr<mozilla::dom::HTMLCanvasElement> > mCurrentCanvasList;

  // Selection Printing Info
  nscoord      mSelectionHeight;
  nscoord      mYSelOffset;

  // Asynch Printing
  bool mPrintThisPage;
  bool mDoingPageRange;

  bool mIsPrintingSelection;

  bool mCalledBeginPage;

  bool mCurrentCanvasListSetup;
};

#endif /* nsSimplePageSequenceFrame_h___ */

