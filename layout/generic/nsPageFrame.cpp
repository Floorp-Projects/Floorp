/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPageFrame.h"

#include "mozilla/AppUnits.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/gfx/2D.h"
#include "gfxContext.h"
#include "nsDeviceContext.h"
#include "nsFontMetrics.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsFieldSetFrame.h"
#include "nsPageContentFrame.h"
#include "nsDisplayList.h"
#include "nsPageSequenceFrame.h"  // for nsSharedPageData
#include "nsTextFormatter.h"      // for page number localization formatting
#include "nsBidiUtils.h"
#include "nsIPrintSettings.h"

#include "mozilla/Logging.h"
extern mozilla::LazyLogModule gLayoutPrintingLog;
#define PR_PL(_p1) MOZ_LOG(gLayoutPrintingLog, mozilla::LogLevel::Debug, _p1)

using namespace mozilla;
using namespace mozilla::gfx;

nsPageFrame* NS_NewPageFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsPageFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsPageFrame)

NS_QUERYFRAME_HEAD(nsPageFrame)
  NS_QUERYFRAME_ENTRY(nsPageFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

nsPageFrame::nsPageFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
    : nsContainerFrame(aStyle, aPresContext, kClassID) {}

nsPageFrame::~nsPageFrame() = default;

nsReflowStatus nsPageFrame::ReflowPageContent(
    nsPresContext* aPresContext, const ReflowInput& aPageReflowInput) {
  nsIFrame* const frame = PageContentFrame();
  // XXX Pay attention to the page's border and padding...
  //
  // Reflow our ::-moz-page-content frame, allowing it only to be as big as we
  // are (minus margins).
  nsSize maxSize = ComputePageSize();

  float scale = aPresContext->GetPageScale();
  // When the reflow size is NS_UNCONSTRAINEDSIZE it means we are reflowing
  // a single page to print selection. So this means we want to use
  // NS_UNCONSTRAINEDSIZE without altering it.
  //
  // FIXME(emilio): Is this still true?
  maxSize.width = NSToCoordCeil(maxSize.width / scale);
  if (maxSize.height != NS_UNCONSTRAINEDSIZE) {
    maxSize.height = NSToCoordCeil(maxSize.height / scale);
  }

  // Get the number of Twips per pixel from the PresContext
  const nscoord onePixel = AppUnitsPerCSSPixel();

  // insurance against infinite reflow, when reflowing less than a pixel
  // XXX Shouldn't we do something more friendly when invalid margins
  //     are set?
  if (maxSize.width < onePixel || maxSize.height < onePixel) {
    NS_WARNING("Reflow aborted; no space for content");
    return {};
  }

  ReflowInput kidReflowInput(aPresContext, aPageReflowInput, frame,
                             LogicalSize(frame->GetWritingMode(), maxSize));
  kidReflowInput.mFlags.mIsTopOfPage = true;
  kidReflowInput.mFlags.mTableIsSplittable = true;

  mPageContentMargin = aPresContext->GetDefaultPageMargin();

  // Use the margins given in the @page rule if told to do so.
  // We clamp to the paper's unwriteable margins to avoid clipping, *except*
  // that we will respect a margin of zero if specified, assuming this means
  // the document is intended to fit the paper size exactly, and the client is
  // taking full responsibility for what happens around the edges.
  if (mPD->mPrintSettings->GetHonorPageRuleMargins()) {
    const auto& margin = kidReflowInput.mStyleMargin->mMargin;
    for (const auto side : mozilla::AllPhysicalSides()) {
      if (!margin.Get(side).IsAuto()) {
        nscoord computed = kidReflowInput.ComputedPhysicalMargin().Side(side);
        // Respecting a zero margin is particularly important when the client
        // is PDF.js where the PDF already contains the margins.
        if (computed == 0) {
          mPageContentMargin.Side(side) = 0;
        } else {
          nscoord unwriteable = nsPresContext::CSSTwipsToAppUnits(
              mPD->mPrintSettings->GetUnwriteableMarginInTwips().Side(side));
          mPageContentMargin.Side(side) = std::max(
              kidReflowInput.ComputedPhysicalMargin().Side(side), unwriteable);
        }
      }
    }
  }

  nscoord maxWidth = maxSize.width - mPageContentMargin.LeftRight() / scale;
  nscoord maxHeight;
  if (maxSize.height == NS_UNCONSTRAINEDSIZE) {
    maxHeight = NS_UNCONSTRAINEDSIZE;
  } else {
    maxHeight = maxSize.height - mPageContentMargin.TopBottom() / scale;
  }

  // Check the width and height, if they're too small we reset the margins
  // back to the default.
  if (maxWidth < onePixel || maxHeight < onePixel) {
    mPageContentMargin = aPresContext->GetDefaultPageMargin();
    maxWidth = maxSize.width - mPageContentMargin.LeftRight() / scale;
    if (maxHeight != NS_UNCONSTRAINEDSIZE) {
      maxHeight = maxSize.height - mPageContentMargin.TopBottom() / scale;
    }
  }

  // And if they're still too small, we give up.
  if (maxWidth < onePixel || maxHeight < onePixel) {
    NS_WARNING("Reflow aborted; no space for content");
    return {};
  }

  kidReflowInput.SetComputedWidth(maxWidth);
  kidReflowInput.SetComputedHeight(maxHeight);

  // calc location of frame
  nscoord xc = mPageContentMargin.left;
  nscoord yc = mPageContentMargin.top;

  // Get the child's desired size
  ReflowOutput kidOutput(kidReflowInput);
  nsReflowStatus kidStatus;
  ReflowChild(frame, aPresContext, kidOutput, kidReflowInput, xc, yc,
              ReflowChildFlags::Default, kidStatus);

  // Place and size the child
  FinishReflowChild(frame, aPresContext, kidOutput, &kidReflowInput, xc, yc,
                    ReflowChildFlags::Default);

  NS_ASSERTION(!kidStatus.IsFullyComplete() || !frame->GetNextInFlow(),
               "bad child flow list");
  return kidStatus;
}

void nsPageFrame::Reflow(nsPresContext* aPresContext,
                         ReflowOutput& aReflowOutput,
                         const ReflowInput& aReflowInput,
                         nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsPageFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aReflowOutput, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  MOZ_ASSERT(mPD, "Need a pointer to nsSharedPageData before reflow starts");

  // Our status is the same as our child's.
  aStatus = ReflowPageContent(aPresContext, aReflowInput);

  PR_PL(("PageFrame::Reflow %p ", this));
  PR_PL(("[%d,%d][%d,%d]\n", aReflowOutput.Width(), aReflowOutput.Height(),
         aReflowInput.AvailableWidth(), aReflowInput.AvailableHeight()));

  // Return our desired size
  WritingMode wm = aReflowInput.GetWritingMode();
  aReflowOutput.ISize(wm) = aReflowInput.AvailableISize();
  if (aReflowInput.AvailableBSize() != NS_UNCONSTRAINEDSIZE) {
    aReflowOutput.BSize(wm) = aReflowInput.AvailableBSize();
  }

  aReflowOutput.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aReflowOutput);

  PR_PL(("PageFrame::Reflow %p ", this));
  PR_PL(("[%d,%d]\n", aReflowInput.AvailableWidth(),
         aReflowInput.AvailableHeight()));

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aReflowOutput);
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsPageFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"Page"_ns, aResult);
}
#endif

