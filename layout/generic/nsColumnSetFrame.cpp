/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for css3 multi-column layout */

#include "nsColumnSetFrame.h"
#include "nsCSSRendering.h"
#include "nsDisplayList.h"

using namespace mozilla;
using namespace mozilla::layout;

/**
 * Tracking issues:
 *
 * XXX cursor movement around the top and bottom of colums seems to make the editor
 * lose the caret.
 *
 * XXX should we support CSS columns applied to table elements?
 */
nsIFrame*
NS_NewColumnSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, nsFrameState aStateFlags)
{
  nsColumnSetFrame* it = new (aPresShell) nsColumnSetFrame(aContext);
  it->AddStateBits(aStateFlags | NS_BLOCK_MARGIN_ROOT);
  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsColumnSetFrame)

nsColumnSetFrame::nsColumnSetFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext), mLastBalanceHeight(NS_INTRINSICSIZE),
    mLastFrameStatus(NS_FRAME_COMPLETE)
{
}

nsIAtom*
nsColumnSetFrame::GetType() const
{
  return nsGkAtoms::columnSetFrame;
}

static void
PaintColumnRule(nsIFrame* aFrame, nsRenderingContext* aCtx,
                const nsRect& aDirtyRect, nsPoint aPt)
{
  static_cast<nsColumnSetFrame*>(aFrame)->PaintColumnRule(aCtx, aDirtyRect, aPt);
}

void
nsColumnSetFrame::PaintColumnRule(nsRenderingContext* aCtx,
                                  const nsRect& aDirtyRect,
                                  const nsPoint& aPt)
{
  nsIFrame* child = mFrames.FirstChild();
  if (!child)
    return;  // no columns

  nsIFrame* nextSibling = child->GetNextSibling();
  if (!nextSibling)
    return;  // 1 column only - this means no gap to draw on

  bool isRTL = StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
  const nsStyleColumn* colStyle = StyleColumn();

  uint8_t ruleStyle;
  // Per spec, inset => ridge and outset => groove
  if (colStyle->mColumnRuleStyle == NS_STYLE_BORDER_STYLE_INSET)
    ruleStyle = NS_STYLE_BORDER_STYLE_RIDGE;
  else if (colStyle->mColumnRuleStyle == NS_STYLE_BORDER_STYLE_OUTSET)
    ruleStyle = NS_STYLE_BORDER_STYLE_GROOVE;
  else
    ruleStyle = colStyle->mColumnRuleStyle;

  nsPresContext* presContext = PresContext();
  nscoord ruleWidth = colStyle->GetComputedColumnRuleWidth();
  if (!ruleWidth)
    return;

  nscolor ruleColor =
    GetVisitedDependentColor(eCSSProperty__moz_column_rule_color);

  // In order to re-use a large amount of code, we treat the column rule as a border.
  // We create a new border style object and fill in all the details of the column rule as
  // the left border. PaintBorder() does all the rendering for us, so we not
  // only save an enormous amount of code but we'll support all the line styles that
  // we support on borders!
  nsStyleBorder border(presContext);
  border.SetBorderWidth(NS_SIDE_LEFT, ruleWidth);
  border.SetBorderStyle(NS_SIDE_LEFT, ruleStyle);
  border.SetBorderColor(NS_SIDE_LEFT, ruleColor);

  // Get our content rect as an absolute coordinate, not relative to
  // our parent (which is what the X and Y normally is)
  nsRect contentRect = GetContentRect() - GetRect().TopLeft() + aPt;
  nsSize ruleSize(ruleWidth, contentRect.height);

  while (nextSibling) {
    // The frame tree goes RTL in RTL
    nsIFrame* leftSibling = isRTL ? nextSibling : child;
    nsIFrame* rightSibling = isRTL ? child : nextSibling;

    // Each child frame's position coordinates is actually relative to this nsColumnSetFrame.
    // linePt will be at the top-left edge to paint the line.
    nsPoint edgeOfLeftSibling = leftSibling->GetRect().TopRight() + aPt;
    nsPoint edgeOfRightSibling = rightSibling->GetRect().TopLeft() + aPt;
    nsPoint linePt((edgeOfLeftSibling.x + edgeOfRightSibling.x - ruleWidth) / 2,
                   contentRect.y);

    nsRect lineRect(linePt, ruleSize);
    nsCSSRendering::PaintBorderWithStyleBorder(presContext, *aCtx, this,
        aDirtyRect, lineRect, border, StyleContext(),
        // Remember, we only have the "left" "border". Skip everything else
        (1 << NS_SIDE_TOP | 1 << NS_SIDE_RIGHT | 1 << NS_SIDE_BOTTOM));

    child = nextSibling;
    nextSibling = nextSibling->GetNextSibling();
  }
}

NS_IMETHODIMP
nsColumnSetFrame::SetInitialChildList(ChildListID     aListID,
                                      nsFrameList&    aChildList)
{
  if (aListID == kAbsoluteList) {
    return nsContainerFrame::SetInitialChildList(aListID, aChildList);
  }

  NS_ASSERTION(aListID == kPrincipalList,
               "Only default child list supported");
  NS_ASSERTION(aChildList.OnlyChild(),
               "initial child list must have exaisRevertingctly one child");
  // Queue up the frames for the content frame
  return nsContainerFrame::SetInitialChildList(kPrincipalList, aChildList);
}

