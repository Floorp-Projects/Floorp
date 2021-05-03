/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsPageContentFrame.h"

#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_layout.h"

#include "nsContentUtils.h"
#include "nsPageFrame.h"
#include "nsCSSFrameConstructor.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "nsPageSequenceFrame.h"

using namespace mozilla;

nsPageContentFrame* NS_NewPageContentFrame(PresShell* aPresShell,
                                           ComputedStyle* aStyle) {
  return new (aPresShell)
      nsPageContentFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsPageContentFrame)

void nsPageContentFrame::Reflow(nsPresContext* aPresContext,
                                ReflowOutput& aReflowOutput,
                                const ReflowInput& aReflowInput,
                                nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsPageContentFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aReflowOutput, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  MOZ_ASSERT(mPD, "Need a pointer to nsSharedPageData before reflow starts");

  if (GetPrevInFlow() && HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    nsresult rv =
        aPresContext->PresShell()->FrameConstructor()->ReplicateFixedFrames(
            this);
    if (NS_FAILED(rv)) {
      return;
    }
  }

  // Set our size up front, since some parts of reflow depend on it
  // being already set.  Note that the computed height may be
  // unconstrained; that's ok.  Consumers should watch out for that.
  nsSize maxSize(aReflowInput.ComputedWidth(), aReflowInput.ComputedHeight());
  SetSize(maxSize);

  WritingMode wm = aReflowInput.GetWritingMode();
  aReflowOutput.ISize(wm) = aReflowInput.ComputedISize();
  if (aReflowInput.ComputedBSize() != NS_UNCONSTRAINEDSIZE) {
    aReflowOutput.BSize(wm) = aReflowInput.ComputedBSize();
  }
  aReflowOutput.SetOverflowAreasToDesiredBounds();

  // A PageContentFrame must always have one child: the canvas frame.
  // Resize our frame allowing it only to be as big as we are
  // XXX Pay attention to the page's border and padding...
  if (mFrames.NotEmpty()) {
    nsIFrame* frame = mFrames.FirstChild();
    WritingMode wm = frame->GetWritingMode();
    LogicalSize logicalSize(wm, maxSize);
    ReflowInput kidReflowInput(aPresContext, aReflowInput, frame, logicalSize);
    kidReflowInput.SetComputedBSize(logicalSize.BSize(wm));
    ReflowOutput kidReflowOutput(kidReflowInput);
    ReflowChild(frame, aPresContext, kidReflowOutput, kidReflowInput, 0, 0,
                ReflowChildFlags::Default, aStatus);

    // The document element's background should cover the entire canvas, so
    // take into account the combined area and any space taken up by
    // absolutely positioned elements
    nsMargin padding(0, 0, 0, 0);

    // XXXbz this screws up percentage padding (sets padding to zero
    // in the percentage padding case)
    kidReflowInput.mStylePadding->GetPadding(padding);

    // This is for shrink-to-fit, and therefore we want to use the
    // scrollable overflow, since the purpose of shrink to fit is to
    // make the content that ought to be reachable (represented by the
    // scrollable overflow) fit in the page.
    if (frame->HasOverflowAreas()) {
      // The background covers the content area and padding area, so check
      // for children sticking outside the child frame's padding edge
      nscoord xmost = kidReflowOutput.ScrollableOverflow().XMost();
      if (xmost > kidReflowOutput.Width()) {
        nscoord widthToFit =
            xmost + padding.right +
            kidReflowInput.mStyleBorder->GetComputedBorderWidth(eSideRight);
        float ratio = float(maxSize.width) / widthToFit;
        NS_ASSERTION(ratio >= 0.0 && ratio < 1.0,
                     "invalid shrink-to-fit ratio");
        mPD->mShrinkToFitRatio = std::min(mPD->mShrinkToFitRatio, ratio);
      }
      // In the case of pdf.js documents, we also want to consider the height,
      // so that we don't clip the page in either axis if the aspect ratio of
      // the PDF doesn't match the destination.
      if (nsContentUtils::IsPDFJS(PresContext()->Document()->GetPrincipal())) {
        nscoord ymost = kidReflowOutput.ScrollableOverflow().YMost();
        if (ymost > kidReflowOutput.Height()) {
          nscoord heightToFit =
              ymost + padding.bottom +
              kidReflowInput.mStyleBorder->GetComputedBorderWidth(eSideBottom);
          float ratio = float(maxSize.height) / heightToFit;
          MOZ_ASSERT(ratio >= 0.0 && ratio < 1.0);
          mPD->mShrinkToFitRatio = std::min(mPD->mShrinkToFitRatio, ratio);
        }

        // pdf.js pages should never overflow given the scaling above.
        // nsPrintJob::SetupToPrintContent ignores some ratios close to 1.0
        // though and doesn't reflow us again in that case, so we need to clear
        // the overflow area here in case that happens. (bug 1689789)
        frame->ClearOverflowRects();
        kidReflowOutput.mOverflowAreas = aReflowOutput.mOverflowAreas;
      }
    }

    // Place and size the child
    FinishReflowChild(frame, aPresContext, kidReflowOutput, &kidReflowInput, 0,
                      0, ReflowChildFlags::Default);

    NS_ASSERTION(aPresContext->IsDynamic() || !aStatus.IsFullyComplete() ||
                     !frame->GetNextInFlow(),
                 "bad child flow list");

    aReflowOutput.mOverflowAreas.UnionWith(kidReflowOutput.mOverflowAreas);
  }

  FinishAndStoreOverflow(&aReflowOutput);

  // Reflow our fixed frames
  nsReflowStatus fixedStatus;
  ReflowAbsoluteFrames(aPresContext, aReflowOutput, aReflowInput, fixedStatus);
  NS_ASSERTION(fixedStatus.IsComplete(),
               "fixed frames can be truncated, but not incomplete");

  if (StaticPrefs::layout_display_list_improve_fragmentation() &&
      mFrames.NotEmpty()) {
    auto* previous = static_cast<nsPageContentFrame*>(GetPrevContinuation());
    const nscoord previousPageOverflow =
        previous ? previous->mRemainingOverflow : 0;
    const nsSize containerSize(aReflowInput.AvailableWidth(),
                               aReflowInput.AvailableHeight());
    const nscoord pageBSize = GetLogicalRect(containerSize).BSize(wm);
    const nscoord overflowBSize =
        LogicalRect(wm, ScrollableOverflowRect(), GetSize()).BEnd(wm);
    const nscoord currentPageOverflow = overflowBSize - pageBSize;
    nscoord remainingOverflow =
        std::max(currentPageOverflow, previousPageOverflow - pageBSize);

    if (aStatus.IsFullyComplete() && remainingOverflow > 0) {
      // If we have ScrollableOverflow off the end of our page, then we report
      // ourselves as overflow-incomplete in order to produce an additional
      // content-less page, which we expect to draw our overflow on our behalf.
      aStatus.SetOverflowIncomplete();
    }

    mRemainingOverflow = std::max(remainingOverflow, 0);
  }

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aReflowOutput);
}

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

