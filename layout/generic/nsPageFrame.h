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

  // We must allow Print Preview UI to have a background, no matter what the
  // user's settings
  bool HonorPrintBackgroundSettings() const override { return false; }

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

  bool mHaveReflowed;

  friend nsIFrame* NS_NewPageBreakFrame(mozilla::PresShell* aPresShell,
                                        ComputedStyle* aStyle);
};

#endif /* nsPageFrame_h___ */