static nscoord
GetAvailableContentWidth(const nsHTMLReflowState& aReflowState)
{
  if (aReflowState.AvailableWidth() == NS_INTRINSICSIZE) {
    return NS_INTRINSICSIZE;
  }
  nscoord borderPaddingWidth =
    aReflowState.ComputedPhysicalBorderPadding().left +
    aReflowState.ComputedPhysicalBorderPadding().right;
  return std::max(0, aReflowState.AvailableWidth() - borderPaddingWidth);
}

nscoord
nsColumnSetFrame::GetAvailableContentHeight(const nsHTMLReflowState& aReflowState)
{
  if (aReflowState.AvailableHeight() == NS_INTRINSICSIZE) {
    return NS_INTRINSICSIZE;
  }

  nsMargin bp = aReflowState.ComputedPhysicalBorderPadding();
  ApplySkipSides(bp, &aReflowState);
  bp.bottom = aReflowState.ComputedPhysicalBorderPadding().bottom;
  return std::max(0, aReflowState.AvailableHeight() - bp.TopBottom());
}

static nscoord
GetColumnGap(nsColumnSetFrame*    aFrame,
             const nsStyleColumn* aColStyle)
{
  if (eStyleUnit_Normal == aColStyle->mColumnGap.GetUnit())
    return aFrame->StyleFont()->mFont.size;
  if (eStyleUnit_Coord == aColStyle->mColumnGap.GetUnit()) {
    nscoord colGap = aColStyle->mColumnGap.GetCoordValue();
    NS_ASSERTION(colGap >= 0, "negative column gap");
    return colGap;
  }

  NS_NOTREACHED("Unknown gap type");
  return 0;
}

nsColumnSetFrame::ReflowConfig
nsColumnSetFrame::ChooseColumnStrategy(const nsHTMLReflowState& aReflowState,
                                       bool aForceAuto = false,
                                       nscoord aFeasibleHeight = NS_INTRINSICSIZE,
                                       nscoord aInfeasibleHeight = 0)

{
  nscoord knownFeasibleHeight = aFeasibleHeight;
  nscoord knownInfeasibleHeight = aInfeasibleHeight;

  const nsStyleColumn* colStyle = StyleColumn();
  nscoord availContentWidth = GetAvailableContentWidth(aReflowState);
  if (aReflowState.ComputedWidth() != NS_INTRINSICSIZE) {
    availContentWidth = aReflowState.ComputedWidth();
  }

  nscoord consumedHeight = GetConsumedHeight();

  // The effective computed height is the height of the current continuation
  // of the column set frame. This should be the same as the computed height
  // if we have an unconstrained available height.
  nscoord computedHeight = GetEffectiveComputedHeight(aReflowState,
                                                      consumedHeight);
  nscoord colHeight = GetAvailableContentHeight(aReflowState);

  if (aReflowState.ComputedHeight() != NS_INTRINSICSIZE) {
    colHeight = aReflowState.ComputedHeight();
  } else if (aReflowState.ComputedMaxHeight() != NS_INTRINSICSIZE) {
    colHeight = std::min(colHeight, aReflowState.ComputedMaxHeight());
  }

  nscoord colGap = GetColumnGap(this, colStyle);
  int32_t numColumns = colStyle->mColumnCount;

  // If column-fill is set to 'balance', then we want to balance the columns.
  const bool isBalancing = colStyle->mColumnFill == NS_STYLE_COLUMN_FILL_BALANCE
                           && !aForceAuto;
  if (isBalancing) {
    const uint32_t MAX_NESTED_COLUMN_BALANCING = 2;
    uint32_t cnt = 0;
    for (const nsHTMLReflowState* rs = aReflowState.parentReflowState;
         rs && cnt < MAX_NESTED_COLUMN_BALANCING; rs = rs->parentReflowState) {
      if (rs->mFlags.mIsColumnBalancing) {
        ++cnt;
      }
    }
    if (cnt == MAX_NESTED_COLUMN_BALANCING) {
      numColumns = 1;
    }
  }

  nscoord colWidth;
  if (colStyle->mColumnWidth.GetUnit() == eStyleUnit_Coord) {
    colWidth = colStyle->mColumnWidth.GetCoordValue();
    NS_ASSERTION(colWidth >= 0, "negative column width");
    // Reduce column count if necessary to make columns fit in the
    // available width. Compute max number of columns that fit in
    // availContentWidth, satisfying colGap*(maxColumns - 1) +
    // colWidth*maxColumns <= availContentWidth
    if (availContentWidth != NS_INTRINSICSIZE && colGap + colWidth > 0
        && numColumns > 0) {
      // This expression uses truncated rounding, which is what we
      // want
      int32_t maxColumns =
        std::min(nscoord(nsStyleColumn::kMaxColumnCount),
                 (availContentWidth + colGap)/(colGap + colWidth));
      numColumns = std::max(1, std::min(numColumns, maxColumns));
    }
  } else if (numColumns > 0 && availContentWidth != NS_INTRINSICSIZE) {
    nscoord widthMinusGaps = availContentWidth - colGap*(numColumns - 1);
    colWidth = widthMinusGaps/numColumns;
  } else {
    colWidth = NS_INTRINSICSIZE;
  }
  // Take care of the situation where there's only one column but it's
  // still too wide
  colWidth = std::max(1, std::min(colWidth, availContentWidth));

  nscoord expectedWidthLeftOver = 0;

  if (colWidth != NS_INTRINSICSIZE && availContentWidth != NS_INTRINSICSIZE) {
    // distribute leftover space

    // First, determine how many columns will be showing if the column
    // count is auto
    if (numColumns <= 0) {
      // choose so that colGap*(nominalColumnCount - 1) +
      // colWidth*nominalColumnCount is nearly availContentWidth
      // make sure to round down
      if (colGap + colWidth > 0) {
        numColumns = (availContentWidth + colGap)/(colGap + colWidth);
        // The number of columns should never exceed kMaxColumnCount.
        numColumns = std::min(nscoord(nsStyleColumn::kMaxColumnCount),
                              numColumns);
      }
      if (numColumns <= 0) {
        numColumns = 1;
      }
    }

    // Compute extra space and divide it among the columns
    nscoord extraSpace =
      std::max(0, availContentWidth - (colWidth*numColumns + colGap*(numColumns - 1)));
    nscoord extraToColumns = extraSpace/numColumns;
    colWidth += extraToColumns;
    expectedWidthLeftOver = extraSpace - (extraToColumns*numColumns);
  }

  if (isBalancing) {
    if (numColumns <= 0) {
      // Hmm, auto column count, column width or available width is unknown,
      // and balancing is required. Let's just use one column then.
      numColumns = 1;
    }
    colHeight = std::min(mLastBalanceHeight, colHeight);
  } else {
    // This is the case when the column-fill property is set to 'auto'.
    // No balancing, so don't limit the column count
    numColumns = INT32_MAX;

    // XXX_jwir3: If a page's height is set to 0, we could continually
    //            create continuations, resulting in an infinite loop, since
    //            no progress is ever made. This is an issue with the spec
    //            (css3-multicol, css3-page, and css3-break) that is
    //            unresolved as of 27 Feb 2013. For the time being, we set this
    //            to have a minimum of 1 css px. Once a resolution is made
    //            on what minimum to have for a page height, we may need to
    //            change this value to match the appropriate spec(s).
    colHeight = std::max(colHeight, nsPresContext::CSSPixelsToAppUnits(1));
  }

#ifdef DEBUG_roc
  printf("*** nsColumnSetFrame::ChooseColumnStrategy: numColumns=%d, colWidth=%d, expectedWidthLeftOver=%d, colHeight=%d, colGap=%d\n",
         numColumns, colWidth, expectedWidthLeftOver, colHeight, colGap);
#endif
  ReflowConfig config = { numColumns, colWidth, expectedWidthLeftOver, colGap,
                          colHeight, isBalancing, knownFeasibleHeight,
                          knownInfeasibleHeight, computedHeight, consumedHeight };
  return config;
}