void nsPageFrame::ProcessSpecialCodes(const nsString& aStr, nsString& aNewStr) {
  aNewStr = aStr;

  // Search to see if the &D code is in the string
  // then subst in the current date/time
  constexpr auto kDate = u"&D"_ns;
  if (aStr.Find(kDate) != kNotFound) {
    aNewStr.ReplaceSubstring(kDate, mPD->mDateTimeStr);
  }

  // NOTE: Must search for &PT before searching for &P
  //
  // Search to see if the "page number and page" total code are in the string
  // and replace the page number and page total code with the actual
  // values
  constexpr auto kPageAndTotal = u"&PT"_ns;
  if (aStr.Find(kPageAndTotal) != kNotFound) {
    nsAutoString uStr;
    nsTextFormatter::ssprintf(uStr, mPD->mPageNumAndTotalsFormat.get(),
                              mPageNum, mPD->mRawNumPages);
    aNewStr.ReplaceSubstring(kPageAndTotal, uStr);
  }

  // Search to see if the page number code is in the string
  // and replace the page number code with the actual value
  constexpr auto kPage = u"&P"_ns;
  if (aStr.Find(kPage) != kNotFound) {
    nsAutoString uStr;
    nsTextFormatter::ssprintf(uStr, mPD->mPageNumFormat.get(), mPageNum);
    aNewStr.ReplaceSubstring(kPage, uStr);
  }

  constexpr auto kTitle = u"&T"_ns;
  if (aStr.Find(kTitle) != kNotFound) {
    aNewStr.ReplaceSubstring(kTitle, mPD->mDocTitle);
  }

  constexpr auto kDocURL = u"&U"_ns;
  if (aStr.Find(kDocURL) != kNotFound) {
    aNewStr.ReplaceSubstring(kDocURL, mPD->mDocURL);
  }

  constexpr auto kPageTotal = u"&L"_ns;
  if (aStr.Find(kPageTotal) != kNotFound) {
    nsAutoString uStr;
    nsTextFormatter::ssprintf(uStr, mPD->mPageNumFormat.get(),
                              mPD->mRawNumPages);
    aNewStr.ReplaceSubstring(kPageTotal, uStr);
  }
}

