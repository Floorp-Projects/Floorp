/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/Likely.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/ResultExtensions.h"

#include "gfxUtils.h"
#include "nsAlgorithm.h"
#include "nsCOMPtr.h"
#include "nsFontMetrics.h"
#include "nsPresContext.h"
#include "nsNameSpaceManager.h"

#include "nsTreeBodyFrame.h"
#include "nsTreeSelection.h"
#include "nsTreeImageListener.h"

#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"

#include "gfxContext.h"
#include "nsIContent.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/dom/Document.h"
#include "nsCSSRendering.h"
#include "nsString.h"
#include "nsContainerFrame.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsVariant.h"
#include "nsWidgetsCID.h"
#include "nsIFrameInlines.h"
#include "nsBoxFrame.h"
#include "nsBoxLayoutState.h"
#include "nsTreeContentView.h"
#include "nsTreeUtils.h"
#include "nsStyleConsts.h"
#include "nsITheme.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsIScrollableFrame.h"
#include "nsDisplayList.h"
#include "mozilla/dom/CustomEvent.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/TreeColumnBinding.h"
#include <algorithm>
#include "ScrollbarActivity.h"

#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#  include "nsIWritablePropertyBag2.h"
#endif
#include "nsBidiUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;
using namespace mozilla::layout;

// Function that cancels all the image requests in our cache.
void nsTreeBodyFrame::CancelImageRequests() {
  for (auto iter = mImageCache.Iter(); !iter.Done(); iter.Next()) {
    // If our imgIRequest object was registered with the refresh driver
    // then we need to deregister it.
    nsTreeImageCacheEntry entry = iter.UserData();
    nsLayoutUtils::DeregisterImageRequest(PresContext(), entry.request,
                                          nullptr);
    entry.request->UnlockImage();
    entry.request->CancelAndForgetObserver(NS_BINDING_ABORTED);
  }
}

//
// NS_NewTreeFrame
//
// Creates a new tree frame
//
nsIFrame* NS_NewTreeBodyFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsTreeBodyFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsTreeBodyFrame)

NS_QUERYFRAME_HEAD(nsTreeBodyFrame)
  NS_QUERYFRAME_ENTRY(nsIScrollbarMediator)
  NS_QUERYFRAME_ENTRY(nsTreeBodyFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsLeafBoxFrame)

// Constructor
nsTreeBodyFrame::nsTreeBodyFrame(ComputedStyle* aStyle,
                                 nsPresContext* aPresContext)
    : nsLeafBoxFrame(aStyle, aPresContext, kClassID),
      mSlots(nullptr),
      mImageCache(),
      mTopRowIndex(0),
      mPageLength(0),
      mHorzPosition(0),
      mOriginalHorzWidth(-1),
      mHorzWidth(0),
      mAdjustWidth(0),
      mRowHeight(0),
      mIndentation(0),
      mStringWidth(-1),
      mUpdateBatchNest(0),
      mRowCount(0),
      mMouseOverRow(-1),
      mFocused(false),
      mHasFixedRowCount(false),
      mVerticalOverflow(false),
      mHorizontalOverflow(false),
      mReflowCallbackPosted(false),
      mCheckingOverflow(false) {
  mColumns = new nsTreeColumns(this);
}

// Destructor
nsTreeBodyFrame::~nsTreeBodyFrame() {
  CancelImageRequests();
  DetachImageListeners();
  delete mSlots;
}

static void GetBorderPadding(ComputedStyle* aStyle, nsMargin& aMargin) {
  aMargin.SizeTo(0, 0, 0, 0);
  aStyle->StylePadding()->GetPadding(aMargin);
  aMargin += aStyle->StyleBorder()->GetComputedBorder();
}

static void AdjustForBorderPadding(ComputedStyle* aStyle, nsRect& aRect) {
  nsMargin borderPadding(0, 0, 0, 0);
  GetBorderPadding(aStyle, borderPadding);
  aRect.Deflate(borderPadding);
}

void nsTreeBodyFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                           nsIFrame* aPrevInFlow) {
  nsLeafBoxFrame::Init(aContent, aParent, aPrevInFlow);

  mIndentation = GetIndentation();
  mRowHeight = GetRowHeight();

  // Call GetBaseElement so that mTree is assigned.
  GetBaseElement();

  if (LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars) != 0) {
    mScrollbarActivity =
        new ScrollbarActivity(static_cast<nsIScrollbarMediator*>(this));
  }
}

nsSize nsTreeBodyFrame::GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) {
  EnsureView();

  RefPtr<XULTreeElement> tree(GetBaseElement());

  nsSize min(0, 0);
  int32_t desiredRows;
  if (MOZ_UNLIKELY(!tree)) {
    desiredRows = 0;
  } else {
    nsAutoString rows;
    tree->GetAttr(kNameSpaceID_None, nsGkAtoms::rows, rows);
    if (!rows.IsEmpty()) {
      nsresult err;
      desiredRows = rows.ToInteger(&err);
      mPageLength = desiredRows;
    } else {
      desiredRows = 0;
    }
  }

  min.height = mRowHeight * desiredRows;

  AddXULBorderAndPadding(min);
  bool widthSet, heightSet;
  nsIFrame::AddXULMinSize(this, min, widthSet, heightSet);

  return min;
}

nscoord nsTreeBodyFrame::CalcMaxRowWidth() {
  if (mStringWidth != -1) return mStringWidth;

  if (!mView) return 0;

  ComputedStyle* rowContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeRow());
  nsMargin rowMargin(0, 0, 0, 0);
  GetBorderPadding(rowContext, rowMargin);

  nscoord rowWidth;
  nsTreeColumn* col;

  RefPtr<gfxContext> rc = PresShell()->CreateReferenceRenderingContext();

  for (int32_t row = 0; row < mRowCount; ++row) {
    rowWidth = 0;

    for (col = mColumns->GetFirstColumn(); col; col = col->GetNext()) {
      nscoord desiredWidth, currentWidth;
      nsresult rv = GetCellWidth(row, col, rc, desiredWidth, currentWidth);
      if (NS_FAILED(rv)) {
        MOZ_ASSERT_UNREACHABLE("invalid column");
        continue;
      }
      rowWidth += desiredWidth;
    }

    if (rowWidth > mStringWidth) mStringWidth = rowWidth;
  }

  mStringWidth += rowMargin.left + rowMargin.right;
  return mStringWidth;
}

void nsTreeBodyFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                  PostDestroyData& aPostDestroyData) {
  if (mScrollbarActivity) {
    mScrollbarActivity->Destroy();
    mScrollbarActivity = nullptr;
  }

  mScrollEvent.Revoke();
  // Make sure we cancel any posted callbacks.
  if (mReflowCallbackPosted) {
    PresShell()->CancelReflowCallback(this);
    mReflowCallbackPosted = false;
  }

  if (mColumns) mColumns->SetTree(nullptr);

  if (mTree) {
    mTree->BodyDestroyed(mTopRowIndex);
  }

  if (mView) {
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel) sel->SetTree(nullptr);
    mView->SetTree(nullptr);
    mView = nullptr;
  }

  nsLeafBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void nsTreeBodyFrame::EnsureView() {
  if (!mView) {
    if (PresShell()->IsReflowLocked()) {
      if (!mReflowCallbackPosted) {
        mReflowCallbackPosted = true;
        PresShell()->PostReflowCallback(this);
      }
      return;
    }

    AutoWeakFrame weakFrame(this);

    RefPtr<XULTreeElement> tree = GetBaseElement();
    if (tree) {
      nsCOMPtr<nsITreeView> treeView = tree->GetView();
      if (treeView && weakFrame.IsAlive()) {
        int32_t rowIndex = tree->GetCachedTopVisibleRow();

        // Set our view.
        SetView(treeView);
        NS_ENSURE_TRUE_VOID(weakFrame.IsAlive());

        // Scroll to the given row.
        // XXX is this optimal if we haven't laid out yet?
        ScrollToRow(rowIndex);
        NS_ENSURE_TRUE_VOID(weakFrame.IsAlive());
      }
    }
  }
}

void nsTreeBodyFrame::ManageReflowCallback(const nsRect& aRect,
                                           nscoord aHorzWidth) {
  if (!mReflowCallbackPosted &&
      (!aRect.IsEqualEdges(mRect) || mHorzWidth != aHorzWidth)) {
    PresShell()->PostReflowCallback(this);
    mReflowCallbackPosted = true;
    mOriginalHorzWidth = mHorzWidth;
  } else if (mReflowCallbackPosted && mHorzWidth != aHorzWidth &&
             mOriginalHorzWidth == aHorzWidth) {
    PresShell()->CancelReflowCallback(this);
    mReflowCallbackPosted = false;
    mOriginalHorzWidth = -1;
  }
}

void nsTreeBodyFrame::SetXULBounds(nsBoxLayoutState& aBoxLayoutState,
                                   const nsRect& aRect,
                                   bool aRemoveOverflowArea) {
  nscoord horzWidth = CalcHorzWidth(GetScrollParts());
  ManageReflowCallback(aRect, horzWidth);
  mHorzWidth = horzWidth;

  nsLeafBoxFrame::SetXULBounds(aBoxLayoutState, aRect, aRemoveOverflowArea);
}

bool nsTreeBodyFrame::ReflowFinished() {
  if (!mView) {
    AutoWeakFrame weakFrame(this);
    EnsureView();
    NS_ENSURE_TRUE(weakFrame.IsAlive(), false);
  }
  if (mView) {
    CalcInnerBox();
    ScrollParts parts = GetScrollParts();
    mHorzWidth = CalcHorzWidth(parts);
    if (!mHasFixedRowCount) {
      mPageLength =
          (mRowHeight > 0) ? (mInnerBox.height / mRowHeight) : mRowCount;
    }

    int32_t lastPageTopRow = std::max(0, mRowCount - mPageLength);
    if (mTopRowIndex > lastPageTopRow)
      ScrollToRowInternal(parts, lastPageTopRow);

    XULTreeElement* treeContent = GetBaseElement();
    if (treeContent && treeContent->AttrValueIs(
                           kNameSpaceID_None, nsGkAtoms::keepcurrentinview,
                           nsGkAtoms::_true, eCaseMatters)) {
      // make sure that the current selected item is still
      // visible after the tree changes size.
      nsCOMPtr<nsITreeSelection> sel;
      mView->GetSelection(getter_AddRefs(sel));
      if (sel) {
        int32_t currentIndex;
        sel->GetCurrentIndex(&currentIndex);
        if (currentIndex != -1) EnsureRowIsVisibleInternal(parts, currentIndex);
      }
    }

    if (!FullScrollbarsUpdate(false)) {
      return false;
    }
  }

  mReflowCallbackPosted = false;
  return false;
}

void nsTreeBodyFrame::ReflowCallbackCanceled() {
  mReflowCallbackPosted = false;
}

nsresult nsTreeBodyFrame::GetView(nsITreeView** aView) {
  *aView = nullptr;
  AutoWeakFrame weakFrame(this);
  EnsureView();
  NS_ENSURE_STATE(weakFrame.IsAlive());
  NS_IF_ADDREF(*aView = mView);
  return NS_OK;
}

nsresult nsTreeBodyFrame::SetView(nsITreeView* aView) {
  // First clear out the old view.
  if (mView) {
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel) sel->SetTree(nullptr);
    mView->SetTree(nullptr);

    // Only reset the top row index and delete the columns if we had an old
    // non-null view.
    mTopRowIndex = 0;
  }

  // Tree, meet the view.
  mView = aView;

  // Changing the view causes us to refetch our data.  This will
  // necessarily entail a full invalidation of the tree.
  Invalidate();

  RefPtr<XULTreeElement> treeContent = GetBaseElement();
  if (treeContent) {
#ifdef ACCESSIBILITY
    if (nsAccessibilityService* accService =
            PresShell::GetAccessibilityService()) {
      accService->TreeViewChanged(PresContext()->GetPresShell(), treeContent,
                                  mView);
    }
#endif  // #ifdef ACCESSIBILITY
    FireDOMEvent(NS_LITERAL_STRING("TreeViewChanged"), treeContent);
  }

  if (mView) {
    // Give the view a new empty selection object to play with, but only if it
    // doesn't have one already.
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel) {
      sel->SetTree(treeContent);
    } else {
      NS_NewTreeSelection(treeContent, getter_AddRefs(sel));
      mView->SetSelection(sel);
    }

    // View, meet the tree.
    AutoWeakFrame weakFrame(this);
    mView->SetTree(treeContent);
    NS_ENSURE_STATE(weakFrame.IsAlive());
    mView->GetRowCount(&mRowCount);

    if (!PresShell()->IsReflowLocked()) {
      // The scrollbar will need to be updated.
      FullScrollbarsUpdate(false);
    } else if (!mReflowCallbackPosted) {
      mReflowCallbackPosted = true;
      PresShell()->PostReflowCallback(this);
    }
  }

  return NS_OK;
}

nsresult nsTreeBodyFrame::SetFocused(bool aFocused) {
  if (mFocused != aFocused) {
    mFocused = aFocused;
    if (mView) {
      nsCOMPtr<nsITreeSelection> sel;
      mView->GetSelection(getter_AddRefs(sel));
      if (sel) sel->InvalidateSelection();
    }
  }
  return NS_OK;
}

nsresult nsTreeBodyFrame::GetTreeBody(Element** aElement) {
  // NS_ASSERTION(mContent, "no content, see bug #104878");
  if (!mContent) return NS_ERROR_NULL_POINTER;

  RefPtr<Element> element = mContent->AsElement();
  element.forget(aElement);
  return NS_OK;
}

int32_t nsTreeBodyFrame::RowHeight() const {
  return nsPresContext::AppUnitsToIntCSSPixels(mRowHeight);
}

int32_t nsTreeBodyFrame::RowWidth() {
  return nsPresContext::AppUnitsToIntCSSPixels(CalcHorzWidth(GetScrollParts()));
}

int32_t nsTreeBodyFrame::GetHorizontalPosition() const {
  return nsPresContext::AppUnitsToIntCSSPixels(mHorzPosition);
}

Maybe<CSSIntRegion> nsTreeBodyFrame::GetSelectionRegion() {
  nsCOMPtr<nsITreeSelection> selection;
  mView->GetSelection(getter_AddRefs(selection));
  if (!selection) {
    return Nothing();
  }

  RefPtr<nsPresContext> presContext = PresContext();
  nsIntRect rect = mRect.ToOutsidePixels(AppUnitsPerCSSPixel());

  nsIFrame* rootFrame = presContext->PresShell()->GetRootFrame();
  nsPoint origin = GetOffsetTo(rootFrame);

  CSSIntRegion region;

  // iterate through the visible rows and add the selected ones to the
  // drag region
  int32_t x = nsPresContext::AppUnitsToIntCSSPixels(origin.x);
  int32_t y = nsPresContext::AppUnitsToIntCSSPixels(origin.y);
  int32_t top = y;
  int32_t end = LastVisibleRow();
  int32_t rowHeight = nsPresContext::AppUnitsToIntCSSPixels(mRowHeight);
  for (int32_t i = mTopRowIndex; i <= end; i++) {
    bool isSelected;
    selection->IsSelected(i, &isSelected);
    if (isSelected) {
      region.OrWith(CSSIntRect(x, y, rect.width, rowHeight));
    }
    y += rowHeight;
  }

  // clip to the tree boundary in case one row extends past it
  region.AndWith(CSSIntRect(x, top, rect.width, rect.height));

  return Some(region);
}

nsresult nsTreeBodyFrame::Invalidate() {
  if (mUpdateBatchNest) return NS_OK;

  InvalidateFrame();

  return NS_OK;
}

nsresult nsTreeBodyFrame::InvalidateColumn(nsTreeColumn* aCol) {
  if (mUpdateBatchNest) return NS_OK;

  if (!aCol) return NS_ERROR_INVALID_ARG;

#ifdef ACCESSIBILITY
  if (PresShell::IsAccessibilityActive()) {
    FireInvalidateEvent(-1, -1, aCol, aCol);
  }
#endif  // #ifdef ACCESSIBILITY

  nsRect columnRect;
  nsresult rv = aCol->GetRect(this, mInnerBox.y, mInnerBox.height, &columnRect);
  NS_ENSURE_SUCCESS(rv, rv);

  // When false then column is out of view
  if (OffsetForHorzScroll(columnRect, true))
    InvalidateFrameWithRect(columnRect);

  return NS_OK;
}

nsresult nsTreeBodyFrame::InvalidateRow(int32_t aIndex) {
  if (mUpdateBatchNest) return NS_OK;

#ifdef ACCESSIBILITY
  if (PresShell::IsAccessibilityActive()) {
    FireInvalidateEvent(aIndex, aIndex, nullptr, nullptr);
  }
#endif  // #ifdef ACCESSIBILITY

  aIndex -= mTopRowIndex;
  if (aIndex < 0 || aIndex > mPageLength) return NS_OK;

  nsRect rowRect(mInnerBox.x, mInnerBox.y + mRowHeight * aIndex,
                 mInnerBox.width, mRowHeight);
  InvalidateFrameWithRect(rowRect);

  return NS_OK;
}

nsresult nsTreeBodyFrame::InvalidateCell(int32_t aIndex, nsTreeColumn* aCol) {
  if (mUpdateBatchNest) return NS_OK;

#ifdef ACCESSIBILITY
  if (PresShell::IsAccessibilityActive()) {
    FireInvalidateEvent(aIndex, aIndex, aCol, aCol);
  }
#endif  // #ifdef ACCESSIBILITY

  aIndex -= mTopRowIndex;
  if (aIndex < 0 || aIndex > mPageLength) return NS_OK;

  if (!aCol) return NS_ERROR_INVALID_ARG;

  nsRect cellRect;
  nsresult rv = aCol->GetRect(this, mInnerBox.y + mRowHeight * aIndex,
                              mRowHeight, &cellRect);
  NS_ENSURE_SUCCESS(rv, rv);

  if (OffsetForHorzScroll(cellRect, true)) InvalidateFrameWithRect(cellRect);

  return NS_OK;
}

nsresult nsTreeBodyFrame::InvalidateRange(int32_t aStart, int32_t aEnd) {
  if (mUpdateBatchNest) return NS_OK;

  if (aStart == aEnd) return InvalidateRow(aStart);

  int32_t last = LastVisibleRow();
  if (aStart > aEnd || aEnd < mTopRowIndex || aStart > last) return NS_OK;

  if (aStart < mTopRowIndex) aStart = mTopRowIndex;

  if (aEnd > last) aEnd = last;

#ifdef ACCESSIBILITY
  if (PresShell::IsAccessibilityActive()) {
    int32_t end =
        mRowCount > 0 ? ((mRowCount <= aEnd) ? mRowCount - 1 : aEnd) : 0;
    FireInvalidateEvent(aStart, end, nullptr, nullptr);
  }
#endif  // #ifdef ACCESSIBILITY

  nsRect rangeRect(mInnerBox.x,
                   mInnerBox.y + mRowHeight * (aStart - mTopRowIndex),
                   mInnerBox.width, mRowHeight * (aEnd - aStart + 1));
  InvalidateFrameWithRect(rangeRect);

  return NS_OK;
}

static void FindScrollParts(nsIFrame* aCurrFrame,
                            nsTreeBodyFrame::ScrollParts* aResult) {
  if (!aResult->mColumnsScrollFrame) {
    nsIScrollableFrame* f = do_QueryFrame(aCurrFrame);
    if (f) {
      aResult->mColumnsFrame = aCurrFrame;
      aResult->mColumnsScrollFrame = f;
    }
  }

  nsScrollbarFrame* sf = do_QueryFrame(aCurrFrame);
  if (sf) {
    if (!aCurrFrame->IsXULHorizontal()) {
      if (!aResult->mVScrollbar) {
        aResult->mVScrollbar = sf;
      }
    } else {
      if (!aResult->mHScrollbar) {
        aResult->mHScrollbar = sf;
      }
    }
    // don't bother searching inside a scrollbar
    return;
  }

  nsIFrame* child = aCurrFrame->PrincipalChildList().FirstChild();
  while (child && !child->GetContent()->IsRootOfNativeAnonymousSubtree() &&
         (!aResult->mVScrollbar || !aResult->mHScrollbar ||
          !aResult->mColumnsScrollFrame)) {
    FindScrollParts(child, aResult);
    child = child->GetNextSibling();
  }
}

