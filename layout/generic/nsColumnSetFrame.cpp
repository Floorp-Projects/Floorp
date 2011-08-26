/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <roc@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* rendering object for css3 multi-column layout */

#include "nsHTMLContainerFrame.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsHTMLParts.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsCOMPtr.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsCSSRendering.h"

class nsColumnSetFrame : public nsHTMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsColumnSetFrame(nsStyleContext* aContext);

  NS_IMETHOD SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList);

  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
                               
  NS_IMETHOD  AppendFrames(ChildListID     aListID,
                           nsFrameList&    aFrameList);
  NS_IMETHOD  InsertFrames(ChildListID     aListID,
                           nsIFrame*       aPrevFrame,
                           nsFrameList&    aFrameList);
  NS_IMETHOD  RemoveFrame(ChildListID     aListID,
                          nsIFrame*       aOldFrame);

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);  
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);

  virtual nsIFrame* GetContentInsertionFrame() {
    nsIFrame* frame = GetFirstPrincipalChild();

    // if no children return nsnull
    if (!frame)
      return nsnull;

    return frame->GetContentInsertionFrame();
  }

  virtual nsresult StealFrame(nsPresContext* aPresContext,
                              nsIFrame*      aChild,
                              PRBool         aForceNormal)
  { // nsColumnSetFrame keeps overflow containers in main child list
    return nsContainerFrame::StealFrame(aPresContext, aChild, PR_TRUE);
  }

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  virtual nsIAtom* GetType() const;

  virtual void PaintColumnRule(nsRenderingContext* aCtx,
                               const nsRect&        aDirtyRect,
                               const nsPoint&       aPt);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("ColumnSet"), aResult);
  }
#endif

protected:
  nscoord        mLastBalanceHeight;
  nsReflowStatus mLastFrameStatus;

  virtual PRIntn GetSkipSides() const;

  /**
   * These are the parameters that control the layout of columns.
   */
  struct ReflowConfig {
    PRInt32 mBalanceColCount;
    nscoord mColWidth;
    nscoord mExpectedWidthLeftOver;
    nscoord mColGap;
    nscoord mColMaxHeight;
  };

  /**
   * Some data that is better calculated during reflow
   */
  struct ColumnBalanceData {
    // The maximum "content height" of any column
    nscoord mMaxHeight;
    // The sum of the "content heights" for all columns
    nscoord mSumHeight;
    // The "content height" of the last column
    nscoord mLastHeight;
    // The maximum "content height" of all columns that overflowed
    // their available height
    nscoord mMaxOverflowingHeight;
    void Reset() {
      mMaxHeight = mSumHeight = mLastHeight = mMaxOverflowingHeight = 0;
    }
  };
  
  /**
   * Similar to nsBlockFrame::DrainOverflowLines. Locate any columns not
   * handled by our prev-in-flow, and any columns sitting on our own
   * overflow list, and put them in our primary child list for reflowing.
   */
  void DrainOverflowColumns();

  /**
   * The basic reflow strategy is to call this function repeatedly to
   * obtain specific parameters that determine the layout of the
   * columns. This function will compute those parameters from the CSS
   * style. This function will also be responsible for implementing
   * the state machine that controls column balancing.
   */
  ReflowConfig ChooseColumnStrategy(const nsHTMLReflowState& aReflowState);

  /**
   * Reflow column children. Returns PR_TRUE iff the content that was reflowed 
   * fit into the mColMaxHeight.
   */
  PRBool ReflowChildren(nsHTMLReflowMetrics& aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus& aStatus,
                        const ReflowConfig& aConfig,
                        PRBool aLastColumnUnbounded,
                        nsCollapsingMargin* aCarriedOutBottomMargin,
                        ColumnBalanceData& aColData);
};

/**
 * Tracking issues:
 *
 * XXX cursor movement around the top and bottom of colums seems to make the editor
 * lose the caret.
 *
 * XXX should we support CSS columns applied to table elements?
 */
nsIFrame*
NS_NewColumnSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aStateFlags)
{
  nsColumnSetFrame* it = new (aPresShell) nsColumnSetFrame(aContext);
  if (it) {
    // set the state flags (if any are provided)
    it->AddStateBits(aStateFlags);
  }

  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsColumnSetFrame)