//------------------------------------------------------------------------------
nscoord nsPageFrame::GetXPosition(gfxContext& aRenderingContext,
                                  nsFontMetrics& aFontMetrics,
                                  const nsRect& aRect, int32_t aJust,
                                  const nsString& aStr) {
  nscoord width = nsLayoutUtils::AppUnitWidthOfStringBidi(
      aStr, this, aFontMetrics, aRenderingContext);
  nscoord x = aRect.x;
  switch (aJust) {
    case nsIPrintSettings::kJustLeft:
      x += mPD->mEdgePaperMargin.left;
      break;

    case nsIPrintSettings::kJustCenter:
      x += (aRect.width - width) / 2;
      break;

    case nsIPrintSettings::kJustRight:
      x += aRect.width - width - mPD->mEdgePaperMargin.right;
      break;
  }  // switch

  return x;
}

// Draw a header or footer
// @param aRenderingContext - rendering content to draw into
// @param aHeaderFooter - indicates whether it is a header or footer
// @param aStrLeft - string for the left header or footer; can be empty
// @param aStrCenter - string for the center header or footer; can be empty
// @param aStrRight - string for the right header or footer; can be empty
// @param aRect - the rect of the page
// @param aAscent - the ascent of the font
// @param aHeight - the height of the font
void nsPageFrame::DrawHeaderFooter(
    gfxContext& aRenderingContext, nsFontMetrics& aFontMetrics,
    nsHeaderFooterEnum aHeaderFooter, const nsString& aStrLeft,
    const nsString& aStrCenter, const nsString& aStrRight, const nsRect& aRect,
    nscoord aAscent, nscoord aHeight) {
  int32_t numStrs = 0;
  if (!aStrLeft.IsEmpty()) numStrs++;
  if (!aStrCenter.IsEmpty()) numStrs++;
  if (!aStrRight.IsEmpty()) numStrs++;

  if (numStrs == 0) return;
  const nscoord contentWidth =
      aRect.width - (mPD->mEdgePaperMargin.left + mPD->mEdgePaperMargin.right);
  const nscoord strSpace = contentWidth / numStrs;

  if (!aStrLeft.IsEmpty()) {
    DrawHeaderFooter(aRenderingContext, aFontMetrics, aHeaderFooter,
                     nsIPrintSettings::kJustLeft, aStrLeft, aRect, aAscent,
                     aHeight, strSpace);
  }
  if (!aStrCenter.IsEmpty()) {
    DrawHeaderFooter(aRenderingContext, aFontMetrics, aHeaderFooter,
                     nsIPrintSettings::kJustCenter, aStrCenter, aRect, aAscent,
                     aHeight, strSpace);
  }
  if (!aStrRight.IsEmpty()) {
    DrawHeaderFooter(aRenderingContext, aFontMetrics, aHeaderFooter,
                     nsIPrintSettings::kJustRight, aStrRight, aRect, aAscent,
                     aHeight, strSpace);
  }
}