nsTreeBodyFrame::ScrollParts nsTreeBodyFrame::GetScrollParts() {
  ScrollParts result = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  XULTreeElement* tree = GetBaseElement();
  nsIFrame* treeFrame = tree ? tree->GetPrimaryFrame() : nullptr;
  if (treeFrame) {
    // The way we do this, searching through the entire frame subtree, is pretty
    // dumb! We should know where these frames are.
    FindScrollParts(treeFrame, &result);
    if (result.mHScrollbar) {
      result.mHScrollbar->SetScrollbarMediatorContent(GetContent());
      nsIFrame* f = do_QueryFrame(result.mHScrollbar);
      result.mHScrollbarContent = f->GetContent()->AsElement();
    }
    if (result.mVScrollbar) {
      result.mVScrollbar->SetScrollbarMediatorContent(GetContent());
      nsIFrame* f = do_QueryFrame(result.mVScrollbar);
      result.mVScrollbarContent = f->GetContent()->AsElement();
    }
  }
  return result;
}

void nsTreeBodyFrame::UpdateScrollbars(const ScrollParts& aParts) {
  nscoord rowHeightAsPixels = nsPresContext::AppUnitsToIntCSSPixels(mRowHeight);

  AutoWeakFrame weakFrame(this);

  if (aParts.mVScrollbar) {
    nsAutoString curPos;
    curPos.AppendInt(mTopRowIndex * rowHeightAsPixels);
    aParts.mVScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::curpos,
                                       curPos, true);
    // 'this' might be deleted here
  }

  if (weakFrame.IsAlive() && aParts.mHScrollbar) {
    nsAutoString curPos;
    curPos.AppendInt(mHorzPosition);
    aParts.mHScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::curpos,
                                       curPos, true);
    // 'this' might be deleted here
  }

  if (weakFrame.IsAlive() && mScrollbarActivity) {
    mScrollbarActivity->ActivityOccurred();
  }
}

void nsTreeBodyFrame::CheckOverflow(const ScrollParts& aParts) {
  bool verticalOverflowChanged = false;
  bool horizontalOverflowChanged = false;

  if (!mVerticalOverflow && mRowCount > mPageLength) {
    mVerticalOverflow = true;
    verticalOverflowChanged = true;
  } else if (mVerticalOverflow && mRowCount <= mPageLength) {
    mVerticalOverflow = false;
    verticalOverflowChanged = true;
  }

  if (aParts.mColumnsFrame) {
    nsRect bounds = aParts.mColumnsFrame->GetRect();
    if (bounds.width != 0) {
      /* Ignore overflows that are less than half a pixel. Yes these happen
         all over the place when flex boxes are compressed real small.
         Probably a result of a rounding errors somewhere in the layout code. */
      bounds.width += nsPresContext::CSSPixelsToAppUnits(0.5f);
      if (!mHorizontalOverflow && bounds.width < mHorzWidth) {
        mHorizontalOverflow = true;
        horizontalOverflowChanged = true;
      } else if (mHorizontalOverflow && bounds.width >= mHorzWidth) {
        mHorizontalOverflow = false;
        horizontalOverflowChanged = true;
      }
    }
  }

  if (!horizontalOverflowChanged && !verticalOverflowChanged) {
    return;
  }

  AutoWeakFrame weakFrame(this);

  RefPtr<nsPresContext> presContext = PresContext();
  RefPtr<mozilla::PresShell> presShell = presContext->GetPresShell();
  nsCOMPtr<nsIContent> content = mContent;

  if (verticalOverflowChanged) {
    InternalScrollPortEvent event(
        true, mVerticalOverflow ? eScrollPortOverflow : eScrollPortUnderflow,
        nullptr);
    event.mOrient = InternalScrollPortEvent::eVertical;
    EventDispatcher::Dispatch(content, presContext, &event);
  }

  if (horizontalOverflowChanged) {
    InternalScrollPortEvent event(
        true, mHorizontalOverflow ? eScrollPortOverflow : eScrollPortUnderflow,
        nullptr);
    event.mOrient = InternalScrollPortEvent::eHorizontal;
    EventDispatcher::Dispatch(content, presContext, &event);
  }

  // The synchronous event dispatch above can trigger reflow notifications.
  // Flush those explicitly now, so that we can guard against potential infinite
  // recursion. See bug 905909.
  if (!weakFrame.IsAlive()) {
    return;
  }
  NS_ASSERTION(!mCheckingOverflow,
               "mCheckingOverflow should not already be set");
  // Don't use AutoRestore since we want to not touch mCheckingOverflow if we
  // fail the weakFrame.IsAlive() check below
  mCheckingOverflow = true;
  presShell->FlushPendingNotifications(FlushType::Layout);
  if (!weakFrame.IsAlive()) {
    return;
  }
  mCheckingOverflow = false;
}

void nsTreeBodyFrame::InvalidateScrollbars(const ScrollParts& aParts,
                                           AutoWeakFrame& aWeakColumnsFrame) {
  if (mUpdateBatchNest || !mView) return;
  AutoWeakFrame weakFrame(this);

  if (aParts.mVScrollbar) {
    // Do Vertical Scrollbar
    nsAutoString maxposStr;

    nscoord rowHeightAsPixels =
        nsPresContext::AppUnitsToIntCSSPixels(mRowHeight);

    int32_t size = rowHeightAsPixels *
                   (mRowCount > mPageLength ? mRowCount - mPageLength : 0);
    maxposStr.AppendInt(size);
    aParts.mVScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::maxpos,
                                       maxposStr, true);
    NS_ENSURE_TRUE_VOID(weakFrame.IsAlive());

    // Also set our page increment and decrement.
    nscoord pageincrement = mPageLength * rowHeightAsPixels;
    nsAutoString pageStr;
    pageStr.AppendInt(pageincrement);
    aParts.mVScrollbarContent->SetAttr(kNameSpaceID_None,
                                       nsGkAtoms::pageincrement, pageStr, true);
    NS_ENSURE_TRUE_VOID(weakFrame.IsAlive());
  }

  if (aParts.mHScrollbar && aParts.mColumnsFrame &&
      aWeakColumnsFrame.IsAlive()) {
    // And now Horizontal scrollbar
    nsRect bounds = aParts.mColumnsFrame->GetRect();
    nsAutoString maxposStr;

    maxposStr.AppendInt(mHorzWidth > bounds.width ? mHorzWidth - bounds.width
                                                  : 0);
    aParts.mHScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::maxpos,
                                       maxposStr, true);
    NS_ENSURE_TRUE_VOID(weakFrame.IsAlive());

    nsAutoString pageStr;
    pageStr.AppendInt(bounds.width);
    aParts.mHScrollbarContent->SetAttr(kNameSpaceID_None,
                                       nsGkAtoms::pageincrement, pageStr, true);
    NS_ENSURE_TRUE_VOID(weakFrame.IsAlive());

    pageStr.Truncate();
    pageStr.AppendInt(nsPresContext::CSSPixelsToAppUnits(16));
    aParts.mHScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::increment,
                                       pageStr, true);
  }

  if (weakFrame.IsAlive() && mScrollbarActivity) {
    mScrollbarActivity->ActivityOccurred();
  }
}

// Takes client x/y in pixels, converts them to appunits, and converts into
// values relative to this nsTreeBodyFrame frame.
nsPoint nsTreeBodyFrame::AdjustClientCoordsToBoxCoordSpace(int32_t aX,
                                                           int32_t aY) {
  nsPoint point(nsPresContext::CSSPixelsToAppUnits(aX),
                nsPresContext::CSSPixelsToAppUnits(aY));

  nsPresContext* presContext = PresContext();
  point -= GetOffsetTo(presContext->GetPresShell()->GetRootFrame());

  // Adjust by the inner box coords, so that we're in the inner box's
  // coordinate space.
  point -= mInnerBox.TopLeft();
  return point;
}  // AdjustClientCoordsToBoxCoordSpace

int32_t nsTreeBodyFrame::GetRowAt(int32_t aX, int32_t aY) {
  if (!mView) {
    return 0;
  }

  nsPoint point = AdjustClientCoordsToBoxCoordSpace(aX, aY);

  // Check if the coordinates are above our visible space.
  if (point.y < 0) {
    return -1;
  }

  return GetRowAtInternal(point.x, point.y);
}

nsresult nsTreeBodyFrame::GetCellAt(int32_t aX, int32_t aY, int32_t* aRow,
                                    nsTreeColumn** aCol,
                                    nsACString& aChildElt) {
  if (!mView) return NS_OK;

  nsPoint point = AdjustClientCoordsToBoxCoordSpace(aX, aY);

  // Check if the coordinates are above our visible space.
  if (point.y < 0) {
    *aRow = -1;
    return NS_OK;
  }

  nsTreeColumn* col;
  nsCSSAnonBoxPseudoStaticAtom* child;
  GetCellAt(point.x, point.y, aRow, &col, &child);

  if (col) {
    NS_ADDREF(*aCol = col);
    if (child == nsCSSAnonBoxes::mozTreeCell())
      aChildElt.AssignLiteral("cell");
    else if (child == nsCSSAnonBoxes::mozTreeTwisty())
      aChildElt.AssignLiteral("twisty");
    else if (child == nsCSSAnonBoxes::mozTreeImage())
      aChildElt.AssignLiteral("image");
    else if (child == nsCSSAnonBoxes::mozTreeCellText())
      aChildElt.AssignLiteral("text");
  }

  return NS_OK;
}

//
// GetCoordsForCellItem
//
// Find the x/y location and width/height (all in PIXELS) of the given object
// in the given column.
//
// XXX IMPORTANT XXX:
// Hyatt says in the bug for this, that the following needs to be done:
// (1) You need to deal with overflow when computing cell rects.  See other
// column iteration examples... if you don't deal with this, you'll mistakenly
// extend the cell into the scrollbar's rect.
//
// (2) You are adjusting the cell rect by the *row" border padding.  That's
// wrong.  You need to first adjust a row rect by its border/padding, and then
// the cell rect fits inside the adjusted row rect.  It also can have
// border/padding as well as margins.  The vertical direction isn't that
// important, but you need to get the horizontal direction right.
//
// (3) GetImageSize() does not include margins (but it does include
// border/padding). You need to make sure to add in the image's margins as well.
//
nsresult nsTreeBodyFrame::GetCoordsForCellItem(int32_t aRow, nsTreeColumn* aCol,
                                               const nsACString& aElement,
                                               int32_t* aX, int32_t* aY,
                                               int32_t* aWidth,
                                               int32_t* aHeight) {
  *aX = 0;
  *aY = 0;
  *aWidth = 0;
  *aHeight = 0;

  bool isRTL = StyleVisibility()->mDirection == StyleDirection::Rtl;
  nscoord currX = mInnerBox.x - mHorzPosition;

  // The Rect for the requested item.
  nsRect theRect;

  nsPresContext* presContext = PresContext();

  for (nsTreeColumn* currCol = mColumns->GetFirstColumn(); currCol;
       currCol = currCol->GetNext()) {
    // The Rect for the current cell.
    nscoord colWidth;
#ifdef DEBUG
    nsresult rv =
#endif
        currCol->GetWidthInTwips(this, &colWidth);
    NS_ASSERTION(NS_SUCCEEDED(rv), "invalid column");

    nsRect cellRect(currX, mInnerBox.y + mRowHeight * (aRow - mTopRowIndex),
                    colWidth, mRowHeight);

    // Check the ID of the current column to see if it matches. If it doesn't
    // increment the current X value and continue to the next column.
    if (currCol != aCol) {
      currX += cellRect.width;
      continue;
    }
    // Now obtain the properties for our cell.
    PrefillPropertyArray(aRow, currCol);

    nsAutoString properties;
    mView->GetCellProperties(aRow, currCol, properties);
    nsTreeUtils::TokenizeProperties(properties, mScratchArray);

    ComputedStyle* rowContext =
        GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeRow());

    // We don't want to consider any of the decorations that may be present
    // on the current row, so we have to deflate the rect by the border and
    // padding and offset its left and top coordinates appropriately.
    AdjustForBorderPadding(rowContext, cellRect);

    ComputedStyle* cellContext =
        GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeCell());

    NS_NAMED_LITERAL_CSTRING(cell, "cell");
    if (currCol->IsCycler() || cell.Equals(aElement)) {
      // If the current Column is a Cycler, then the Rect is just the cell - the
      // margins. Similarly, if we're just being asked for the cell rect,
      // provide it.

      theRect = cellRect;
      nsMargin cellMargin;
      cellContext->StyleMargin()->GetMargin(cellMargin);
      theRect.Deflate(cellMargin);
      break;
    }

    // Since we're not looking for the cell, and since the cell isn't a cycler,
    // we're looking for some subcomponent, and now we need to subtract the
    // borders and padding of the cell from cellRect so this does not
    // interfere with our computations.
    AdjustForBorderPadding(cellContext, cellRect);

    RefPtr<gfxContext> rc =
        presContext->PresShell()->CreateReferenceRenderingContext();

    // Now we'll start making our way across the cell, starting at the edge of
    // the cell and proceeding until we hit the right edge. |cellX| is the
    // working X value that we will increment as we crawl from left to right.
    nscoord cellX = cellRect.x;
    nscoord remainWidth = cellRect.width;

    if (currCol->IsPrimary()) {
      // If the current Column is a Primary, then we need to take into account
      // the indentation and possibly a twisty.

      // The amount of indentation is the indentation width (|mIndentation|) by
      // the level.
      int32_t level;
      mView->GetLevel(aRow, &level);
      if (!isRTL) cellX += mIndentation * level;
      remainWidth -= mIndentation * level;

      // Find the twisty rect by computing its size.
      nsRect imageRect;
      nsRect twistyRect(cellRect);
      ComputedStyle* twistyContext =
          GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeTwisty());
      GetTwistyRect(aRow, currCol, imageRect, twistyRect, presContext,
                    twistyContext);

      if (NS_LITERAL_CSTRING("twisty").Equals(aElement)) {
        // If we're looking for the twisty Rect, just return the size
        theRect = twistyRect;
        break;
      }

      // Now we need to add in the margins of the twisty element, so that we
      // can find the offset of the next element in the cell.
      nsMargin twistyMargin;
      twistyContext->StyleMargin()->GetMargin(twistyMargin);
      twistyRect.Inflate(twistyMargin);

      // Adjust our working X value with the twisty width (image size, margins,
      // borders, padding.
      if (!isRTL) cellX += twistyRect.width;
    }

    // Cell Image
    ComputedStyle* imageContext =
        GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeImage());

    nsRect imageSize = GetImageSize(aRow, currCol, false, imageContext);
    if (NS_LITERAL_CSTRING("image").Equals(aElement)) {
      theRect = imageSize;
      theRect.x = cellX;
      theRect.y = cellRect.y;
      break;
    }

    // Add in the margins of the cell image.
    nsMargin imageMargin;
    imageContext->StyleMargin()->GetMargin(imageMargin);
    imageSize.Inflate(imageMargin);

    // Increment cellX by the image width
    if (!isRTL) cellX += imageSize.width;

    // Cell Text
    nsAutoString cellText;
    mView->GetCellText(aRow, currCol, cellText);
    // We're going to measure this text so we need to ensure bidi is enabled if
    // necessary
    CheckTextForBidi(cellText);

    // Create a scratch rect to represent the text rectangle, with the current
    // X and Y coords, and a guess at the width and height. The width is the
    // remaining width we have left to traverse in the cell, which will be the
    // widest possible value for the text rect, and the row height.
    nsRect textRect(cellX, cellRect.y, remainWidth, cellRect.height);

    // Measure the width of the text. If the width of the text is greater than
    // the remaining width available, then we just assume that the text has
    // been cropped and use the remaining rect as the text Rect. Otherwise,
    // we add in borders and padding to the text dimension and give that back.
    ComputedStyle* textContext =
        GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeCellText());

    RefPtr<nsFontMetrics> fm =
        nsLayoutUtils::GetFontMetricsForComputedStyle(textContext, presContext);
    nscoord height = fm->MaxHeight();

    nsMargin textMargin;
    textContext->StyleMargin()->GetMargin(textMargin);
    textRect.Deflate(textMargin);

    // Center the text. XXX Obey vertical-align style prop?
    if (height < textRect.height) {
      textRect.y += (textRect.height - height) / 2;
      textRect.height = height;
    }

    nsMargin bp(0, 0, 0, 0);
    GetBorderPadding(textContext, bp);
    textRect.height += bp.top + bp.bottom;

    AdjustForCellText(cellText, aRow, currCol, *rc, *fm, textRect);

    theRect = textRect;
  }

  if (isRTL) theRect.x = mInnerBox.width - theRect.x - theRect.width;

  *aX = nsPresContext::AppUnitsToIntCSSPixels(theRect.x);
  *aY = nsPresContext::AppUnitsToIntCSSPixels(theRect.y);
  *aWidth = nsPresContext::AppUnitsToIntCSSPixels(theRect.width);
  *aHeight = nsPresContext::AppUnitsToIntCSSPixels(theRect.height);

  return NS_OK;
}

int32_t nsTreeBodyFrame::GetRowAtInternal(nscoord aX, nscoord aY) {
  if (mRowHeight <= 0) return -1;

  // Now just mod by our total inner box height and add to our top row index.
  int32_t row = (aY / mRowHeight) + mTopRowIndex;

  // Check if the coordinates are below our visible space (or within our visible
  // space but below any row).
  if (row > mTopRowIndex + mPageLength || row >= mRowCount) return -1;

  return row;
}

void nsTreeBodyFrame::CheckTextForBidi(nsAutoString& aText) {
  // We could check to see whether the prescontext already has bidi enabled,
  // but usually it won't, so it's probably faster to avoid the call to
  // GetPresContext() when it's not needed.
  if (HasRTLChars(aText)) {
    PresContext()->SetBidiEnabled();
  }
}