static gfx::Matrix4x4 ComputePageContentTransform(const nsIFrame* aFrame,
                                                  float aAppUnitsPerPixel) {
  float scale = aFrame->PresContext()->GetPageScale();
  return gfx::Matrix4x4::Scaling(scale, scale, 1);
}

nsIFrame::ComputeTransformFunction nsPageContentFrame::GetTransformGetter()
    const {
  return ComputePageContentTransform;
}

void nsPageContentFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                          const nsDisplayListSet& aLists) {
  MOZ_ASSERT(GetParent());
  MOZ_ASSERT(GetParent()->IsPageFrame());
  auto* pageFrame = static_cast<nsPageFrame*>(GetParent());
  auto pageNum = pageFrame->GetPageNum();
  NS_ASSERTION(pageNum <= 255, "Too many pages to handle OOFs");

  if (aBuilder->GetBuildingExtraPagesForPageNum()) {
    return mozilla::ViewportFrame::BuildDisplayList(aBuilder, aLists);
  }

  nsDisplayListCollection set(aBuilder);

  const nsRect clipRect(aBuilder->ToReferenceFrame(this), GetSize());
  DisplayListClipState::AutoSaveRestore clipState(aBuilder);

  // Overwrite current clip, since we're going to wrap in a transform and the
  // current clip is no longer meaningful.
  clipState.Clear();
  clipState.ClipContainingBlockDescendants(clipRect);

  if (StaticPrefs::layout_display_list_improve_fragmentation() &&
      pageNum <= 255) {
    nsDisplayListBuilder::AutoPageNumberSetter p(aBuilder, pageNum);
    BuildPreviousPageOverflow(aBuilder, pageFrame, this, set);
  }

  mozilla::ViewportFrame::BuildDisplayList(aBuilder, set);

  nsDisplayList content;
  set.SerializeWithCorrectZOrder(&content, GetContent());

  // We may need to paint out-of-flow frames whose placeholders are on other
  // pages. Add those pages to our display list. Note that out-of-flow frames
  // can't be placed after their placeholders so
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
  if (pageNum <= 255) {
    const nsRect overflowRect = ScrollableOverflowRectRelativeToSelf();
    nsDisplayListBuilder::AutoPageNumberSetter p(aBuilder, pageNum);

    // The static_cast here is technically unnecessary, but it helps
    // devirtualize the GetNextContinuation() function call if pcf has a
    // concrete type (with an inherited `final` GetNextContinuation() impl).
    auto* pageCF = this;
    while ((pageCF = static_cast<nsPageContentFrame*>(
                pageCF->GetNextContinuation()))) {
      nsRect childVisible = overflowRect + GetOffsetTo(pageCF);

      nsDisplayListBuilder::AutoBuildingDisplayList buildingForChild(
          aBuilder, pageCF, childVisible, childVisible);
      BuildDisplayListForExtraPage(aBuilder, pageFrame, pageCF, &content);
    }
  }

  // Add the canvas background color to the bottom of the list. This
  // happens after we've built the list so that AddCanvasBackgroundColorItem
  // can monkey with the contents if necessary.
  const nsRect backgroundRect(aBuilder->ToReferenceFrame(this), GetSize());
  PresShell()->AddCanvasBackgroundColorItem(
      aBuilder, &content, this, backgroundRect, NS_RGBA(0, 0, 0, 0));

  content.AppendNewToTop<nsDisplayTransform>(
      aBuilder, this, &content, content.GetBuildingRect(),
      nsDisplayTransform::WithTransformGetter);

  aLists.Content()->AppendToTop(&content);
}

void nsPageContentFrame::AppendDirectlyOwnedAnonBoxes(
    nsTArray<OwnedAnonBox>& aResult) {
  MOZ_ASSERT(mFrames.FirstChild(),
             "pageContentFrame must have a canvasFrame child");
  aResult.AppendElement(mFrames.FirstChild());
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsPageContentFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"PageContent"_ns, aResult);
}
#endif