// Draw a header or footer string
// @param aRenderingContext - rendering context to draw into
// @param aHeaderFooter - indicates whether it is a header or footer
// @param aJust - indicates where the string is located within the header/footer
// @param aStr - the string to be drawn
// @param aRect - the rect of the page
// @param aHeight - the height of the font
// @param aAscent - the ascent of the font
// @param aWidth - available width for the string
void nsPageFrame::DrawHeaderFooter(gfxContext& aRenderingContext,
                                   nsFontMetrics& aFontMetrics,
                                   nsHeaderFooterEnum aHeaderFooter,
                                   int32_t aJust, const nsString& aStr,
                                   const nsRect& aRect, nscoord aAscent,
                                   nscoord aHeight, nscoord aWidth) {
  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();

  if ((aHeaderFooter == eHeader && aHeight < mPageContentMargin.top) ||
      (aHeaderFooter == eFooter && aHeight < mPageContentMargin.bottom)) {
    nsAutoString str;
    ProcessSpecialCodes(aStr, str);

    int32_t indx;
    int32_t textWidth = 0;
    const char16_t* text = str.get();

    int32_t len = (int32_t)str.Length();
    if (len == 0) {
      return;  // bail is empty string
    }
    // find how much text fits, the "position" is the size of the available area
    if (nsLayoutUtils::BinarySearchForPosition(drawTarget, aFontMetrics, text,
                                               0, 0, 0, len, int32_t(aWidth),
                                               indx, textWidth)) {
      if (indx < len - 1) {
        // we can't fit in all the text
        if (indx > 3) {
          // But we can fit in at least 4 chars.  Show all but 3 of them, then
          // an ellipsis.
          // XXXbz for non-plane0 text, this may be cutting things in the
          // middle of a codepoint!  Also, we have no guarantees that the three
          // dots will fit in the space the three chars we removed took up with
          // these font metrics!
          str.Truncate(indx - 3);
          str.AppendLiteral("...");
        } else {
          // We can only fit 3 or fewer chars.  Just show nothing
          str.Truncate();
        }
      }
    } else {
      return;  // bail if couldn't find the correct length
    }

    if (HasRTLChars(str)) {
      PresContext()->SetBidiEnabled();
    }

    // calc the x and y positions of the text
    nscoord x =
        GetXPosition(aRenderingContext, aFontMetrics, aRect, aJust, str);
    nscoord y;
    if (aHeaderFooter == eHeader) {
      y = aRect.y + mPD->mEdgePaperMargin.top;
    } else {
      y = aRect.YMost() - aHeight - mPD->mEdgePaperMargin.bottom;
    }

    // set up new clip and draw the text
    aRenderingContext.Save();
    aRenderingContext.Clip(NSRectToSnappedRect(
        aRect, PresContext()->AppUnitsPerDevPixel(), *drawTarget));
    aRenderingContext.SetColor(sRGBColor::OpaqueBlack());
    nsLayoutUtils::DrawString(this, aFontMetrics, &aRenderingContext, str.get(),
                              str.Length(), nsPoint(x, y + aAscent), nullptr,
                              DrawStringFlags::ForceHorizontal);
    aRenderingContext.Restore();
  }
}

class nsDisplayHeaderFooter final : public nsPaintedDisplayItem {
 public:
  nsDisplayHeaderFooter(nsDisplayListBuilder* aBuilder, nsPageFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayHeaderFooter);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayHeaderFooter)

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) override {
#ifdef DEBUG
    nsPageFrame* pageFrame = do_QueryFrame(mFrame);
    MOZ_ASSERT(pageFrame, "We should have an nsPageFrame");
#endif
    static_cast<nsPageFrame*>(mFrame)->PaintHeaderFooter(
        *aCtx, ToReferenceFrame(), false);
  }
  NS_DISPLAY_DECL_NAME("HeaderFooter", TYPE_HEADER_FOOTER)

  virtual nsRect GetComponentAlphaBounds(
      nsDisplayListBuilder* aBuilder) const override {
    bool snap;
    return GetBounds(aBuilder, &snap);
  }
};