void nsTreeBodyFrame::AdjustForCellText(nsAutoString& aText, int32_t aRowIndex,
                                        nsTreeColumn* aColumn,
                                        gfxContext& aRenderingContext,
                                        nsFontMetrics& aFontMetrics,
                                        nsRect& aTextRect) {
  MOZ_ASSERT(aColumn && aColumn->GetFrame(), "invalid column passed");

  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();

  nscoord maxWidth = aTextRect.width;
  bool widthIsGreater = nsLayoutUtils::StringWidthIsGreaterThan(
      aText, aFontMetrics, drawTarget, maxWidth);

  if (aColumn->Overflow()) {
    DebugOnly<nsresult> rv;
    nsTreeColumn* nextColumn = aColumn->GetNext();
    while (nextColumn && widthIsGreater) {
      while (nextColumn) {
        nscoord width;
        rv = nextColumn->GetWidthInTwips(this, &width);
        NS_ASSERTION(NS_SUCCEEDED(rv), "nextColumn is invalid");

        if (width != 0) break;

        nextColumn = nextColumn->GetNext();
      }

      if (nextColumn) {
        nsAutoString nextText;
        mView->GetCellText(aRowIndex, nextColumn, nextText);
        // We don't measure or draw this text so no need to check it for
        // bidi-ness

        if (nextText.Length() == 0) {
          nscoord width;
          rv = nextColumn->GetWidthInTwips(this, &width);
          NS_ASSERTION(NS_SUCCEEDED(rv), "nextColumn is invalid");

          maxWidth += width;
          widthIsGreater = nsLayoutUtils::StringWidthIsGreaterThan(
              aText, aFontMetrics, drawTarget, maxWidth);

          nextColumn = nextColumn->GetNext();
        } else {
          nextColumn = nullptr;
        }
      }
    }
  }

  nscoord width;
  if (widthIsGreater) {
    // See if the width is even smaller than the ellipsis
    // If so, clear the text completely.
    const nsDependentString& kEllipsis = nsContentUtils::GetLocalizedEllipsis();
    aFontMetrics.SetTextRunRTL(false);
    nscoord ellipsisWidth = nsLayoutUtils::AppUnitWidthOfString(
        kEllipsis, aFontMetrics, drawTarget);

    width = maxWidth;
    if (ellipsisWidth > width)
      aText.SetLength(0);
    else if (ellipsisWidth == width)
      aText.Assign(kEllipsis);
    else {
      // We will be drawing an ellipsis, thank you very much.
      // Subtract out the required width of the ellipsis.
      // This is the total remaining width we have to play with.
      width -= ellipsisWidth;

      // Now we crop.
      switch (aColumn->GetCropStyle()) {
        default:
        case 0: {
          // Crop right.
          nscoord cwidth;
          nscoord twidth = 0;
          uint32_t length = aText.Length();
          uint32_t i;
          for (i = 0; i < length; ++i) {
            char16_t ch = aText[i];
            // XXX this is horrible and doesn't handle clusters
            cwidth = nsLayoutUtils::AppUnitWidthOfString(ch, aFontMetrics,
                                                         drawTarget);
            if (twidth + cwidth > width) break;
            twidth += cwidth;
          }
          aText.Truncate(i);
          aText.Append(kEllipsis);
        } break;

        case 2: {
          // Crop left.
          nscoord cwidth;
          nscoord twidth = 0;
          int32_t length = aText.Length();
          int32_t i;
          for (i = length - 1; i >= 0; --i) {
            char16_t ch = aText[i];
            cwidth = nsLayoutUtils::AppUnitWidthOfString(ch, aFontMetrics,
                                                         drawTarget);
            if (twidth + cwidth > width) break;
            twidth += cwidth;
          }

          nsAutoString copy;
          aText.Right(copy, length - 1 - i);
          aText.Assign(kEllipsis);
          aText += copy;
        } break;

        case 1: {
          // Crop center.
          nsAutoString leftStr, rightStr;
          nscoord cwidth, twidth = 0;
          int32_t length = aText.Length();
          int32_t rightPos = length - 1;
          for (int32_t leftPos = 0; leftPos < rightPos; ++leftPos) {
            char16_t ch = aText[leftPos];
            cwidth = nsLayoutUtils::AppUnitWidthOfString(ch, aFontMetrics,
                                                         drawTarget);
            twidth += cwidth;
            if (twidth > width) break;
            leftStr.Append(ch);

            ch = aText[rightPos];
            cwidth = nsLayoutUtils::AppUnitWidthOfString(ch, aFontMetrics,
                                                         drawTarget);
            twidth += cwidth;
            if (twidth > width) break;
            rightStr.Insert(ch, 0);
            --rightPos;
          }
          aText = leftStr;
          aText.Append(kEllipsis);
          aText += rightStr;
        } break;
      }
    }
  }

  width = nsLayoutUtils::AppUnitWidthOfStringBidi(aText, this, aFontMetrics,
                                                  aRenderingContext);

  switch (aColumn->GetTextAlignment()) {
    case mozilla::StyleTextAlign::Right:
      aTextRect.x += aTextRect.width - width;
      break;
    case mozilla::StyleTextAlign::Center:
      aTextRect.x += (aTextRect.width - width) / 2;
      break;
    default:
      break;
  }

  aTextRect.width = width;
}

nsCSSAnonBoxPseudoStaticAtom* nsTreeBodyFrame::GetItemWithinCellAt(
    nscoord aX, const nsRect& aCellRect, int32_t aRowIndex,
    nsTreeColumn* aColumn) {
  MOZ_ASSERT(aColumn && aColumn->GetFrame(), "invalid column passed");

  // Obtain the properties for our cell.
  PrefillPropertyArray(aRowIndex, aColumn);
  nsAutoString properties;
  mView->GetCellProperties(aRowIndex, aColumn, properties);
  nsTreeUtils::TokenizeProperties(properties, mScratchArray);

  // Resolve style for the cell.
  ComputedStyle* cellContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeCell());

  // Obtain the margins for the cell and then deflate our rect by that
  // amount.  The cell is assumed to be contained within the deflated rect.
  nsRect cellRect(aCellRect);
  nsMargin cellMargin;
  cellContext->StyleMargin()->GetMargin(cellMargin);
  cellRect.Deflate(cellMargin);

  // Adjust the rect for its border and padding.
  AdjustForBorderPadding(cellContext, cellRect);

  if (aX < cellRect.x || aX >= cellRect.x + cellRect.width) {
    // The user clicked within the cell's margins/borders/padding.  This
    // constitutes a click on the cell.
    return nsCSSAnonBoxes::mozTreeCell();
  }

  nscoord currX = cellRect.x;
  nscoord remainingWidth = cellRect.width;

  // Handle right alignment hit testing.
  bool isRTL = StyleVisibility()->mDirection == StyleDirection::Rtl;

  nsPresContext* presContext = PresContext();
  RefPtr<gfxContext> rc =
      presContext->PresShell()->CreateReferenceRenderingContext();

  if (aColumn->IsPrimary()) {
    // If we're the primary column, we have indentation and a twisty.
    int32_t level;
    mView->GetLevel(aRowIndex, &level);

    if (!isRTL) currX += mIndentation * level;
    remainingWidth -= mIndentation * level;

    if ((isRTL && aX > currX + remainingWidth) || (!isRTL && aX < currX)) {
      // The user clicked within the indentation.
      return nsCSSAnonBoxes::mozTreeCell();
    }

    // Always leave space for the twisty.
    nsRect twistyRect(currX, cellRect.y, remainingWidth, cellRect.height);
    bool hasTwisty = false;
    bool isContainer = false;
    mView->IsContainer(aRowIndex, &isContainer);
    if (isContainer) {
      bool isContainerEmpty = false;
      mView->IsContainerEmpty(aRowIndex, &isContainerEmpty);
      if (!isContainerEmpty) hasTwisty = true;
    }

    // Resolve style for the twisty.
    ComputedStyle* twistyContext =
        GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeTwisty());

    nsRect imageSize;
    GetTwistyRect(aRowIndex, aColumn, imageSize, twistyRect, presContext,
                  twistyContext);

    // We will treat a click as hitting the twisty if it happens on the margins,
    // borders, padding, or content of the twisty object.  By allowing a "slop"
    // into the margin, we make it a little bit easier for a user to hit the
    // twisty.  (We don't want to be too picky here.)
    nsMargin twistyMargin;
    twistyContext->StyleMargin()->GetMargin(twistyMargin);
    twistyRect.Inflate(twistyMargin);
    if (isRTL) twistyRect.x = currX + remainingWidth - twistyRect.width;

    // Now we test to see if aX is actually within the twistyRect.  If it is,
    // and if the item should have a twisty, then we return "twisty".  If it is
    // within the rect but we shouldn't have a twisty, then we return "cell".
    if (aX >= twistyRect.x && aX < twistyRect.x + twistyRect.width) {
      if (hasTwisty)
        return nsCSSAnonBoxes::mozTreeTwisty();
      else
        return nsCSSAnonBoxes::mozTreeCell();
    }

    if (!isRTL) currX += twistyRect.width;
    remainingWidth -= twistyRect.width;
  }

  // Now test to see if the user hit the icon for the cell.
  nsRect iconRect(currX, cellRect.y, remainingWidth, cellRect.height);

  // Resolve style for the image.
  ComputedStyle* imageContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeImage());

  nsRect iconSize = GetImageSize(aRowIndex, aColumn, false, imageContext);
  nsMargin imageMargin;
  imageContext->StyleMargin()->GetMargin(imageMargin);
  iconSize.Inflate(imageMargin);
  iconRect.width = iconSize.width;
  if (isRTL) iconRect.x = currX + remainingWidth - iconRect.width;

  if (aX >= iconRect.x && aX < iconRect.x + iconRect.width) {
    // The user clicked on the image.
    return nsCSSAnonBoxes::mozTreeImage();
  }

  if (!isRTL) currX += iconRect.width;
  remainingWidth -= iconRect.width;

  nsAutoString cellText;
  mView->GetCellText(aRowIndex, aColumn, cellText);
  // We're going to measure this text so we need to ensure bidi is enabled if
  // necessary
  CheckTextForBidi(cellText);

  nsRect textRect(currX, cellRect.y, remainingWidth, cellRect.height);

  ComputedStyle* textContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeCellText());

  nsMargin textMargin;
  textContext->StyleMargin()->GetMargin(textMargin);
  textRect.Deflate(textMargin);

  AdjustForBorderPadding(textContext, textRect);

  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetFontMetricsForComputedStyle(textContext, presContext);
  AdjustForCellText(cellText, aRowIndex, aColumn, *rc, *fm, textRect);

  if (aX >= textRect.x && aX < textRect.x + textRect.width)
    return nsCSSAnonBoxes::mozTreeCellText();
  else
    return nsCSSAnonBoxes::mozTreeCell();
}

void nsTreeBodyFrame::GetCellAt(nscoord aX, nscoord aY, int32_t* aRow,
                                nsTreeColumn** aCol,
                                nsCSSAnonBoxPseudoStaticAtom** aChildElt) {
  *aCol = nullptr;
  *aChildElt = nullptr;

  *aRow = GetRowAtInternal(aX, aY);
  if (*aRow < 0) return;

  // Determine the column hit.
  for (nsTreeColumn* currCol = mColumns->GetFirstColumn(); currCol;
       currCol = currCol->GetNext()) {
    nsRect cellRect;
    nsresult rv = currCol->GetRect(
        this, mInnerBox.y + mRowHeight * (*aRow - mTopRowIndex), mRowHeight,
        &cellRect);
    if (NS_FAILED(rv)) {
      MOZ_ASSERT_UNREACHABLE("column has no frame");
      continue;
    }

    if (!OffsetForHorzScroll(cellRect, false)) continue;

    if (aX >= cellRect.x && aX < cellRect.x + cellRect.width) {
      // We know the column hit now.
      *aCol = currCol;

      if (currCol->IsCycler())
        // Cyclers contain only images.  Fill this in immediately and return.
        *aChildElt = nsCSSAnonBoxes::mozTreeImage();
      else
        *aChildElt = GetItemWithinCellAt(aX, cellRect, *aRow, currCol);
      break;
    }
  }
}

nsresult nsTreeBodyFrame::GetCellWidth(int32_t aRow, nsTreeColumn* aCol,
                                       gfxContext* aRenderingContext,
                                       nscoord& aDesiredSize,
                                       nscoord& aCurrentSize) {
  MOZ_ASSERT(aCol, "aCol must not be null");
  MOZ_ASSERT(aRenderingContext, "aRenderingContext must not be null");

  // The rect for the current cell.
  nscoord colWidth;
  nsresult rv = aCol->GetWidthInTwips(this, &colWidth);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRect cellRect(0, 0, colWidth, mRowHeight);

  int32_t overflow =
      cellRect.x + cellRect.width - (mInnerBox.x + mInnerBox.width);
  if (overflow > 0) cellRect.width -= overflow;

  // Adjust borders and padding for the cell.
  ComputedStyle* cellContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeCell());
  nsMargin bp(0, 0, 0, 0);
  GetBorderPadding(cellContext, bp);

  aCurrentSize = cellRect.width;
  aDesiredSize = bp.left + bp.right;

  if (aCol->IsPrimary()) {
    // If the current Column is a Primary, then we need to take into account
    // the indentation and possibly a twisty.

    // The amount of indentation is the indentation width (|mIndentation|) by
    // the level.
    int32_t level;
    mView->GetLevel(aRow, &level);
    aDesiredSize += mIndentation * level;

    // Find the twisty rect by computing its size.
    ComputedStyle* twistyContext =
        GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeTwisty());

    nsRect imageSize;
    nsRect twistyRect(cellRect);
    GetTwistyRect(aRow, aCol, imageSize, twistyRect, PresContext(),
                  twistyContext);

    // Add in the margins of the twisty element.
    nsMargin twistyMargin;
    twistyContext->StyleMargin()->GetMargin(twistyMargin);
    twistyRect.Inflate(twistyMargin);

    aDesiredSize += twistyRect.width;
  }

  ComputedStyle* imageContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeImage());

  // Account for the width of the cell image.
  nsRect imageSize = GetImageSize(aRow, aCol, false, imageContext);
  // Add in the margins of the cell image.
  nsMargin imageMargin;
  imageContext->StyleMargin()->GetMargin(imageMargin);
  imageSize.Inflate(imageMargin);

  aDesiredSize += imageSize.width;

  // Get the cell text.
  nsAutoString cellText;
  mView->GetCellText(aRow, aCol, cellText);
  // We're going to measure this text so we need to ensure bidi is enabled if
  // necessary
  CheckTextForBidi(cellText);

  ComputedStyle* textContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeCellText());

  // Get the borders and padding for the text.
  GetBorderPadding(textContext, bp);

  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetFontMetricsForComputedStyle(textContext, PresContext());
  // Get the width of the text itself
  nscoord width = nsLayoutUtils::AppUnitWidthOfStringBidi(cellText, this, *fm,
                                                          *aRenderingContext);
  nscoord totalTextWidth = width + bp.left + bp.right;
  aDesiredSize += totalTextWidth;
  return NS_OK;
}

nsresult nsTreeBodyFrame::IsCellCropped(int32_t aRow, nsTreeColumn* aCol,
                                        bool* _retval) {
  nscoord currentSize, desiredSize;
  nsresult rv;

  if (!aCol) return NS_ERROR_INVALID_ARG;

  RefPtr<gfxContext> rc = PresShell()->CreateReferenceRenderingContext();

  rv = GetCellWidth(aRow, aCol, rc, desiredSize, currentSize);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = desiredSize > currentSize;

  return NS_OK;
}

nsresult nsTreeBodyFrame::CreateTimer(const LookAndFeel::IntID aID,
                                      nsTimerCallbackFunc aFunc, int32_t aType,
                                      nsITimer** aTimer, const char* aName) {
  // Get the delay from the look and feel service.
  int32_t delay = LookAndFeel::GetInt(aID, 0);

  nsCOMPtr<nsITimer> timer;

  // Create a new timer only if the delay is greater than zero.
  // Zero value means that this feature is completely disabled.
  if (delay > 0) {
    MOZ_TRY_VAR(timer,
                NS_NewTimerWithFuncCallback(
                    aFunc, this, delay, aType, aName,
                    mContent->OwnerDoc()->EventTargetFor(TaskCategory::Other)));
  }

  timer.forget(aTimer);
  return NS_OK;
}

nsresult nsTreeBodyFrame::RowCountChanged(int32_t aIndex, int32_t aCount) {
  if (aCount == 0 || !mView) return NS_OK;  // Nothing to do.

#ifdef ACCESSIBILITY
  if (PresShell::IsAccessibilityActive()) {
    FireRowCountChangedEvent(aIndex, aCount);
  }
#endif  // #ifdef ACCESSIBILITY

  // Adjust our selection.
  nsCOMPtr<nsITreeSelection> sel;
  mView->GetSelection(getter_AddRefs(sel));
  if (sel) sel->AdjustSelection(aIndex, aCount);

  if (mUpdateBatchNest) return NS_OK;

  mRowCount += aCount;
#ifdef DEBUG
  int32_t rowCount = mRowCount;
  mView->GetRowCount(&rowCount);
  NS_ASSERTION(
      rowCount == mRowCount,
      "row count did not change by the amount suggested, check caller");
#endif

  int32_t count = Abs(aCount);
  int32_t last = LastVisibleRow();
  if (aIndex >= mTopRowIndex && aIndex <= last) InvalidateRange(aIndex, last);

  ScrollParts parts = GetScrollParts();

  if (mTopRowIndex == 0) {
    // Just update the scrollbar and return.
    FullScrollbarsUpdate(false);
    return NS_OK;
  }

  bool needsInvalidation = false;
  // Adjust our top row index.
  if (aCount > 0) {
    if (mTopRowIndex > aIndex) {
      // Rows came in above us.  Augment the top row index.
      mTopRowIndex += aCount;
    }
  } else if (aCount < 0) {
    if (mTopRowIndex > aIndex + count - 1) {
      // No need to invalidate. The remove happened
      // completely above us (offscreen).
      mTopRowIndex -= count;
    } else if (mTopRowIndex >= aIndex) {
      // This is a full-blown invalidate.
      if (mTopRowIndex + mPageLength > mRowCount - 1) {
        mTopRowIndex = std::max(0, mRowCount - 1 - mPageLength);
      }
      needsInvalidation = true;
    }
  }

  FullScrollbarsUpdate(needsInvalidation);
  return NS_OK;
}

nsresult nsTreeBodyFrame::BeginUpdateBatch() {
  ++mUpdateBatchNest;

  return NS_OK;
}

nsresult nsTreeBodyFrame::EndUpdateBatch() {
  NS_ASSERTION(mUpdateBatchNest > 0, "badly nested update batch");

  if (--mUpdateBatchNest == 0) {
    if (mView) {
      Invalidate();
      int32_t countBeforeUpdate = mRowCount;
      mView->GetRowCount(&mRowCount);
      if (countBeforeUpdate != mRowCount) {
        if (mTopRowIndex + mPageLength > mRowCount - 1) {
          mTopRowIndex = std::max(0, mRowCount - 1 - mPageLength);
        }
        FullScrollbarsUpdate(false);
      }
    }
  }

  return NS_OK;
}

void nsTreeBodyFrame::PrefillPropertyArray(int32_t aRowIndex,
                                           nsTreeColumn* aCol) {
  MOZ_ASSERT(!aCol || aCol->GetFrame(), "invalid column passed");
  mScratchArray.Clear();

  // focus
  if (mFocused)
    mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::focus);
  else
    mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::blur);

  // sort
  bool sorted = false;
  mView->IsSorted(&sorted);
  if (sorted) mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::sorted);

  // drag session
  if (mSlots && mSlots->mIsDragging)
    mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::dragSession);

  if (aRowIndex != -1) {
    if (aRowIndex == mMouseOverRow)
      mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::hover);

    nsCOMPtr<nsITreeSelection> selection;
    mView->GetSelection(getter_AddRefs(selection));

    if (selection) {
      // selected
      bool isSelected;
      selection->IsSelected(aRowIndex, &isSelected);
      if (isSelected)
        mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::selected);

      // current
      int32_t currentIndex;
      selection->GetCurrentIndex(&currentIndex);
      if (aRowIndex == currentIndex)
        mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::current);
    }

    // container or leaf
    bool isContainer = false;
    mView->IsContainer(aRowIndex, &isContainer);
    if (isContainer) {
      mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::container);

      // open or closed
      bool isOpen = false;
      mView->IsContainerOpen(aRowIndex, &isOpen);
      if (isOpen)
        mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::open);
      else
        mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::closed);
    } else {
      mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::leaf);
    }

    // drop orientation
    if (mSlots && mSlots->mDropAllowed && mSlots->mDropRow == aRowIndex) {
      if (mSlots->mDropOrient == nsITreeView::DROP_BEFORE)
        mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::dropBefore);
      else if (mSlots->mDropOrient == nsITreeView::DROP_ON)
        mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::dropOn);
      else if (mSlots->mDropOrient == nsITreeView::DROP_AFTER)
        mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::dropAfter);
    }

    // odd or even
    if (aRowIndex % 2)
      mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::odd);
    else
      mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::even);

    XULTreeElement* tree = GetBaseElement();
    if (tree && tree->HasAttr(kNameSpaceID_None, nsGkAtoms::editing)) {
      mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::editing);
    }

    // multiple columns
    if (mColumns->GetColumnAt(1))
      mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::multicol);
  }

  if (aCol) {
    mScratchArray.AppendElement(aCol->GetAtom());

    if (aCol->IsPrimary())
      mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::primary);

    if (aCol->GetType() == TreeColumn_Binding::TYPE_CHECKBOX) {
      mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::checkbox);

      if (aRowIndex != -1) {
        nsAutoString value;
        mView->GetCellValue(aRowIndex, aCol, value);
        if (value.EqualsLiteral("true"))
          mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::checked);
      }
    }

    // Read special properties from attributes on the column content node
    if (aCol->mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::insertbefore,
                                    nsGkAtoms::_true, eCaseMatters))
      mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::insertbefore);
    if (aCol->mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::insertafter,
                                    nsGkAtoms::_true, eCaseMatters))
      mScratchArray.AppendElement((nsStaticAtom*)nsGkAtoms::insertafter);
  }
}

