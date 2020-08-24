/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPageSequenceFrame_h___
#define nsPageSequenceFrame_h___

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsContainerFrame.h"
#include "nsIPrintSettings.h"

namespace mozilla {

class PresShell;

namespace dom {

class HTMLCanvasElement;

}  // namespace dom
}  // namespace mozilla

/**
 * This class maintains various shared data that is used by printing-related
 * frames. The nsPageSequenceFrame strongly owns an instance of this class,
 * which lives for as long as the nsPageSequenceFrame does.
 */
class nsSharedPageData {
 public:
  nsString mDateTimeStr;
  nsString mPageNumFormat;
  nsString mPageNumAndTotalsFormat;
  nsString mDocTitle;
  nsString mDocURL;
  nsFont mHeadFootFont;

  // Total number of pages (populated by PrintedSheetFrame when it determines
  // that it's reflowed the final page):
  int32_t mRawNumPages = 0;

  // Margin for headers and footers; it defaults to 4/100 of an inch on UNIX
  // and 0 elsewhere; I think it has to do with some inconsistency in page size
  // computations
  nsMargin mEdgePaperMargin;

  nsCOMPtr<nsIPrintSettings> mPrintSettings;

  // The scaling ratio we need to apply to make all pages fit horizontally. It's
  // the minimum "ComputedWidth / OverflowWidth" ratio of all page content
  // frames that overflowed.  It's 1.0 if none overflowed horizontally.
  float mShrinkToFitRatio = 1.0f;
};

// Page sequence frame class. Manages a series of pages, in paginated mode.
// (Strictly speaking, this frame's direct children are PrintedSheetFrame
// instances, and each of those will usually contain one nsPageFrame, depending
// on the "pages-per-sheet" setting.)
class nsPageSequenceFrame final : public nsContainerFrame {
 public:
  friend nsPageSequenceFrame* NS_NewPageSequenceFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsPageSequenceFrame)

  // nsIFrame
  void Reflow(nsPresContext* aPresContext, ReflowOutput& aReflowOutput,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  // For Shrink To Fit
  float GetSTFPercent() const { return mPageData->mShrinkToFitRatio; }

  // Gets the final print preview scale that we're applying to the previewed
  // sheets of paper.
  float GetPrintPreviewScale() const;

  // Async Printing
  nsresult StartPrint(nsPresContext* aPresContext,
                      nsIPrintSettings* aPrintSettings,
                      const nsAString& aDocTitle, const nsAString& aDocURL);
  nsresult PrePrintNextPage(nsITimerCallback* aCallback, bool* aDone);
  nsresult PrintNextPage();
  void ResetPrintCanvasList();
  int32_t GetCurrentPageNum() const { return mPageNum; }
  int32_t GetRawNumPages() const { return mPageData->mRawNumPages; }
  bool IsDoingPrintRange() const { return mDoingPageRange; }
  void GetPrintRange(int32_t* aFromPage, int32_t* aToPage) const;
  nsresult DoPageEnd();

  // We must allow Print Preview UI to have a background, no matter what the
  // user's settings
  bool HonorPrintBackgroundSettings() override { return false; }

  bool HasTransformGetter() const override { return true; }

  /**
   * Return our first sheet frame.
   */
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

 protected:
  nsPageSequenceFrame(ComputedStyle*, nsPresContext*);
  virtual ~nsPageSequenceFrame();

  void SetPageNumberFormat(const char* aPropName, const char* aDefPropVal,
                           bool aPageNumOnly);

  // SharedPageData Helper methods
  void SetDateTimeStr(const nsAString& aDateTimeStr);
  void SetPageNumberFormat(const nsAString& aFormatStr, bool aForPageNumOnly);

  // Print scaling is applied in this function.
  void PopulateReflowOutput(ReflowOutput&, const ReflowInput&);

  // Helper function to compute the offset needed to center a child
  // page-frame's margin-box inside our content-box.
  nscoord ComputeCenteringMargin(nscoord aContainerContentBoxWidth,
                                 nscoord aChildPaddingBoxWidth,
                                 const nsMargin& aChildPhysicalMargin);

  void DetermineWhetherToPrintPage();
  nsIFrame* GetCurrentPageFrame();

  nsSize mSize;

  // These next two LogicalSize members are used when we're in print-preview to
  // ensure that each previewed sheet will fit in the print-preview scrollport:
  // -------

  // Each component of this LogicalSize represents the maximum length of all
  // our print-previewed sheets in that axis, plus a little extra for the
  // print-preview margin.  Note that this LogicalSize doesn't necessarily
  // correspond to any one particular sheet's size (especially if our sheets
  // have different sizes), since the components are tracked independently such
  // that we end up storing the maximum in each dimension.
  LogicalSize mMaxSheetSize;
  // The size of the scrollport where we're print-previewing sheets.
  LogicalSize mScrollportSize;

  // Data shared by all the nsPageFrames:
  mozilla::UniquePtr<nsSharedPageData> mPageData;

  // Async Printing
  int32_t mPageNum;
  int32_t mFromPageNum;
  int32_t mToPageNum;

  nsTArray<int32_t> mPageRanges;
  nsTArray<RefPtr<mozilla::dom::HTMLCanvasElement> > mCurrentCanvasList;

  // Asynch Printing
  bool mPrintThisPage;
  bool mDoingPageRange;

  bool mCalledBeginPage;

  bool mCurrentCanvasListSetup;
};

#endif /* nsPageSequenceFrame_h___ */