static void PaintMarginGuides(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                              const nsRect& aDirtyRect, nsPoint aPt) {
  // Set up parameters needed to draw the guides: we draw them in blue,
  // using 2px-long dashes with 2px separation and a line width of 0.5px.
  // Drawing is antialiased, so on a non-hidpi screen where the line width is
  // less than one device pixel, it doesn't disappear but renders fainter
  // than a solid 1px-wide line would be.
  // (In many cases, the entire preview is scaled down so that the guides
  // will be nominally less than 1 dev px even on a hidpi screen, resulting
  // in lighter antialiased rendering so they don't dominate the page.)
  ColorPattern pattern(ToDeviceColor(sRGBColor(0.0f, 0.0f, 1.0f)));
  Float dashes[] = {2.0f, 2.0f};
  StrokeOptions stroke(/* line width (in CSS px) */ 0.5f,
                       JoinStyle::MITER_OR_BEVEL, CapStyle::BUTT,
                       /* mitre limit (default, not used) */ 10.0f,
                       /* set dash pattern of 2px stroke, 2px gap */
                       ArrayLength(dashes), dashes,
                       /* dash offset */ 0.0f);
  DrawOptions options;

  MOZ_RELEASE_ASSERT(aFrame->IsPageFrame());
  const nsMargin& margin =
      static_cast<nsPageFrame*>(aFrame)->GetUsedPageContentMargin();
  int32_t appUnitsPerDevPx = aFrame->PresContext()->AppUnitsPerDevPixel();

  // Get the frame's rect and inset by the margins to get the edges of the
  // content area, where we want to draw the guides.
  // We draw in two stages, first applying the top/bottom margins and drawing
  // the horizontal guides across the full width of the page.
  nsRect rect(aPt, aFrame->GetSize());
  rect.Deflate(nsMargin(margin.top, 0, margin.bottom, 0));
  Rect r = NSRectToRect(rect, appUnitsPerDevPx);
  aDrawTarget->StrokeLine(r.TopLeft(), r.TopRight(), pattern, stroke, options);
  aDrawTarget->StrokeLine(r.BottomLeft(), r.BottomRight(), pattern, stroke,
                          options);

  // Then reset rect, apply the left/right margins, and draw vertical guides
  // extending the full height of the page.
  rect = nsRect(aPt, aFrame->GetSize());
  rect.Deflate(nsMargin(0, margin.right, 0, margin.left));
  r = NSRectToRect(rect, appUnitsPerDevPx);
  aDrawTarget->StrokeLine(r.TopLeft(), r.BottomLeft(), pattern, stroke,
                          options);
  aDrawTarget->StrokeLine(r.TopRight(), r.BottomRight(), pattern, stroke,
                          options);
}

static std::tuple<uint32_t, uint32_t> GetRowAndColFromIdx(uint32_t aIdxOnSheet,
                                                          uint32_t aNumCols) {
  // Compute the row index by *dividing* the item's ordinal position by how
  // many items fit in each row (i.e. the number of columns), and flooring.
  // Compute the column index by getting the remainder of that division:
  // Notably, mNumRows is irrelevant to this computation; that's because
  // we're adding new items column-by-column rather than row-by-row.
  return {aIdxOnSheet / aNumCols, aIdxOnSheet % aNumCols};
}