nsITheme* nsTreeBodyFrame::GetTwistyRect(int32_t aRowIndex,
                                         nsTreeColumn* aColumn,
                                         nsRect& aImageRect,
                                         nsRect& aTwistyRect,
                                         nsPresContext* aPresContext,
                                         ComputedStyle* aTwistyContext) {
  // The twisty rect extends all the way to the end of the cell.  This is
  // incorrect.  We need to determine the twisty rect's true width.  This is
  // done by examining the ComputedStyle for a width first.  If it has one, we
  // use that.  If it doesn't, we use the image's natural width. If the image
  // hasn't loaded and if no width is specified, then we just bail. If there is
  // a -moz-appearance involved, adjust the rect by the minimum widget size
  // provided by the theme implementation.
  aImageRect = GetImageSize(aRowIndex, aColumn, true, aTwistyContext);
  if (aImageRect.height > aTwistyRect.height)
    aImageRect.height = aTwistyRect.height;
  if (aImageRect.width > aTwistyRect.width)
    aImageRect.width = aTwistyRect.width;
  else
    aTwistyRect.width = aImageRect.width;

  bool useTheme = false;
  nsITheme* theme = nullptr;
  const nsStyleDisplay* twistyDisplayData = aTwistyContext->StyleDisplay();
  if (twistyDisplayData->mAppearance != StyleAppearance::None) {
    theme = aPresContext->Theme();
    if (theme->ThemeSupportsWidget(aPresContext, nullptr,
                                   twistyDisplayData->mAppearance))
      useTheme = true;
  }

  if (useTheme) {
    LayoutDeviceIntSize minTwistySizePx;
    bool canOverride = true;
    theme->GetMinimumWidgetSize(aPresContext, this,
                                twistyDisplayData->mAppearance,
                                &minTwistySizePx, &canOverride);

    // GMWS() returns size in pixels, we need to convert it back to app units
    nsSize minTwistySize;
    minTwistySize.width =
        aPresContext->DevPixelsToAppUnits(minTwistySizePx.width);
    minTwistySize.height =
        aPresContext->DevPixelsToAppUnits(minTwistySizePx.height);

    if (aTwistyRect.width < minTwistySize.width || !canOverride)
      aTwistyRect.width = minTwistySize.width;
  }

  return useTheme ? theme : nullptr;
}

nsresult nsTreeBodyFrame::GetImage(int32_t aRowIndex, nsTreeColumn* aCol,
                                   bool aUseContext,
                                   ComputedStyle* aComputedStyle,
                                   bool& aAllowImageRegions,
                                   imgIContainer** aResult) {
  *aResult = nullptr;

  nsAutoString imageSrc;
  mView->GetImageSrc(aRowIndex, aCol, imageSrc);
  RefPtr<imgRequestProxy> styleRequest;
  if (!aUseContext && !imageSrc.IsEmpty()) {
    aAllowImageRegions = false;
  } else {
    // Obtain the URL from the ComputedStyle.
    aAllowImageRegions = true;
    styleRequest = aComputedStyle->StyleList()->GetListStyleImage();
    if (!styleRequest) return NS_OK;
    nsCOMPtr<nsIURI> uri;
    styleRequest->GetURI(getter_AddRefs(uri));
    nsAutoCString spec;
    nsresult rv = uri->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
    CopyUTF8toUTF16(spec, imageSrc);
  }

  // Look the image up in our cache.
  nsTreeImageCacheEntry entry;
  if (mImageCache.Get(imageSrc, &entry)) {
    // Find out if the image has loaded.
    uint32_t status;
    imgIRequest* imgReq = entry.request;
    imgReq->GetImageStatus(&status);
    imgReq->GetImage(aResult);  // We hand back the image here.  The GetImage
                                // call addrefs *aResult.
    bool animated = true;       // Assuming animated is the safe option

    // We can only call GetAnimated if we're decoded
    if (*aResult && (status & imgIRequest::STATUS_DECODE_COMPLETE))
      (*aResult)->GetAnimated(&animated);

    if ((!(status & imgIRequest::STATUS_LOAD_COMPLETE)) || animated) {
      // We either aren't done loading, or we're animating. Add our row as a
      // listener for invalidations.
      nsCOMPtr<imgINotificationObserver> obs;
      imgReq->GetNotificationObserver(getter_AddRefs(obs));

      if (obs) {
        static_cast<nsTreeImageListener*>(obs.get())->AddCell(aRowIndex, aCol);
      }

      return NS_OK;
    }
  }

  if (!*aResult) {
    // Create a new nsTreeImageListener object and pass it our row and column
    // information.
    nsTreeImageListener* listener = new nsTreeImageListener(this);
    if (!listener) return NS_ERROR_OUT_OF_MEMORY;

    if (!mCreatedListeners.PutEntry(listener)) {
      return NS_ERROR_FAILURE;
    }

    listener->AddCell(aRowIndex, aCol);
    nsCOMPtr<imgINotificationObserver> imgNotificationObserver = listener;

    Document* doc = mContent->GetComposedDoc();
    if (!doc)
      // The page is currently being torn down.  Why bother.
      return NS_ERROR_FAILURE;

    RefPtr<imgRequestProxy> imageRequest;
    if (styleRequest) {
      styleRequest->SyncClone(imgNotificationObserver, doc,
                              getter_AddRefs(imageRequest));
    } else {
      nsCOMPtr<nsIURI> srcURI;
      nsContentUtils::NewURIWithDocumentCharset(
          getter_AddRefs(srcURI), imageSrc, doc, mContent->GetBaseURI());
      if (!srcURI) return NS_ERROR_FAILURE;

      auto referrerInfo = MakeRefPtr<mozilla::dom::ReferrerInfo>(*doc);

      // XXXbz what's the origin principal for this stuff that comes from our
      // view?  I guess we should assume that it's the node's principal...
      nsresult rv = nsContentUtils::LoadImage(
          srcURI, mContent, doc, mContent->NodePrincipal(), 0, referrerInfo,
          imgNotificationObserver, nsIRequest::LOAD_NORMAL, EmptyString(),
          getter_AddRefs(imageRequest));
      NS_ENSURE_SUCCESS(rv, rv);

      // NOTE(heycam): If it's an SVG image, and we need to want the image to
      // able to respond to media query changes, it needs to be added to the
      // document's ImageTracker (like nsImageBoxFrame does).  For now, assume
      // we don't need this.
    }
    listener->UnsuppressInvalidation();

    if (!imageRequest) return NS_ERROR_FAILURE;

    // We don't want discarding/decode-on-draw for xul images
    imageRequest->StartDecoding(imgIContainer::FLAG_ASYNC_NOTIFY);
    imageRequest->LockImage();

    // In a case it was already cached.
    imageRequest->GetImage(aResult);
    nsTreeImageCacheEntry cacheEntry(imageRequest, imgNotificationObserver);
    mImageCache.Put(imageSrc, cacheEntry);
  }
  return NS_OK;
}

nsRect nsTreeBodyFrame::GetImageSize(int32_t aRowIndex, nsTreeColumn* aCol,
                                     bool aUseContext,
                                     ComputedStyle* aComputedStyle) {
  // XXX We should respond to visibility rules for collapsed vs. hidden.

  // This method returns the width of the twisty INCLUDING borders and padding.
  // It first checks the ComputedStyle for a width.  If none is found, it tries
  // to use the default image width for the twisty.  If no image is found, it
  // defaults to border+padding.
  nsRect r(0, 0, 0, 0);
  nsMargin bp(0, 0, 0, 0);
  GetBorderPadding(aComputedStyle, bp);
  r.Inflate(bp);

  // Now r contains our border+padding info.  We now need to get our width and
  // height.
  bool needWidth = false;
  bool needHeight = false;

  // We have to load image even though we already have a size.
  // Don't change this, otherwise things start to go crazy.
  bool useImageRegion = true;
  nsCOMPtr<imgIContainer> image;
  GetImage(aRowIndex, aCol, aUseContext, aComputedStyle, useImageRegion,
           getter_AddRefs(image));

  const nsStylePosition* myPosition = aComputedStyle->StylePosition();
  const nsStyleList* myList = aComputedStyle->StyleList();
  nsRect imageRegion = myList->GetImageRegion();
  if (useImageRegion) {
    r.x += imageRegion.x;
    r.y += imageRegion.y;
  }

  if (myPosition->mWidth.ConvertsToLength()) {
    int32_t val = myPosition->mWidth.ToLength();
    r.width += val;
  } else if (useImageRegion && imageRegion.width > 0) {
    r.width += imageRegion.width;
  } else {
    needWidth = true;
  }

  if (myPosition->mHeight.ConvertsToLength()) {
    int32_t val = myPosition->mHeight.ToLength();
    r.height += val;
  } else if (useImageRegion && imageRegion.height > 0)
    r.height += imageRegion.height;
  else
    needHeight = true;

  if (image) {
    if (needWidth || needHeight) {
      // Get the natural image size.

      if (needWidth) {
        // Get the size from the image.
        nscoord width;
        image->GetWidth(&width);
        r.width += nsPresContext::CSSPixelsToAppUnits(width);
      }

      if (needHeight) {
        nscoord height;
        image->GetHeight(&height);
        r.height += nsPresContext::CSSPixelsToAppUnits(height);
      }
    }
  }

  return r;
}

// GetImageDestSize returns the destination size of the image.
// The width and height do not include borders and padding.
// The width and height have not been adjusted to fit in the row height
// or cell width.
// The width and height reflect the destination size specified in CSS,
// or the image region specified in CSS, or the natural size of the
// image.
// If only the destination width has been specified in CSS, the height is
// calculated to maintain the aspect ratio of the image.
// If only the destination height has been specified in CSS, the width is
// calculated to maintain the aspect ratio of the image.
nsSize nsTreeBodyFrame::GetImageDestSize(ComputedStyle* aComputedStyle,
                                         bool useImageRegion,
                                         imgIContainer* image) {
  nsSize size(0, 0);

  // We need to get the width and height.
  bool needWidth = false;
  bool needHeight = false;

  // Get the style position to see if the CSS has specified the
  // destination width/height.
  const nsStylePosition* myPosition = aComputedStyle->StylePosition();

  if (myPosition->mWidth.ConvertsToLength()) {
    // CSS has specified the destination width.
    size.width = myPosition->mWidth.ToLength();
  } else {
    // We'll need to get the width of the image/region.
    needWidth = true;
  }

  if (myPosition->mHeight.ConvertsToLength()) {
    // CSS has specified the destination height.
    size.height = myPosition->mHeight.ToLength();
  } else {
    // We'll need to get the height of the image/region.
    needHeight = true;
  }

  if (needWidth || needHeight) {
    // We need to get the size of the image/region.
    nsSize imageSize(0, 0);

    const nsStyleList* myList = aComputedStyle->StyleList();
    nsRect imageRegion = myList->GetImageRegion();
    if (useImageRegion && imageRegion.width > 0) {
      // CSS has specified an image region.
      // Use the width of the image region.
      imageSize.width = imageRegion.width;
    } else if (image) {
      nscoord width;
      image->GetWidth(&width);
      imageSize.width = nsPresContext::CSSPixelsToAppUnits(width);
    }

    if (useImageRegion && imageRegion.height > 0) {
      // CSS has specified an image region.
      // Use the height of the image region.
      imageSize.height = imageRegion.height;
    } else if (image) {
      nscoord height;
      image->GetHeight(&height);
      imageSize.height = nsPresContext::CSSPixelsToAppUnits(height);
    }

    if (needWidth) {
      if (!needHeight && imageSize.height != 0) {
        // The CSS specified the destination height, but not the destination
        // width. We need to calculate the width so that we maintain the
        // image's aspect ratio.
        size.width = imageSize.width * size.height / imageSize.height;
      } else {
        size.width = imageSize.width;
      }
    }

    if (needHeight) {
      if (!needWidth && imageSize.width != 0) {
        // The CSS specified the destination width, but not the destination
        // height. We need to calculate the height so that we maintain the
        // image's aspect ratio.
        size.height = imageSize.height * size.width / imageSize.width;
      } else {
        size.height = imageSize.height;
      }
    }
  }

  return size;
}

// GetImageSourceRect returns the source rectangle of the image to be
// displayed.
// The width and height reflect the image region specified in CSS, or
// the natural size of the image.
// The width and height do not include borders and padding.
// The width and height do not reflect the destination size specified
// in CSS.
nsRect nsTreeBodyFrame::GetImageSourceRect(ComputedStyle* aComputedStyle,
                                           bool useImageRegion,
                                           imgIContainer* image) {
  const nsStyleList* myList = aComputedStyle->StyleList();
  // CSS has specified an image region.
  if (useImageRegion && myList->mImageRegion.IsRect()) {
    return myList->GetImageRegion();
  }

  if (!image) {
    return nsRect();
  }

  nsRect r;
  // Use the actual image size.
  nscoord coord;
  if (NS_SUCCEEDED(image->GetWidth(&coord))) {
    r.width = nsPresContext::CSSPixelsToAppUnits(coord);
  }
  if (NS_SUCCEEDED(image->GetHeight(&coord))) {
    r.height = nsPresContext::CSSPixelsToAppUnits(coord);
  }
  return r;
}

int32_t nsTreeBodyFrame::GetRowHeight() {
  // Look up the correct height.  It is equal to the specified height
  // + the specified margins.
  mScratchArray.Clear();
  ComputedStyle* rowContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeRow());
  if (rowContext) {
    const nsStylePosition* myPosition = rowContext->StylePosition();

    nscoord minHeight = 0;
    if (myPosition->mMinHeight.ConvertsToLength()) {
      minHeight = myPosition->mMinHeight.ToLength();
    }

    nscoord height = 0;
    if (myPosition->mHeight.ConvertsToLength()) {
      height = myPosition->mHeight.ToLength();
    }

    if (height < minHeight) height = minHeight;

    if (height > 0) {
      height = nsPresContext::AppUnitsToIntCSSPixels(height);
      height += height % 2;
      height = nsPresContext::CSSPixelsToAppUnits(height);

      // XXX Check box-sizing to determine if border/padding should augment the
      // height Inflate the height by our margins.
      nsRect rowRect(0, 0, 0, height);
      nsMargin rowMargin;
      rowContext->StyleMargin()->GetMargin(rowMargin);
      rowRect.Inflate(rowMargin);
      height = rowRect.height;
      return height;
    }
  }

  return nsPresContext::CSSPixelsToAppUnits(18);  // As good a default as any.
}

int32_t nsTreeBodyFrame::GetIndentation() {
  // Look up the correct indentation.  It is equal to the specified indentation
  // width.
  mScratchArray.Clear();
  ComputedStyle* indentContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeIndentation());
  if (indentContext) {
    const nsStylePosition* myPosition = indentContext->StylePosition();
    if (myPosition->mWidth.ConvertsToLength()) {
      return myPosition->mWidth.ToLength();
    }
  }

  return nsPresContext::CSSPixelsToAppUnits(16);  // As good a default as any.
}

void nsTreeBodyFrame::CalcInnerBox() {
  mInnerBox.SetRect(0, 0, mRect.width, mRect.height);
  AdjustForBorderPadding(mComputedStyle, mInnerBox);
}

nscoord nsTreeBodyFrame::CalcHorzWidth(const ScrollParts& aParts) {
  // Compute the adjustment to the last column. This varies depending on the
  // visibility of the columnpicker and the scrollbar.
  if (aParts.mColumnsFrame)
    mAdjustWidth = mRect.width - aParts.mColumnsFrame->GetRect().width;
  else
    mAdjustWidth = 0;

  nscoord width = 0;

  // We calculate this from the scrollable frame, so that it
  // properly covers all contingencies of what could be
  // scrollable (columns, body, etc...)

  if (aParts.mColumnsScrollFrame) {
    width = aParts.mColumnsScrollFrame->GetScrollRange().width +
            aParts.mColumnsScrollFrame->GetScrollPortRect().width;
  }

  // If no horz scrolling periphery is present, then just return our width
  if (width == 0) width = mRect.width;

  return width;
}

Maybe<nsIFrame::Cursor> nsTreeBodyFrame::GetCursor(const nsPoint& aPoint) {
  // Check the GetScriptHandlingObject so we don't end up running code when
  // the document is a zombie.
  bool dummy;
  if (mView && GetContent()->GetComposedDoc()->GetScriptHandlingObject(dummy)) {
    int32_t row;
    nsTreeColumn* col;
    nsCSSAnonBoxPseudoStaticAtom* child;
    GetCellAt(aPoint.x, aPoint.y, &row, &col, &child);

    if (child) {
      // Our scratch array is already prefilled.
      RefPtr<ComputedStyle> childContext = GetPseudoComputedStyle(child);
      StyleCursorKind kind = childContext->StyleUI()->mCursor.keyword;
      if (kind == StyleCursorKind::Auto) {
        kind = StyleCursorKind::Default;
      }
      return Some(
          Cursor{kind, AllowCustomCursorImage::Yes, std::move(childContext)});
    }
  }
  return nsLeafBoxFrame::GetCursor(aPoint);
}

static uint32_t GetDropEffect(WidgetGUIEvent* aEvent) {
  NS_ASSERTION(aEvent->mClass == eDragEventClass, "wrong event type");
  WidgetDragEvent* dragEvent = aEvent->AsDragEvent();
  nsContentUtils::SetDataTransferInEvent(dragEvent);

  uint32_t action = 0;
  if (dragEvent->mDataTransfer) {
    action = dragEvent->mDataTransfer->DropEffectInt();
  }
  return action;
}