nsColumnSetFrame::nsColumnSetFrame(nsStyleContext* aContext)
  : nsHTMLContainerFrame(aContext), mLastBalanceHeight(NS_INTRINSICSIZE),
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

  PRBool isRTL = GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
  const nsStyleColumn* colStyle = GetStyleColumn();

  PRUint8 ruleStyle;
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
        aDirtyRect, lineRect, border, GetStyleContext(),
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
  NS_ASSERTION(aListID == kPrincipalList,
               "Only default child list supported");
  NS_ASSERTION(aChildList.OnlyChild(),
               "initial child list must have exactly one child");
  // Queue up the frames for the content frame
  return nsHTMLContainerFrame::SetInitialChildList(kPrincipalList, aChildList);
}

static nscoord
GetAvailableContentWidth(const nsHTMLReflowState& aReflowState)
{
  if (aReflowState.availableWidth == NS_INTRINSICSIZE) {
    return NS_INTRINSICSIZE;
  }
  nscoord borderPaddingWidth =
    aReflowState.mComputedBorderPadding.left +
    aReflowState.mComputedBorderPadding.right;
  return NS_MAX(0, aReflowState.availableWidth - borderPaddingWidth);
}

static nscoord
GetAvailableContentHeight(const nsHTMLReflowState& aReflowState)
{
  if (aReflowState.availableHeight == NS_INTRINSICSIZE) {
    return NS_INTRINSICSIZE;
  }
  nscoord borderPaddingHeight =
    aReflowState.mComputedBorderPadding.top +
    aReflowState.mComputedBorderPadding.bottom;
  return NS_MAX(0, aReflowState.availableHeight - borderPaddingHeight);
}

static nscoord
GetColumnGap(nsColumnSetFrame*    aFrame,
             const nsStyleColumn* aColStyle)
{
  if (eStyleUnit_Normal == aColStyle->mColumnGap.GetUnit())
    return aFrame->GetStyleFont()->mFont.size;
  if (eStyleUnit_Coord == aColStyle->mColumnGap.GetUnit()) {
    nscoord colGap = aColStyle->mColumnGap.GetCoordValue();
    NS_ASSERTION(colGap >= 0, "negative column gap");
    return colGap;
  }

  NS_NOTREACHED("Unknown gap type");
  return 0;
}

nsColumnSetFrame::ReflowConfig
nsColumnSetFrame::ChooseColumnStrategy(const nsHTMLReflowState& aReflowState)
{
  const nsStyleColumn* colStyle = GetStyleColumn();
  nscoord availContentWidth = GetAvailableContentWidth(aReflowState);
  if (aReflowState.ComputedWidth() != NS_INTRINSICSIZE) {
    availContentWidth = aReflowState.ComputedWidth();
  }
  nscoord colHeight = GetAvailableContentHeight(aReflowState);
  if (aReflowState.ComputedHeight() != NS_INTRINSICSIZE) {
    colHeight = aReflowState.ComputedHeight();
  }

  nscoord colGap = GetColumnGap(this, colStyle);
  PRInt32 numColumns = colStyle->mColumnCount;

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
      PRInt32 maxColumns = (availContentWidth + colGap)/(colGap + colWidth);
      numColumns = NS_MAX(1, NS_MIN(numColumns, maxColumns));
    }
  } else if (numColumns > 0 && availContentWidth != NS_INTRINSICSIZE) {
    nscoord widthMinusGaps = availContentWidth - colGap*(numColumns - 1);
    colWidth = widthMinusGaps/numColumns;
  } else {
    colWidth = NS_INTRINSICSIZE;
  }
  // Take care of the situation where there's only one column but it's
  // still too wide
  colWidth = NS_MAX(1, NS_MIN(colWidth, availContentWidth));

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
      }
      if (numColumns <= 0) {
        numColumns = 1;
      }
    }

    // Compute extra space and divide it among the columns
    nscoord extraSpace =
      NS_MAX(0, availContentWidth - (colWidth*numColumns + colGap*(numColumns - 1)));
    nscoord extraToColumns = extraSpace/numColumns;
    colWidth += extraToColumns;
    expectedWidthLeftOver = extraSpace - (extraToColumns*numColumns);
  }

  // NOTE that the non-balancing behavior for non-auto computed height
  // is not in the CSS3 columns draft as of 18 January 2001
  if (aReflowState.ComputedHeight() == NS_INTRINSICSIZE) {
    // Balancing!
    if (numColumns <= 0) {
      // Hmm, auto column count, column width or available width is unknown,
      // and balancing is required. Let's just use one column then.
      numColumns = 1;
    }
    colHeight = NS_MIN(mLastBalanceHeight, GetAvailableContentHeight(aReflowState));
  } else {
    // No balancing, so don't limit the column count
    numColumns = PR_INT32_MAX;
  }