// Helper for BuildDisplayList:
static gfx::Matrix4x4 ComputePagesPerSheetAndPageSizeTransform(
    const nsIFrame* aFrame, float aAppUnitsPerPixel) {
  MOZ_ASSERT(aFrame->IsPageFrame());
  auto* pageFrame = static_cast<const nsPageFrame*>(aFrame);

  // Variables that we use in our transform (initialized with reasonable
  // defaults that work for the regular one-page-per-sheet scenario):
  float scale = 1.0f;
  nsPoint gridOrigin;
  uint32_t rowIdx = 0;
  uint32_t colIdx = 0;

  // Compute scaling due to a possible mismatch in the paper size we are
  // printing to (from the pres context) and the specified page size when the
  // content uses "@page {size: ...}" to specify a page size for the content.
  const nsSize actualPaperSize = pageFrame->PresContext()->GetPageSize();
  const nsSize contentPageSize = pageFrame->ComputePageSize();
  {
    nscoord contentPageHeight = contentPageSize.height;
    // Scale down if the target is too wide.
    if (contentPageSize.width > actualPaperSize.width) {
      scale *= float(actualPaperSize.width) / float(contentPageSize.width);
      contentPageHeight = NSToCoordRound(contentPageHeight * scale);
    }
    // Scale down if the target is too tall.
    if (contentPageHeight > actualPaperSize.height) {
      scale *= float(actualPaperSize.height) / float(contentPageHeight);
    }
  }
  MOZ_ASSERT(
      scale <= 1.0f,
      "Page-size mismatches should only have caused us to scale down, not up.");

  if (nsSharedPageData* pd = pageFrame->GetSharedPageData()) {
    const auto* ppsInfo = pd->PagesPerSheetInfo();
    if (ppsInfo->mNumPages > 1) {
      scale *= pd->mPagesPerSheetScale;
      gridOrigin = pd->mPagesPerSheetGridOrigin;
      std::tie(rowIdx, colIdx) = GetRowAndColFromIdx(pageFrame->IndexOnSheet(),
                                                     pd->mPagesPerSheetNumCols);
    }
  }

  // Scale down the page based on the above-computed scale:
  auto transform = gfx::Matrix4x4::Scaling(scale, scale, 1);

  // Draw the page at an offset, to get it in its pages-per-sheet "cell":
  transform.PreTranslate(
      NSAppUnitsToFloatPixels(colIdx * contentPageSize.width,
                              aAppUnitsPerPixel),
      NSAppUnitsToFloatPixels(rowIdx * contentPageSize.height,
                              aAppUnitsPerPixel),
      0);

  // Also add the grid origin as an offset (so that we're not drawing into the
  // sheet's unwritable area). Note that this is a PostTranslate operation
  // (vs. PreTranslate above), since gridOrigin is an offset on the sheet
  // itself, whereas the offset above was in the scaled coordinate space of the
  // pages.
  return transform.PostTranslate(
      NSAppUnitsToFloatPixels(gridOrigin.x, aAppUnitsPerPixel),
      NSAppUnitsToFloatPixels(gridOrigin.y, aAppUnitsPerPixel), 0);
}

nsIFrame::ComputeTransformFunction nsPageFrame::GetTransformGetter() const {
  return ComputePagesPerSheetAndPageSizeTransform;
}

nsPageContentFrame* nsPageFrame::PageContentFrame() const {
  nsIFrame* const frame = mFrames.FirstChild();
  MOZ_ASSERT(frame, "pageFrame must have one child");
  MOZ_ASSERT(frame->IsPageContentFrame(),
             "pageFrame must have pageContentFrame as the first child");
  return static_cast<nsPageContentFrame*>(frame);
}

nsSize nsPageFrame::ComputePageSize() const {
  const nsPageContentFrame* const pcf = PageContentFrame();
  nsSize size = PresContext()->GetPageSize();

  // Compute the expected page-size.
  const nsStylePage* const stylePage = pcf->StylePage();
  const StylePageSize& pageSize = stylePage->mSize;

  if (pageSize.IsSize()) {
    // Use the specified size
    return nsSize{pageSize.AsSize().width.ToAppUnits(),
                  pageSize.AsSize().height.ToAppUnits()};
  }
  if (pageSize.IsOrientation()) {
    // Ensure the correct orientation is applied.
    if (pageSize.AsOrientation() == StyleOrientation::Portrait) {
      if (size.width > size.height) {
        std::swap(size.width, size.height);
      }
    } else {
      MOZ_ASSERT(pageSize.AsOrientation() == StyleOrientation::Landscape);
      if (size.width < size.height) {
        std::swap(size.width, size.height);
      }
    }
  } else {
    MOZ_ASSERT(pageSize.IsAuto(), "Impossible page-size value?");
  }
  return size;
}

void nsPageFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                   const nsDisplayListSet& aLists) {
  nsDisplayList content;
  nsDisplayListSet set(&content, &content, &content, &content, &content,
                       &content);
  {
    DisplayListClipState::AutoSaveRestore clipState(aBuilder);
    clipState.Clear();

    // We need to extend the building rect to include the specified page size
    // (scaled by the print scaling factor), in case it is larger than the
    // physical page size. In that case the nsPageFrame will be the size of the
    // physical page, but the child nsPageContentFrame will be the larger
    // specified page size. The more correct way to do this would be to fully
    // reverse the result of ComputePagesPerSheetAndPageSizeTransform to handle
    // this scaling, but this should have the same result and is easier.
    nsPresContext* const pc = PresContext();
    const float scale = pc->GetPageScale();
    const nsSize pageSize = ComputePageSize();
    const nsRect scaledPageRect{0, 0, NSToCoordCeil(pageSize.width / scale),
                                NSToCoordCeil(pageSize.height / scale)};
    nsDisplayListBuilder::AutoBuildingDisplayList buildingForPageContentFrame(
        aBuilder, this, scaledPageRect, scaledPageRect);

    nsContainerFrame::BuildDisplayList(aBuilder, set);

    if (pc->IsRootPaginatedDocument()) {
      content.AppendNewToTop<nsDisplayHeaderFooter>(aBuilder, this);

      // For print-preview, show margin guides if requested in the settings.
      if (pc->Type() == nsPresContext::eContext_PrintPreview &&
          mPD->mPrintSettings->GetShowMarginGuides()) {
        content.AppendNewToTop<nsDisplayGeneric>(
            aBuilder, this, PaintMarginGuides, "MarginGuides",
            DisplayItemType::TYPE_MARGIN_GUIDES);
      }
    }
  }

  // We'll be drawing the page with a (usually-trivial)
  // N-pages-per-sheet transform applied, so our passed-in visible rect
  // isn't meaningful while we're drawing our children, because the
  // transform could scale down content whose coordinates are off-screen
  // such that it ends up on-screen. So: we temporarily update the visible
  // rect to be the child nsPageFrame's whole frame-rect (represented in
  // this PrintedSheetFrame's coordinate space.
  content.AppendNewToTop<nsDisplayTransform>(
      aBuilder, this, &content, content.GetBuildingRect(),
      nsDisplayTransform::WithTransformGetter);

  set.MoveTo(aLists);
}

//------------------------------------------------------------------------------
void nsPageFrame::DeterminePageNum() {
  // If we have no previous continuation, we're page 1. Otherwise, we're
  // just one more than our previous continuation's page number.
  auto* prevContinuation = static_cast<nsPageFrame*>(GetPrevContinuation());
  mPageNum = prevContinuation ? prevContinuation->GetPageNum() + 1 : 1;
}

void nsPageFrame::PaintHeaderFooter(gfxContext& aRenderingContext, nsPoint aPt,
                                    bool aDisableSubpixelAA) {
  nsPresContext* pc = PresContext();

  nsRect rect(aPt, ComputePageSize());
  aRenderingContext.SetColor(sRGBColor::OpaqueBlack());

  DrawTargetAutoDisableSubpixelAntialiasing disable(
      aRenderingContext.GetDrawTarget(), aDisableSubpixelAA);

  // Get the FontMetrics to determine width.height of strings
  nsFontMetrics::Params params;
  params.userFontSet = pc->GetUserFontSet();
  params.textPerf = pc->GetTextPerfMetrics();
  params.featureValueLookup = pc->GetFontFeatureValuesLookup();
  RefPtr<nsFontMetrics> fontMet = pc->GetMetricsFor(mPD->mHeadFootFont, params);

  nscoord ascent = fontMet->MaxAscent();
  nscoord visibleHeight = fontMet->MaxHeight();

  // print document headers and footers
  nsString headerLeft, headerCenter, headerRight;
  mPD->mPrintSettings->GetHeaderStrLeft(headerLeft);
  mPD->mPrintSettings->GetHeaderStrCenter(headerCenter);
  mPD->mPrintSettings->GetHeaderStrRight(headerRight);
  DrawHeaderFooter(aRenderingContext, *fontMet, eHeader, headerLeft,
                   headerCenter, headerRight, rect, ascent, visibleHeight);

  nsString footerLeft, footerCenter, footerRight;
  mPD->mPrintSettings->GetFooterStrLeft(footerLeft);
  mPD->mPrintSettings->GetFooterStrCenter(footerCenter);
  mPD->mPrintSettings->GetFooterStrRight(footerRight);
  DrawHeaderFooter(aRenderingContext, *fontMet, eFooter, footerLeft,
                   footerCenter, footerRight, rect, ascent, visibleHeight);
}