nsresult nsTreeBodyFrame::HandleEvent(nsPresContext* aPresContext,
                                      WidgetGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus) {
  if (aEvent->mMessage == eMouseOver || aEvent->mMessage == eMouseMove) {
    nsPoint pt =
        nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, RelativeTo{this});
    int32_t xTwips = pt.x - mInnerBox.x;
    int32_t yTwips = pt.y - mInnerBox.y;
    int32_t newrow = GetRowAtInternal(xTwips, yTwips);
    if (mMouseOverRow != newrow) {
      // redraw the old and the new row
      if (mMouseOverRow != -1) InvalidateRow(mMouseOverRow);
      mMouseOverRow = newrow;
      if (mMouseOverRow != -1) InvalidateRow(mMouseOverRow);
    }
  } else if (aEvent->mMessage == eMouseOut) {
    if (mMouseOverRow != -1) {
      InvalidateRow(mMouseOverRow);
      mMouseOverRow = -1;
    }
  } else if (aEvent->mMessage == eDragEnter) {
    if (!mSlots) mSlots = new Slots();

    // Cache several things we'll need throughout the course of our work. These
    // will all get released on a drag exit.

    if (mSlots->mTimer) {
      mSlots->mTimer->Cancel();
      mSlots->mTimer = nullptr;
    }

    // Cache the drag session.
    mSlots->mIsDragging = true;
    mSlots->mDropRow = -1;
    mSlots->mDropOrient = -1;
    mSlots->mDragAction = GetDropEffect(aEvent);
  } else if (aEvent->mMessage == eDragOver) {
    // The mouse is hovering over this tree. If we determine things are
    // different from the last time, invalidate the drop feedback at the old
    // position, query the view to see if the current location is droppable,
    // and then invalidate the drop feedback at the new location if it is.
    // The mouse may or may not have changed position from the last time
    // we were called, so optimize out a lot of the extra notifications by
    // checking if anything changed first. For drop feedback we use drop,
    // dropBefore and dropAfter property.

    if (!mView || !mSlots) return NS_OK;

    // Save last values, we will need them.
    int32_t lastDropRow = mSlots->mDropRow;
    int16_t lastDropOrient = mSlots->mDropOrient;
#ifndef XP_MACOSX
    int16_t lastScrollLines = mSlots->mScrollLines;
#endif

    // Find out the current drag action
    uint32_t lastDragAction = mSlots->mDragAction;
    mSlots->mDragAction = GetDropEffect(aEvent);

    // Compute the row mouse is over and the above/below/on state.
    // Below we'll use this to see if anything changed.
    // Also check if we want to auto-scroll.
    ComputeDropPosition(aEvent, &mSlots->mDropRow, &mSlots->mDropOrient,
                        &mSlots->mScrollLines);

    // While we're here, handle tracking of scrolling during a drag.
    if (mSlots->mScrollLines) {
      if (mSlots->mDropAllowed) {
        // Invalidate primary cell at old location.
        mSlots->mDropAllowed = false;
        InvalidateDropFeedback(lastDropRow, lastDropOrient);
      }
#ifdef XP_MACOSX
      ScrollByLines(mSlots->mScrollLines);
#else
      if (!lastScrollLines) {
        // Cancel any previously initialized timer.
        if (mSlots->mTimer) {
          mSlots->mTimer->Cancel();
          mSlots->mTimer = nullptr;
        }

        // Set a timer to trigger the tree scrolling.
        CreateTimer(LookAndFeel::eIntID_TreeLazyScrollDelay, LazyScrollCallback,
                    nsITimer::TYPE_ONE_SHOT, getter_AddRefs(mSlots->mTimer),
                    "nsTreeBodyFrame::LazyScrollCallback");
      }
#endif
      // Bail out to prevent spring loaded timer and feedback line settings.
      return NS_OK;
    }

    // If changed from last time, invalidate primary cell at the old location
    // and if allowed, invalidate primary cell at the new location. If nothing
    // changed, just bail.
    if (mSlots->mDropRow != lastDropRow ||
        mSlots->mDropOrient != lastDropOrient ||
        mSlots->mDragAction != lastDragAction) {
      // Invalidate row at the old location.
      if (mSlots->mDropAllowed) {
        mSlots->mDropAllowed = false;
        InvalidateDropFeedback(lastDropRow, lastDropOrient);
      }

      if (mSlots->mTimer) {
        // Timer is active but for a different row than the current one, kill
        // it.
        mSlots->mTimer->Cancel();
        mSlots->mTimer = nullptr;
      }

      if (mSlots->mDropRow >= 0) {
        if (!mSlots->mTimer && mSlots->mDropOrient == nsITreeView::DROP_ON) {
          // Either there wasn't a timer running or it was just killed above.
          // If over a folder, start up a timer to open the folder.
          bool isContainer = false;
          mView->IsContainer(mSlots->mDropRow, &isContainer);
          if (isContainer) {
            bool isOpen = false;
            mView->IsContainerOpen(mSlots->mDropRow, &isOpen);
            if (!isOpen) {
              // This node isn't expanded, set a timer to expand it.
              CreateTimer(LookAndFeel::eIntID_TreeOpenDelay, OpenCallback,
                          nsITimer::TYPE_ONE_SHOT,
                          getter_AddRefs(mSlots->mTimer),
                          "nsTreeBodyFrame::OpenCallback");
            }
          }
        }

        // The dataTransfer was initialized by the call to GetDropEffect above.
        bool canDropAtNewLocation = false;
        mView->CanDrop(mSlots->mDropRow, mSlots->mDropOrient,
                       aEvent->AsDragEvent()->mDataTransfer,
                       &canDropAtNewLocation);

        if (canDropAtNewLocation) {
          // Invalidate row at the new location.
          mSlots->mDropAllowed = canDropAtNewLocation;
          InvalidateDropFeedback(mSlots->mDropRow, mSlots->mDropOrient);
        }
      }
    }

    // Indicate that the drop is allowed by preventing the default behaviour.
    if (mSlots->mDropAllowed) *aEventStatus = nsEventStatus_eConsumeNoDefault;
  } else if (aEvent->mMessage == eDrop) {
    // this event was meant for another frame, so ignore it
    if (!mSlots) return NS_OK;

    // Tell the view where the drop happened.

    // Remove the drop folder and all its parents from the array.
    int32_t parentIndex;
    nsresult rv = mView->GetParentIndex(mSlots->mDropRow, &parentIndex);
    while (NS_SUCCEEDED(rv) && parentIndex >= 0) {
      mSlots->mArray.RemoveElement(parentIndex);
      rv = mView->GetParentIndex(parentIndex, &parentIndex);
    }

    NS_ASSERTION(aEvent->mClass == eDragEventClass, "wrong event type");
    WidgetDragEvent* dragEvent = aEvent->AsDragEvent();
    nsContentUtils::SetDataTransferInEvent(dragEvent);

    mView->Drop(mSlots->mDropRow, mSlots->mDropOrient,
                dragEvent->mDataTransfer);
    mSlots->mDropRow = -1;
    mSlots->mDropOrient = -1;
    mSlots->mIsDragging = false;
    *aEventStatus =
        nsEventStatus_eConsumeNoDefault;  // already handled the drop
  } else if (aEvent->mMessage == eDragExit) {
    // this event was meant for another frame, so ignore it
    if (!mSlots) return NS_OK;

    // Clear out all our tracking vars.

    if (mSlots->mDropAllowed) {
      mSlots->mDropAllowed = false;
      InvalidateDropFeedback(mSlots->mDropRow, mSlots->mDropOrient);
    } else
      mSlots->mDropAllowed = false;
    mSlots->mIsDragging = false;
    mSlots->mScrollLines = 0;
    // If a drop is occuring, the exit event will fire just before the drop
    // event, so don't reset mDropRow or mDropOrient as these fields are used
    // by the drop event.
    if (mSlots->mTimer) {
      mSlots->mTimer->Cancel();
      mSlots->mTimer = nullptr;
    }

    if (!mSlots->mArray.IsEmpty()) {
      // Close all spring loaded folders except the drop folder.
      CreateTimer(LookAndFeel::eIntID_TreeCloseDelay, CloseCallback,
                  nsITimer::TYPE_ONE_SHOT, getter_AddRefs(mSlots->mTimer),
                  "nsTreeBodyFrame::CloseCallback");
    }
  }

  return NS_OK;
}

class nsDisplayTreeBody final : public nsPaintedDisplayItem {
 public:
  nsDisplayTreeBody(nsDisplayListBuilder* aBuilder, nsFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayTreeBody);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayTreeBody)

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplayItemGenericImageGeometry(this, aBuilder);
  }

  void Destroy(nsDisplayListBuilder* aBuilder) override {
    aBuilder->UnregisterThemeGeometry(this);
    nsPaintedDisplayItem::Destroy(aBuilder);
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override {
    auto geometry =
        static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

    if (aBuilder->ShouldSyncDecodeImages() &&
        geometry->ShouldInvalidateToSyncDecodeImages()) {
      bool snap;
      aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
    }

    nsPaintedDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry,
                                                    aInvalidRegion);
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) override {
    MOZ_ASSERT(aBuilder);
    DrawTargetAutoDisableSubpixelAntialiasing disable(aCtx->GetDrawTarget(),
                                                      IsSubpixelAADisabled());

    ImgDrawResult result = static_cast<nsTreeBodyFrame*>(mFrame)->PaintTreeBody(
        *aCtx, GetPaintRect(), ToReferenceFrame(), aBuilder);

    nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
  }

  NS_DISPLAY_DECL_NAME("XULTreeBody", TYPE_XUL_TREE_BODY)

  virtual nsRect GetComponentAlphaBounds(
      nsDisplayListBuilder* aBuilder) const override {
    bool snap;
    return GetBounds(aBuilder, &snap);
  }
};

// Painting routines
void nsTreeBodyFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                       const nsDisplayListSet& aLists) {
  // REVIEW: why did we paint if we were collapsed? that makes no sense!
  if (!IsVisibleForPainting()) return;  // We're invisible.  Don't paint.

  // Handles painting our background, border, and outline.
  nsLeafBoxFrame::BuildDisplayList(aBuilder, aLists);

  // Bail out now if there's no view or we can't run script because the
  // document is a zombie
  if (!mView || !GetContent()->GetComposedDoc()->GetWindow()) return;

  nsDisplayItem* item = MakeDisplayItem<nsDisplayTreeBody>(aBuilder, this);
  if (!item) {
    return;
  }
  aLists.Content()->AppendToTop(item);

#ifdef XP_MACOSX
  XULTreeElement* tree = GetBaseElement();
  nsIFrame* treeFrame = tree ? tree->GetPrimaryFrame() : nullptr;
  nsCOMPtr<nsITreeSelection> selection;
  mView->GetSelection(getter_AddRefs(selection));
  nsITheme* theme = PresContext()->Theme();
  // On Mac, we support native theming of selected rows. On 10.10 and higher,
  // this means applying vibrancy which require us to register the theme
  // geometrics for the row. In order to make the vibrancy effect to work
  // properly, we also need the tree to be themed as a source list.
  if (selection && treeFrame && theme &&
      treeFrame->StyleDisplay()->mAppearance ==
          StyleAppearance::MozMacSourceList) {
    // Loop through our onscreen rows. If the row is selected and a
    // -moz-appearance is provided, RegisterThemeGeometry might be necessary.
    const auto end = std::min(mRowCount, LastVisibleRow() + 1);
    for (auto i = FirstVisibleRow(); i < end; i++) {
      bool isSelected;
      selection->IsSelected(i, &isSelected);
      if (isSelected) {
        PrefillPropertyArray(i, nullptr);
        nsAutoString properties;
        mView->GetRowProperties(i, properties);
        nsTreeUtils::TokenizeProperties(properties, mScratchArray);
        ComputedStyle* rowContext =
            GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeRow());
        auto appearance = rowContext->StyleDisplay()->mAppearance;
        if (appearance != StyleAppearance::None) {
          if (theme->ThemeSupportsWidget(PresContext(), this, appearance)) {
            nsITheme::ThemeGeometryType type =
                theme->ThemeGeometryTypeForWidget(this, appearance);
            if (type != nsITheme::eThemeGeometryTypeUnknown) {
              nsRect rowRect(mInnerBox.x,
                             mInnerBox.y + mRowHeight * (i - FirstVisibleRow()),
                             mInnerBox.width, mRowHeight);
              aBuilder->RegisterThemeGeometry(
                  type, item,
                  LayoutDeviceIntRect::FromUnknownRect(
                      (rowRect + aBuilder->ToReferenceFrame(this))
                          .ToNearestPixels(
                              PresContext()->AppUnitsPerDevPixel())));
            }
          }
        }
      }
    }
  }
#endif
}

ImgDrawResult nsTreeBodyFrame::PaintTreeBody(gfxContext& aRenderingContext,
                                             const nsRect& aDirtyRect,
                                             nsPoint aPt,
                                             nsDisplayListBuilder* aBuilder) {
  // Update our available height and our page count.
  CalcInnerBox();

  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();

  aRenderingContext.Save();
  aRenderingContext.Clip(NSRectToSnappedRect(
      mInnerBox + aPt, PresContext()->AppUnitsPerDevPixel(), *drawTarget));
  int32_t oldPageCount = mPageLength;
  if (!mHasFixedRowCount)
    mPageLength =
        (mRowHeight > 0) ? (mInnerBox.height / mRowHeight) : mRowCount;

  if (oldPageCount != mPageLength ||
      mHorzWidth != CalcHorzWidth(GetScrollParts())) {
    // Schedule a ResizeReflow that will update our info properly.
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::Resize,
                                  NS_FRAME_IS_DIRTY);
  }
#ifdef DEBUG
  int32_t rowCount = mRowCount;
  mView->GetRowCount(&rowCount);
  NS_WARNING_ASSERTION(mRowCount == rowCount, "row count changed unexpectedly");
#endif

  ImgDrawResult result = ImgDrawResult::SUCCESS;

  // Loop through our columns and paint them (e.g., for sorting).  This is only
  // relevant when painting backgrounds, since columns contain no content.
  // Content is contained in the rows.
  for (nsTreeColumn* currCol = mColumns->GetFirstColumn(); currCol;
       currCol = currCol->GetNext()) {
    nsRect colRect;
    nsresult rv =
        currCol->GetRect(this, mInnerBox.y, mInnerBox.height, &colRect);
    // Don't paint hidden columns.
    if (NS_FAILED(rv) || colRect.width == 0) continue;

    if (OffsetForHorzScroll(colRect, false)) {
      nsRect dirtyRect;
      colRect += aPt;
      if (dirtyRect.IntersectRect(aDirtyRect, colRect)) {
        result &= PaintColumn(currCol, colRect, PresContext(),
                              aRenderingContext, aDirtyRect);
      }
    }
  }
  // Loop through our on-screen rows.
  for (int32_t i = mTopRowIndex;
       i < mRowCount && i <= mTopRowIndex + mPageLength; i++) {
    nsRect rowRect(mInnerBox.x, mInnerBox.y + mRowHeight * (i - mTopRowIndex),
                   mInnerBox.width, mRowHeight);
    nsRect dirtyRect;
    if (dirtyRect.IntersectRect(aDirtyRect, rowRect + aPt) &&
        rowRect.y < (mInnerBox.y + mInnerBox.height)) {
      result &= PaintRow(i, rowRect + aPt, PresContext(), aRenderingContext,
                         aDirtyRect, aPt, aBuilder);
    }
  }

  if (mSlots && mSlots->mDropAllowed &&
      (mSlots->mDropOrient == nsITreeView::DROP_BEFORE ||
       mSlots->mDropOrient == nsITreeView::DROP_AFTER)) {
    nscoord yPos = mInnerBox.y +
                   mRowHeight * (mSlots->mDropRow - mTopRowIndex) -
                   mRowHeight / 2;
    nsRect feedbackRect(mInnerBox.x, yPos, mInnerBox.width, mRowHeight);
    if (mSlots->mDropOrient == nsITreeView::DROP_AFTER)
      feedbackRect.y += mRowHeight;

    nsRect dirtyRect;
    feedbackRect += aPt;
    if (dirtyRect.IntersectRect(aDirtyRect, feedbackRect)) {
      result &= PaintDropFeedback(feedbackRect, PresContext(),
                                  aRenderingContext, aDirtyRect, aPt);
    }
  }
  aRenderingContext.Restore();

  return result;
}

ImgDrawResult nsTreeBodyFrame::PaintColumn(nsTreeColumn* aColumn,
                                           const nsRect& aColumnRect,
                                           nsPresContext* aPresContext,
                                           gfxContext& aRenderingContext,
                                           const nsRect& aDirtyRect) {
  MOZ_ASSERT(aColumn && aColumn->GetFrame(), "invalid column passed");

  // Now obtain the properties for our cell.
  PrefillPropertyArray(-1, aColumn);
  nsAutoString properties;
  mView->GetColumnProperties(aColumn, properties);
  nsTreeUtils::TokenizeProperties(properties, mScratchArray);

  // Resolve style for the column.  It contains all the info we need to lay
  // ourselves out and to paint.
  ComputedStyle* colContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeColumn());

  // Obtain the margins for the cell and then deflate our rect by that
  // amount.  The cell is assumed to be contained within the deflated rect.
  nsRect colRect(aColumnRect);
  nsMargin colMargin;
  colContext->StyleMargin()->GetMargin(colMargin);
  colRect.Deflate(colMargin);

  return PaintBackgroundLayer(colContext, aPresContext, aRenderingContext,
                              colRect, aDirtyRect);
}

ImgDrawResult nsTreeBodyFrame::PaintRow(int32_t aRowIndex,
                                        const nsRect& aRowRect,
                                        nsPresContext* aPresContext,
                                        gfxContext& aRenderingContext,
                                        const nsRect& aDirtyRect, nsPoint aPt,
                                        nsDisplayListBuilder* aBuilder) {
  // We have been given a rect for our row.  We treat this row like a full-blown
  // frame, meaning that it can have borders, margins, padding, and a
  // background.

  // Without a view, we have no data. Check for this up front.
  if (!mView) {
    return ImgDrawResult::SUCCESS;
  }

  nsresult rv;

  // Now obtain the properties for our row.
  // XXX Automatically fill in the following props: open, closed, container,
  // leaf, selected, focused
  PrefillPropertyArray(aRowIndex, nullptr);

  nsAutoString properties;
  mView->GetRowProperties(aRowIndex, properties);
  nsTreeUtils::TokenizeProperties(properties, mScratchArray);

  // Resolve style for the row.  It contains all the info we need to lay
  // ourselves out and to paint.
  ComputedStyle* rowContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeRow());

  // Obtain the margins for the row and then deflate our rect by that
  // amount.  The row is assumed to be contained within the deflated rect.
  nsRect rowRect(aRowRect);
  nsMargin rowMargin;
  rowContext->StyleMargin()->GetMargin(rowMargin);
  rowRect.Deflate(rowMargin);

  ImgDrawResult result = ImgDrawResult::SUCCESS;

  // Paint our borders and background for our row rect.
  nsITheme* theme = nullptr;
  auto appearance = rowContext->StyleDisplay()->mAppearance;
  if (appearance != StyleAppearance::None) {
    theme = aPresContext->Theme();
  }

  if (theme && theme->ThemeSupportsWidget(aPresContext, nullptr, appearance)) {
    nsRect dirty;
    dirty.IntersectRect(rowRect, aDirtyRect);
    theme->DrawWidgetBackground(&aRenderingContext, this, appearance, rowRect,
                                dirty);
  } else {
    result &= PaintBackgroundLayer(rowContext, aPresContext, aRenderingContext,
                                   rowRect, aDirtyRect);
  }

  // Adjust the rect for its border and padding.
  nsRect originalRowRect = rowRect;
  AdjustForBorderPadding(rowContext, rowRect);

  bool isSeparator = false;
  mView->IsSeparator(aRowIndex, &isSeparator);
  if (isSeparator) {
    // The row is a separator.

    nscoord primaryX = rowRect.x;
    nsTreeColumn* primaryCol = mColumns->GetPrimaryColumn();
    if (primaryCol) {
      // Paint the primary cell.
      nsRect cellRect;
      rv = primaryCol->GetRect(this, rowRect.y, rowRect.height, &cellRect);
      if (NS_FAILED(rv)) {
        MOZ_ASSERT_UNREACHABLE("primary column is invalid");
        return result;
      }

      if (OffsetForHorzScroll(cellRect, false)) {
        cellRect.x += aPt.x;
        nsRect dirtyRect;
        nsRect checkRect(cellRect.x, originalRowRect.y, cellRect.width,
                         originalRowRect.height);
        if (dirtyRect.IntersectRect(aDirtyRect, checkRect)) {
          result &=
              PaintCell(aRowIndex, primaryCol, cellRect, aPresContext,
                        aRenderingContext, aDirtyRect, primaryX, aPt, aBuilder);
        }
      }

      // Paint the left side of the separator.
      nscoord currX;
      nsTreeColumn* previousCol = primaryCol->GetPrevious();
      if (previousCol) {
        nsRect prevColRect;
        rv = previousCol->GetRect(this, 0, 0, &prevColRect);
        if (NS_SUCCEEDED(rv)) {
          currX = (prevColRect.x - mHorzPosition) + prevColRect.width + aPt.x;
        } else {
          MOZ_ASSERT_UNREACHABLE(
              "The column before the primary column is "
              "invalid");
          currX = rowRect.x;
        }
      } else {
        currX = rowRect.x;
      }

      int32_t level;
      mView->GetLevel(aRowIndex, &level);
      if (level == 0) currX += mIndentation;

      if (currX > rowRect.x) {
        nsRect separatorRect(rowRect);
        separatorRect.width -= rowRect.x + rowRect.width - currX;
        result &= PaintSeparator(aRowIndex, separatorRect, aPresContext,
                                 aRenderingContext, aDirtyRect);
      }
    }

    // Paint the right side (whole) separator.
    nsRect separatorRect(rowRect);
    if (primaryX > rowRect.x) {
      separatorRect.width -= primaryX - rowRect.x;
      separatorRect.x += primaryX - rowRect.x;
    }
    result &= PaintSeparator(aRowIndex, separatorRect, aPresContext,
                             aRenderingContext, aDirtyRect);
  } else {
    // Now loop over our cells. Only paint a cell if it intersects with our
    // dirty rect.
    for (nsTreeColumn* currCol = mColumns->GetFirstColumn(); currCol;
         currCol = currCol->GetNext()) {
      nsRect cellRect;
      rv = currCol->GetRect(this, rowRect.y, rowRect.height, &cellRect);
      // Don't paint cells in hidden columns.
      if (NS_FAILED(rv) || cellRect.width == 0) continue;

      if (OffsetForHorzScroll(cellRect, false)) {
        cellRect.x += aPt.x;

        // for primary columns, use the row's vertical size so that the
        // lines get drawn properly
        nsRect checkRect = cellRect;
        if (currCol->IsPrimary())
          checkRect = nsRect(cellRect.x, originalRowRect.y, cellRect.width,
                             originalRowRect.height);

        nsRect dirtyRect;
        nscoord dummy;
        if (dirtyRect.IntersectRect(aDirtyRect, checkRect))
          result &=
              PaintCell(aRowIndex, currCol, cellRect, aPresContext,
                        aRenderingContext, aDirtyRect, dummy, aPt, aBuilder);
      }
    }
  }

  return result;
}