bool
nsColumnSetFrame::ReflowColumns(nsHTMLReflowMetrics& aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus& aReflowStatus,
                                ReflowConfig& aConfig,
                                bool aLastColumnUnbounded,
                                nsCollapsingMargin* aCarriedOutBottomMargin,
                                ColumnBalanceData& aColData)
{
  bool feasible = ReflowChildren(aDesiredSize, aReflowState,
                                 aReflowStatus, aConfig, aLastColumnUnbounded,
                                 aCarriedOutBottomMargin, aColData);

  if (aColData.mHasExcessHeight) {
    aConfig = ChooseColumnStrategy(aReflowState, true);

    // We need to reflow our children again one last time, otherwise we might
    // end up with a stale column height for some of our columns, since we
    // bailed out of balancing.
    feasible = ReflowChildren(aDesiredSize, aReflowState, aReflowStatus,
                              aConfig, aLastColumnUnbounded,
                              aCarriedOutBottomMargin, aColData);
  }

  return feasible;
}

static void MoveChildTo(nsIFrame* aParent, nsIFrame* aChild, nsPoint aOrigin) {
  if (aChild->GetPosition() == aOrigin) {
    return;
  }

  aChild->SetPosition(aOrigin);
  nsContainerFrame::PlaceFrameView(aChild);
}

nscoord
nsColumnSetFrame::GetMinWidth(nsRenderingContext *aRenderingContext) {
  nscoord width = 0;
  DISPLAY_MIN_WIDTH(this, width);
  if (mFrames.FirstChild()) {
    width = mFrames.FirstChild()->GetMinWidth(aRenderingContext);
  }
  const nsStyleColumn* colStyle = StyleColumn();
  nscoord colWidth;
  if (colStyle->mColumnWidth.GetUnit() == eStyleUnit_Coord) {
    colWidth = colStyle->mColumnWidth.GetCoordValue();
    // As available width reduces to zero, we reduce our number of columns
    // to one, and don't enforce the column width, so just return the min
    // of the child's min-width with any specified column width.
    width = std::min(width, colWidth);
  } else {
    NS_ASSERTION(colStyle->mColumnCount > 0,
                 "column-count and column-width can't both be auto");
    // As available width reduces to zero, we still have mColumnCount columns,
    // so multiply the child's min-width by the number of columns.
    colWidth = width;
    width *= colStyle->mColumnCount;
    // The multiplication above can make 'width' negative (integer overflow),
    // so use std::max to protect against that.
    width = std::max(width, colWidth);
  }
  // XXX count forced column breaks here? Maybe we should return the child's
  // min-width times the minimum number of columns.
  return width;
}

nscoord
nsColumnSetFrame::GetPrefWidth(nsRenderingContext *aRenderingContext) {
  // Our preferred width is our desired column width, if specified, otherwise
  // the child's preferred width, times the number of columns, plus the width
  // of any required column gaps
  // XXX what about forced column breaks here?
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);
  const nsStyleColumn* colStyle = StyleColumn();
  nscoord colGap = GetColumnGap(this, colStyle);

  nscoord colWidth;
  if (colStyle->mColumnWidth.GetUnit() == eStyleUnit_Coord) {
    colWidth = colStyle->mColumnWidth.GetCoordValue();
  } else if (mFrames.FirstChild()) {
    colWidth = mFrames.FirstChild()->GetPrefWidth(aRenderingContext);
  } else {
    colWidth = 0;
  }

  int32_t numColumns = colStyle->mColumnCount;
  if (numColumns <= 0) {
    // if column-count is auto, assume one column
    numColumns = 1;
  }
  
  nscoord width = colWidth*numColumns + colGap*(numColumns - 1);
  // The multiplication above can make 'width' negative (integer overflow),
  // so use std::max to protect against that.
  result = std::max(width, colWidth);
  return result;
}