void nsPageFrame::SetSharedPageData(nsSharedPageData* aPD) {
  mPD = aPD;
  // Set the shared data into the page frame before reflow
  PageContentFrame()->SetSharedPageData(mPD);
}

nsIFrame* NS_NewPageBreakFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  MOZ_ASSERT(aPresShell, "null PresShell");
  // check that we are only creating page break frames when printing
  NS_ASSERTION(aPresShell->GetPresContext()->IsPaginated(),
               "created a page break frame while not printing");

  return new (aPresShell)
      nsPageBreakFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsPageBreakFrame)

nsPageBreakFrame::nsPageBreakFrame(ComputedStyle* aStyle,
                                   nsPresContext* aPresContext)
    : nsLeafFrame(aStyle, aPresContext, kClassID), mHaveReflowed(false) {}

nsPageBreakFrame::~nsPageBreakFrame() = default;

nscoord nsPageBreakFrame::GetIntrinsicISize() {
  return nsPresContext::CSSPixelsToAppUnits(1);
}

nscoord nsPageBreakFrame::GetIntrinsicBSize() { return 0; }

void nsPageBreakFrame::Reflow(nsPresContext* aPresContext,
                              ReflowOutput& aReflowOutput,
                              const ReflowInput& aReflowInput,
                              nsReflowStatus& aStatus) {
  DO_GLOBAL_REFLOW_COUNT("nsPageBreakFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aReflowOutput, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  // Override reflow, since we don't want to deal with what our
  // computed values are.
  WritingMode wm = aReflowInput.GetWritingMode();
  nscoord bSize = aReflowInput.AvailableBSize();
  if (aReflowInput.AvailableBSize() == NS_UNCONSTRAINEDSIZE) {
    bSize = nscoord(0);
  } else if (GetContent()->IsHTMLElement(nsGkAtoms::legend)) {
    // If this is a page break frame for a _rendered legend_ then it should be
    // ignored since these frames are inserted inside the fieldset's inner
    // frame and thus "misplaced".  nsFieldSetFrame::Reflow deals with these
    // forced breaks explicitly instead.
    nsContainerFrame* parent = GetParent();
    if (parent &&
        parent->Style()->GetPseudoType() == PseudoStyleType::fieldsetContent) {
      while ((parent = parent->GetParent())) {
        if (nsFieldSetFrame* fieldset = do_QueryFrame(parent)) {
          auto* legend = fieldset->GetLegend();
          if (legend && legend->GetContent() == GetContent()) {
            bSize = nscoord(0);
          }
          break;
        }
      }
    }
  }
  LogicalSize finalSize(wm, GetIntrinsicISize(), bSize);
  // round the height down to the nearest pixel
  // XXX(mats) why???
  finalSize.BSize(wm) -=
      finalSize.BSize(wm) % nsPresContext::CSSPixelsToAppUnits(1);
  aReflowOutput.SetSize(wm, finalSize);

  // Note: not using NS_FRAME_FIRST_REFLOW here, since it's not clear whether
  // DidReflow will always get called before the next Reflow() call.
  mHaveReflowed = true;
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsPageBreakFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"PageBreak"_ns, aResult);
}
#endif