ImgDrawResult nsTreeBodyFrame::PaintSeparator(int32_t aRowIndex,
                                              const nsRect& aSeparatorRect,
                                              nsPresContext* aPresContext,
                                              gfxContext& aRenderingContext,
                                              const nsRect& aDirtyRect) {
  // Resolve style for the separator.
  ComputedStyle* separatorContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeSeparator());
  bool useTheme = false;
  nsITheme* theme = nullptr;
  const nsStyleDisplay* displayData = separatorContext->StyleDisplay();
  if (displayData->HasAppearance()) {
    theme = aPresContext->Theme();
    if (theme->ThemeSupportsWidget(aPresContext, nullptr,
                                   displayData->mAppearance))
      useTheme = true;
  }

  ImgDrawResult result = ImgDrawResult::SUCCESS;

  // use -moz-appearance if provided.
  if (useTheme) {
    nsRect dirty;
    dirty.IntersectRect(aSeparatorRect, aDirtyRect);
    theme->DrawWidgetBackground(&aRenderingContext, this,
                                displayData->mAppearance, aSeparatorRect,
                                dirty);
  } else {
    const nsStylePosition* stylePosition = separatorContext->StylePosition();

    // Obtain the height for the separator or use the default value.
    nscoord height;
    if (stylePosition->mHeight.ConvertsToLength()) {
      height = stylePosition->mHeight.ToLength();
    } else {
      // Use default height 2px.
      height = nsPresContext::CSSPixelsToAppUnits(2);
    }

    // Obtain the margins for the separator and then deflate our rect by that
    // amount. The separator is assumed to be contained within the deflated
    // rect.
    nsRect separatorRect(aSeparatorRect.x, aSeparatorRect.y,
                         aSeparatorRect.width, height);
    nsMargin separatorMargin;
    separatorContext->StyleMargin()->GetMargin(separatorMargin);
    separatorRect.Deflate(separatorMargin);

    // Center the separator.
    separatorRect.y += (aSeparatorRect.height - height) / 2;

    result &=
        PaintBackgroundLayer(separatorContext, aPresContext, aRenderingContext,
                             separatorRect, aDirtyRect);
  }

  return result;
}

ImgDrawResult nsTreeBodyFrame::PaintCell(
    int32_t aRowIndex, nsTreeColumn* aColumn, const nsRect& aCellRect,
    nsPresContext* aPresContext, gfxContext& aRenderingContext,
    const nsRect& aDirtyRect, nscoord& aCurrX, nsPoint aPt,
    nsDisplayListBuilder* aBuilder) {
  MOZ_ASSERT(aColumn && aColumn->GetFrame(), "invalid column passed");

  // Now obtain the properties for our cell.
  // XXX Automatically fill in the following props: open, closed, container,
  // leaf, selected, focused, and the col ID.
  PrefillPropertyArray(aRowIndex, aColumn);
  nsAutoString properties;
  mView->GetCellProperties(aRowIndex, aColumn, properties);
  nsTreeUtils::TokenizeProperties(properties, mScratchArray);

  // Resolve style for the cell.  It contains all the info we need to lay
  // ourselves out and to paint.
  ComputedStyle* cellContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeCell());

  bool isRTL = StyleVisibility()->mDirection == StyleDirection::Rtl;

  // Obtain the margins for the cell and then deflate our rect by that
  // amount.  The cell is assumed to be contained within the deflated rect.
  nsRect cellRect(aCellRect);
  nsMargin cellMargin;
  cellContext->StyleMargin()->GetMargin(cellMargin);
  cellRect.Deflate(cellMargin);

  // Paint our borders and background for our row rect.
  ImgDrawResult result = PaintBackgroundLayer(
      cellContext, aPresContext, aRenderingContext, cellRect, aDirtyRect);

  // Adjust the rect for its border and padding.
  AdjustForBorderPadding(cellContext, cellRect);

  nscoord currX = cellRect.x;
  nscoord remainingWidth = cellRect.width;

  // Now we paint the contents of the cells.
  // Directionality of the tree determines the order in which we paint.
  // StyleDirection::Ltr means paint from left to right.
  // StyleDirection::Rtl means paint from right to left.

  if (aColumn->IsPrimary()) {
    // If we're the primary column, we need to indent and paint the twisty and
    // any connecting lines between siblings.

    int32_t level;
    mView->GetLevel(aRowIndex, &level);

    if (!isRTL) currX += mIndentation * level;
    remainingWidth -= mIndentation * level;

    // Resolve the style to use for the connecting lines.
    ComputedStyle* lineContext =
        GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeLine());

    if (mIndentation && level &&
        lineContext->StyleVisibility()->IsVisibleOrCollapsed()) {
      // Paint the thread lines.

      // Get the size of the twisty. We don't want to paint the twisty
      // before painting of connecting lines since it would paint lines over
      // the twisty. But we need to leave a place for it.
      ComputedStyle* twistyContext =
          GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeTwisty());

      nsRect imageSize;
      nsRect twistyRect(aCellRect);
      GetTwistyRect(aRowIndex, aColumn, imageSize, twistyRect, aPresContext,
                    twistyContext);

      nsMargin twistyMargin;
      twistyContext->StyleMargin()->GetMargin(twistyMargin);
      twistyRect.Inflate(twistyMargin);

      const nsStyleBorder* borderStyle = lineContext->StyleBorder();
      // Resolve currentcolor values against the treeline context
      nscolor color = borderStyle->mBorderLeftColor.CalcColor(*lineContext);
      ColorPattern colorPatt(ToDeviceColor(color));

      StyleBorderStyle style = borderStyle->GetBorderStyle(eSideLeft);
      StrokeOptions strokeOptions;
      nsLayoutUtils::InitDashPattern(strokeOptions, style);

      nscoord srcX = currX + twistyRect.width - mIndentation / 2;
      nscoord lineY = (aRowIndex - mTopRowIndex) * mRowHeight + aPt.y;

      DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();
      nsPresContext* pc = PresContext();

      // Don't paint off our cell.
      if (srcX <= cellRect.x + cellRect.width) {
        nscoord destX = currX + twistyRect.width;
        if (destX > cellRect.x + cellRect.width)
          destX = cellRect.x + cellRect.width;
        if (isRTL) {
          srcX = currX + remainingWidth - (srcX - cellRect.x);
          destX = currX + remainingWidth - (destX - cellRect.x);
        }
        Point p1(pc->AppUnitsToGfxUnits(srcX),
                 pc->AppUnitsToGfxUnits(lineY + mRowHeight / 2));
        Point p2(pc->AppUnitsToGfxUnits(destX),
                 pc->AppUnitsToGfxUnits(lineY + mRowHeight / 2));
        SnapLineToDevicePixelsForStroking(p1, p2, *drawTarget,
                                          strokeOptions.mLineWidth);
        drawTarget->StrokeLine(p1, p2, colorPatt, strokeOptions);
      }

      int32_t currentParent = aRowIndex;
      for (int32_t i = level; i > 0; i--) {
        if (srcX <= cellRect.x + cellRect.width) {
          // Paint full vertical line only if we have next sibling.
          bool hasNextSibling;
          mView->HasNextSibling(currentParent, aRowIndex, &hasNextSibling);
          if (hasNextSibling || i == level) {
            Point p1(pc->AppUnitsToGfxUnits(srcX),
                     pc->AppUnitsToGfxUnits(lineY));
            Point p2;
            p2.x = pc->AppUnitsToGfxUnits(srcX);

            if (hasNextSibling)
              p2.y = pc->AppUnitsToGfxUnits(lineY + mRowHeight);
            else if (i == level)
              p2.y = pc->AppUnitsToGfxUnits(lineY + mRowHeight / 2);

            SnapLineToDevicePixelsForStroking(p1, p2, *drawTarget,
                                              strokeOptions.mLineWidth);
            drawTarget->StrokeLine(p1, p2, colorPatt, strokeOptions);
          }
        }

        int32_t parent;
        if (NS_FAILED(mView->GetParentIndex(currentParent, &parent)) ||
            parent < 0)
          break;
        currentParent = parent;
        srcX -= mIndentation;
      }
    }

    // Always leave space for the twisty.
    nsRect twistyRect(currX, cellRect.y, remainingWidth, cellRect.height);
    result &= PaintTwisty(aRowIndex, aColumn, twistyRect, aPresContext,
                          aRenderingContext, aDirtyRect, remainingWidth, currX);
  }

  // Now paint the icon for our cell.
  nsRect iconRect(currX, cellRect.y, remainingWidth, cellRect.height);
  nsRect dirtyRect;
  if (dirtyRect.IntersectRect(aDirtyRect, iconRect)) {
    result &= PaintImage(aRowIndex, aColumn, iconRect, aPresContext,
                         aRenderingContext, aDirtyRect, remainingWidth, currX,
                         aBuilder);
  }

  // Now paint our element, but only if we aren't a cycler column.
  // XXX until we have the ability to load images, allow the view to
  // insert text into cycler columns...
  if (!aColumn->IsCycler()) {
    nsRect elementRect(currX, cellRect.y, remainingWidth, cellRect.height);
    nsRect dirtyRect;
    if (dirtyRect.IntersectRect(aDirtyRect, elementRect)) {
      switch (aColumn->GetType()) {
        case TreeColumn_Binding::TYPE_TEXT:
          result &= PaintText(aRowIndex, aColumn, elementRect, aPresContext,
                              aRenderingContext, aDirtyRect, currX);
          break;
        case TreeColumn_Binding::TYPE_CHECKBOX:
          result &= PaintCheckbox(aRowIndex, aColumn, elementRect, aPresContext,
                                  aRenderingContext, aDirtyRect);
          break;
      }
    }
  }

  aCurrX = currX;

  return result;
}

ImgDrawResult nsTreeBodyFrame::PaintTwisty(
    int32_t aRowIndex, nsTreeColumn* aColumn, const nsRect& aTwistyRect,
    nsPresContext* aPresContext, gfxContext& aRenderingContext,
    const nsRect& aDirtyRect, nscoord& aRemainingWidth, nscoord& aCurrX) {
  MOZ_ASSERT(aColumn && aColumn->GetFrame(), "invalid column passed");

  bool isRTL = StyleVisibility()->mDirection == StyleDirection::Rtl;
  nscoord rightEdge = aCurrX + aRemainingWidth;
  // Paint the twisty, but only if we are a non-empty container.
  bool shouldPaint = false;
  bool isContainer = false;
  mView->IsContainer(aRowIndex, &isContainer);
  if (isContainer) {
    bool isContainerEmpty = false;
    mView->IsContainerEmpty(aRowIndex, &isContainerEmpty);
    if (!isContainerEmpty) shouldPaint = true;
  }

  // Resolve style for the twisty.
  ComputedStyle* twistyContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeTwisty());

  // Obtain the margins for the twisty and then deflate our rect by that
  // amount.  The twisty is assumed to be contained within the deflated rect.
  nsRect twistyRect(aTwistyRect);
  nsMargin twistyMargin;
  twistyContext->StyleMargin()->GetMargin(twistyMargin);
  twistyRect.Deflate(twistyMargin);

  nsRect imageSize;
  nsITheme* theme = GetTwistyRect(aRowIndex, aColumn, imageSize, twistyRect,
                                  aPresContext, twistyContext);

  // Subtract out the remaining width.  This is done even when we don't actually
  // paint a twisty in this cell, so that cells in different rows still line up.
  nsRect copyRect(twistyRect);
  copyRect.Inflate(twistyMargin);
  aRemainingWidth -= copyRect.width;
  if (!isRTL) aCurrX += copyRect.width;

  ImgDrawResult result = ImgDrawResult::SUCCESS;

  if (shouldPaint) {
    // Paint our borders and background for our image rect.
    result &= PaintBackgroundLayer(twistyContext, aPresContext,
                                   aRenderingContext, twistyRect, aDirtyRect);

    if (theme) {
      if (isRTL) twistyRect.x = rightEdge - twistyRect.width;
      // yeah, I know it says we're drawing a background, but a twisty is really
      // a fg object since it doesn't have anything that gecko would want to
      // draw over it. Besides, we have to prevent imagelib from drawing it.
      nsRect dirty;
      dirty.IntersectRect(twistyRect, aDirtyRect);
      theme->DrawWidgetBackground(&aRenderingContext, this,
                                  twistyContext->StyleDisplay()->mAppearance,
                                  twistyRect, dirty);
    } else {
      // Time to paint the twisty.
      // Adjust the rect for its border and padding.
      nsMargin bp(0, 0, 0, 0);
      GetBorderPadding(twistyContext, bp);
      twistyRect.Deflate(bp);
      if (isRTL) twistyRect.x = rightEdge - twistyRect.width;
      imageSize.Deflate(bp);

      // Get the image for drawing.
      nsCOMPtr<imgIContainer> image;
      bool useImageRegion = true;
      GetImage(aRowIndex, aColumn, true, twistyContext, useImageRegion,
               getter_AddRefs(image));
      if (image) {
        nsPoint anchorPoint = twistyRect.TopLeft();

        // Center the image. XXX Obey vertical-align style prop?
        if (imageSize.height < twistyRect.height) {
          anchorPoint.y += (twistyRect.height - imageSize.height) / 2;
        }

        // Apply context paint if applicable
        Maybe<SVGImageContext> svgContext;
        SVGImageContext::MaybeStoreContextPaint(svgContext, twistyContext,
                                                image);

        // Paint the image.
        result &= nsLayoutUtils::DrawSingleUnscaledImage(
            aRenderingContext, aPresContext, image, SamplingFilter::POINT,
            anchorPoint, &aDirtyRect, svgContext, imgIContainer::FLAG_NONE,
            &imageSize);
      }
    }
  }

  return result;
}

ImgDrawResult nsTreeBodyFrame::PaintImage(
    int32_t aRowIndex, nsTreeColumn* aColumn, const nsRect& aImageRect,
    nsPresContext* aPresContext, gfxContext& aRenderingContext,
    const nsRect& aDirtyRect, nscoord& aRemainingWidth, nscoord& aCurrX,
    nsDisplayListBuilder* aBuilder) {
  MOZ_ASSERT(aColumn && aColumn->GetFrame(), "invalid column passed");

  bool isRTL = StyleVisibility()->mDirection == StyleDirection::Rtl;
  nscoord rightEdge = aCurrX + aRemainingWidth;
  // Resolve style for the image.
  ComputedStyle* imageContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeImage());

  // Obtain opacity value for the image.
  float opacity = imageContext->StyleEffects()->mOpacity;

  // Obtain the margins for the image and then deflate our rect by that
  // amount.  The image is assumed to be contained within the deflated rect.
  nsRect imageRect(aImageRect);
  nsMargin imageMargin;
  imageContext->StyleMargin()->GetMargin(imageMargin);
  imageRect.Deflate(imageMargin);

  // Get the image.
  bool useImageRegion = true;
  nsCOMPtr<imgIContainer> image;
  GetImage(aRowIndex, aColumn, false, imageContext, useImageRegion,
           getter_AddRefs(image));

  // Get the image destination size.
  nsSize imageDestSize = GetImageDestSize(imageContext, useImageRegion, image);
  if (!imageDestSize.width || !imageDestSize.height) {
    return ImgDrawResult::SUCCESS;
  }

  // Get the borders and padding.
  nsMargin bp(0, 0, 0, 0);
  GetBorderPadding(imageContext, bp);

  // destRect will be passed as the aDestRect argument in the DrawImage method.
  // Start with the imageDestSize width and height.
  nsRect destRect(0, 0, imageDestSize.width, imageDestSize.height);
  // Inflate destRect for borders and padding so that we can compare/adjust
  // with respect to imageRect.
  destRect.Inflate(bp);

  // The destRect width and height have not been adjusted to fit within the
  // cell width and height.
  // We must adjust the width even if image is null, because the width is used
  // to update the aRemainingWidth and aCurrX values.
  // Since the height isn't used unless the image is not null, we will adjust
  // the height inside the if (image) block below.

  if (destRect.width > imageRect.width) {
    // The destRect is too wide to fit within the cell width.
    // Adjust destRect width to fit within the cell width.
    destRect.width = imageRect.width;
  } else {
    // The cell is wider than the destRect.
    // In a cycler column, the image is centered horizontally.
    if (!aColumn->IsCycler()) {
      // If this column is not a cycler, we won't center the image horizontally.
      // We adjust the imageRect width so that the image is placed at the start
      // of the cell.
      imageRect.width = destRect.width;
    }
  }

  ImgDrawResult result = ImgDrawResult::SUCCESS;

  if (image) {
    if (isRTL) imageRect.x = rightEdge - imageRect.width;
    // Paint our borders and background for our image rect
    result &= PaintBackgroundLayer(imageContext, aPresContext,
                                   aRenderingContext, imageRect, aDirtyRect);

    // The destRect x and y have not been set yet. Let's do that now.
    // Initially, we use the imageRect x and y.
    destRect.x = imageRect.x;
    destRect.y = imageRect.y;

    if (destRect.width < imageRect.width) {
      // The destRect width is smaller than the cell width.
      // Center the image horizontally in the cell.
      // Adjust the destRect x accordingly.
      destRect.x += (imageRect.width - destRect.width) / 2;
    }

    // Now it's time to adjust the destRect height to fit within the cell
    // height.
    if (destRect.height > imageRect.height) {
      // The destRect height is larger than the cell height.
      // Adjust destRect height to fit within the cell height.
      destRect.height = imageRect.height;
    } else if (destRect.height < imageRect.height) {
      // The destRect height is smaller than the cell height.
      // Center the image vertically in the cell.
      // Adjust the destRect y accordingly.
      destRect.y += (imageRect.height - destRect.height) / 2;
    }

    // It's almost time to paint the image.
    // Deflate destRect for the border and padding.
    destRect.Deflate(bp);

    // Compute the area where our whole image would be mapped, to get the
    // desired subregion onto our actual destRect:
    nsRect wholeImageDest;
    CSSIntSize rawImageCSSIntSize;
    if (NS_SUCCEEDED(image->GetWidth(&rawImageCSSIntSize.width)) &&
        NS_SUCCEEDED(image->GetHeight(&rawImageCSSIntSize.height))) {
      // Get the image source rectangle - the rectangle containing the part of
      // the image that we are going to display.  sourceRect will be passed as
      // the aSrcRect argument in the DrawImage method.
      nsRect sourceRect =
          GetImageSourceRect(imageContext, useImageRegion, image);

      // Let's say that the image is 100 pixels tall and that the CSS has
      // specified that the destination height should be 50 pixels tall. Let's
      // say that the cell height is only 20 pixels. So, in those 20 visible
      // pixels, we want to see the top 20/50ths of the image.  So, the
      // sourceRect.height should be 100 * 20 / 50, which is 40 pixels.
      // Essentially, we are scaling the image as dictated by the CSS
      // destination height and width, and we are then clipping the scaled
      // image by the cell width and height.
      nsSize rawImageSize(CSSPixel::ToAppUnits(rawImageCSSIntSize));
      wholeImageDest = nsLayoutUtils::GetWholeImageDestination(
          rawImageSize, sourceRect, nsRect(destRect.TopLeft(), imageDestSize));
    } else {
      // GetWidth/GetHeight failed, so we can't easily map a subregion of the
      // source image onto the destination area.
      // * If this happens with a RasterImage, it probably means the image is
      // in an error state, and we shouldn't draw anything. Hence, we leave
      // wholeImageDest as an empty rect (its initial state).
      // * If this happens with a VectorImage, it probably means the image has
      // no explicit width or height attribute -- but we can still proceed and
      // just treat the destination area as our whole SVG image area. Hence, we
      // set wholeImageDest to the full destRect.
      if (image->GetType() == imgIContainer::TYPE_VECTOR) {
        wholeImageDest = destRect;
      }
    }

    if (opacity != 1.0f) {
      aRenderingContext.PushGroupForBlendBack(gfxContentType::COLOR_ALPHA,
                                              opacity);
    }

    uint32_t drawFlags = aBuilder && aBuilder->IsPaintingToWindow()
                             ? imgIContainer::FLAG_HIGH_QUALITY_SCALING
                             : imgIContainer::FLAG_NONE;
    result &= nsLayoutUtils::DrawImage(
        aRenderingContext, imageContext, aPresContext, image,
        nsLayoutUtils::GetSamplingFilterForFrame(this), wholeImageDest,
        destRect, destRect.TopLeft(), aDirtyRect, drawFlags);

    if (opacity != 1.0f) {
      aRenderingContext.PopGroupAndBlend();
    }
  }

  // Update the aRemainingWidth and aCurrX values.
  imageRect.Inflate(imageMargin);
  aRemainingWidth -= imageRect.width;
  if (!isRTL) {
    aCurrX += imageRect.width;
  }

  return result;
}