bool
nsColumnSetFrame::ReflowChildren(nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus&          aStatus,
                                 const ReflowConfig&      aConfig,
                                 bool                     aUnboundedLastColumn,
                                 nsCollapsingMargin*      aBottomMarginCarriedOut,
                                 ColumnBalanceData&       aColData)
{
  aColData.Reset();
  bool allFit = true;
  bool RTL = StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
  bool shrinkingHeightOnly = !NS_SUBTREE_DIRTY(this) &&
    mLastBalanceHeight > aConfig.mColMaxHeight;
  
#ifdef DEBUG_roc
  printf("*** Doing column reflow pass: mLastBalanceHeight=%d, mColMaxHeight=%d, RTL=%d\n, mBalanceColCount=%d, mColWidth=%d, mColGap=%d\n",
         mLastBalanceHeight, aConfig.mColMaxHeight, RTL, aConfig.mBalanceColCount,
         aConfig.mColWidth, aConfig.mColGap);
#endif

  DrainOverflowColumns();

  const bool colHeightChanged = mLastBalanceHeight != aConfig.mColMaxHeight;

  if (colHeightChanged) {
    mLastBalanceHeight = aConfig.mColMaxHeight;
    // XXX Seems like this could fire if incremental reflow pushed the column set
    // down so we reflow incrementally with a different available height.
    // We need a way to do an incremental reflow and be sure availableHeight
    // changes are taken account of! Right now I think block frames with absolute
    // children might exit early.
    //NS_ASSERTION(aKidReason != eReflowReason_Incremental,
    //             "incremental reflow should not have changed the balance height");
  }

  // get our border and padding
  nsMargin borderPadding = aReflowState.ComputedPhysicalBorderPadding();
  ApplySkipSides(borderPadding, &aReflowState);
  
  nsRect contentRect(0, 0, 0, 0);
  nsOverflowAreas overflowRects;

  nsIFrame* child = mFrames.FirstChild();
  nsPoint childOrigin = nsPoint(borderPadding.left, borderPadding.top);
  // For RTL, figure out where the last column's left edge should be. Since the
  // columns might not fill the frame exactly, we need to account for the
  // slop. Otherwise we'll waste time moving the columns by some tiny
  // amount unnecessarily.
  if (RTL) {
    nscoord availWidth = aReflowState.AvailableWidth();
    if (aReflowState.ComputedWidth() != NS_INTRINSICSIZE) {
      availWidth = aReflowState.ComputedWidth();
    }
    if (availWidth != NS_INTRINSICSIZE) {
      childOrigin.x += availWidth - aConfig.mColWidth;
#ifdef DEBUG_roc
      printf("*** childOrigin.x = %d\n", childOrigin.x);
#endif
    }
  }
  int columnCount = 0;
  int contentBottom = 0;
  bool reflowNext = false;

  while (child) {
    // Try to skip reflowing the child. We can't skip if the child is dirty. We also can't
    // skip if the next column is dirty, because the next column's first line(s)
    // might be pullable back to this column. We can't skip if it's the last child
    // because we need to obtain the bottom margin. We can't skip
    // if this is the last column and we're supposed to assign unbounded
    // height to it, because that could change the available height from
    // the last time we reflowed it and we should try to pull all the
    // content from its next sibling. (Note that it might be the last
    // column, but not be the last child because the desired number of columns
    // has changed.)
    bool skipIncremental = !aReflowState.ShouldReflowAllKids()
      && !NS_SUBTREE_DIRTY(child)
      && child->GetNextSibling()
      && !(aUnboundedLastColumn && columnCount == aConfig.mBalanceColCount - 1)
      && !NS_SUBTREE_DIRTY(child->GetNextSibling());
    // If we need to pull up content from the prev-in-flow then this is not just
    // a height shrink. The prev in flow will have set the dirty bit.
    // Check the overflow rect YMost instead of just the child's content height. The child
    // may have overflowing content that cares about the available height boundary.
    // (It may also have overflowing content that doesn't care about the available height
    // boundary, but if so, too bad, this optimization is defeated.)
    // We want scrollable overflow here since this is a calculation that
    // affects layout.
    bool skipResizeHeightShrink = shrinkingHeightOnly
      && child->GetScrollableOverflowRect().YMost() <= aConfig.mColMaxHeight;

    nscoord childContentBottom = 0;
    if (!reflowNext && (skipIncremental || skipResizeHeightShrink)) {
      // This child does not need to be reflowed, but we may need to move it
      MoveChildTo(this, child, childOrigin);
      
      // If this is the last frame then make sure we get the right status
      nsIFrame* kidNext = child->GetNextSibling();
      if (kidNext) {
        aStatus = (kidNext->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)
                  ? NS_FRAME_OVERFLOW_INCOMPLETE
                  : NS_FRAME_NOT_COMPLETE;
      } else {
        aStatus = mLastFrameStatus;
      }
      childContentBottom = nsLayoutUtils::CalculateContentBottom(child);
#ifdef DEBUG_roc
      printf("*** Skipping child #%d %p (incremental %d, resize height shrink %d): status = %d\n",
             columnCount, (void*)child, skipIncremental, skipResizeHeightShrink, aStatus);
#endif
    } else {
      nsSize availSize(aConfig.mColWidth, aConfig.mColMaxHeight);
      
      if (aUnboundedLastColumn && columnCount == aConfig.mBalanceColCount - 1) {
        availSize.height = GetAvailableContentHeight(aReflowState);
      }

      if (reflowNext)
        child->AddStateBits(NS_FRAME_IS_DIRTY);

      nsHTMLReflowState kidReflowState(PresContext(), aReflowState, child,
                                       availSize, availSize.width,
                                       aReflowState.ComputedHeight());
      kidReflowState.mFlags.mIsTopOfPage = true;
      kidReflowState.mFlags.mTableIsSplittable = false;
      kidReflowState.mFlags.mIsColumnBalancing = aConfig.mBalanceColCount < INT32_MAX;

      // We need to reflow any float placeholders, even if our column height
      // hasn't changed.
      kidReflowState.mFlags.mMustReflowPlaceholders = !colHeightChanged;

#ifdef DEBUG_roc
      printf("*** Reflowing child #%d %p: availHeight=%d\n",
             columnCount, (void*)child,availSize.height);
#endif

      // Note if the column's next in flow is not being changed by this incremental reflow.
      // This may allow the current column to avoid trying to pull lines from the next column.
      if (child->GetNextSibling() &&
          !(GetStateBits() & NS_FRAME_IS_DIRTY) &&
        !(child->GetNextSibling()->GetStateBits() & NS_FRAME_IS_DIRTY)) {
        kidReflowState.mFlags.mNextInFlowUntouched = true;
      }
    
      nsHTMLReflowMetrics kidDesiredSize(aReflowState.GetWritingMode(),
                                         aDesiredSize.mFlags);

      // XXX it would be cool to consult the float manager for the
      // previous block to figure out the region of floats from the
      // previous column that extend into this column, and subtract
      // that region from the new float manager.  So you could stick a
      // really big float in the first column and text in following
      // columns would flow around it.

      // Reflow the frame
      ReflowChild(child, PresContext(), kidDesiredSize, kidReflowState,
                  childOrigin.x + kidReflowState.ComputedPhysicalMargin().left,
                  childOrigin.y + kidReflowState.ComputedPhysicalMargin().top,
                  0, aStatus);

      reflowNext = (aStatus & NS_FRAME_REFLOW_NEXTINFLOW) != 0;
    
#ifdef DEBUG_roc
      printf("*** Reflowed child #%d %p: status = %d, desiredSize=%d,%d CarriedOutBottomMargin=%d\n",
             columnCount, (void*)child, aStatus, kidDesiredSize.Width(), kidDesiredSize.Height(),
             kidDesiredSize.mCarriedOutBottomMargin.get());
#endif

      NS_FRAME_TRACE_REFLOW_OUT("Column::Reflow", aStatus);

      *aBottomMarginCarriedOut = kidDesiredSize.mCarriedOutBottomMargin;
      
      FinishReflowChild(child, PresContext(), kidDesiredSize,
                        &kidReflowState, childOrigin.x, childOrigin.y, 0);

      childContentBottom = nsLayoutUtils::CalculateContentBottom(child);
      if (childContentBottom > aConfig.mColMaxHeight) {
        allFit = false;
      }
      if (childContentBottom > availSize.height) {
        aColData.mMaxOverflowingHeight = std::max(childContentBottom,
            aColData.mMaxOverflowingHeight);
      }
    }

    contentRect.UnionRect(contentRect, child->GetRect());

    ConsiderChildOverflow(overflowRects, child);
    contentBottom = std::max(contentBottom, childContentBottom);
    aColData.mLastHeight = childContentBottom;
    aColData.mSumHeight += childContentBottom;

    // Build a continuation column if necessary
    nsIFrame* kidNextInFlow = child->GetNextInFlow();

    if (NS_FRAME_IS_FULLY_COMPLETE(aStatus) && !NS_FRAME_IS_TRUNCATED(aStatus)) {
      NS_ASSERTION(!kidNextInFlow, "next in flow should have been deleted");
      child = nullptr;
      break;
    } else {
      ++columnCount;
      // Make sure that the column has a next-in-flow. If not, we must
      // create one to hold the overflowing stuff, even if we're just
      // going to put it on our overflow list and let *our*
      // next in flow handle it.
      if (!kidNextInFlow) {
        NS_ASSERTION(aStatus & NS_FRAME_REFLOW_NEXTINFLOW,
                     "We have to create a continuation, but the block doesn't want us to reflow it?");

        // We need to create a continuing column
        nsresult rv = CreateNextInFlow(PresContext(), child, kidNextInFlow);
        
        if (NS_FAILED(rv)) {
          NS_NOTREACHED("Couldn't create continuation");
          child = nullptr;
          break;
        }
      }

      // Make sure we reflow a next-in-flow when it switches between being
      // normal or overflow container
      if (NS_FRAME_OVERFLOW_IS_INCOMPLETE(aStatus)) {
        if (!(kidNextInFlow->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)) {
          aStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
          reflowNext = true;
          kidNextInFlow->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
        }
      }
      else if (kidNextInFlow->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
        aStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
        reflowNext = true;
        kidNextInFlow->RemoveStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
      }

      if ((contentBottom > aReflowState.ComputedMaxHeight() ||
           contentBottom > aReflowState.ComputedHeight()) &&
           aConfig.mBalanceColCount < INT32_MAX) {
        // We overflowed vertically, but have not exceeded the number of
        // columns. We're going to go into overflow columns now, so balancing
        // no longer applies.
        aColData.mHasExcessHeight = true;
      }

      if (columnCount >= aConfig.mBalanceColCount) {
        // No more columns allowed here. Stop.
        aStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
        kidNextInFlow->AddStateBits(NS_FRAME_IS_DIRTY);
        // Move any of our leftover columns to our overflow list. Our
        // next-in-flow will eventually pick them up.
        const nsFrameList& continuationColumns = mFrames.RemoveFramesAfter(child);
        if (continuationColumns.NotEmpty()) {
          SetOverflowFrames(PresContext(), continuationColumns);
        }
        child = nullptr;
        break;
      }
    }

    if (PresContext()->HasPendingInterrupt()) {
      // Stop the loop now while |child| still points to the frame that bailed
      // out.  We could keep going here and condition a bunch of the code in
      // this loop on whether there's an interrupt, or even just keep going and
      // trying to reflow the blocks (even though we know they'll interrupt
      // right after their first line), but stopping now is conceptually the
      // simplest (and probably fastest) thing.
      break;
    }

    // Advance to the next column
    child = child->GetNextSibling();

    if (child) {
      if (!RTL) {
        childOrigin.x += aConfig.mColWidth + aConfig.mColGap;
      } else {
        childOrigin.x -= aConfig.mColWidth + aConfig.mColGap;
      }
      
#ifdef DEBUG_roc
      printf("*** NEXT CHILD ORIGIN.x = %d\n", childOrigin.x);
#endif
    }
  }

  if (PresContext()->CheckForInterrupt(this) &&
      (GetStateBits() & NS_FRAME_IS_DIRTY)) {
    // Mark all our kids starting with |child| dirty

    // Note that this is a CheckForInterrupt call, not a HasPendingInterrupt,
    // because we might have interrupted while reflowing |child|, and since
    // we're about to add a dirty bit to |child| we need to make sure that
    // |this| is scheduled to have dirty bits marked on it and its ancestors.
    // Otherwise, when we go to mark dirty bits on |child|'s ancestors we'll
    // bail out immediately, since it'll already have a dirty bit.
    for (; child; child = child->GetNextSibling()) {
      child->AddStateBits(NS_FRAME_IS_DIRTY);
    }
  }
  
  aColData.mMaxHeight = contentBottom;
  contentRect.height = std::max(contentRect.height, contentBottom);
  mLastFrameStatus = aStatus;
  
  // contentRect included the borderPadding.left,borderPadding.top of the child rects
  contentRect -= nsPoint(borderPadding.left, borderPadding.top);
  
  nsSize contentSize = nsSize(contentRect.XMost(), contentRect.YMost());

  // Apply computed and min/max values
  if (aConfig.mComputedHeight != NS_INTRINSICSIZE) {
    if (aReflowState.AvailableHeight() != NS_INTRINSICSIZE) {
      contentSize.height = std::min(contentSize.height,
                                    aConfig.mComputedHeight);
    } else {
      contentSize.height = aConfig.mComputedHeight;
    }
  } else {
    // We add the "consumed" height back in so that we're applying
    // constraints to the correct height value, then subtract it again
    // after we've finished with the min/max calculation. This prevents us from
    // having a last continuation that is smaller than the min height. but which
    // has prev-in-flows, trigger a larger height than actually required.
    contentSize.height = aReflowState.ApplyMinMaxHeight(contentSize.height,
                                                        aConfig.mConsumedHeight);
  }
  if (aReflowState.ComputedWidth() != NS_INTRINSICSIZE) {
    contentSize.width = aReflowState.ComputedWidth();
  } else {
    contentSize.width = aReflowState.ApplyMinMaxWidth(contentSize.width);
  }

  aDesiredSize.Height() = contentSize.height +
                        borderPadding.TopBottom();
  aDesiredSize.Width() = contentSize.width +
                       borderPadding.LeftRight();
  aDesiredSize.mOverflowAreas = overflowRects;
  aDesiredSize.UnionOverflowAreasWithDesiredBounds();

#ifdef DEBUG_roc
  printf("*** DONE PASS feasible=%d\n", allFit && NS_FRAME_IS_FULLY_COMPLETE(aStatus)
         && !NS_FRAME_IS_TRUNCATED(aStatus));
#endif
  return allFit && NS_FRAME_IS_FULLY_COMPLETE(aStatus)
    && !NS_FRAME_IS_TRUNCATED(aStatus);
}

