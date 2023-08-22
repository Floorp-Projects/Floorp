/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPageFrame_h___
#define nsPageFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsLeafFrame.h"

class nsFontMetrics;
class nsPageContentFrame;
class nsSharedPageData;

namespace mozilla {
class PresShell;
}  // namespace mozilla

// Page frame class. Represents an individual page, in paginated mode.
class nsPageFrame final : public nsContainerFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsPageFrame)

  friend nsPageFrame* NS_NewPageFrame(mozilla::PresShell* aPresShell,
                                      ComputedStyle* aStyle);

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aReflowOutput,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

  //////////////////
  // For Printing
  //////////////////

  // Determine this page's page-number, based on its previous continuation
  // (whose page number is presumed to already be known).
  void DeterminePageNum();
  int32_t GetPageNum() const { return mPageNum; }

  void SetSharedPageData(nsSharedPageData* aPD);
  nsSharedPageData* GetSharedPageData() const { return mPD; }

  void PaintHeaderFooter(gfxContext& aRenderingContext, nsPoint aPt,
                         bool aSubpixelAA);

  const nsMargin& GetUsedPageContentMargin() const {
    return mPageContentMargin;
  }

  uint32_t IndexOnSheet() const { return mIndexOnSheet; }
  void SetIndexOnSheet(uint32_t aIndexOnSheet) {
    mIndexOnSheet = aIndexOnSheet;
  }

  ComputeTransformFunction GetTransformGetter() const override;

  nsPageContentFrame* PageContentFrame() const;

  nsSize ComputePageSize() const;

  // Computes the scaling factor to fit the page to the sheet in the single
  // page-per-sheet case. (The multiple pages-per-sheet case is currently
  // different - see the comment for
  // PrintedSheetFrame::ComputePagesPerSheetGridMetrics and code in
  // ComputePagesPerSheetAndPageSizeTransform.) The page and sheet dimensions
  // may be different due to a CSS page-size that gives the page a size that is
  // too large to fit on the sheet that we are printing to.
  float ComputeSinglePPSPageSizeScale(const nsSize aContentPageSize) const;

  // Returns the rotation from CSS `page-orientation` property, if set, and if
  // it applies. Note: the single page-per-sheet case is special since in that
  // case we effectively rotate the sheet (as opposed to rotating pages in
  // their pages-per-sheet grid cell). In this case we return zero if the
  // output medium does not support changing the dimensions (orientation) of
  // the sheet (i.e. only print preview and save-to-PDF are supported).
  double GetPageOrientationRotation(nsSharedPageData* aPD) const;

  // The default implementation of FirstContinuation in nsSplittableFrame is
  // implemented in linear time, walking back through the linked list of
  // continuations via mPrevContinuation.
  // For nsPageFrames, we can find the first continuation through the frame
  // tree structure in constant time.
  nsIFrame* FirstContinuation() const final;

 protected:
  explicit nsPageFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);
  virtual ~nsPageFrame();

  typedef enum { eHeader, eFooter } nsHeaderFooterEnum;

  nscoord GetXPosition(gfxContext& aRenderingContext,
                       nsFontMetrics& aFontMetrics, const nsRect& aRect,
                       int32_t aJust, const nsString& aStr);

  nsReflowStatus ReflowPageContent(nsPresContext*,
                                   const ReflowInput& aPageReflowInput);

  void DrawHeaderFooter(gfxContext& aRenderingContext,
                        nsFontMetrics& aFontMetrics,
                        nsHeaderFooterEnum aHeaderFooter, int32_t aJust,
                        const nsString& sStr, const nsRect& aRect,
                        nscoord aHeight, nscoord aAscent, nscoord aWidth);

  void DrawHeaderFooter(gfxContext& aRenderingContext,
                        nsFontMetrics& aFontMetrics,
                        nsHeaderFooterEnum aHeaderFooter,
                        const nsString& aStrLeft, const nsString& aStrRight,
                        const nsString& aStrCenter, const nsRect& aRect,
                        nscoord aAscent, nscoord aHeight);

  void ProcessSpecialCodes(const nsString& aStr, nsString& aNewStr);

  static constexpr int32_t kPageNumUnset = -1;
  // 1-based page-num
  int32_t mPageNum = kPageNumUnset;

  // 0-based index on the sheet that we belong to. Unused/meaningless if this
  // page has frame state bit NS_PAGE_SKIPPED_BY_CUSTOM_RANGE.
  uint32_t mIndexOnSheet = 0;

  // Note: this will be set before reflow, and it's strongly owned by our
  // nsPageSequenceFrame, which outlives us.
  nsSharedPageData* mPD = nullptr;

  // Computed page content margins.
  //
  // This is the amount of space from the edges of the content to the edges,
  // measured in the content coordinate space. This is as opposed to the
  // coordinate space of the physical paper. This might be different due to
  // a CSS page-size that is too large to fit on the paper, causing content to
  // be scaled to fit.
  //
  // These margins take into account:
  //    * CSS-defined margins (content units)
  //    * User-supplied margins (physical units)
  //    * Unwriteable-supplied margins (physical units)
  //
  // When computing these margins, all physical units have the inverse of the
  // scaling factor caused by CSS page-size downscaling applied. This ensures
  // that even if the content will be downscaled, it will respect the (now
  // upscaled) physical unwriteable margins required by the printer.
  // For user-supplied margins, it isn't immediately obvious to the user what
  // the intended page-size of the document is, so we consider these margins to
  // be in the physical space of the paper.
  nsMargin mPageContentMargin;
};

class nsPageBreakFrame final : public nsLeafFrame {
  NS_DECL_FRAMEARENA_HELPERS(nsPageBreakFrame)

  explicit nsPageBreakFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);
  ~nsPageBreakFrame();

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aReflowOutput,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

 protected:
  nscoord GetIntrinsicISize() override;
  nscoord GetIntrinsicBSize() override;

  friend nsIFrame* NS_NewPageBreakFrame(mozilla::PresShell* aPresShell,
                                        ComputedStyle* aStyle);
};

#endif /* nsPageFrame_h___ */