ImgDrawResult nsTreeBodyFrame::PaintText(
    int32_t aRowIndex, nsTreeColumn* aColumn, const nsRect& aTextRect,
    nsPresContext* aPresContext, gfxContext& aRenderingContext,
    const nsRect& aDirtyRect, nscoord& aCurrX) {
  MOZ_ASSERT(aColumn && aColumn->GetFrame(), "invalid column passed");

  bool isRTL = StyleVisibility()->mDirection == StyleDirection::Rtl;

  // Now obtain the text for our cell.
  nsAutoString text;
  mView->GetCellText(aRowIndex, aColumn, text);

  // We're going to paint this text so we need to ensure bidi is enabled if
  // necessary
  CheckTextForBidi(text);

  ImgDrawResult result = ImgDrawResult::SUCCESS;

  if (text.Length() == 0) {
    // Don't paint an empty string. XXX What about background/borders? Still
    // paint?
    return result;
  }

  int32_t appUnitsPerDevPixel = PresContext()->AppUnitsPerDevPixel();
  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();

  // Resolve style for the text.  It contains all the info we need to lay
  // ourselves out and to paint.
  ComputedStyle* textContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeCellText());

  // Obtain opacity value for the image.
  float opacity = textContext->StyleEffects()->mOpacity;

  // Obtain the margins for the text and then deflate our rect by that
  // amount.  The text is assumed to be contained within the deflated rect.
  nsRect textRect(aTextRect);
  nsMargin textMargin;
  textContext->StyleMargin()->GetMargin(textMargin);
  textRect.Deflate(textMargin);

  // Adjust the rect for its border and padding.
  nsMargin bp(0, 0, 0, 0);
  GetBorderPadding(textContext, bp);
  textRect.Deflate(bp);

  // Compute our text size.
  RefPtr<nsFontMetrics> fontMet =
      nsLayoutUtils::GetFontMetricsForComputedStyle(textContext, PresContext());

  nscoord height = fontMet->MaxHeight();
  nscoord baseline = fontMet->MaxAscent();

  // Center the text. XXX Obey vertical-align style prop?
  if (height < textRect.height) {
    textRect.y += (textRect.height - height) / 2;
    textRect.height = height;
  }

  // Set our font.
  AdjustForCellText(text, aRowIndex, aColumn, aRenderingContext, *fontMet,
                    textRect);
  textRect.Inflate(bp);

  // Subtract out the remaining width.
  if (!isRTL) aCurrX += textRect.width + textMargin.LeftRight();

  result &= PaintBackgroundLayer(textContext, aPresContext, aRenderingContext,
                                 textRect, aDirtyRect);

  // Time to paint our text.
  textRect.Deflate(bp);

  // Set our color.
  ColorPattern color(ToDeviceColor(textContext->StyleText()->mColor));

  // Draw decorations.
  StyleTextDecorationLine decorations =
      textContext->StyleTextReset()->mTextDecorationLine;

  nscoord offset;
  nscoord size;
  if (decorations & (StyleTextDecorationLine::OVERLINE |
                     StyleTextDecorationLine::UNDERLINE)) {
    fontMet->GetUnderline(offset, size);
    if (decorations & StyleTextDecorationLine::OVERLINE) {
      nsRect r(textRect.x, textRect.y, textRect.width, size);
      Rect devPxRect = NSRectToSnappedRect(r, appUnitsPerDevPixel, *drawTarget);
      drawTarget->FillRect(devPxRect, color);
    }
    if (decorations & StyleTextDecorationLine::UNDERLINE) {
      nsRect r(textRect.x, textRect.y + baseline - offset, textRect.width,
               size);
      Rect devPxRect = NSRectToSnappedRect(r, appUnitsPerDevPixel, *drawTarget);
      drawTarget->FillRect(devPxRect, color);
    }
  }
  if (decorations & StyleTextDecorationLine::LINE_THROUGH) {
    fontMet->GetStrikeout(offset, size);
    nsRect r(textRect.x, textRect.y + baseline - offset, textRect.width, size);
    Rect devPxRect = NSRectToSnappedRect(r, appUnitsPerDevPixel, *drawTarget);
    drawTarget->FillRect(devPxRect, color);
  }
  ComputedStyle* cellContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeCell());

  if (opacity != 1.0f) {
    aRenderingContext.PushGroupForBlendBack(gfxContentType::COLOR_ALPHA,
                                            opacity);
  }

  aRenderingContext.SetColor(
      sRGBColor::FromABGR(textContext->StyleText()->mColor.ToColor()));
  nsLayoutUtils::DrawString(
      this, *fontMet, &aRenderingContext, text.get(), text.Length(),
      textRect.TopLeft() + nsPoint(0, baseline), cellContext);

  if (opacity != 1.0f) {
    aRenderingContext.PopGroupAndBlend();
  }

  return result;
}

ImgDrawResult nsTreeBodyFrame::PaintCheckbox(int32_t aRowIndex,
                                             nsTreeColumn* aColumn,
                                             const nsRect& aCheckboxRect,
                                             nsPresContext* aPresContext,
                                             gfxContext& aRenderingContext,
                                             const nsRect& aDirtyRect) {
  MOZ_ASSERT(aColumn && aColumn->GetFrame(), "invalid column passed");

  // Resolve style for the checkbox.
  ComputedStyle* checkboxContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeCheckbox());

  nscoord rightEdge = aCheckboxRect.XMost();

  // Obtain the margins for the checkbox and then deflate our rect by that
  // amount.  The checkbox is assumed to be contained within the deflated rect.
  nsRect checkboxRect(aCheckboxRect);
  nsMargin checkboxMargin;
  checkboxContext->StyleMargin()->GetMargin(checkboxMargin);
  checkboxRect.Deflate(checkboxMargin);

  nsRect imageSize = GetImageSize(aRowIndex, aColumn, true, checkboxContext);

  if (imageSize.height > checkboxRect.height) {
    imageSize.height = checkboxRect.height;
  }
  if (imageSize.width > checkboxRect.width) {
    imageSize.width = checkboxRect.width;
  }

  if (StyleVisibility()->mDirection == StyleDirection::Rtl) {
    checkboxRect.x = rightEdge - checkboxRect.width;
  }

  // Paint our borders and background for our image rect.
  ImgDrawResult result =
      PaintBackgroundLayer(checkboxContext, aPresContext, aRenderingContext,
                           checkboxRect, aDirtyRect);

  // Time to paint the checkbox.
  // Adjust the rect for its border and padding.
  nsMargin bp(0, 0, 0, 0);
  GetBorderPadding(checkboxContext, bp);
  checkboxRect.Deflate(bp);

  // Get the image for drawing.
  nsCOMPtr<imgIContainer> image;
  bool useImageRegion = true;
  GetImage(aRowIndex, aColumn, true, checkboxContext, useImageRegion,
           getter_AddRefs(image));
  if (image) {
    nsPoint pt = checkboxRect.TopLeft();

    if (imageSize.height < checkboxRect.height) {
      pt.y += (checkboxRect.height - imageSize.height) / 2;
    }

    if (imageSize.width < checkboxRect.width) {
      pt.x += (checkboxRect.width - imageSize.width) / 2;
    }

    // Apply context paint if applicable
    Maybe<SVGImageContext> svgContext;
    SVGImageContext::MaybeStoreContextPaint(svgContext, checkboxContext, image);
    // Paint the image.
    result &= nsLayoutUtils::DrawSingleUnscaledImage(
        aRenderingContext, aPresContext, image, SamplingFilter::POINT, pt,
        &aDirtyRect, svgContext, imgIContainer::FLAG_NONE, &imageSize);
  }

  return result;
}

ImgDrawResult nsTreeBodyFrame::PaintDropFeedback(
    const nsRect& aDropFeedbackRect, nsPresContext* aPresContext,
    gfxContext& aRenderingContext, const nsRect& aDirtyRect, nsPoint aPt) {
  // Paint the drop feedback in between rows.

  nscoord currX;

  // Adjust for the primary cell.
  nsTreeColumn* primaryCol = mColumns->GetPrimaryColumn();

  if (primaryCol) {
#ifdef DEBUG
    nsresult rv =
#endif
        primaryCol->GetXInTwips(this, &currX);
    NS_ASSERTION(NS_SUCCEEDED(rv), "primary column is invalid?");

    currX += aPt.x - mHorzPosition;
  } else {
    currX = aDropFeedbackRect.x;
  }

  PrefillPropertyArray(mSlots->mDropRow, primaryCol);

  // Resolve the style to use for the drop feedback.
  ComputedStyle* feedbackContext =
      GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeDropFeedback());

  ImgDrawResult result = ImgDrawResult::SUCCESS;

  // Paint only if it is visible.
  if (feedbackContext->StyleVisibility()->IsVisibleOrCollapsed()) {
    int32_t level;
    mView->GetLevel(mSlots->mDropRow, &level);

    // If our previous or next row has greater level use that for
    // correct visual indentation.
    if (mSlots->mDropOrient == nsITreeView::DROP_BEFORE) {
      if (mSlots->mDropRow > 0) {
        int32_t previousLevel;
        mView->GetLevel(mSlots->mDropRow - 1, &previousLevel);
        if (previousLevel > level) level = previousLevel;
      }
    } else {
      if (mSlots->mDropRow < mRowCount - 1) {
        int32_t nextLevel;
        mView->GetLevel(mSlots->mDropRow + 1, &nextLevel);
        if (nextLevel > level) level = nextLevel;
      }
    }

    currX += mIndentation * level;

    if (primaryCol) {
      ComputedStyle* twistyContext =
          GetPseudoComputedStyle(nsCSSAnonBoxes::mozTreeTwisty());
      nsRect imageSize;
      nsRect twistyRect;
      GetTwistyRect(mSlots->mDropRow, primaryCol, imageSize, twistyRect,
                    aPresContext, twistyContext);
      nsMargin twistyMargin;
      twistyContext->StyleMargin()->GetMargin(twistyMargin);
      twistyRect.Inflate(twistyMargin);
      currX += twistyRect.width;
    }

    const nsStylePosition* stylePosition = feedbackContext->StylePosition();

    // Obtain the width for the drop feedback or use default value.
    nscoord width;
    if (stylePosition->mWidth.ConvertsToLength()) {
      width = stylePosition->mWidth.ToLength();
    } else {
      // Use default width 50px.
      width = nsPresContext::CSSPixelsToAppUnits(50);
    }

    // Obtain the height for the drop feedback or use default value.
    nscoord height;
    if (stylePosition->mHeight.ConvertsToLength()) {
      height = stylePosition->mHeight.ToLength();
    } else {
      // Use default height 2px.
      height = nsPresContext::CSSPixelsToAppUnits(2);
    }

    // Obtain the margins for the drop feedback and then deflate our rect
    // by that amount.
    nsRect feedbackRect(currX, aDropFeedbackRect.y, width, height);
    nsMargin margin;
    feedbackContext->StyleMargin()->GetMargin(margin);
    feedbackRect.Deflate(margin);

    feedbackRect.y += (aDropFeedbackRect.height - height) / 2;

    // Finally paint the drop feedback.
    result &= PaintBackgroundLayer(feedbackContext, aPresContext,
                                   aRenderingContext, feedbackRect, aDirtyRect);
  }

  return result;
}

ImgDrawResult nsTreeBodyFrame::PaintBackgroundLayer(
    ComputedStyle* aComputedStyle, nsPresContext* aPresContext,
    gfxContext& aRenderingContext, const nsRect& aRect,
    const nsRect& aDirtyRect) {
  const nsStyleBorder* myBorder = aComputedStyle->StyleBorder();
  nsCSSRendering::PaintBGParams params =
      nsCSSRendering::PaintBGParams::ForAllLayers(
          *aPresContext, aDirtyRect, aRect, this,
          nsCSSRendering::PAINTBG_SYNC_DECODE_IMAGES);
  ImgDrawResult result = nsCSSRendering::PaintStyleImageLayerWithSC(
      params, aRenderingContext, aComputedStyle, *myBorder);

  result &= nsCSSRendering::PaintBorderWithStyleBorder(
      aPresContext, aRenderingContext, this, aDirtyRect, aRect, *myBorder,
      mComputedStyle, PaintBorderFlags::SyncDecodeImages);

  nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                               aDirtyRect, aRect, aComputedStyle);

  return result;
}

// Scrolling
nsresult nsTreeBodyFrame::EnsureRowIsVisible(int32_t aRow) {
  ScrollParts parts = GetScrollParts();
  nsresult rv = EnsureRowIsVisibleInternal(parts, aRow);
  NS_ENSURE_SUCCESS(rv, rv);
  UpdateScrollbars(parts);
  return rv;
}

nsresult nsTreeBodyFrame::EnsureRowIsVisibleInternal(const ScrollParts& aParts,
                                                     int32_t aRow) {
  if (!mView || !mPageLength) return NS_OK;

  if (mTopRowIndex <= aRow && mTopRowIndex + mPageLength > aRow) return NS_OK;

  if (aRow < mTopRowIndex)
    ScrollToRowInternal(aParts, aRow);
  else {
    // Bring it just on-screen.
    int32_t distance = aRow - (mTopRowIndex + mPageLength) + 1;
    ScrollToRowInternal(aParts, mTopRowIndex + distance);
  }

  return NS_OK;
}

nsresult nsTreeBodyFrame::EnsureCellIsVisible(int32_t aRow,
                                              nsTreeColumn* aCol) {
  if (!aCol) return NS_ERROR_INVALID_ARG;

  ScrollParts parts = GetScrollParts();

  nscoord result = -1;
  nsresult rv;

  nscoord columnPos;
  rv = aCol->GetXInTwips(this, &columnPos);
  if (NS_FAILED(rv)) return rv;

  nscoord columnWidth;
  rv = aCol->GetWidthInTwips(this, &columnWidth);
  if (NS_FAILED(rv)) return rv;

  // If the start of the column is before the
  // start of the horizontal view, then scroll
  if (columnPos < mHorzPosition) result = columnPos;
  // If the end of the column is past the end of
  // the horizontal view, then scroll
  else if ((columnPos + columnWidth) > (mHorzPosition + mInnerBox.width))
    result = ((columnPos + columnWidth) - (mHorzPosition + mInnerBox.width)) +
             mHorzPosition;

  if (result != -1) {
    rv = ScrollHorzInternal(parts, result);
    if (NS_FAILED(rv)) return rv;
  }

  rv = EnsureRowIsVisibleInternal(parts, aRow);
  NS_ENSURE_SUCCESS(rv, rv);
  UpdateScrollbars(parts);
  return rv;
}

void nsTreeBodyFrame::ScrollToRow(int32_t aRow) {
  ScrollParts parts = GetScrollParts();
  ScrollToRowInternal(parts, aRow);
  UpdateScrollbars(parts);
}

nsresult nsTreeBodyFrame::ScrollToRowInternal(const ScrollParts& aParts,
                                              int32_t aRow) {
  ScrollInternal(aParts, aRow);

  return NS_OK;
}

void nsTreeBodyFrame::ScrollByLines(int32_t aNumLines) {
  if (!mView) {
    return;
  }
  int32_t newIndex = mTopRowIndex + aNumLines;
  ScrollToRow(newIndex);
}

void nsTreeBodyFrame::ScrollByPages(int32_t aNumPages) {
  if (!mView) {
    return;
  }
  int32_t newIndex = mTopRowIndex + aNumPages * mPageLength;
  ScrollToRow(newIndex);
}

nsresult nsTreeBodyFrame::ScrollInternal(const ScrollParts& aParts,
                                         int32_t aRow) {
  if (!mView) {
    return NS_OK;
  }

  // Note that we may be "over scrolled" at this point; that is the
  // current mTopRowIndex may be larger than mRowCount - mPageLength.
  // This can happen when items are removed for example. (bug 1085050)

  int32_t maxTopRowIndex = std::max(0, mRowCount - mPageLength);
  aRow = mozilla::clamped(aRow, 0, maxTopRowIndex);
  if (aRow == mTopRowIndex) {
    return NS_OK;
  }
  mTopRowIndex = aRow;
  Invalidate();
  PostScrollEvent();
  return NS_OK;
}