void
nsColumnSetFrame::DrainOverflowColumns()
{
  // First grab the prev-in-flows overflows and reparent them to this
  // frame.
  nsPresContext* presContext = PresContext();
  nsColumnSetFrame* prev = static_cast<nsColumnSetFrame*>(GetPrevInFlow());
  if (prev) {
    AutoFrameListPtr overflows(presContext, prev->StealOverflowFrames());
    if (overflows) {
      nsContainerFrame::ReparentFrameViewList(presContext, *overflows,
                                              prev, this);

      mFrames.InsertFrames(this, nullptr, *overflows);
    }
  }
  
  // Now pull back our own overflows and append them to our children.
  // We don't need to reparent them since we're already their parent.
  AutoFrameListPtr overflows(presContext, StealOverflowFrames());
  if (overflows) {
    // We're already the parent for these frames, so no need to set
    // their parent again.
    mFrames.AppendFrames(nullptr, *overflows);
  }
}

void
nsColumnSetFrame::FindBestBalanceHeight(const nsHTMLReflowState& aReflowState,
                                        nsPresContext* aPresContext,
                                        ReflowConfig& aConfig,
                                        ColumnBalanceData& aColData,
                                        nsHTMLReflowMetrics& aDesiredSize,
                                        nsCollapsingMargin& aOutMargin,
                                        bool& aUnboundedLastColumn,
                                        bool& aRunWasFeasible,
                                        nsReflowStatus& aStatus)
{
  bool feasible = aRunWasFeasible;

  nsMargin bp = aReflowState.ComputedPhysicalBorderPadding();
  ApplySkipSides(bp);
  bp.bottom = aReflowState.ComputedPhysicalBorderPadding().bottom;

  nscoord availableContentHeight =
    GetAvailableContentHeight(aReflowState);

  // Termination of the algorithm below is guaranteed because
  // aConfig.knownFeasibleHeight - aConfig.knownInfeasibleHeight decreases in every
  // iteration.

  // We set this flag when we detect that we may contain a frame
  // that can break anywhere (thus foiling the linear decrease-by-one
  // search)
  bool maybeContinuousBreakingDetected = false;

  while (!aPresContext->HasPendingInterrupt()) {
    nscoord lastKnownFeasibleHeight = aConfig.mKnownFeasibleHeight;

    // Record what we learned from the last reflow
    if (feasible) {
      // maxHeight is feasible. Also, mLastBalanceHeight is feasible.
      aConfig.mKnownFeasibleHeight = std::min(aConfig.mKnownFeasibleHeight,
                                              aColData.mMaxHeight);
      aConfig.mKnownFeasibleHeight = std::min(aConfig.mKnownFeasibleHeight,
                                              mLastBalanceHeight);

      // Furthermore, no height less than the height of the last
      // column can ever be feasible. (We might be able to reduce the
      // height of a non-last column by moving content to a later column,
      // but we can't do that with the last column.)
      if (mFrames.GetLength() == aConfig.mBalanceColCount) {
        aConfig.mKnownInfeasibleHeight = std::max(aConfig.mKnownInfeasibleHeight,
                                       aColData.mLastHeight - 1);
      }
    } else {
      aConfig.mKnownInfeasibleHeight = std::max(aConfig.mKnownInfeasibleHeight,
                                                mLastBalanceHeight);
      // If a column didn't fit in its available height, then its current
      // height must be the minimum height for unbreakable content in
      // the column, and therefore no smaller height can be feasible.
      aConfig.mKnownInfeasibleHeight = std::max(aConfig.mKnownInfeasibleHeight,
                                         aColData.mMaxOverflowingHeight - 1);

      if (aUnboundedLastColumn) {
        // The last column is unbounded, so all content got reflowed, so the
        // mColMaxHeight is feasible.
        aConfig.mKnownFeasibleHeight = std::min(aConfig.mKnownFeasibleHeight,
                                                aColData.mMaxHeight);
      }
    }

#ifdef DEBUG_roc
    printf("*** nsColumnSetFrame::Reflow balancing knownInfeasible=%d knownFeasible=%d\n",
           aConfig.mKnownInfeasibleHeight, aConfig.mKnownFeasibleHeight);
#endif


    if (aConfig.mKnownInfeasibleHeight >= aConfig.mKnownFeasibleHeight - 1) {
      // aConfig.mKnownFeasibleHeight is where we want to be
      break;
    }

    if (aConfig.mKnownInfeasibleHeight >= availableContentHeight) {
      break;
    }

    if (lastKnownFeasibleHeight - aConfig.mKnownFeasibleHeight == 1) {
      // We decreased the feasible height by one twip only. This could
      // indicate that there is a continuously breakable child frame
      // that we are crawling through.
      maybeContinuousBreakingDetected = true;
    }

    nscoord nextGuess = (aConfig.mKnownFeasibleHeight + aConfig.mKnownInfeasibleHeight)/2;
    // The constant of 600 twips is arbitrary. It's about two line-heights.
    if (aConfig.mKnownFeasibleHeight - nextGuess < 600 &&
        !maybeContinuousBreakingDetected) {
      // We're close to our target, so just try shrinking just the
      // minimum amount that will cause one of our columns to break
      // differently.
      nextGuess = aConfig.mKnownFeasibleHeight - 1;
    } else if (aUnboundedLastColumn) {
      // Make a guess by dividing that into N columns. Add some slop
      // to try to make it on the feasible side.  The constant of
      // 600 twips is arbitrary. It's about two line-heights.
      nextGuess = aColData.mSumHeight/aConfig.mBalanceColCount + 600;
      // Sanitize it
      nextGuess = clamped(nextGuess, aConfig.mKnownInfeasibleHeight + 1,
                                     aConfig.mKnownFeasibleHeight - 1);
    } else if (aConfig.mKnownFeasibleHeight == NS_INTRINSICSIZE) {
      // This can happen when we had a next-in-flow so we didn't
      // want to do an unbounded height measuring step. Let's just increase
      // from the infeasible height by some reasonable amount.
      nextGuess = aConfig.mKnownInfeasibleHeight*2 + 600;
    }
    // Don't bother guessing more than our height constraint.
    nextGuess = std::min(availableContentHeight, nextGuess);

#ifdef DEBUG_roc
    printf("*** nsColumnSetFrame::Reflow balancing choosing next guess=%d\n", nextGuess);
#endif

    aConfig.mColMaxHeight = nextGuess;

    aUnboundedLastColumn = false;
    AddStateBits(NS_FRAME_IS_DIRTY);
    feasible = ReflowColumns(aDesiredSize, aReflowState, aStatus, aConfig, false,
                             &aOutMargin, aColData);

    if (!aConfig.mIsBalancing) {
      // Looks like we had excess height when balancing, so we gave up on
      // trying to balance.
      break;
    }
  }

  if (aConfig.mIsBalancing && !feasible &&
      !aPresContext->HasPendingInterrupt()) {
    // We may need to reflow one more time at the feasible height to
    // get a valid layout.
    bool skip = false;
    if (aConfig.mKnownInfeasibleHeight >= availableContentHeight) {
      aConfig.mColMaxHeight = availableContentHeight;
      if (mLastBalanceHeight == availableContentHeight) {
        skip = true;
      }
    } else {
      aConfig.mColMaxHeight = aConfig.mKnownFeasibleHeight;
    }
    if (!skip) {
      // If our height is unconstrained, make sure that the last column is
      // allowed to have arbitrary height here, even though we were balancing.
      // Otherwise we'd have to split, and it's not clear what we'd do with
      // that.
      AddStateBits(NS_FRAME_IS_DIRTY);
      feasible = ReflowColumns(aDesiredSize, aReflowState, aStatus, aConfig,
                               availableContentHeight == NS_UNCONSTRAINEDSIZE,
                               &aOutMargin, aColData);
    }
  }

  aRunWasFeasible = feasible;
}