#ifdef DEBUG_roc
  printf("*** nsColumnSetFrame::ChooseColumnStrategy: numColumns=%d, colWidth=%d, expectedWidthLeftOver=%d, colHeight=%d, colGap=%d\n",
         numColumns, colWidth, expectedWidthLeftOver, colHeight, colGap);
#endif
  ReflowConfig config = { numColumns, colWidth, expectedWidthLeftOver, colGap, colHeight };
  return config;
}

// XXX copied from nsBlockFrame, should this be moved to nsContainerFrame?
static void
PlaceFrameView(nsIFrame* aFrame)
{
  if (aFrame->HasView())
    nsContainerFrame::PositionFrameView(aFrame);
  else
    nsContainerFrame::PositionChildViews(aFrame);
}

static void MoveChildTo(nsIFrame* aParent, nsIFrame* aChild, nsPoint aOrigin) {
  if (aChild->GetPosition() == aOrigin) {
    return;
  }
  
  nsRect r = aChild->GetVisualOverflowRect();
  r += aChild->GetPosition();
  aParent->Invalidate(r);
  r -= aChild->GetPosition();
  aChild->SetPosition(aOrigin);
  r += aOrigin;
  aParent->Invalidate(r);
  PlaceFrameView(aChild);
}

nscoord
nsColumnSetFrame::GetMinWidth(nsRenderingContext *aRenderingContext) {
  nscoord width = 0;
  DISPLAY_MIN_WIDTH(this, width);
  if (mFrames.FirstChild()) {
    width = mFrames.FirstChild()->GetMinWidth(aRenderingContext);
  }
  const nsStyleColumn* colStyle = GetStyleColumn();
  nscoord colWidth;
  if (colStyle->mColumnWidth.GetUnit() == eStyleUnit_Coord) {
    colWidth = colStyle->mColumnWidth.GetCoordValue();
    // As available width reduces to zero, we reduce our number of columns
    // to one, and don't enforce the column width, so just return the min
    // of the child's min-width with any specified column width.
    width = NS_MIN(width, colWidth);
  } else {
    NS_ASSERTION(colStyle->mColumnCount > 0,
                 "column-count and column-width can't both be auto");
    // As available width reduces to zero, we still have mColumnCount columns,
    // so multiply the child's min-width by the number of columns.
    colWidth = width;
    width *= colStyle->mColumnCount;
    // The multiplication above can make 'width' negative (integer overflow),
    // so use NS_MAX to protect against that.
    width = NS_MAX(width, colWidth);
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
  const nsStyleColumn* colStyle = GetStyleColumn();
  nscoord colGap = GetColumnGap(this, colStyle);

  nscoord colWidth;
  if (colStyle->mColumnWidth.GetUnit() == eStyleUnit_Coord) {
    colWidth = colStyle->mColumnWidth.GetCoordValue();
  } else if (mFrames.FirstChild()) {
    colWidth = mFrames.FirstChild()->GetPrefWidth(aRenderingContext);
  } else {
    colWidth = 0;
  }

  PRInt32 numColumns = colStyle->mColumnCount;
  if (numColumns <= 0) {
    // if column-count is auto, assume one column
    numColumns = 1;
  }
  
  nscoord width = colWidth*numColumns + colGap*(numColumns - 1);
  // The multiplication above can make 'width' negative (integer overflow),
  // so use NS_MAX to protect against that.
  result = NS_MAX(width, colWidth);
  return result;
}

PRBool
nsColumnSetFrame::ReflowChildren(nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus&          aStatus,
                                 const ReflowConfig&      aConfig,
                                 PRBool                   aUnboundedLastColumn,
                                 nsCollapsingMargin*      aBottomMarginCarriedOut,
                                 ColumnBalanceData&       aColData)
{
  aColData.Reset();
  PRBool allFit = PR_TRUE;
  PRBool RTL = GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
  PRBool shrinkingHeightOnly = !NS_SUBTREE_DIRTY(this) &&
    mLastBalanceHeight > aConfig.mColMaxHeight;
  
#ifdef DEBUG_roc
  printf("*** Doing column reflow pass: mLastBalanceHeight=%d, mColMaxHeight=%d, RTL=%d\n, mBalanceColCount=%d, mColWidth=%d, mColGap=%d\n",
         mLastBalanceHeight, aConfig.mColMaxHeight, RTL, aConfig.mBalanceColCount,
         aConfig.mColWidth, aConfig.mColGap);
#endif

  DrainOverflowColumns();
  
  if (mLastBalanceHeight != aConfig.mColMaxHeight) {
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
  const nsMargin &borderPadding = aReflowState.mComputedBorderPadding;
  
  nsRect contentRect(0, 0, 0, 0);
  nsOverflowAreas overflowRects;

  nsIFrame* child = mFrames.FirstChild();
  nsPoint childOrigin = nsPoint(borderPadding.left, borderPadding.top);
  // For RTL, figure out where the last column's left edge should be. Since the
  // columns might not fill the frame exactly, we need to account for the
  // slop. Otherwise we'll waste time moving the columns by some tiny
  // amount unnecessarily.
  nscoord targetX = borderPadding.left;
  if (RTL) {
    nscoord availWidth = aReflowState.availableWidth;
    if (aReflowState.ComputedWidth() != NS_INTRINSICSIZE) {
      availWidth = aReflowState.ComputedWidth();
    }
    if (availWidth != NS_INTRINSICSIZE) {
      childOrigin.x += availWidth - aConfig.mColWidth;
      targetX += aConfig.mExpectedWidthLeftOver;
#ifdef DEBUG_roc
      printf("*** childOrigin.x = %d\n", childOrigin.x);
#endif
    }
  }
  int columnCount = 0;
  int contentBottom = 0;
  PRBool reflowNext = PR_FALSE;

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
    PRBool skipIncremental = !aReflowState.ShouldReflowAllKids()
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
    PRBool skipResizeHeightShrink = shrinkingHeightOnly
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
      kidReflowState.mFlags.mIsTopOfPage = PR_TRUE;
      kidReflowState.mFlags.mTableIsSplittable = PR_FALSE;
          
#ifdef DEBUG_roc
      printf("*** Reflowing child #%d %p: availHeight=%d\n",
             columnCount, (void*)child,availSize.height);
#endif

      // Note if the column's next in flow is not being changed by this incremental reflow.
      // This may allow the current column to avoid trying to pull lines from the next column.
      if (child->GetNextSibling() &&
          !(GetStateBits() & NS_FRAME_IS_DIRTY) &&
        !(child->GetNextSibling()->GetStateBits() & NS_FRAME_IS_DIRTY)) {
        kidReflowState.mFlags.mNextInFlowUntouched = PR_TRUE;
      }
    
      nsHTMLReflowMetrics kidDesiredSize(aDesiredSize.mFlags);

      // XXX it would be cool to consult the float manager for the
      // previous block to figure out the region of floats from the
      // previous column that extend into this column, and subtract
      // that region from the new float manager.  So you could stick a
      // really big float in the first column and text in following
      // columns would flow around it.

      // Reflow the frame
      ReflowChild(child, PresContext(), kidDesiredSize, kidReflowState,
                  childOrigin.x + kidReflowState.mComputedMargin.left,
                  childOrigin.y + kidReflowState.mComputedMargin.top,
                  0, aStatus);

      reflowNext = (aStatus & NS_FRAME_REFLOW_NEXTINFLOW) != 0;
    
#ifdef DEBUG_roc
      printf("*** Reflowed child #%d %p: status = %d, desiredSize=%d,%d\n",
             columnCount, (void*)child, aStatus, kidDesiredSize.width, kidDesiredSize.height);
#endif

      NS_FRAME_TRACE_REFLOW_OUT("Column::Reflow", aStatus);

      *aBottomMarginCarriedOut = kidDesiredSize.mCarriedOutBottomMargin;
      
      FinishReflowChild(child, PresContext(), &kidReflowState, 
                        kidDesiredSize, childOrigin.x, childOrigin.y, 0);

      childContentBottom = nsLayoutUtils::CalculateContentBottom(child);
      if (childContentBottom > aConfig.mColMaxHeight) {
        allFit = PR_FALSE;
      }
      if (childContentBottom > availSize.height) {
        aColData.mMaxOverflowingHeight = NS_MAX(childContentBottom,
            aColData.mMaxOverflowingHeight);
      }
    }

    contentRect.UnionRect(contentRect, child->GetRect());

    ConsiderChildOverflow(overflowRects, child);
    contentBottom = NS_MAX(contentBottom, childContentBottom);
    aColData.mLastHeight = childContentBottom;
    aColData.mSumHeight += childContentBottom;

    // Build a continuation column if necessary
    nsIFrame* kidNextInFlow = child->GetNextInFlow();

    if (NS_FRAME_IS_FULLY_COMPLETE(aStatus) && !NS_FRAME_IS_TRUNCATED(aStatus)) {
      NS_ASSERTION(!kidNextInFlow, "next in flow should have been deleted");
      child = nsnull;
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
          child = nsnull;
          break;
        }
      }

      // Make sure we reflow a next-in-flow when it switches between being
      // normal or overflow container
      if (NS_FRAME_OVERFLOW_IS_INCOMPLETE(aStatus)) {
        if (!(kidNextInFlow->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)) {
          aStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
          reflowNext = PR_TRUE;
          kidNextInFlow->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
        }
      }
      else if (kidNextInFlow->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
        aStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
        reflowNext = PR_TRUE;
        kidNextInFlow->RemoveStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
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
        child = nsnull;
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
  
  // If we're doing RTL, we need to make sure our last column is at the left-hand side of the frame.
  if (RTL && childOrigin.x != targetX) {
    overflowRects.Clear();
    contentRect = nsRect(0, 0, 0, 0);
    PRInt32 deltaX = targetX - childOrigin.x;
#ifdef DEBUG_roc
    printf("*** CHILDORIGIN.x = %d, targetX = %d, DELTAX = %d\n", childOrigin.x, targetX, deltaX);
#endif
    for (child = mFrames.FirstChild(); child; child = child->GetNextSibling()) {
      MoveChildTo(this, child, child->GetPosition() + nsPoint(deltaX, 0));
      ConsiderChildOverflow(overflowRects, child);
      contentRect.UnionRect(contentRect, child->GetRect());
    }
  }
  aColData.mMaxHeight = contentBottom;
  contentRect.height = NS_MAX(contentRect.height, contentBottom);
  mLastFrameStatus = aStatus;
  
  // contentRect included the borderPadding.left,borderPadding.top of the child rects
  contentRect -= nsPoint(borderPadding.left, borderPadding.top);
  
  nsSize contentSize = nsSize(contentRect.XMost(), contentRect.YMost());

  // Apply computed and min/max values
  if (aReflowState.ComputedHeight() != NS_INTRINSICSIZE) {
    contentSize.height = aReflowState.ComputedHeight();
  } else {
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMaxHeight) {
      contentSize.height = NS_MIN(aReflowState.mComputedMaxHeight, contentSize.height);
    }
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMinHeight) {
      contentSize.height = NS_MAX(aReflowState.mComputedMinHeight, contentSize.height);
    }
  }
  if (aReflowState.ComputedWidth() != NS_INTRINSICSIZE) {
    contentSize.width = aReflowState.ComputedWidth();
  } else {
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMaxWidth) {
      contentSize.width = NS_MIN(aReflowState.mComputedMaxWidth, contentSize.width);
    }
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMinWidth) {
      contentSize.width = NS_MAX(aReflowState.mComputedMinWidth, contentSize.width);
    }
  }
    
  aDesiredSize.height = borderPadding.top + contentSize.height +
    borderPadding.bottom;
  aDesiredSize.width = contentSize.width + borderPadding.left + borderPadding.right;
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
  nsColumnSetFrame* prev = static_cast<nsColumnSetFrame*>(GetPrevInFlow());
  if (prev) {
    nsAutoPtr<nsFrameList> overflows(prev->StealOverflowFrames());
    if (overflows) {
      nsContainerFrame::ReparentFrameViewList(PresContext(), *overflows,
                                              prev, this);

      mFrames.InsertFrames(this, nsnull, *overflows);
    }
  }
  
  // Now pull back our own overflows and append them to our children.
  // We don't need to reparent them since we're already their parent.
  nsAutoPtr<nsFrameList> overflows(StealOverflowFrames());
  if (overflows) {
    // We're already the parent for these frames, so no need to set
    // their parent again.
    mFrames.AppendFrames(nsnull, *overflows);
  }
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
                 "Unexpected mComputedHeight");
    AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
  }
  else {
    RemoveStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
  }

  //------------ Handle Incremental Reflow -----------------

  ReflowConfig config = ChooseColumnStrategy(aReflowState);
  PRBool isBalancing = config.mBalanceColCount < PR_INT32_MAX;
  
  // If balancing, then we allow the last column to grow to unbounded
  // height during the first reflow. This gives us a way to estimate
  // what the average column height should be, because we can measure
  // the heights of all the columns and sum them up. But don't do this
  // if we have a next in flow because we don't want to suck all its
  // content back here and then have to push it out again!
  nsIFrame* nextInFlow = GetNextInFlow();
  PRBool unboundedLastColumn = isBalancing && !nextInFlow;
  nsCollapsingMargin carriedOutBottomMargin;
  ColumnBalanceData colData;
  PRBool feasible = ReflowChildren(aDesiredSize, aReflowState,
    aStatus, config, unboundedLastColumn, &carriedOutBottomMargin, colData);

  if (isBalancing && !aPresContext->HasPendingInterrupt()) {
    nscoord availableContentHeight = GetAvailableContentHeight(aReflowState);
  
    // Termination of the algorithm below is guaranteed because
    // knownFeasibleHeight - knownInfeasibleHeight decreases in every
    // iteration.
    nscoord knownFeasibleHeight = NS_INTRINSICSIZE;
    nscoord knownInfeasibleHeight = 0;
    // We set this flag when we detect that we may contain a frame
    // that can break anywhere (thus foiling the linear decrease-by-one
    // search)
    PRBool maybeContinuousBreakingDetected = PR_FALSE;

    while (!aPresContext->HasPendingInterrupt()) {
      nscoord lastKnownFeasibleHeight = knownFeasibleHeight;

      // Record what we learned from the last reflow
      if (feasible) {
        // maxHeight is feasible. Also, mLastBalanceHeight is feasible.
        knownFeasibleHeight = NS_MIN(knownFeasibleHeight, colData.mMaxHeight);
        knownFeasibleHeight = NS_MIN(knownFeasibleHeight, mLastBalanceHeight);

        // Furthermore, no height less than the height of the last
        // column can ever be feasible. (We might be able to reduce the
        // height of a non-last column by moving content to a later column,
        // but we can't do that with the last column.)
        if (mFrames.GetLength() == config.mBalanceColCount) {
          knownInfeasibleHeight = NS_MAX(knownInfeasibleHeight,
                                         colData.mLastHeight - 1);
        }
      } else {
        knownInfeasibleHeight = NS_MAX(knownInfeasibleHeight, mLastBalanceHeight);
        // If a column didn't fit in its available height, then its current
        // height must be the minimum height for unbreakable content in
        // the column, and therefore no smaller height can be feasible.
        knownInfeasibleHeight = NS_MAX(knownInfeasibleHeight,
                                       colData.mMaxOverflowingHeight - 1);

        if (unboundedLastColumn) {
          // The last column is unbounded, so all content got reflowed, so the
          // mColMaxHeight is feasible.
          knownFeasibleHeight = NS_MIN(knownFeasibleHeight,
                                       colData.mMaxHeight);
        }
      }

#ifdef DEBUG_roc
      printf("*** nsColumnSetFrame::Reflow balancing knownInfeasible=%d knownFeasible=%d\n",
             knownInfeasibleHeight, knownFeasibleHeight);
#endif

      if (knownInfeasibleHeight >= knownFeasibleHeight - 1) {
        // knownFeasibleHeight is where we want to be
        break;
      }

      if (knownInfeasibleHeight >= availableContentHeight) {
        break;
      }

      if (lastKnownFeasibleHeight - knownFeasibleHeight == 1) {
        // We decreased the feasible height by one twip only. This could
        // indicate that there is a continuously breakable child frame
        // that we are crawling through.
        maybeContinuousBreakingDetected = PR_TRUE;
      }

      nscoord nextGuess = (knownFeasibleHeight + knownInfeasibleHeight)/2;
      // The constant of 600 twips is arbitrary. It's about two line-heights.
      if (knownFeasibleHeight - nextGuess < 600 &&
          !maybeContinuousBreakingDetected) {
        // We're close to our target, so just try shrinking just the
        // minimum amount that will cause one of our columns to break
        // differently.
        nextGuess = knownFeasibleHeight - 1;
      } else if (unboundedLastColumn) {
        // Make a guess by dividing that into N columns. Add some slop
        // to try to make it on the feasible side.  The constant of
        // 600 twips is arbitrary. It's about two line-heights.
        nextGuess = colData.mSumHeight/config.mBalanceColCount + 600;
        // Sanitize it
        nextGuess = NS_MIN(NS_MAX(nextGuess, knownInfeasibleHeight + 1),
                           knownFeasibleHeight - 1);
      } else if (knownFeasibleHeight == NS_INTRINSICSIZE) {
        // This can happen when we had a next-in-flow so we didn't
        // want to do an unbounded height measuring step. Let's just increase
        // from the infeasible height by some reasonable amount.
        nextGuess = knownInfeasibleHeight*2 + 600;
      }
      // Don't bother guessing more than our height constraint.
      nextGuess = NS_MIN(availableContentHeight, nextGuess);

#ifdef DEBUG_roc
      printf("*** nsColumnSetFrame::Reflow balancing choosing next guess=%d\n", nextGuess);
#endif

      config.mColMaxHeight = nextGuess;
      
      unboundedLastColumn = PR_FALSE;
      AddStateBits(NS_FRAME_IS_DIRTY);
      feasible = ReflowChildren(aDesiredSize, aReflowState,
                                aStatus, config, PR_FALSE, 
                                &carriedOutBottomMargin, colData);
    }

    if (!feasible && !aPresContext->HasPendingInterrupt()) {
      // We may need to reflow one more time at the feasible height to
      // get a valid layout.
      PRBool skip = PR_FALSE;
      if (knownInfeasibleHeight >= availableContentHeight) {
        config.mColMaxHeight = availableContentHeight;
        if (mLastBalanceHeight == availableContentHeight) {
          skip = PR_TRUE;
        }
      } else {
        config.mColMaxHeight = knownFeasibleHeight;
      }
      if (!skip) {
        // If our height is unconstrained, make sure that the last column is
        // allowed to have arbitrary height here, even though we were balancing.
        // Otherwise we'd have to split, and it's not clear what we'd do with
        // that.
        AddStateBits(NS_FRAME_IS_DIRTY);
        ReflowChildren(aDesiredSize, aReflowState, aStatus, config,
                       availableContentHeight == NS_UNCONSTRAINEDSIZE,
                       &carriedOutBottomMargin, colData);
      }
    }
  }

  if (aPresContext->HasPendingInterrupt() &&
      aReflowState.availableHeight == NS_UNCONSTRAINEDSIZE) {
    // In this situation, we might be lying about our reflow status, because
    // our last kid (the one that got interrupted) was incomplete.  Fix that.
    aStatus = NS_FRAME_COMPLETE;
  }
  
  CheckInvalidateSizeChange(aDesiredSize);

  FinishAndStoreOverflow(&aDesiredSize);
  aDesiredSize.mCarriedOutBottomMargin = carriedOutBottomMargin;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  NS_ASSERTION(NS_FRAME_IS_FULLY_COMPLETE(aStatus) ||
               aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE,
               "Column set should be complete if the available height is unconstrained");

  return NS_OK;
}

NS_IMETHODIMP
nsColumnSetFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                   const nsRect&           aDirtyRect,
                                   const nsDisplayListSet& aLists) {
  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
      nsDisplayGeneric(aBuilder, this, ::PaintColumnRule, "ColumnRule",
                       nsDisplayItem::TYPE_COLUMN_RULE));
  
  nsIFrame* kid = mFrames.FirstChild();
  // Our children won't have backgrounds so it doesn't matter where we put them.
  while (kid) {
    nsresult rv = BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}

PRIntn
nsColumnSetFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsColumnSetFrame::AppendFrames(ChildListID     aListID,
                               nsFrameList&    aFrameList)
{
  NS_NOTREACHED("AppendFrames not supported");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsColumnSetFrame::InsertFrames(ChildListID     aListID,
                               nsIFrame*       aPrevFrame,
                               nsFrameList&    aFrameList)
{
  NS_NOTREACHED("InsertFrames not supported");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsColumnSetFrame::RemoveFrame(ChildListID     aListID,
                              nsIFrame*       aOldFrame)
{
  NS_NOTREACHED("RemoveFrame not supported");
  return NS_ERROR_NOT_IMPLEMENTED;
}