nsresult nsTreeBodyFrame::ScrollHorzInternal(const ScrollParts& aParts,
                                             int32_t aPosition) {
  if (!mView || !aParts.mColumnsScrollFrame || !aParts.mHScrollbar)
    return NS_OK;

  if (aPosition == mHorzPosition) return NS_OK;

  if (aPosition < 0 || aPosition > mHorzWidth) return NS_OK;

  nsRect bounds = aParts.mColumnsFrame->GetRect();
  if (aPosition > (mHorzWidth - bounds.width))
    aPosition = mHorzWidth - bounds.width;

  mHorzPosition = aPosition;

  Invalidate();

  // Update the column scroll view
  AutoWeakFrame weakFrame(this);
  aParts.mColumnsScrollFrame->ScrollTo(nsPoint(mHorzPosition, 0),
                                       ScrollMode::Instant);
  if (!weakFrame.IsAlive()) {
    return NS_ERROR_FAILURE;
  }
  // And fire off an event about it all
  PostScrollEvent();
  return NS_OK;
}

void nsTreeBodyFrame::ScrollByPage(nsScrollbarFrame* aScrollbar,
                                   int32_t aDirection,
                                   nsIScrollbarMediator::ScrollSnapMode aSnap) {
  // CSS Scroll Snapping is not enabled for XUL, aSnap is ignored
  MOZ_ASSERT(aScrollbar != nullptr);
  ScrollByPages(aDirection);
}

void nsTreeBodyFrame::ScrollByWhole(
    nsScrollbarFrame* aScrollbar, int32_t aDirection,
    nsIScrollbarMediator::ScrollSnapMode aSnap) {
  // CSS Scroll Snapping is not enabled for XUL, aSnap is ignored
  MOZ_ASSERT(aScrollbar != nullptr);
  int32_t newIndex = aDirection < 0 ? 0 : mTopRowIndex;
  ScrollToRow(newIndex);
}

void nsTreeBodyFrame::ScrollByLine(nsScrollbarFrame* aScrollbar,
                                   int32_t aDirection,
                                   nsIScrollbarMediator::ScrollSnapMode aSnap) {
  // CSS Scroll Snapping is not enabled for XUL, aSnap is ignored
  MOZ_ASSERT(aScrollbar != nullptr);
  ScrollByLines(aDirection);
}

void nsTreeBodyFrame::RepeatButtonScroll(nsScrollbarFrame* aScrollbar) {
  ScrollParts parts = GetScrollParts();
  int32_t increment = aScrollbar->GetIncrement();
  int32_t direction = 0;
  if (increment < 0) {
    direction = -1;
  } else if (increment > 0) {
    direction = 1;
  }
  bool isHorizontal = aScrollbar->IsXULHorizontal();

  AutoWeakFrame weakFrame(this);
  if (isHorizontal) {
    int32_t curpos = aScrollbar->MoveToNewPosition();
    if (weakFrame.IsAlive()) {
      ScrollHorzInternal(parts, curpos);
    }
  } else {
    ScrollToRowInternal(parts, mTopRowIndex + direction);
  }

  if (weakFrame.IsAlive() && mScrollbarActivity) {
    mScrollbarActivity->ActivityOccurred();
  }
  if (weakFrame.IsAlive()) {
    UpdateScrollbars(parts);
  }
}

void nsTreeBodyFrame::ThumbMoved(nsScrollbarFrame* aScrollbar, nscoord aOldPos,
                                 nscoord aNewPos) {
  ScrollParts parts = GetScrollParts();

  if (aOldPos == aNewPos) return;

  AutoWeakFrame weakFrame(this);

  // Vertical Scrollbar
  if (parts.mVScrollbar == aScrollbar) {
    nscoord rh = nsPresContext::AppUnitsToIntCSSPixels(mRowHeight);
    nscoord newIndex = nsPresContext::AppUnitsToIntCSSPixels(aNewPos);
    nscoord newrow = (rh > 0) ? (newIndex / rh) : 0;
    ScrollInternal(parts, newrow);
    // Horizontal Scrollbar
  } else if (parts.mHScrollbar == aScrollbar) {
    int32_t newIndex = nsPresContext::AppUnitsToIntCSSPixels(aNewPos);
    ScrollHorzInternal(parts, newIndex);
  }
  if (weakFrame.IsAlive()) {
    UpdateScrollbars(parts);
  }
}

// The style cache.
ComputedStyle* nsTreeBodyFrame::GetPseudoComputedStyle(
    nsCSSAnonBoxPseudoStaticAtom* aPseudoElement) {
  return mStyleCache.GetComputedStyle(PresContext(), mContent, mComputedStyle,
                                      aPseudoElement, mScratchArray);
}

XULTreeElement* nsTreeBodyFrame::GetBaseElement() {
  if (!mTree) {
    nsIFrame* parent = GetParent();
    while (parent) {
      nsIContent* content = parent->GetContent();
      if (content && content->IsXULElement(nsGkAtoms::tree)) {
        mTree = XULTreeElement::FromNodeOrNull(content->AsElement());
        break;
      }

      parent = parent->GetInFlowParent();
    }
  }

  return mTree;
}

nsresult nsTreeBodyFrame::ClearStyleAndImageCaches() {
  mStyleCache.Clear();
  CancelImageRequests();
  mImageCache.Clear();
  return NS_OK;
}

void nsTreeBodyFrame::RemoveImageCacheEntry(int32_t aRowIndex,
                                            nsTreeColumn* aCol) {
  nsAutoString imageSrc;
  if (NS_SUCCEEDED(mView->GetImageSrc(aRowIndex, aCol, imageSrc))) {
    nsTreeImageCacheEntry entry;
    if (mImageCache.Get(imageSrc, &entry)) {
      nsLayoutUtils::DeregisterImageRequest(PresContext(), entry.request,
                                            nullptr);
      entry.request->UnlockImage();
      entry.request->CancelAndForgetObserver(NS_BINDING_ABORTED);
      mImageCache.Remove(imageSrc);
    }
  }
}

/* virtual */
void nsTreeBodyFrame::DidSetComputedStyle(ComputedStyle* aOldComputedStyle) {
  nsLeafBoxFrame::DidSetComputedStyle(aOldComputedStyle);

  // Clear the style cache; the pointers are no longer even valid
  mStyleCache.Clear();
  // XXX The following is hacky, but it's not incorrect,
  // and appears to fix a few bugs with style changes, like text zoom and
  // dpi changes
  mIndentation = GetIndentation();
  mRowHeight = GetRowHeight();
  mStringWidth = -1;
}

bool nsTreeBodyFrame::OffsetForHorzScroll(nsRect& rect, bool clip) {
  rect.x -= mHorzPosition;

  // Scrolled out before
  if (rect.XMost() <= mInnerBox.x) return false;

  // Scrolled out after
  if (rect.x > mInnerBox.XMost()) return false;

  if (clip) {
    nscoord leftEdge = std::max(rect.x, mInnerBox.x);
    nscoord rightEdge = std::min(rect.XMost(), mInnerBox.XMost());
    rect.x = leftEdge;
    rect.width = rightEdge - leftEdge;

    // Should have returned false above
    NS_ASSERTION(rect.width >= 0, "horz scroll code out of sync");
  }

  return true;
}

bool nsTreeBodyFrame::CanAutoScroll(int32_t aRowIndex) {
  // Check first for partially visible last row.
  if (aRowIndex == mRowCount - 1) {
    nscoord y = mInnerBox.y + (aRowIndex - mTopRowIndex) * mRowHeight;
    if (y < mInnerBox.height && y + mRowHeight > mInnerBox.height) return true;
  }

  if (aRowIndex > 0 && aRowIndex < mRowCount - 1) return true;

  return false;
}

// Given a dom event, figure out which row in the tree the mouse is over,
// if we should drop before/after/on that row or we should auto-scroll.
// Doesn't query the content about if the drag is allowable, that's done
// elsewhere.
//
// For containers, we break up the vertical space of the row as follows: if in
// the topmost 25%, the drop is _before_ the row the mouse is over; if in the
// last 25%, _after_; in the middle 50%, we consider it a drop _on_ the
// container.
//
// For non-containers, if the mouse is in the top 50% of the row, the drop is
// _before_ and the bottom 50% _after_
void nsTreeBodyFrame::ComputeDropPosition(WidgetGUIEvent* aEvent, int32_t* aRow,
                                          int16_t* aOrient,
                                          int16_t* aScrollLines) {
  *aOrient = -1;
  *aScrollLines = 0;

  // Convert the event's point to our coordinates.  We want it in
  // the coordinates of our inner box's coordinates.
  nsPoint pt =
      nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, RelativeTo{this});
  int32_t xTwips = pt.x - mInnerBox.x;
  int32_t yTwips = pt.y - mInnerBox.y;

  *aRow = GetRowAtInternal(xTwips, yTwips);
  if (*aRow >= 0) {
    // Compute the top/bottom of the row in question.
    int32_t yOffset = yTwips - mRowHeight * (*aRow - mTopRowIndex);

    bool isContainer = false;
    mView->IsContainer(*aRow, &isContainer);
    if (isContainer) {
      // for a container, use a 25%/50%/25% breakdown
      if (yOffset < mRowHeight / 4)
        *aOrient = nsITreeView::DROP_BEFORE;
      else if (yOffset > mRowHeight - (mRowHeight / 4))
        *aOrient = nsITreeView::DROP_AFTER;
      else
        *aOrient = nsITreeView::DROP_ON;
    } else {
      // for a non-container use a 50%/50% breakdown
      if (yOffset < mRowHeight / 2)
        *aOrient = nsITreeView::DROP_BEFORE;
      else
        *aOrient = nsITreeView::DROP_AFTER;
    }
  }

  if (CanAutoScroll(*aRow)) {
    // Get the max value from the look and feel service.
    int32_t scrollLinesMax =
        LookAndFeel::GetInt(LookAndFeel::eIntID_TreeScrollLinesMax, 0);
    scrollLinesMax--;
    if (scrollLinesMax < 0) scrollLinesMax = 0;

    // Determine if we're w/in a margin of the top/bottom of the tree during a
    // drag. This will ultimately cause us to scroll, but that's done elsewhere.
    nscoord height = (3 * mRowHeight) / 4;
    if (yTwips < height) {
      // scroll up
      *aScrollLines =
          NSToIntRound(-scrollLinesMax * (1 - (float)yTwips / height) - 1);
    } else if (yTwips > mRect.height - height) {
      // scroll down
      *aScrollLines = NSToIntRound(
          scrollLinesMax * (1 - (float)(mRect.height - yTwips) / height) + 1);
    }
  }
}  // ComputeDropPosition

void nsTreeBodyFrame::OpenCallback(nsITimer* aTimer, void* aClosure) {
  nsTreeBodyFrame* self = static_cast<nsTreeBodyFrame*>(aClosure);
  if (self) {
    aTimer->Cancel();
    self->mSlots->mTimer = nullptr;

    if (self->mSlots->mDropRow >= 0) {
      self->mSlots->mArray.AppendElement(self->mSlots->mDropRow);
      self->mView->ToggleOpenState(self->mSlots->mDropRow);
    }
  }
}

void nsTreeBodyFrame::CloseCallback(nsITimer* aTimer, void* aClosure) {
  nsTreeBodyFrame* self = static_cast<nsTreeBodyFrame*>(aClosure);
  if (self) {
    aTimer->Cancel();
    self->mSlots->mTimer = nullptr;

    for (uint32_t i = self->mSlots->mArray.Length(); i--;) {
      if (self->mView) self->mView->ToggleOpenState(self->mSlots->mArray[i]);
    }
    self->mSlots->mArray.Clear();
  }
}

void nsTreeBodyFrame::LazyScrollCallback(nsITimer* aTimer, void* aClosure) {
  nsTreeBodyFrame* self = static_cast<nsTreeBodyFrame*>(aClosure);
  if (self) {
    aTimer->Cancel();
    self->mSlots->mTimer = nullptr;

    if (self->mView) {
      // Set a new timer to scroll the tree repeatedly.
      self->CreateTimer(LookAndFeel::eIntID_TreeScrollDelay, ScrollCallback,
                        nsITimer::TYPE_REPEATING_SLACK,
                        getter_AddRefs(self->mSlots->mTimer),
                        "nsTreeBodyFrame::ScrollCallback");
      self->ScrollByLines(self->mSlots->mScrollLines);
      // ScrollByLines may have deleted |self|.
    }
  }
}

void nsTreeBodyFrame::ScrollCallback(nsITimer* aTimer, void* aClosure) {
  nsTreeBodyFrame* self = static_cast<nsTreeBodyFrame*>(aClosure);
  if (self) {
    // Don't scroll if we are already at the top or bottom of the view.
    if (self->mView && self->CanAutoScroll(self->mSlots->mDropRow)) {
      self->ScrollByLines(self->mSlots->mScrollLines);
    } else {
      aTimer->Cancel();
      self->mSlots->mTimer = nullptr;
    }
  }
}

NS_IMETHODIMP
nsTreeBodyFrame::ScrollEvent::Run() {
  if (mInner) {
    mInner->FireScrollEvent();
  }
  return NS_OK;
}

void nsTreeBodyFrame::FireScrollEvent() {
  mScrollEvent.Forget();
  WidgetGUIEvent event(true, eScroll, nullptr);
  // scroll events fired at elements don't bubble
  event.mFlags.mBubbles = false;
  EventDispatcher::Dispatch(GetContent(), PresContext(), &event);
}

void nsTreeBodyFrame::PostScrollEvent() {
  if (mScrollEvent.IsPending()) return;

  RefPtr<ScrollEvent> event = new ScrollEvent(this);
  nsresult rv =
      mContent->OwnerDoc()->Dispatch(TaskCategory::Other, do_AddRef(event));
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to dispatch ScrollEvent");
  } else {
    mScrollEvent = std::move(event);
  }
}

void nsTreeBodyFrame::ScrollbarActivityStarted() const {
  if (mScrollbarActivity) {
    mScrollbarActivity->ActivityStarted();
  }
}

void nsTreeBodyFrame::ScrollbarActivityStopped() const {
  if (mScrollbarActivity) {
    mScrollbarActivity->ActivityStopped();
  }
}

void nsTreeBodyFrame::DetachImageListeners() { mCreatedListeners.Clear(); }

void nsTreeBodyFrame::RemoveTreeImageListener(nsTreeImageListener* aListener) {
  if (aListener) {
    mCreatedListeners.RemoveEntry(aListener);
  }
}

#ifdef ACCESSIBILITY
static void InitCustomEvent(CustomEvent* aEvent, const nsAString& aType,
                            nsIWritablePropertyBag2* aDetail) {
  AutoJSAPI jsapi;
  if (!jsapi.Init(aEvent->GetParentObject())) {
    return;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> detail(cx);
  if (!ToJSValue(cx, aDetail, &detail)) {
    jsapi.ClearException();
    return;
  }

  aEvent->InitCustomEvent(cx, aType, /* aCanBubble = */ true,
                          /* aCancelable = */ false, detail);
}

void nsTreeBodyFrame::FireRowCountChangedEvent(int32_t aIndex, int32_t aCount) {
  RefPtr<XULTreeElement> tree(GetBaseElement());
  if (!tree) return;

  RefPtr<Document> doc = tree->OwnerDoc();
  MOZ_ASSERT(doc);

  RefPtr<Event> event = doc->CreateEvent(NS_LITERAL_STRING("customevent"),
                                         CallerType::System, IgnoreErrors());

  CustomEvent* treeEvent = event->AsCustomEvent();
  if (!treeEvent) {
    return;
  }

  nsCOMPtr<nsIWritablePropertyBag2> propBag(
      do_CreateInstance("@mozilla.org/hash-property-bag;1"));
  if (!propBag) {
    return;
  }

  // Set 'index' data - the row index rows are changed from.
  propBag->SetPropertyAsInt32(NS_LITERAL_STRING("index"), aIndex);

  // Set 'count' data - the number of changed rows.
  propBag->SetPropertyAsInt32(NS_LITERAL_STRING("count"), aCount);

  InitCustomEvent(treeEvent, NS_LITERAL_STRING("TreeRowCountChanged"), propBag);

  event->SetTrusted(true);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(tree, event);
  asyncDispatcher->PostDOMEvent();
}

void nsTreeBodyFrame::FireInvalidateEvent(int32_t aStartRowIdx,
                                          int32_t aEndRowIdx,
                                          nsTreeColumn* aStartCol,
                                          nsTreeColumn* aEndCol) {
  RefPtr<XULTreeElement> tree(GetBaseElement());
  if (!tree) return;

  RefPtr<Document> doc = tree->OwnerDoc();

  RefPtr<Event> event = doc->CreateEvent(NS_LITERAL_STRING("customevent"),
                                         CallerType::System, IgnoreErrors());

  CustomEvent* treeEvent = event->AsCustomEvent();
  if (!treeEvent) {
    return;
  }

  nsCOMPtr<nsIWritablePropertyBag2> propBag(
      do_CreateInstance("@mozilla.org/hash-property-bag;1"));
  if (!propBag) {
    return;
  }

  if (aStartRowIdx != -1 && aEndRowIdx != -1) {
    // Set 'startrow' data - the start index of invalidated rows.
    propBag->SetPropertyAsInt32(NS_LITERAL_STRING("startrow"), aStartRowIdx);

    // Set 'endrow' data - the end index of invalidated rows.
    propBag->SetPropertyAsInt32(NS_LITERAL_STRING("endrow"), aEndRowIdx);
  }

  if (aStartCol && aEndCol) {
    // Set 'startcolumn' data - the start index of invalidated rows.
    int32_t startColIdx = aStartCol->GetIndex();

    propBag->SetPropertyAsInt32(NS_LITERAL_STRING("startcolumn"), startColIdx);

    // Set 'endcolumn' data - the start index of invalidated rows.
    int32_t endColIdx = aEndCol->GetIndex();
    propBag->SetPropertyAsInt32(NS_LITERAL_STRING("endcolumn"), endColIdx);
  }

  InitCustomEvent(treeEvent, NS_LITERAL_STRING("TreeInvalidated"), propBag);

  event->SetTrusted(true);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(tree, event);
  asyncDispatcher->PostDOMEvent();
}
#endif

class nsOverflowChecker : public Runnable {
 public:
  explicit nsOverflowChecker(nsTreeBodyFrame* aFrame)
      : mozilla::Runnable("nsOverflowChecker"), mFrame(aFrame) {}
  NS_IMETHOD Run() override {
    if (mFrame.IsAlive()) {
      nsTreeBodyFrame* tree = static_cast<nsTreeBodyFrame*>(mFrame.GetFrame());
      nsTreeBodyFrame::ScrollParts parts = tree->GetScrollParts();
      tree->CheckOverflow(parts);
    }
    return NS_OK;
  }

 private:
  WeakFrame mFrame;
};

bool nsTreeBodyFrame::FullScrollbarsUpdate(bool aNeedsFullInvalidation) {
  ScrollParts parts = GetScrollParts();
  AutoWeakFrame weakFrame(this);
  AutoWeakFrame weakColumnsFrame(parts.mColumnsFrame);
  UpdateScrollbars(parts);
  NS_ENSURE_TRUE(weakFrame.IsAlive(), false);
  if (aNeedsFullInvalidation) {
    Invalidate();
  }
  InvalidateScrollbars(parts, weakColumnsFrame);
  NS_ENSURE_TRUE(weakFrame.IsAlive(), false);

  // Overflow checking dispatches synchronous events, which can cause infinite
  // recursion during reflow. Do the first overflow check synchronously, but
  // force any nested checks to round-trip through the event loop. See bug
  // 905909.
  RefPtr<nsOverflowChecker> checker = new nsOverflowChecker(this);
  if (!mCheckingOverflow) {
    nsContentUtils::AddScriptRunner(checker);
  } else {
    mContent->OwnerDoc()->Dispatch(TaskCategory::Other, checker.forget());
  }
  return weakFrame.IsAlive();
}

void nsTreeBodyFrame::OnImageIsAnimated(imgIRequest* aRequest) {
  nsLayoutUtils::RegisterImageRequest(PresContext(), aRequest, nullptr);
}