NS_IMETHODIMP 
nsColumnSetFrame::Reflow(nsPresContext*           aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus&          aStatus)
{
  // Don't support interruption in columns
  nsPresContext::InterruptPreventer noInterrupts(aPresContext);

  DO_GLOBAL_REFLOW_COUNT("nsColumnSetFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  // Our children depend on our height if we have a fixed height.
  if (aReflowState.ComputedHeight() != NS_AUTOHEIGHT) {
    NS_ASSERTION(aReflowState.ComputedHeight() != NS_INTRINSICSIZE,
                 "Unexpected computed height");
    AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
  }
  else {
    RemoveStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
  }

  //------------ Handle Incremental Reflow -----------------

  ReflowConfig config = ChooseColumnStrategy(aReflowState);
  
  // If balancing, then we allow the last column to grow to unbounded
  // height during the first reflow. This gives us a way to estimate
  // what the average column height should be, because we can measure
  // the heights of all the columns and sum them up. But don't do this
  // if we have a next in flow because we don't want to suck all its
  // content back here and then have to push it out again!
  nsIFrame* nextInFlow = GetNextInFlow();
  bool unboundedLastColumn = config.mIsBalancing && !nextInFlow;
  nsCollapsingMargin carriedOutBottomMargin;
  ColumnBalanceData colData;
  colData.mHasExcessHeight = false;

  bool feasible = ReflowColumns(aDesiredSize, aReflowState, aStatus, config,
                                unboundedLastColumn, &carriedOutBottomMargin,
                                colData);

  // If we're not balancing, then we're already done, since we should have
  // reflown all of our children, and there is no need for a binary search to
  // determine proper column height.
  if (config.mIsBalancing && !aPresContext->HasPendingInterrupt()) {
    FindBestBalanceHeight(aReflowState, aPresContext, config, colData,
                          aDesiredSize, carriedOutBottomMargin,
                          unboundedLastColumn, feasible, aStatus);
  }

  if (aPresContext->HasPendingInterrupt() &&
      aReflowState.AvailableHeight() == NS_UNCONSTRAINEDSIZE) {
    // In this situation, we might be lying about our reflow status, because
    // our last kid (the one that got interrupted) was incomplete.  Fix that.
    aStatus = NS_FRAME_COMPLETE;
  }

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowState, aStatus, false);

  aDesiredSize.mCarriedOutBottomMargin = carriedOutBottomMargin;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  NS_ASSERTION(NS_FRAME_IS_FULLY_COMPLETE(aStatus) ||
               aReflowState.AvailableHeight() != NS_UNCONSTRAINEDSIZE,
               "Column set should be complete if the available height is unconstrained");

  return NS_OK;
}

