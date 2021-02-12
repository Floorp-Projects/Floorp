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
  // XXX Pay attention to the page's border and padding...
  //
  // Reflow our ::-moz-page-content frame, allowing it only to be as big as we
  // are (minus margins).
  if (mFrames.IsEmpty()) {
    return {};
  }

  nsIFrame* frame = mFrames.FirstChild();
  nsSize maxSize = aPresContext->GetPageSize();
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

  NS_ASSERTION(
      mFrames.FirstChild() && mFrames.FirstChild()->IsPageContentFrame(),
      "pageFrame must have a pageContentFrame child");

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
  nscoord strSpace = aRect.width / numStrs;

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
  nscoord contentWidth =
      aWidth - (mPD->mEdgePaperMargin.left + mPD->mEdgePaperMargin.right);

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
    if (nsLayoutUtils::BinarySearchForPosition(
            drawTarget, aFontMetrics, text, 0, 0, 0, len, int32_t(contentWidth),
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

/**
 * Remove all leaf display items that are not for descendants of
 * aBuilder->GetReferenceFrame() from aList.
 * @param aPage the page we're constructing the display list for
 * @param aExtraPage the page we constructed aList for
 * @param aList the list that is modified in-place
 */
static void PruneDisplayListForExtraPage(nsDisplayListBuilder* aBuilder,
                                         nsPageFrame* aPage,
                                         nsIFrame* aExtraPage,
                                         nsDisplayList* aList) {
  nsDisplayList newList;

  while (true) {
    nsDisplayItem* i = aList->RemoveBottom();
    if (!i) break;
    nsDisplayList* subList = i->GetSameCoordinateSystemChildren();
    if (subList) {
      PruneDisplayListForExtraPage(aBuilder, aPage, aExtraPage, subList);
      i->UpdateBounds(aBuilder);
    } else {
      nsIFrame* f = i->Frame();
      if (!nsLayoutUtils::IsProperAncestorFrameCrossDoc(aPage, f)) {
        // We're throwing this away so call its destructor now. The memory
        // is owned by aBuilder which destroys all items at once.
        i->Destroy(aBuilder);
        continue;
      }
    }
    newList.AppendToTop(i);
  }
  aList->AppendToTop(&newList);
}

static void BuildDisplayListForExtraPage(nsDisplayListBuilder* aBuilder,
                                         nsPageFrame* aPage,
                                         nsIFrame* aExtraPage,
                                         nsDisplayList* aList) {
  // The only content in aExtraPage we care about is out-of-flow content from
  // aPage, whose placeholders have occurred in aExtraPage. If
  // NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO is not set, then aExtraPage has
  // no such content.
  if (!aExtraPage->HasAnyStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO)) {
    return;
  }
  nsDisplayList list;
  aExtraPage->BuildDisplayListForStackingContext(aBuilder, &list);
  PruneDisplayListForExtraPage(aBuilder, aPage, aExtraPage, &list);
  aList->AppendToTop(&list);
}

static gfx::Matrix4x4 ComputePageTransform(nsIFrame* aFrame,
                                           float aAppUnitsPerPixel) {
  float scale = aFrame->PresContext()->GetPageScale();
  return gfx::Matrix4x4::Scaling(scale, scale, 1);
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
        *aCtx, ToReferenceFrame(), IsSubpixelAADisabled());
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

//------------------------------------------------------------------------------
using PageAndOffset = std::pair<nsPageContentFrame*, nscoord>;

// Returns the previous continuation PageContentFrames that have overflow areas,
// and their offsets to the top of the given PageContentFrame |aPage|. Since the
// iteration is done backwards, the returned pages are arranged in descending
// order of page number.
static nsTArray<PageAndOffset> GetPreviousPagesWithOverflow(
    nsPageContentFrame* aPage) {
  nsTArray<PageAndOffset> pages(8);

  auto GetPreviousPageContentFrame = [](nsPageContentFrame* aPageCF) {
    nsIFrame* prevCont = aPageCF->GetPrevContinuation();
    MOZ_ASSERT(!prevCont || prevCont->IsPageContentFrame(),
               "Expected nsPageContentFrame or nullptr");

    return static_cast<nsPageContentFrame*>(prevCont);
  };

  nsPageContentFrame* pageCF = aPage;
  // The collective height of all prev-continuations we've traversed so far:
  nscoord offsetToCurrentPageBStart = 0;
  const auto wm = pageCF->GetWritingMode();
  while ((pageCF = GetPreviousPageContentFrame(pageCF))) {
    offsetToCurrentPageBStart += pageCF->BSize(wm);

    if (pageCF->HasOverflowAreas()) {
      pages.EmplaceBack(pageCF, offsetToCurrentPageBStart);
    }
  }

  return pages;
}

static void BuildPreviousPageOverflow(nsDisplayListBuilder* aBuilder,
                                      nsPageFrame* aPageFrame,
                                      nsPageContentFrame* aCurrentPageCF,
                                      const nsDisplayListSet& aLists) {
  const auto previousPagesAndOffsets =
      GetPreviousPagesWithOverflow(aCurrentPageCF);

  const auto wm = aCurrentPageCF->GetWritingMode();
  for (const PageAndOffset& pair : Reversed(previousPagesAndOffsets)) {
    auto* prevPageCF = pair.first;
    const nscoord offsetToCurrentPageBStart = pair.second;
    // Only scrollable overflow create new pages, not ink overflow.
    const LogicalRect scrollableOverflow(
        wm, prevPageCF->ScrollableOverflowRectRelativeToSelf(),
        prevPageCF->GetSize());
    const auto remainingOverflow =
        scrollableOverflow.BEnd(wm) - offsetToCurrentPageBStart;
    if (remainingOverflow <= 0) {
      continue;
    }

    // This rect represents the piece of prevPageCF's overflow that ends up on
    // the current pageContentFrame (in prevPageCF's coordinate system).
    // Note that we use InkOverflow here since this is for painting.
    LogicalRect overflowRect(wm, prevPageCF->InkOverflowRectRelativeToSelf(),
                             prevPageCF->GetSize());
    overflowRect.BStart(wm) = offsetToCurrentPageBStart;
    overflowRect.BSize(wm) = std::min(remainingOverflow, prevPageCF->BSize(wm));

    {
      // Convert the overflowRect to the coordinate system of aPageFrame, and
      // set it as the visible rect for display list building.
      const nsRect visibleRect =
          overflowRect.GetPhysicalRect(wm, aPageFrame->GetSize()) +
          prevPageCF->GetOffsetTo(aPageFrame);
      nsDisplayListBuilder::AutoBuildingDisplayList buildingForChild(
          aBuilder, aPageFrame, visibleRect, visibleRect);

      // This part is tricky. Because display items are positioned based on the
      // frame tree, building a display list for the previous page yields
      // display items that are outside of the current page bounds.
      // To fix that, an additional reference frame offset is added, which
      // shifts the display items down (block axis) as if the current and
      // previous page were one long page in the same coordinate system.
      const nsSize containerSize = aPageFrame->GetSize();
      LogicalPoint pageOffset(wm, aCurrentPageCF->GetOffsetTo(prevPageCF),
                              containerSize);
      pageOffset.B(wm) -= offsetToCurrentPageBStart;
      buildingForChild.SetAdditionalOffset(
          pageOffset.GetPhysicalPoint(wm, containerSize));

      aPageFrame->BuildDisplayListForChild(aBuilder, prevPageCF, aLists);
    }
  }
}

void nsPageFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                   const nsDisplayListSet& aLists) {
  nsDisplayListCollection set(aBuilder);

  nsPresContext* pc = PresContext();

  nsIFrame* child = mFrames.FirstChild();
  float scale = pc->GetPageScale();
  nsRect clipRect(nsPoint(0, 0), child->GetSize());
  // Note: this computation matches how we compute maxSize.height
  // in nsPageFrame::Reflow
  nscoord expectedPageContentHeight = NSToCoordCeil(GetSize().height / scale);
  if (clipRect.height > expectedPageContentHeight) {
    // We're doing print-selection, with one long page-content frame.
    // Clip to the appropriate page-content slice for the current page.
    NS_ASSERTION(mPageNum > 0, "page num should be positive");
    // Note: The pageContentFrame's y-position has been set such that a zero
    // y-value matches the top edge of the current page.  So, to clip to the
    // current page's content (in coordinates *relative* to the page content
    // frame), we just negate its y-position and add the top margin.
    //
    // FIXME(emilio): Rather than the default margin, this should probably use
    // mPageContentMargin?
    clipRect.y = NSToCoordCeil(
        (-child->GetRect().y + pc->GetDefaultPageMargin().top) / scale);
    clipRect.height = expectedPageContentHeight;
    NS_ASSERTION(clipRect.y < child->GetSize().height,
                 "Should be clipping to region inside the page content bounds");
  }
  clipRect += aBuilder->ToReferenceFrame(child);

  nsDisplayList content;
  {
    DisplayListClipState::AutoSaveRestore clipState(aBuilder);

    // Overwrite current clip, since we're going to wrap in a transform
    // and the current clip is no longer meaningful.
    clipState.Clear();
    clipState.ClipContainingBlockDescendants(clipRect);

    MOZ_ASSERT(child->IsPageContentFrame(), "unexpected child frame type");
    auto* currentPageCF = static_cast<nsPageContentFrame*>(child);

    if (StaticPrefs::layout_display_list_improve_fragmentation() &&
        mPageNum <= 255) {
      nsDisplayListBuilder::AutoPageNumberSetter p(aBuilder, mPageNum);
      BuildPreviousPageOverflow(aBuilder, this, currentPageCF, set);
    }

    // Set the visible rect to scrollable overflow rect of the child
    // nsPageContentFrame in parent nsPageFrame coordinate space.
    const nsRect childOverflowRect =
        child->ScrollableOverflowRectRelativeToSelf();
    const nsRect visibleRect = childOverflowRect + child->GetOffsetTo(this);

    nsDisplayListBuilder::AutoBuildingDisplayList buildingForChild(
        aBuilder, this, visibleRect, visibleRect);
    BuildDisplayListForChild(aBuilder, child, set);

    set.SerializeWithCorrectZOrder(&content, child->GetContent());

    // We may need to paint out-of-flow frames whose placeholders are
    // on other pages. Add those pages to our display list. Note that
    // out-of-flow frames can't be placed after their placeholders so
    // we don't have to process earlier pages. The display lists for
    // these extra pages are pruned so that only display items for the
    // page we currently care about (which we would have reached by
    // following placeholders to their out-of-flows) end up on the list.
    //
    // Stacking context frames that wrap content on their normal page,
    // as well as OOF content for this page will have their container
    // items duplicated. We tell the builder to include our page number
    // in the unique key for any extra page items so that they can be
    // differentiated from the ones created on the normal page.
    NS_ASSERTION(mPageNum <= 255, "Too many pages to handle OOFs");
    if (mPageNum <= 255) {
      nsDisplayListBuilder::AutoPageNumberSetter p(aBuilder, mPageNum);
      // The static_cast here is technically unnecessary, but it helps
      // devirtualize the GetNextContinuation() function call if pcf has a
      // concrete type (with an inherited `final` GetNextContinuation() impl).
      auto* pageCF = currentPageCF;
      while ((pageCF = static_cast<nsPageContentFrame*>(
                  pageCF->GetNextContinuation()))) {
        nsRect childVisible = childOverflowRect + child->GetOffsetTo(pageCF);

        nsDisplayListBuilder::AutoBuildingDisplayList buildingForChild(
            aBuilder, pageCF, childVisible, childVisible);
        BuildDisplayListForExtraPage(aBuilder, this, pageCF, &content);
      }
    }

    // Invoke AutoBuildingDisplayList to ensure that the correct visibleRect
    // is used to compute the visible rect if AddCanvasBackgroundColorItem
    // creates a display item.
    nsDisplayListBuilder::AutoBuildingDisplayList building(
        aBuilder, child, childOverflowRect, childOverflowRect);

    // Add the canvas background color to the bottom of the list. This
    // happens after we've built the list so that AddCanvasBackgroundColorItem
    // can monkey with the contents if necessary.
    nsRect backgroundRect =
        nsRect(aBuilder->ToReferenceFrame(child), child->GetSize());

    pc->GetPresShell()->AddCanvasBackgroundColorItem(
        aBuilder, &content, child, backgroundRect, NS_RGBA(0, 0, 0, 0));
  }

  content.AppendNewToTop<nsDisplayTransform>(aBuilder, child, &content,
                                             content.GetBuildingRect(),
                                             ::ComputePageTransform);

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

  aLists.Content()->AppendToTop(&content);
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

  nsRect rect(aPt, mRect.Size());
  aRenderingContext.SetColor(sRGBColor::OpaqueBlack());

  DrawTargetAutoDisableSubpixelAntialiasing disable(
      aRenderingContext.GetDrawTarget(), aDisableSubpixelAA);

  // Get the FontMetrics to determine width.height of strings
  nsFontMetrics::Params params;
  params.userFontSet = pc->GetUserFontSet();
  params.textPerf = pc->GetTextPerfMetrics();
  params.fontStats = pc->GetFontMatchingStats();
  params.featureValueLookup = pc->GetFontFeatureValuesLookup();
  RefPtr<nsFontMetrics> fontMet =
      pc->DeviceContext()->GetMetricsFor(mPD->mHeadFootFont, params);

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
  nsPageContentFrame* pcf =
      static_cast<nsPageContentFrame*>(mFrames.FirstChild());
  if (pcf) {
    pcf->SetSharedPageData(mPD);
  }
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