void
nsColumnSetFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                   const nsRect&           aDirtyRect,
                                   const nsDisplayListSet& aLists) {
  DisplayBorderBackgroundOutline(aBuilder, aLists);

  if (IsVisibleForPainting(aBuilder)) {
    aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
      nsDisplayGenericOverflow(aBuilder, this, ::PaintColumnRule, "ColumnRule",
                               nsDisplayItem::TYPE_COLUMN_RULE));
  }

  // Our children won't have backgrounds so it doesn't matter where we put them.
  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    BuildDisplayListForChild(aBuilder, e.get(), aDirtyRect, aLists);
  }
}

NS_IMETHODIMP
nsColumnSetFrame::AppendFrames(ChildListID     aListID,
                               nsFrameList&    aFrameList)
{
  if (aListID == kAbsoluteList) {
    return nsContainerFrame::AppendFrames(aListID, aFrameList);
  }

  NS_ERROR("unexpected child list");
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsColumnSetFrame::InsertFrames(ChildListID     aListID,
                               nsIFrame*       aPrevFrame,
                               nsFrameList&    aFrameList)
{
  if (aListID == kAbsoluteList) {
    return nsContainerFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
  }

  NS_ERROR("unexpected child list");
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsColumnSetFrame::RemoveFrame(ChildListID     aListID,
                              nsIFrame*       aOldFrame)
{
  if (aListID == kAbsoluteList) {
    return nsContainerFrame::RemoveFrame(aListID, aOldFrame);
  }

  NS_ERROR("unexpected child list");
  return NS_ERROR_INVALID_ARG;
}
